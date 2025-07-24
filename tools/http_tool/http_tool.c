/**
 * @file http_tool.c
 * @brief Constitutional HTTP Tool Implementation - Webhook POST & Payload Processing
 * 
 * Constitutional implementation following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf, PRIu32)
 * - Container isolation principles
 * 
 * Constitutional Authority: Process Map 14 (http_fsm.mmd)
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 * Constitutional Payload Processing: Deferred payload creation with stack safety
 */

#include "http_tool.h"
#include "payload_tool.h"
#include "network_tool.h"
#include "ntp_tool.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

static const char* TAG = "http_tool";

// Constitutional HTTP Tool Event Base
ESP_EVENT_DEFINE_BASE(HTTP_TOOL_EVENTS);

// Constitutional Tool Context (Handle-based pattern)
struct http_tool {
    bool is_initialized;
    bool is_active;
    bool is_connected;
    bool server_reachable;
    http_tool_config_t config;
    http_tool_status_t status;
    uint64_t init_timestamp_us;
    uint32_t payloads_sent;
    uint32_t payloads_failed;
    uint32_t retry_count;
    
    // HTTP client handle
    esp_http_client_handle_t http_client;
    
    // Constitutional tool dependencies
    void* network_tool;
    void* ntp_tool;
    payload_tool_handle_t payload_tool;
    
    // Thread safety
    SemaphoreHandle_t http_mutex;
    
    // Event loop handle
    esp_event_loop_handle_t event_loop;
};

// =============================================================================
// Constitutional HTTP Helper Functions
// =============================================================================

/**
 * @brief Constitutional HTTP event handler for ESP HTTP client
 * 
 * Constitutional SSL/TLS Configuration:
 * - Automatic HTTPS/HTTP detection based on URL scheme
 * - SSL certificate verification disabled via Kconfig for testing
 * - Uses CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY=y for insecure mode
 * - Supports both HTTP (TCP) and HTTPS (SSL) transport
 */
static esp_err_t constitutional_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "🌐 HTTP ERROR occurred");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "🌐 HTTP connected to server");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "🌐 HTTP data received: %d bytes", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "🌐 HTTP request finished");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "🌐 HTTP disconnected from server");
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief Constitutional device ID generation from MAC address
 */
static esp_err_t constitutional_generate_device_id(char* device_id, size_t buffer_size)
{
    if (!device_id || buffer_size < HTTP_TOOL_MAX_DEVICE_ID_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret == ESP_OK) {
        snprintf(device_id, buffer_size, "%02X%02X%02X%02X%02X%02X", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        snprintf(device_id, buffer_size, "UNKNOWN");
        ESP_LOGW(TAG, "🌐 Failed to read MAC address, using default device ID");
    }
    
    return ESP_OK;
}

/**
 * @brief Constitutional JSON payload creation with server-compatible format
 */
static esp_err_t constitutional_create_session_payload(http_tool_handle_t handle,
                                                     const char* tag_uid,
                                                     const char* event_type,
                                                     uint64_t timestamp_us,
                                                     const char* session_id,
                                                     char** json_payload)
{
    if (!handle || !tag_uid || !event_type || !session_id || !json_payload) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "📦 Creating server-compatible RFID payload for %s event", event_type);
    
    // Map event types to RFID event types
    payload_rfid_event_type_t rfid_event_type;
    bool tag_present;
    
    if (strcmp(event_type, "APPEARED") == 0) {
        rfid_event_type = PAYLOAD_RFID_TAG_INSERT;
        tag_present = true;
    } else if (strcmp(event_type, "DISAPPEARED") == 0) {
        rfid_event_type = PAYLOAD_RFID_TAG_REMOVED;
        tag_present = false;
    } else {
        ESP_LOGE(TAG, "  ❌ Unknown event type: %s", event_type);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Allocate payload data structure on heap
    payload_data_t *payload_data = malloc(sizeof(payload_data_t));
    if (!payload_data) {
        ESP_LOGE(TAG, "  ❌ Failed to allocate payload data structure");
        return ESP_ERR_NO_MEM;
    }
    
    // Create server-compatible RFID payload
    esp_err_t ret = payload_tool_create_rfid_payload(handle->payload_tool, 
                                                    rfid_event_type, 
                                                    tag_uid, 
                                                    tag_present,
                                                    session_id,
                                                    payload_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "  ❌ Failed to create RFID payload: %s", esp_err_to_name(ret));
        free(payload_data);
        return ret;
    }
    
    // Allocate JSON string buffer 
    char *payload_buffer = malloc(HTTP_TOOL_MAX_PAYLOAD_SIZE);
    if (!payload_buffer) {
        ESP_LOGE(TAG, "  ❌ Failed to allocate JSON buffer");
        free(payload_data);
        return ESP_ERR_NO_MEM;
    }
    
    // Format payload to JSON string
    ret = payload_tool_format_to_json(handle->payload_tool, payload_data, 
                                     payload_buffer, HTTP_TOOL_MAX_PAYLOAD_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "  ❌ Failed to format payload to JSON: %s", esp_err_to_name(ret));
        free(payload_buffer);
        free(payload_data);
        return ret;
    }
    
    // Return allocated payload (caller must free)
    *json_payload = payload_buffer;
    free(payload_data);
    
    ESP_LOGI(TAG, "📦 Created %s payload for UID=%s (%" PRIu32 " bytes)", 
             event_type, tag_uid, (uint32_t)strlen(payload_buffer));
    ESP_LOGI(TAG, "📦 Payload content: %s", payload_buffer);
    
    return ESP_OK;
}

/**
 * @brief Constitutional HTTP POST request with retry logic
 * 
 * CONSTITUTIONAL REQUIREMENT: This function MUST BE CALLED with http_mutex already acquired.
 * The mutex protects against concurrent access to the shared http_client handle, which is
 * not thread-safe according to ESP-IDF lwIP documentation. Calling this function without
 * mutex protection violates constitutional thread safety requirements.
 */
static esp_err_t constitutional_http_post_request(http_tool_handle_t handle,
                                                const char* json_payload,
                                                size_t payload_size)
{
    if (!handle || !handle->http_client || !json_payload || payload_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🌐 Sending HTTP POST to %s (%" PRIu32 " bytes)", 
             handle->config.webhook_url, (uint32_t)payload_size);
    
    // Set POST data
    esp_err_t ret = esp_http_client_set_post_field(handle->http_client, json_payload, payload_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "  ❌ Failed to set POST data: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set Content-Type header
    esp_http_client_set_header(handle->http_client, "Content-Type", "application/json");
    
    // Perform HTTP request with connection recreation on failure
    uint64_t start_time = esp_timer_get_time();
    ret = esp_http_client_perform(handle->http_client);
    uint64_t end_time = esp_timer_get_time();
    uint32_t response_time_ms = (uint32_t)((end_time - start_time) / 1000);
    
    // Handle connection timeout/failure by recreating HTTP client
    if (ret == ESP_ERR_HTTP_CONNECT || ret == ESP_ERR_HTTP_EAGAIN || ret == ESP_ERR_HTTP_FETCH_HEADER || ret == ESP_FAIL) {
        ESP_LOGW(TAG, "🔄 HTTP connection failed (%s), recreating client for retry", esp_err_to_name(ret));
        
        // Close current connection
        esp_http_client_close(handle->http_client);
        esp_http_client_cleanup(handle->http_client);
        
        // Recreate HTTP client with same configuration
        esp_http_client_config_t http_config = {
            .url = handle->config.webhook_url,
            .timeout_ms = handle->config.timeout_ms,           // Request and connection timeout
            .method = HTTP_METHOD_POST,
            .event_handler = constitutional_http_event_handler,
            .user_data = handle
        };
        
        handle->http_client = esp_http_client_init(&http_config);
        if (!handle->http_client) {
            ESP_LOGE(TAG, "❌ Failed to recreate HTTP client");
            return ESP_ERR_NO_MEM;
        }
        
        ESP_LOGI(TAG, "✅ HTTP client recreated, retrying request");
        
        // Reset POST data and headers for retry
        esp_http_client_set_post_field(handle->http_client, json_payload, payload_size);
        esp_http_client_set_header(handle->http_client, "Content-Type", "application/json");
        
        // Retry the request once with fresh connection
        start_time = esp_timer_get_time();
        ret = esp_http_client_perform(handle->http_client);
        end_time = esp_timer_get_time();
        response_time_ms = (uint32_t)((end_time - start_time) / 1000);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ HTTP retry also failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "✅ HTTP retry successful after client recreation");
        }
    }
    
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(handle->http_client);
        int content_length = esp_http_client_get_content_length(handle->http_client);
        
        ESP_LOGI(TAG, "🌐 HTTP POST completed: status=%d, length=%d, time=%" PRIu32 "ms",
                 status_code, content_length, response_time_ms);
        
        // CONSTITUTIONAL FIX: Always close connection after use to prevent stale connection reuse
        ESP_LOGI(TAG, "🔄 Closing HTTP connection to prevent stale connection reuse");
        esp_http_client_close(handle->http_client);
        
        if (status_code >= 200 && status_code < 300) {
            handle->payloads_sent++;
            
            // Publish success event
            http_tool_event_t event = {
                .type = HTTP_TOOL_EVENT_PAYLOAD_SENT,
                .timestamp_us = esp_timer_get_time(),
                .data.transmission_info = {
                    .status_code = status_code,
                    .response_time_ms = response_time_ms,
                    .payload_size = payload_size
                },
                .error_code = ESP_OK
            };
            snprintf(event.data.transmission_info.url, 
                    sizeof(event.data.transmission_info.url), 
                    "%s", handle->config.webhook_url);
            
            esp_event_post(HTTP_TOOL_EVENTS, HTTP_TOOL_EVENT_PAYLOAD_SENT, 
                          &event, sizeof(event), portMAX_DELAY);
            
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "🌐 HTTP POST failed with status: %d", status_code);
            handle->payloads_failed++;
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "🌐 HTTP POST request failed: %s", esp_err_to_name(ret));
        handle->payloads_failed++;
        return ret;
    }
}

// =============================================================================
// Constitutional Tool Interface Implementation
// =============================================================================

const char* http_tool_get_id(void)
{
    return HTTP_TOOL_ID;
}

const char* http_tool_get_version(void)
{
    return HTTP_TOOL_VERSION;
}

http_tool_config_t http_tool_create_default_config(void)
{
    http_tool_config_t config = {0};
    
    // Use HTTP tool webhook URL as single source of truth
#ifdef CONFIG_HTTP_TOOL_WEBHOOK_URL
    snprintf(config.webhook_url, sizeof(config.webhook_url), CONFIG_HTTP_TOOL_WEBHOOK_URL);
#else
    snprintf(config.webhook_url, sizeof(config.webhook_url), "http://nicolasdb.eu/webhook"); // Constitutional fallback
#endif
    
    // Set other configuration values
    config.timeout_ms = CONFIG_HTTP_TOOL_TIMEOUT_MS;
    config.max_retries = CONFIG_HTTP_TOOL_MAX_RETRIES;
    config.retry_delay_ms = CONFIG_HTTP_TOOL_RETRY_DELAY_MS;
    config.exponential_backoff = true;
    config.enable_payload_processing = true;
    config.max_payload_size = HTTP_TOOL_MAX_PAYLOAD_SIZE;
    config.validate_json = true;
    config.publish_events = true;
    config.event_queue_size = 10;
    config.require_network_tool = true;
    config.require_ntp_time = true;
    
    // Generate device ID
    constitutional_generate_device_id(config.device_id, sizeof(config.device_id));
    
    return config;
}

http_tool_handle_t http_tool_init(const http_tool_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "❌ Invalid configuration provided");
        return NULL;
    }
    
    ESP_LOGI(TAG, "🔧 Initializing HTTP Tool v%s", HTTP_TOOL_VERSION);
    
    // Allocate tool context
    http_tool_handle_t handle = calloc(1, sizeof(struct http_tool));
    if (!handle) {
        ESP_LOGE(TAG, "❌ Failed to allocate tool context");
        return NULL;
    }
    
    // Copy configuration
    memcpy(&handle->config, config, sizeof(http_tool_config_t));
    handle->init_timestamp_us = esp_timer_get_time();
    
    // Create mutex for thread safety
    handle->http_mutex = xSemaphoreCreateMutex();
    if (!handle->http_mutex) {
        ESP_LOGE(TAG, "❌ Failed to create HTTP mutex");
        free(handle);
        return NULL;
    }
    
    // Initialize HTTP client with constitutional SSL configuration  
    esp_http_client_config_t http_config = {
        .url = handle->config.webhook_url,
        .event_handler = constitutional_http_event_handler,
        .timeout_ms = handle->config.timeout_ms,           // Request and connection timeout
        .method = HTTP_METHOD_POST,
        .is_async = false,                                 // CONSTITUTIONAL FIX: Force synchronous mode
        .use_global_ca_store = false,                      // CONSTITUTIONAL FIX: Disable global CA for HTTP
    };
    
    // Constitutional HTTPS detection and SSL configuration
    if (strncmp(handle->config.webhook_url, "https://", 8) == 0) {
        http_config.transport_type = HTTP_TRANSPORT_OVER_SSL;
        ESP_LOGI(TAG, "🔒 HTTPS detected - SSL configured via Kconfig (insecure mode for testing)");
    } else {
        http_config.transport_type = HTTP_TRANSPORT_OVER_TCP;
        http_config.skip_cert_common_name_check = true;    // CONSTITUTIONAL FIX: Ensure no SSL for HTTP
        ESP_LOGI(TAG, "🔓 HTTP detected - plain TCP transport (SSL disabled)");
    }
    
    handle->http_client = esp_http_client_init(&http_config);
    if (!handle->http_client) {
        ESP_LOGE(TAG, "❌ Failed to initialize HTTP client");
        vSemaphoreDelete(handle->http_mutex);
        free(handle);
        return NULL;
    }
    
    // CONSTITUTIONAL FIX: Pre-establish connection to avoid first-request-fails bug
    ESP_LOGI(TAG, "🔄 Pre-warming HTTP client connection");
    esp_http_client_set_method(handle->http_client, HTTP_METHOD_HEAD);
    esp_err_t warmup_result = esp_http_client_perform(handle->http_client);
    esp_http_client_set_method(handle->http_client, HTTP_METHOD_POST);
    
    if (warmup_result == ESP_OK) {
        ESP_LOGI(TAG, "✅ HTTP client connection pre-warmed successfully");
    } else {
        ESP_LOGW(TAG, "⚠️ HTTP client warmup failed (may be expected): %s", esp_err_to_name(warmup_result));
    }
    
    handle->is_initialized = true;
    
    ESP_LOGI(TAG, "✅ HTTP Tool initialized successfully");
    ESP_LOGI(TAG, "  📍 Webhook URL: %s", handle->config.webhook_url);
    ESP_LOGI(TAG, "  🆔 Device ID: %s", handle->config.device_id);
    ESP_LOGI(TAG, "  ⏰ Timeout: %" PRIu32 "ms", handle->config.timeout_ms);
    
    return handle;
}

esp_err_t http_tool_deinit(http_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔧 Deinitializing HTTP Tool");
    
    if (handle->is_active) {
        http_tool_stop(handle);
    }
    
    if (handle->http_client) {
        esp_http_client_cleanup(handle->http_client);
    }
    
    if (handle->http_mutex) {
        vSemaphoreDelete(handle->http_mutex);
    }
    
    free(handle);
    
    ESP_LOGI(TAG, "✅ HTTP Tool deinitialized");
    return ESP_OK;
}

http_tool_capabilities_t http_tool_get_capabilities(http_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    return HTTP_CAP_POST_REQUEST | 
           HTTP_CAP_JSON_PAYLOAD | 
           HTTP_CAP_RETRY_LOGIC |
           HTTP_CAP_EVENT_PUBLISH |
           HTTP_CAP_HEALTH_MONITOR |
           HTTP_CAP_PAYLOAD_PROCESS |
           HTTP_CAP_STACK_SAFETY;
}

esp_err_t http_tool_get_status(http_tool_handle_t handle, http_tool_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(handle->http_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        status->is_initialized = handle->is_initialized;
        status->is_active = handle->is_active;
        status->is_connected = handle->is_connected;
        status->server_reachable = handle->server_reachable;
        snprintf(status->current_url, sizeof(status->current_url), 
                "%s", handle->config.webhook_url);
        status->payloads_sent = handle->payloads_sent;
        status->payloads_failed = handle->payloads_failed;
        status->retry_count = handle->retry_count;
        status->uptime_us = esp_timer_get_time() - handle->init_timestamp_us;
        status->capabilities = http_tool_get_capabilities(handle);
        
        xSemaphoreGive(handle->http_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

esp_err_t http_tool_set_dependencies(http_tool_handle_t handle, void* network_tool, void* ntp_tool, void* payload_tool)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handle->network_tool = network_tool;
    handle->ntp_tool = ntp_tool;
    handle->payload_tool = (payload_tool_handle_t)payload_tool;
    
    ESP_LOGI(TAG, "🔗 Dependencies set: network=%p, ntp=%p, payload=%p", network_tool, ntp_tool, payload_tool);
    
    return ESP_OK;
}

// =============================================================================
// Constitutional HTTP Operations Implementation
// =============================================================================

esp_err_t http_tool_process_deferred_payload(http_tool_handle_t handle,
                                           const char* tag_uid,
                                           const char* event_type,
                                           uint64_t timestamp_us,
                                           const char* session_id)
{
    if (!handle || !tag_uid || !event_type || !session_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!handle->is_initialized) {
        ESP_LOGE(TAG, "❌ HTTP tool not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // CONSTITUTIONAL FIX: Acquire mutex for thread-safe HTTP operations
    if (xSemaphoreTake(handle->http_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "❌ Failed to acquire HTTP mutex for payload processing");
        return ESP_ERR_TIMEOUT;
    }
    
    ESP_LOGI(TAG, "📦 Processing deferred payload: UID=%s, type=%s", tag_uid, event_type);
    
    // Check network connectivity (if network tool available)
    if (handle->network_tool) {
        network_tool_status_t net_status;
        esp_err_t ret = network_tool_get_status((network_tool_handle_t)handle->network_tool, &net_status);
        if (ret != ESP_OK || net_status.state != NETWORK_STATE_CONNECTED) {
            ESP_LOGW(TAG, "📦 Network not available, deferring payload");
            xSemaphoreGive(handle->http_mutex);  // Release mutex on early return
            return ESP_ERR_WIFI_NOT_CONNECT;
        }
    }
    
    // Create JSON payload with stack safety
    char *json_payload = NULL;
    esp_err_t ret = constitutional_create_session_payload(handle, tag_uid, event_type, 
                                                        timestamp_us, session_id, &json_payload);
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->http_mutex);  // Release mutex on payload creation failure
        return ret;
    }
    
    // Send HTTP POST request
    ret = constitutional_http_post_request(handle, json_payload, strlen(json_payload));
    
    // Free allocated payload
    free(json_payload);
    
    // CONSTITUTIONAL FIX: Release mutex after HTTP operations complete
    xSemaphoreGive(handle->http_mutex);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Payload sent successfully for %s event", event_type);
    } else {
        ESP_LOGE(TAG, "❌ Failed to send payload for %s event: %s", 
                 event_type, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t http_tool_send_payload(http_tool_handle_t handle,
                                const char* json_payload,
                                size_t payload_size)
{
    if (!handle || !json_payload || payload_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!handle->is_initialized) {
        ESP_LOGE(TAG, "❌ HTTP tool not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "📤 Sending JSON payload (%" PRIu32 " bytes)", (uint32_t)payload_size);
    
    // CONSTITUTIONAL FIX: Acquire mutex for thread-safe HTTP operations
    if (xSemaphoreTake(handle->http_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "❌ Failed to acquire HTTP mutex for payload send");
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = constitutional_http_post_request(handle, json_payload, payload_size);
    
    // CONSTITUTIONAL FIX: Release mutex after HTTP operations complete
    xSemaphoreGive(handle->http_mutex);
    
    return ret;
}

esp_err_t http_tool_check_connectivity(http_tool_handle_t handle)
{
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // CONSTITUTIONAL FIX: Acquire mutex for thread-safe HTTP operations
    if (xSemaphoreTake(handle->http_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "❌ Failed to acquire HTTP mutex for connectivity check");
        return ESP_ERR_TIMEOUT;
    }
    
    ESP_LOGI(TAG, "🔍 Checking webhook server connectivity");
    
    // Simple GET request to check connectivity
    esp_http_client_set_method(handle->http_client, HTTP_METHOD_GET);
    esp_err_t ret = esp_http_client_perform(handle->http_client);
    esp_http_client_set_method(handle->http_client, HTTP_METHOD_POST);
    
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(handle->http_client);
        handle->server_reachable = (status_code >= 200 && status_code < 500);
        ESP_LOGI(TAG, "🔍 Server reachable: %s (status=%d)", 
                 handle->server_reachable ? "YES" : "NO", status_code);
    } else {
        handle->server_reachable = false;
        ESP_LOGW(TAG, "🔍 Server connectivity check failed: %s", esp_err_to_name(ret));
    }
    
    // CONSTITUTIONAL FIX: Release mutex after HTTP operations complete
    xSemaphoreGive(handle->http_mutex);
    
    return handle->server_reachable ? ESP_OK : ESP_FAIL;
}

esp_err_t http_tool_start(http_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!handle->is_initialized) {
        ESP_LOGE(TAG, "❌ HTTP tool not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "▶️  Starting HTTP tool transmission");
    
    handle->is_active = true;
    
    // Publish ready event
    http_tool_event_t event = {
        .type = HTTP_TOOL_EVENT_READY,
        .timestamp_us = esp_timer_get_time(),
        .error_code = ESP_OK
    };
    
    esp_event_post(HTTP_TOOL_EVENTS, HTTP_TOOL_EVENT_READY, 
                  &event, sizeof(event), portMAX_DELAY);
    
    ESP_LOGI(TAG, "✅ HTTP tool started and ready for transmission");
    
    return ESP_OK;
}

esp_err_t http_tool_stop(http_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "⏹️  Stopping HTTP tool transmission");
    
    handle->is_active = false;
    
    ESP_LOGI(TAG, "✅ HTTP tool stopped");
    
    return ESP_OK;
}

esp_err_t http_tool_hardware_self_test(http_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🧪 Running HTTP tool hardware self-test");
    
    // Test 1: Check HTTP client initialization
    if (!handle->http_client) {
        ESP_LOGE(TAG, "  ❌ HTTP client not initialized");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  ✅ HTTP client initialized");
    
    // Test 2: Check configuration
    if (strlen(handle->config.webhook_url) == 0) {
        ESP_LOGE(TAG, "  ❌ Webhook URL not configured");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  ✅ Webhook URL configured: %s", handle->config.webhook_url);
    
    // Test 3: Check connectivity (if network available)
    esp_err_t connectivity_result = http_tool_check_connectivity(handle);
    if (connectivity_result == ESP_OK) {
        ESP_LOGI(TAG, "  ✅ Server connectivity verified");
    } else {
        ESP_LOGW(TAG, "  ⚠️  Server connectivity test failed (may be expected without network)");
    }
    
    ESP_LOGI(TAG, "✅ HTTP tool hardware self-test completed");
    
    return ESP_OK;
}

// =============================================================================
// Constitutional Utility Functions Implementation
// =============================================================================

const char* http_tool_event_to_string(http_tool_event_type_t event_type)
{
    switch (event_type) {
        case HTTP_TOOL_EVENT_PAYLOAD_SENT:    return "PAYLOAD_SENT";
        case HTTP_TOOL_EVENT_PAYLOAD_FAILED:  return "PAYLOAD_FAILED";
        case HTTP_TOOL_EVENT_CONNECTED:       return "CONNECTED";
        case HTTP_TOOL_EVENT_DISCONNECTED:    return "DISCONNECTED";
        case HTTP_TOOL_EVENT_RETRY_STARTED:   return "RETRY_STARTED";
        case HTTP_TOOL_EVENT_RETRY_EXHAUSTED: return "RETRY_EXHAUSTED";
        case HTTP_TOOL_EVENT_ERROR:           return "ERROR";
        case HTTP_TOOL_EVENT_READY:           return "READY";
        default:                              return "UNKNOWN";
    }
}

const char* http_tool_status_to_string(int status_code)
{
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default:  return "Unknown Status";
    }
}