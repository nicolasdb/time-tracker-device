/**
 * @file http_tool.c
 * @brief MCP-Inspired HTTP Tool Implementation
 * 
 * Event-driven HTTP webhook transmission tool that subscribes to WiFi and RFID events.
 * Breaks coupling violations by using ESP event system instead of direct function calls.
 * Follows MCP patterns with handle-based lifecycle and capabilities discovery.
 * Renamed from webhook_tool per process map authority.
 */

#include "http_tool.h"
#include "network_tool.h"
#include "rfid_tool.h"
#include "../fs_tool/include/fs_tool.h"
#include "event_system.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_littlefs.h"
#include "sdkconfig.h"
#include "cJSON.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "HTTP_TOOL";

// =============================================================================
// Tool Context Structure (Handle-based Design)
// =============================================================================

/**
 * @brief Webhook Tool Context (Replaces static globals)
 */
struct http_tool_context {
    // MCP Tool Metadata
    http_tool_config_t config;
    http_tool_capabilities_t capabilities;
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    
    // State Management (No static globals)
    bool wifi_connected;
    bool webhook_reachable;
    uint32_t pending_count;
    uint32_t success_count;
    uint32_t failed_count;
    uint32_t last_transmission_ms;
    uint32_t last_retry_time;
    
    // Event Queue & Retry Management
    webhook_event_t events[HTTP_TOOL_MAX_LOG_ENTRIES];
    uint16_t event_count;
    QueueHandle_t event_queue;
    TaskHandle_t transmission_task;
    SemaphoreHandle_t mutex;
    
    // HTTP Resources
    esp_http_client_handle_t http_client;
    char json_payload_buffer[1024];
    
    // Event Handlers
    esp_event_handler_instance_t wifi_event_handler;
    esp_event_handler_instance_t rfid_event_handler;
    
    // Persistent Storage
    fs_tool_handle_t fs_handle;
    
    // Rate Limiting
    uint32_t last_transmission_attempt_ms;
    uint32_t min_transmission_interval_ms;
    bool is_rate_limited;
    uint32_t rate_limit_end_time_ms;
};

// =============================================================================
// Forward Declarations
// =============================================================================

static esp_err_t http_tool_load_log_internal(http_tool_handle_t handle);
static esp_err_t http_tool_save_log_internal(http_tool_handle_t handle);
static esp_err_t http_tool_send_http_request(http_tool_handle_t handle, const webhook_event_t *event);
static char* http_tool_create_json_payload(http_tool_handle_t handle, const webhook_event_t *event);
static void http_tool_format_iso_time(char* buf, size_t buf_size, time_t time_value);
static uint32_t http_tool_calculate_exponential_backoff(http_tool_handle_t handle, uint8_t attempts);
// static esp_err_t http_tool_create_default_log(http_tool_handle_t handle); // TODO: Implement if needed

// Event Handlers (Breaking coupling violations)
static void http_tool_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void http_tool_rfid_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// Background transmission task
static void http_tool_transmission_task(void* pvParameters);

// HTTP event handler
static esp_err_t http_tool_http_event_handler(esp_http_client_event_t *evt);

// =============================================================================
// MCP Tool Interface Implementation
// =============================================================================

const char* http_tool_get_id(void) {
    return HTTP_TOOL_ID;
}

const char* http_tool_get_version(void) {
    return HTTP_TOOL_VERSION;
}

http_tool_config_t http_tool_create_default_config(void) {
    // Generate device ID from MAC address
    uint8_t mac[6];
    char device_id_str[HTTP_TOOL_MAX_DEVICE_ID_LEN];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret == ESP_OK) {
        snprintf(device_id_str, sizeof(device_id_str), "%02X%02X%02X%02X%02X%02X", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        snprintf(device_id_str, sizeof(device_id_str), "ESP32_UNKNOWN");
    }
    
    http_tool_config_t config = {
        .webhook_url = CONFIG_HTTP_TOOL_DEFAULT_URL,
        .timeout_ms = CONFIG_HTTP_TOOL_REQUEST_TIMEOUT_MS,
        .max_retries = CONFIG_HTTP_TOOL_MAX_RETRIES,
        .retry_delay_ms = CONFIG_HTTP_TOOL_RETRY_DELAY_MS,
        .exponential_backoff = false,
        .max_queue_size = CONFIG_HTTP_TOOL_MAX_QUEUE_SIZE,
        .queue_timeout_ms = 1000,
        .config_file_path = "/littlefs/webhook_config.json",
        .log_file_path = "/littlefs/webhook_log.json",
        .auto_save_log = true,
        .auto_process_pending = true,
        .subscribe_to_rfid_events = true,   // RE-ENABLED - Phase 6.0: restore RFID→HTTP event chain
        .subscribe_to_wifi_events = true,
        .publish_events = true,
        .event_task_stack_size = CONFIG_HTTP_TOOL_TASK_STACK_SIZE,
    };
    
    // Copy generated device ID into config
    snprintf(config.device_id, sizeof(config.device_id), "%s", device_id_str);
    
    return config;
}

http_tool_handle_t http_tool_init(const http_tool_config_t *config) {
    ESP_LOGI(TAG, "Initializing webhook tool with MCP architecture");
    
    if (config == NULL) {
        ESP_LOGE(TAG, "Configuration is NULL");
        return NULL;
    }
    
    // Allocate context (handle-based design)
    http_tool_handle_t handle = calloc(1, sizeof(struct http_tool_context));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate webhook tool context");
        return NULL;
    }
    
    // Copy configuration
    memcpy(&handle->config, config, sizeof(http_tool_config_t));
    
    // Initialize MCP metadata
    handle->capabilities = HTTP_CAP_HTTP_POST | HTTP_CAP_RETRY_QUEUE | 
                          HTTP_CAP_EVENT_SUBSCRIBE | HTTP_CAP_PERSISTENT_LOG |
                          HTTP_CAP_JSON_PAYLOAD | HTTP_CAP_AUTO_TRANSMISSION;
    
    handle->is_initialized = false;
    handle->is_active = false;
    handle->uptime_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Initialize state
    handle->wifi_connected = false;
    handle->webhook_reachable = false;
    handle->pending_count = 0;
    handle->success_count = 0;
    handle->failed_count = 0;
    handle->last_transmission_ms = 0;
    handle->last_retry_time = 0;
    handle->event_count = 0;
    
    // Initialize rate limiting
    handle->last_transmission_attempt_ms = 0;
    handle->min_transmission_interval_ms = 100; // Minimum 100ms between requests
    handle->is_rate_limited = false;
    handle->rate_limit_end_time_ms = 0;
    
    // Create synchronization primitives
    handle->mutex = xSemaphoreCreateMutex();
    if (handle->mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(handle);
        return NULL;
    }
    
    // Create event queue
    handle->event_queue = xQueueCreate(handle->config.max_queue_size, sizeof(webhook_event_t));
    if (handle->event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        vSemaphoreDelete(handle->mutex);
        free(handle);
        return NULL;
    }
    
    // Initialize filesystem tool for persistent storage
    fs_tool_config_t fs_config = fs_tool_create_default_config();
    handle->fs_handle = fs_tool_init(&fs_config);
    if (handle->fs_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize filesystem tool for persistent storage");
        // Continue without persistent storage - tool will still function
    } else {
        ESP_LOGI(TAG, "✅ Filesystem tool initialized for persistent event storage");
    }
    
    // Load persistent event log
    http_tool_load_log_internal(handle);
    
    // Subscribe to WiFi events (BREAKING COUPLING VIOLATION)
    if (handle->config.subscribe_to_wifi_events) {
        esp_err_t err = esp_event_handler_instance_register(
            NETWORK_TOOL_EVENTS, ESP_EVENT_ANY_ID,
            http_tool_wifi_event_handler, handle,
            &handle->wifi_event_handler
        );
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "✅ Subscribed to NETWORK_TOOL_EVENTS (coupling broken!)");
        }
    }
    
    // RFID event subscription DISABLED per Process Map Authority
    // Main.c will call http_send_payload() directly instead of auto-subscription
    if (handle->config.subscribe_to_rfid_events) {
        ESP_LOGW(TAG, "⚠️ RFID auto-subscription is DEPRECATED - use http_send_payload() instead");
        // Legacy support: keep for backward compatibility but log warning
        esp_err_t err = esp_event_handler_instance_register(
            RFID_EVENTS, ESP_EVENT_ANY_ID,
            http_tool_rfid_event_handler, handle,
            &handle->rfid_event_handler
        );
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "❌ Failed to register RFID event handler: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "✅ RFID event handler registered successfully for ALL RFID_EVENTS (ANY_ID)");
            ESP_LOGI(TAG, "✅ HTTP tool will receive: TAG_DETECTED(%d), TAG_REMOVED(%d), and all other RFID events", 
                     RFID_EVENT_TAG_DETECTED, RFID_EVENT_TAG_REMOVED);
        }
    } else {
        ESP_LOGI(TAG, "✅ RFID auto-subscription DISABLED - Process Map compliant");
    }
    
    // Create background transmission task
    BaseType_t task_result = xTaskCreate(
        http_tool_transmission_task,
        "webhook_task",
        handle->config.event_task_stack_size,
        handle,
        tskIDLE_PRIORITY + 1,
        &handle->transmission_task
    );
    
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create transmission task");
        http_tool_deinit(handle);
        return NULL;
    }
    
    handle->is_initialized = true;
    handle->is_active = true;
    
    ESP_LOGI(TAG, "🎯 Webhook tool initialized successfully");
    ESP_LOGI(TAG, "📡 URL: %s", handle->config.webhook_url);
    ESP_LOGI(TAG, "🔄 Max retries: %d, Delay: %lu ms", handle->config.max_retries, handle->config.retry_delay_ms);
    ESP_LOGI(TAG, "📱 Device ID: %s", handle->config.device_id);
    
    return handle;
}

esp_err_t http_tool_deinit(http_tool_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Deinitializing webhook tool");
    
    handle->is_active = false;
    
    // Save log before cleanup
    if (handle->config.auto_save_log) {
        http_tool_save_log_internal(handle);
    }
    
    // Unregister event handlers
    if (handle->wifi_event_handler) {
        esp_event_handler_instance_unregister(NETWORK_TOOL_EVENTS, ESP_EVENT_ANY_ID, handle->wifi_event_handler);
    }
    if (handle->rfid_event_handler) {
        esp_event_handler_instance_unregister(RFID_EVENTS, ESP_EVENT_ANY_ID, handle->rfid_event_handler);
    }
    
    // Delete transmission task
    if (handle->transmission_task) {
        vTaskDelete(handle->transmission_task);
    }
    
    // Clean up HTTP client
    if (handle->http_client) {
        esp_http_client_cleanup(handle->http_client);
    }
    
    // Clean up filesystem tool
    if (handle->fs_handle) {
        fs_tool_deinit(handle->fs_handle);
    }
    
    // Clean up synchronization primitives
    if (handle->event_queue) {
        vQueueDelete(handle->event_queue);
    }
    if (handle->mutex) {
        vSemaphoreDelete(handle->mutex);
    }
    
    free(handle);
    
    ESP_LOGI(TAG, "Webhook tool deinitialized");
    return ESP_OK;
}

http_tool_capabilities_t http_tool_get_capabilities(http_tool_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }
    return handle->capabilities;
}

esp_err_t http_tool_get_status(http_tool_handle_t handle, http_tool_status_t *status) {
    if (handle == NULL || status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    status->is_initialized = handle->is_initialized;
    status->is_active = handle->is_active;
    status->wifi_connected = handle->wifi_connected;
    status->webhook_reachable = handle->webhook_reachable;
    snprintf(status->current_url, sizeof(status->current_url), "%s", handle->config.webhook_url);
    status->pending_count = handle->pending_count;
    status->success_count = handle->success_count;
    status->failed_count = handle->failed_count;
    status->uptime_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - handle->uptime_start;
    status->last_transmission_ms = handle->last_transmission_ms;
    status->capabilities = handle->capabilities;
    
    xSemaphoreGive(handle->mutex);
    
    return ESP_OK;
}

const http_tool_registry_t* http_tool_get_registry_entry(void) {
    static const http_tool_registry_t registry = {
        .tool_id = HTTP_TOOL_ID,
        .version = HTTP_TOOL_VERSION,
        .description = HTTP_TOOL_DESCRIPTION,
        .capabilities = HTTP_CAP_HTTP_POST | HTTP_CAP_RETRY_QUEUE | 
                       HTTP_CAP_EVENT_SUBSCRIBE | HTTP_CAP_PERSISTENT_LOG |
                       HTTP_CAP_JSON_PAYLOAD | HTTP_CAP_AUTO_TRANSMISSION,
        .init_func = http_tool_init,
        .deinit_func = http_tool_deinit,
    };
    return &registry;
}

// =============================================================================
// Event Handlers (Breaking Coupling Violations)
// =============================================================================

/**
 * @brief WiFi Event Handler - Replaces direct wifi_manager_is_connected() calls
 * 🔥 FIXES COUPLING VIOLATION: Lines 352, 377 in legacy webhook_manager.c
 */
static void http_tool_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    http_tool_handle_t handle = (http_tool_handle_t)arg;
    
    if (event_base != NETWORK_TOOL_EVENTS || handle == NULL) {
        return;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    switch (event_id) {
        case NETWORK_TOOL_EVENT_STA_CONNECTED:
        case NETWORK_TOOL_EVENT_IP_ACQUIRED:
            if (!handle->wifi_connected) {
                handle->wifi_connected = true;
                ESP_LOGI(TAG, "🌐 WiFi connected - processing pending webhooks");
                
                // Auto-process pending events (replaces manual polling)
                if (handle->config.auto_process_pending) {
                    // Signal transmission task to process pending
                    webhook_event_t dummy_event = {0};
                    xQueueSend(handle->event_queue, &dummy_event, 0);
                }
                
                // Publish connectivity restored event
                if (handle->config.publish_events) {
                    http_tool_event_t pub_event = {
                        .type = HTTP_TOOL_EVENT_CONNECTIVITY_RESTORED,
                        .data.queue_info.pending_count = handle->pending_count
                    };
                    esp_event_post(HTTP_TOOL_EVENTS, HTTP_TOOL_EVENT_CONNECTIVITY_RESTORED, 
                                 &pub_event, sizeof(pub_event), 0);
                }
            }
            break;
            
        case NETWORK_TOOL_EVENT_STA_DISCONNECTED:
        case NETWORK_TOOL_EVENT_IP_LOST:
            if (handle->wifi_connected) {
                handle->wifi_connected = false;
                handle->webhook_reachable = false;
                ESP_LOGW(TAG, "📵 WiFi disconnected - webhooks will be queued");
            }
            break;
            
        default:
            break;
    }
    
    xSemaphoreGive(handle->mutex);
}

/**
 * @brief RFID Event Handler - Automatic webhook transmission on tag events
 * 🎯 ENABLES AUTO-TRANSMISSION: No manual http_tool_send_event() calls needed
 */
static void http_tool_rfid_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    // RAW EVENT DEBUGGING - Log EVERY event that reaches this handler
    ESP_LOGI(TAG, "🔥 HTTP HANDLER ENTRY: base=%s, id=%ld, data=%p", 
             event_base ? (const char*)event_base : "NULL", event_id, event_data);
    
    http_tool_handle_t handle = (http_tool_handle_t)arg;
    
    if (event_base != RFID_EVENTS || handle == NULL || event_data == NULL) {
        ESP_LOGW(TAG, "❌ Event validation failed: base=%s (expected RFID_EVENTS), handle=%p, data=%p", 
                 event_base ? (const char*)event_base : "NULL", handle, event_data);
        return;
    }
    
    ESP_LOGI(TAG, "🔄 RFID event received: event_id=%ld", event_id);
    
    // Cast event data to universal RFID event structure
    rfid_event_data_t* rfid_event = (rfid_event_data_t*)event_data;
    
    webhook_event_type_t webhook_event_type;
    const char* tag_uid = NULL;
    const char* tag_type = "MIFARE";
    
    switch (event_id) {
        case RFID_EVENT_TAG_DETECTED:
            webhook_event_type = WEBHOOK_EVENT_TAG_PLACED;
            tag_uid = rfid_event->tag_uid;
            ESP_LOGI(TAG, "🏷️ Auto-sending webhook: TAG_PLACED (%s)", tag_uid);
            break;
            
        case RFID_EVENT_TAG_REMOVED:
            webhook_event_type = WEBHOOK_EVENT_TAG_REMOVED;
            tag_uid = rfid_event->tag_uid;
            ESP_LOGI(TAG, "📤 Auto-sending webhook: TAG_REMOVED (%s)", tag_uid);
            break;
            
        default:
            ESP_LOGW(TAG, "❓ Unhandled RFID event in HTTP tool: event_id=%ld, base=%s", 
                     event_id, event_base);
            return; // Ignore other RFID events
    }
    
    // Automatically send webhook event (no manual intervention required)
    esp_err_t err = http_tool_send_event(handle, webhook_event_type, tag_uid, tag_type);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to auto-send webhook event: %s", esp_err_to_name(err));
    }
}

// =============================================================================
// Background Transmission Task
// =============================================================================

/**
 * @brief Background task for processing webhook queue
 */
static void http_tool_transmission_task(void* pvParameters) {
    http_tool_handle_t handle = (http_tool_handle_t)pvParameters;
    webhook_event_t event;
    uint32_t last_connectivity_check = 0;
    
    ESP_LOGI(TAG, "🚀 Webhook transmission task started");
    
    while (handle->is_active) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Wait for events in queue or timeout for periodic processing
        if (xQueueReceive(handle->event_queue, &event, pdMS_TO_TICKS(5000)) == pdTRUE) {
            // Process pending events when triggered
            http_tool_process_pending(handle);
        } else {
            // Periodic connectivity check (every 30 seconds)
            if (now - last_connectivity_check > 30000) {
                ESP_LOGD(TAG, "Performing periodic webhook connectivity check");
                http_tool_check_connectivity(handle);
                last_connectivity_check = now;
            }
            
            // Periodic processing (every 5 seconds)
            if (handle->wifi_connected && handle->pending_count > 0) {
                // Double-check webhook reachability before processing
                if (handle->webhook_reachable || http_tool_check_connectivity(handle) == ESP_OK) {
                    http_tool_process_pending(handle);
                } else {
                    ESP_LOGD(TAG, "Webhook endpoint unreachable, skipping pending event processing");
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "Webhook transmission task ended");
    vTaskDelete(NULL);
}

// =============================================================================
// HTTP Event Handler
// =============================================================================

static esp_err_t http_tool_http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP Client Error");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP Client Connected");
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP Client Finished");
            break;
        default:
            break;
    }
    return ESP_OK;
}

// =============================================================================
// Webhook Operations Implementation (Ported from legacy)
// =============================================================================

esp_err_t http_send_payload(http_tool_handle_t handle,
                           const char* json_payload,
                           size_t payload_length) {
    if (handle == NULL || json_payload == NULL || payload_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!handle->is_initialized || !handle->is_active) {
        ESP_LOGW(TAG, "HTTP tool not initialized or not active");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "📡 Process Map Authority: Sending formatted payload (%zu bytes)", payload_length);
    
    // Check WiFi connectivity
    if (!handle->wifi_connected) {
        ESP_LOGW(TAG, "WiFi not connected, payload transmission skipped");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    // Send HTTP request directly with formatted payload
    esp_http_client_config_t config = {
        .url = handle->config.webhook_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = handle->config.timeout_ms,
        .event_handler = http_tool_http_event_handler,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_ERR_NO_MEM;
    }
    
    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "User-Agent", "ESP32-TimeTracker/1.0");
    
    // Set POST data
    esp_err_t err = esp_http_client_set_post_field(client, json_payload, payload_length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set POST data: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    
    // Perform HTTP request
    ESP_LOGI(TAG, "🌐 Sending POST to %s", handle->config.webhook_url);
    err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code >= 200 && status_code < 300) {
            ESP_LOGI(TAG, "✅ Payload sent successfully - HTTP %d", status_code);
            // Prevent overflow - reset counters at max value
            if (handle->success_count < UINT32_MAX) {
                handle->success_count++;
            } else {
                ESP_LOGW(TAG, "Success counter overflow - resetting to 1");
                handle->success_count = 1;
            }
            handle->last_transmission_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        } else {
            ESP_LOGW(TAG, "❌ HTTP request failed - HTTP %d", status_code);
            // Prevent overflow - reset counters at max value
            if (handle->failed_count < UINT32_MAX) {
                handle->failed_count++;
            } else {
                ESP_LOGW(TAG, "Failed counter overflow - resetting to 1");
                handle->failed_count = 1;
            }
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "❌ HTTP request failed: %s", esp_err_to_name(err));
        // Prevent overflow - reset counters at max value
        if (handle->failed_count < UINT32_MAX) {
            handle->failed_count++;
        } else {
            ESP_LOGW(TAG, "Failed counter overflow - resetting to 1");
            handle->failed_count = 1;
        }
    }
    
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t http_tool_send_event(http_tool_handle_t handle,
                                   webhook_event_type_t event_type,
                                   const char *tag_uid,
                                   const char *tag_type) {
    if (handle == NULL || tag_uid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    // If log is full, shift everything to remove oldest
    if (handle->event_count >= HTTP_TOOL_MAX_LOG_ENTRIES) {
        ESP_LOGW(TAG, "Event log full, dropping oldest event");
        for (int i = 0; i < HTTP_TOOL_MAX_LOG_ENTRIES - 1; i++) {
            handle->events[i] = handle->events[i + 1];
        }
        handle->event_count--;
    }
    
    // Create new event
    webhook_event_t *evt = &handle->events[handle->event_count];
    evt->event_type = event_type;
    strncpy(evt->tag_uid, tag_uid, sizeof(evt->tag_uid) - 1);
    
    // Use configured device ID
    snprintf(evt->device_id, sizeof(evt->device_id), "%s", handle->config.device_id);
    
    // Set timestamp
    time_t now;
    time(&now);
    evt->timestamp = (uint32_t)now;
    
    // Set tag type if provided
    if (tag_type != NULL) {
        strncpy(evt->tag_type, tag_type, sizeof(evt->tag_type) - 1);
    } else {
        evt->tag_type[0] = '\0';
    }
    
    evt->sent = false;
    evt->attempts = 0;
    evt->next_retry_time_ms = 0; // Will be set on first failure
    
    // Prevent overflow - reset counters at max value
    if (handle->event_count < UINT16_MAX) {
        handle->event_count++;
    } else {
        ESP_LOGW(TAG, "Event counter overflow - resetting to 1");
        handle->event_count = 1;
    }
    if (handle->pending_count < UINT32_MAX) {
        handle->pending_count++;
    } else {
        ESP_LOGW(TAG, "Pending counter overflow - resetting to 1");
        handle->pending_count = 1;
    }
    
    // Try to send immediately if WiFi is connected (REPLACES COUPLING VIOLATION)
    if (handle->wifi_connected) {
        ESP_LOGI(TAG, "WiFi connected, sending event immediately");
        if (http_tool_send_http_request(handle, evt) == ESP_OK) {
            evt->sent = true;
            // Prevent overflow - reset counters at max value
            if (handle->success_count < UINT32_MAX) {
                handle->success_count++;
            } else {
                ESP_LOGW(TAG, "Success counter overflow - resetting to 1");
                handle->success_count = 1;
            }
            if (handle->pending_count > 0) {
                handle->pending_count--;
            }
            handle->last_transmission_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            ESP_LOGI(TAG, "Event sent successfully");
        } else {
            evt->attempts++;
            // Calculate exponential backoff for next retry
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint32_t backoff_delay = http_tool_calculate_exponential_backoff(handle, evt->attempts);
            evt->next_retry_time_ms = now + backoff_delay;
            ESP_LOGW(TAG, "Failed to send event, will retry in %"PRIu32"ms (attempt %d)", 
                    backoff_delay, evt->attempts);
        }
    } else {
        ESP_LOGW(TAG, "WiFi not connected, event queued for later");
    }
    
    // Save log after adding new event
    if (handle->config.auto_save_log) {
        http_tool_save_log_internal(handle);
    }
    
    xSemaphoreGive(handle->mutex);
    
    return ESP_OK;
}

esp_err_t http_tool_process_pending(http_tool_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    // Check if WiFi is connected (USES INTERNAL STATE, NO COUPLING)
    if (!handle->wifi_connected) {
        ESP_LOGW(TAG, "WiFi not connected, skipping processing of pending events");
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Event aging - remove events older than 24 hours to prevent queue bloat
    int aged_count = 0;
    for (int i = handle->event_count - 1; i >= 0; i--) { // Iterate backwards for safe removal
        webhook_event_t *evt = &handle->events[i];
        
        // Age threshold: 24 hours (86400 seconds)
        if ((now / 1000) - evt->timestamp > 86400) {
            ESP_LOGD(TAG, "Aging out old event (timestamp: %"PRIu32", age: %"PRIu32"s)", 
                    evt->timestamp, (now / 1000) - evt->timestamp);
            
            // Remove aged event by shifting array
            for (int j = i; j < handle->event_count - 1; j++) {
                handle->events[j] = handle->events[j + 1];
            }
            handle->event_count--;
            aged_count++;
            
            // Update pending count if event wasn't sent
            if (!evt->sent && handle->pending_count > 0) {
                handle->pending_count--;
            }
        }
    }
    
    if (aged_count > 0) {
        ESP_LOGI(TAG, "♻️ Aged out %d old events (>24h)", aged_count);
    }
    
    int sent_count = 0;
    int pending_count = 0;
    
    for (int i = 0; i < handle->event_count; i++) {
        webhook_event_t *evt = &handle->events[i];
        
        if (!evt->sent && evt->attempts < handle->config.max_retries) {
            pending_count++;
            
            // Check if enough time has passed for this specific event's exponential backoff
            if (evt->next_retry_time_ms > 0 && now < evt->next_retry_time_ms) {
                ESP_LOGD(TAG, "Event %d not ready for retry - %"PRIu32"ms remaining", 
                        i, evt->next_retry_time_ms - now);
                continue; // Skip this event - not ready for retry yet
            }
            
            if (http_tool_send_http_request(handle, evt) == ESP_OK) {
                evt->sent = true;
                sent_count++;
                // Reset next retry time on success
                evt->next_retry_time_ms = 0;
                
                // Prevent overflow - reset counters at max value
                if (handle->success_count < UINT32_MAX) {
                    handle->success_count++;
                } else {
                    ESP_LOGW(TAG, "Success counter overflow - resetting to 1");
                    handle->success_count = 1;
                }
                if (handle->pending_count > 0) {
                    handle->pending_count--;
                }
                handle->last_transmission_ms = now;
                ESP_LOGI(TAG, "✅ Successfully sent pending event (%s)", 
                        evt->event_type == WEBHOOK_EVENT_TAG_PLACED ? "tag_insert" : "tag_removed");
            } else {
                evt->attempts++;
                
                // Calculate exponential backoff for next retry
                uint32_t backoff_delay = http_tool_calculate_exponential_backoff(handle, evt->attempts);
                evt->next_retry_time_ms = now + backoff_delay;
                
                ESP_LOGW(TAG, "❌ Failed to send pending event, attempts: %d/%d, next retry in %"PRIu32"ms", 
                        evt->attempts, handle->config.max_retries, backoff_delay);
                
                if (evt->attempts >= handle->config.max_retries) {
                    // Prevent overflow - reset counters at max value
                    if (handle->failed_count < UINT32_MAX) {
                        handle->failed_count++;
                    } else {
                        ESP_LOGW(TAG, "Failed counter overflow - resetting to 1");
                        handle->failed_count = 1;
                    }
                    if (handle->pending_count > 0) {
                        handle->pending_count--;
                    }
                    ESP_LOGE(TAG, "⚠️ Event exceeded max retries (%d), giving up", handle->config.max_retries);
                }
            }
        }
    }
    
    if (pending_count > 0) {
        ESP_LOGI(TAG, "Processed pending events: %d/%d sent successfully", sent_count, pending_count);
        
        // Save log after processing
        if (handle->config.auto_save_log) {
            http_tool_save_log_internal(handle);
        }
    }
    
    xSemaphoreGive(handle->mutex);
    
    return ESP_OK;
}

esp_err_t http_tool_get_pending_count(http_tool_handle_t handle, uint32_t *pending_count) {
    if (handle == NULL || pending_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    *pending_count = handle->pending_count;
    xSemaphoreGive(handle->mutex);
    
    return ESP_OK;
}

// =============================================================================
// HTTP Implementation (Ported from legacy webhook_manager)
// =============================================================================

static esp_err_t http_tool_send_http_request(http_tool_handle_t handle, const webhook_event_t *event) {
    if (event == NULL) {
        ESP_LOGE(TAG, "Event is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Check server-imposed rate limiting
    if (handle->is_rate_limited && now < handle->rate_limit_end_time_ms) {
        ESP_LOGD(TAG, "⏳ Server rate limit active, %"PRIu32"ms remaining", 
                handle->rate_limit_end_time_ms - now);
        return ESP_ERR_TIMEOUT; // Treated as temporary failure
    }
    
    // Clear rate limit if expired
    if (handle->is_rate_limited && now >= handle->rate_limit_end_time_ms) {
        handle->is_rate_limited = false;
        ESP_LOGI(TAG, "✅ Server rate limit expired, resuming transmissions");
    }
    
    // Client-side rate limiting - prevent too-frequent requests
    if (now - handle->last_transmission_attempt_ms < handle->min_transmission_interval_ms) {
        ESP_LOGD(TAG, "⏳ Client rate limit - too soon since last request");
        return ESP_ERR_TIMEOUT; // Treated as temporary failure
    }
    
    handle->last_transmission_attempt_ms = now;
    
    esp_err_t err = ESP_FAIL;
    esp_http_client_handle_t client = NULL;
    char *json_payload = NULL;
    
    // Create JSON payload
    json_payload = http_tool_create_json_payload(handle, event);
    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON payload");
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "Sending webhook request to %s", handle->config.webhook_url);
    ESP_LOGI(TAG, "📄 JSON Payload: %s", json_payload);
    
    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = handle->config.webhook_url,
        .event_handler = http_tool_http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = handle->config.timeout_ms,
    };
    
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        goto cleanup;
    }
    
    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    // Set post field
    err = esp_http_client_set_post_field(client, json_payload, strlen(json_payload));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set post field: %s", esp_err_to_name(err));
        goto cleanup;
    }
    
    // Perform request
    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    
    // Check status code
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);
    
    if (status_code >= 200 && status_code < 300) {
        handle->webhook_reachable = true;
        err = ESP_OK;
        
        // Reset rate limiting interval on success (adaptive rate limiting)
        if (handle->min_transmission_interval_ms > 100) {
            handle->min_transmission_interval_ms = 100; // Reset to minimum
            ESP_LOGD(TAG, "✅ Success - reset rate limiting interval to minimum");
        }
        
        // Publish success event
        if (handle->config.publish_events) {
            http_tool_event_t pub_event = {
                .type = HTTP_TOOL_EVENT_TRANSMISSION_SUCCESS,
                .data.transmission_info.status_code = status_code
            };
            snprintf(pub_event.data.transmission_info.url, sizeof(pub_event.data.transmission_info.url), "%s", handle->config.webhook_url);
            esp_event_post(HTTP_TOOL_EVENTS, HTTP_TOOL_EVENT_TRANSMISSION_SUCCESS, 
                         &pub_event, sizeof(pub_event), 0);
        }
    } else if (status_code == 429) {
        // Too Many Requests - respect server rate limiting
        ESP_LOGW(TAG, "⚠️ Rate limited by server (429)");
        
        // Try to get retry-after header
        char* retry_after_str = NULL;
        esp_err_t header_err = esp_http_client_get_header(client, "retry-after", &retry_after_str);
        
        uint32_t retry_after_ms = 0;
        if (header_err == ESP_OK && retry_after_str != NULL) {
            int retry_after_seconds = atoi(retry_after_str);
            if (retry_after_seconds > 0 && retry_after_seconds < 3600) { // Cap at 1 hour
                retry_after_ms = retry_after_seconds * 1000;
                ESP_LOGW(TAG, "Server requested retry-after: %d seconds", retry_after_seconds);
            }
        }
        
        // If no valid retry-after header, use exponential backoff
        if (retry_after_ms == 0) {
            retry_after_ms = http_tool_calculate_exponential_backoff(handle, 1); // Start with attempt 1
            ESP_LOGW(TAG, "No retry-after header, using exponential backoff: %"PRIu32"ms", retry_after_ms);
        }
        
        // Set rate limiting state
        handle->is_rate_limited = true;
        handle->rate_limit_end_time_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) + retry_after_ms;
        
        ESP_LOGW(TAG, "🚫 Rate limiting active for %"PRIu32"ms", retry_after_ms);
        err = ESP_ERR_TIMEOUT; // Treat as temporary failure, not permanent
    } else {
        ESP_LOGE(TAG, "HTTP request failed with status code %d", status_code);
        err = ESP_FAIL;
        
        // Adaptive rate limiting - increase interval on server errors (5xx)
        if (status_code >= 500 && handle->min_transmission_interval_ms < 5000) {
            handle->min_transmission_interval_ms = handle->min_transmission_interval_ms * 2;
            if (handle->min_transmission_interval_ms > 5000) {
                handle->min_transmission_interval_ms = 5000; // Cap at 5 seconds
            }
            ESP_LOGW(TAG, "⚠️ Server error - increased rate limiting interval to %"PRIu32"ms", 
                    handle->min_transmission_interval_ms);
        }
        
        // Publish failure event
        if (handle->config.publish_events) {
            http_tool_event_t pub_event = {
                .type = HTTP_TOOL_EVENT_TRANSMISSION_FAILED,
                .data.error_info.error_code = ESP_FAIL
            };
            esp_event_post(HTTP_TOOL_EVENTS, HTTP_TOOL_EVENT_TRANSMISSION_FAILED, 
                         &pub_event, sizeof(pub_event), 0);
        }
    }
    
cleanup:
    if (json_payload != NULL) {
        free(json_payload);
    }
    
    if (client != NULL) {
        esp_http_client_cleanup(client);
    }
    
    return err;
}

static char* http_tool_create_json_payload(http_tool_handle_t handle, const webhook_event_t *event) {
    if (event == NULL) {
        ESP_LOGE(TAG, "Null event passed to create_json_payload");
        return NULL;
    }
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create root JSON object");
        return NULL;
    }
    
    // Format event type and determine tag_present value
    const char *event_type_str = "unknown";
    bool tag_present = false;

    switch (event->event_type) {
        case WEBHOOK_EVENT_TAG_PLACED:
            event_type_str = "tag_insert";
            tag_present = true;
            break;
        case WEBHOOK_EVENT_TAG_REMOVED:
            event_type_str = "tag_removed";
            tag_present = false;
            break;
        default:
            ESP_LOGW(TAG, "Unknown event type: %d", event->event_type);
            break;
    }
    
    // Format timestamp
    char timestamp_str[32] = {0};
    http_tool_format_iso_time(timestamp_str, sizeof(timestamp_str), (time_t)event->timestamp);
    
    // Create the RFID poll result object (matching server expectations)
    cJSON *rfid_poll_result = cJSON_CreateObject();
    if (rfid_poll_result == NULL) {
        ESP_LOGE(TAG, "Failed to create RFID poll result JSON object");
        cJSON_Delete(root);
        return NULL;
    }
    
    // Add fields to the rfid_poll_result object
    bool success = true;
    
    if (cJSON_AddStringToObject(rfid_poll_result, "timestamp", timestamp_str) == NULL) success = false;
    if (success && cJSON_AddBoolToObject(rfid_poll_result, "tag_present", tag_present) == NULL) success = false;
    if (success && cJSON_AddStringToObject(rfid_poll_result, "tag_id", event->tag_uid) == NULL) success = false;
    if (success && cJSON_AddStringToObject(rfid_poll_result, "device_id", event->device_id) == NULL) success = false;
    if (success && cJSON_AddStringToObject(rfid_poll_result, "event_type", event_type_str) == NULL) success = false;
    
    // Optional tag type
    if (success && strlen(event->tag_type) > 0) {
        if (cJSON_AddStringToObject(rfid_poll_result, "tag_type", event->tag_type) == NULL) {
            success = false;
        }
    }
    
    // Add the rfid_poll_result object to the root
    if (success) {
        cJSON_AddItemToObject(root, "rfid_poll_result", rfid_poll_result);
    } else {
        cJSON_Delete(rfid_poll_result);
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to build JSON payload");
        return NULL;
    }
    
    // Add firmware version and hardware info
    if (success && cJSON_AddStringToObject(root, "firmware_version", "v2.0.0-mcp") == NULL) success = false;
    if (success && cJSON_AddStringToObject(root, "hardware", "ESP32-C3") == NULL) success = false;
    
    // Convert to string
    char *json_str = NULL;
    if (success) {
        json_str = cJSON_Print(root);
        if (json_str == NULL) {
            ESP_LOGE(TAG, "Failed to print JSON to string");
        }
    }
    
    cJSON_Delete(root);
    
    // Check payload size
    if (json_str && strlen(json_str) > 1024) {
        ESP_LOGW(TAG, "JSON payload is very large (%d bytes)", (int)strlen(json_str));
    }
    
    return json_str;
}

static void http_tool_format_iso_time(char* buf, size_t buf_size, time_t time_value) {
    struct tm timeinfo;
    localtime_r(&time_value, &timeinfo);
    strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%S", &timeinfo);
}

/**
 * @brief Calculate exponential backoff delay for retry attempts
 * Implements process map authority: 1s → 2s → 4s → 8s progression
 */
static uint32_t http_tool_calculate_exponential_backoff(http_tool_handle_t handle, uint8_t attempts) {
    if (handle == NULL || !handle->config.exponential_backoff) {
        return handle ? handle->config.retry_delay_ms : 5000; // Fixed delay fallback
    }
    
    // Process Map Authority: exponential backoff with base delay
    uint32_t base_delay = handle->config.retry_delay_ms;
    if (base_delay == 0) {
        base_delay = 1000; // Default to 1 second if not configured
    }
    
    // Calculate 2^attempts * base_delay, with maximum cap
    uint32_t exponential_delay = base_delay;
    for (uint8_t i = 0; i < attempts && i < 6; i++) { // Cap at 2^6 = 64x
        exponential_delay *= 2;
        if (exponential_delay > 60000) { // Cap at 60 seconds max
            exponential_delay = 60000;
            break;
        }
    }
    
    // Add jitter to prevent thundering herd (±10%)
    uint32_t jitter = (esp_random() % (exponential_delay / 5)) - (exponential_delay / 10);
    exponential_delay += jitter;
    
    ESP_LOGD(TAG, "Exponential backoff for attempt %d: %"PRIu32"ms", attempts, exponential_delay);
    return exponential_delay;
}

// =============================================================================
// Configuration & Log Management (Stubs for now)
// =============================================================================

static esp_err_t http_tool_load_log_internal(http_tool_handle_t handle) {
    if (handle == NULL || handle->fs_handle == NULL) {
        ESP_LOGD(TAG, "Cannot load log - filesystem tool not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Loading persistent webhook event log from filesystem");
    
    cJSON *log_json = NULL;
    esp_err_t err = fs_tool_load_json_log(handle->fs_handle, "webhook_events.json", &log_json);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No existing webhook log found, starting fresh");
        return ESP_OK; // Not an error - first run
    }
    
    // Parse the JSON array of events
    if (!cJSON_IsArray(log_json)) {
        ESP_LOGW(TAG, "Invalid webhook log format - expected array");
        cJSON_Delete(log_json);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Load events into memory
    int array_size = cJSON_GetArraySize(log_json);
    int loaded_count = 0;
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->event_count = 0; // Reset count
    
    for (int i = 0; i < array_size && i < HTTP_TOOL_MAX_LOG_ENTRIES; i++) {
        cJSON *event_json = cJSON_GetArrayItem(log_json, i);
        if (!cJSON_IsObject(event_json)) continue;
        
        webhook_event_t *event = &handle->events[handle->event_count];
        
        // Parse event fields
        cJSON *type = cJSON_GetObjectItem(event_json, "event_type");
        cJSON *tag_uid = cJSON_GetObjectItem(event_json, "tag_uid");
        cJSON *device_id = cJSON_GetObjectItem(event_json, "device_id");
        cJSON *tag_type = cJSON_GetObjectItem(event_json, "tag_type");
        cJSON *timestamp = cJSON_GetObjectItem(event_json, "timestamp");
        cJSON *sent = cJSON_GetObjectItem(event_json, "sent");
        cJSON *attempts = cJSON_GetObjectItem(event_json, "attempts");
        cJSON *next_retry_time = cJSON_GetObjectItem(event_json, "next_retry_time_ms");
        
        if (cJSON_IsNumber(type)) {
            event->event_type = (webhook_event_type_t)cJSON_GetNumberValue(type);
        }
        if (cJSON_IsString(tag_uid)) {
            strncpy(event->tag_uid, cJSON_GetStringValue(tag_uid), sizeof(event->tag_uid) - 1);
        }
        if (cJSON_IsString(device_id)) {
            strncpy(event->device_id, cJSON_GetStringValue(device_id), sizeof(event->device_id) - 1);
        }
        if (cJSON_IsString(tag_type)) {
            strncpy(event->tag_type, cJSON_GetStringValue(tag_type), sizeof(event->tag_type) - 1);
        }
        if (cJSON_IsNumber(timestamp)) {
            event->timestamp = (uint32_t)cJSON_GetNumberValue(timestamp);
        }
        if (cJSON_IsBool(sent)) {
            event->sent = cJSON_IsTrue(sent);
        }
        if (cJSON_IsNumber(attempts)) {
            event->attempts = (uint8_t)cJSON_GetNumberValue(attempts);
        }
        if (cJSON_IsNumber(next_retry_time)) {
            event->next_retry_time_ms = (uint32_t)cJSON_GetNumberValue(next_retry_time);
        } else {
            event->next_retry_time_ms = 0; // Default value for legacy data
        }
        
        handle->event_count++;
        loaded_count++;
        
        // If event is not sent, add to pending queue
        if (!event->sent) {
            if (xQueueSend(handle->event_queue, event, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Failed to queue restored event - queue full");
            } else {
                handle->pending_count++;
            }
        }
    }
    
    xSemaphoreGive(handle->mutex);
    
    ESP_LOGI(TAG, "✅ Loaded %d webhook events from persistent storage", loaded_count);
    if (handle->pending_count > 0) {
        ESP_LOGI(TAG, "📤 %"PRIu32" unsent events restored to pending queue", handle->pending_count);
    }
    
    cJSON_Delete(log_json);
    return ESP_OK;
}

static esp_err_t http_tool_save_log_internal(http_tool_handle_t handle) {
    if (handle == NULL || handle->fs_handle == NULL) {
        ESP_LOGD(TAG, "Cannot save log - filesystem tool not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Saving webhook event log to persistent storage");
    
    // Create JSON array for events
    cJSON *log_array = cJSON_CreateArray();
    if (log_array == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON array for webhook log");
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    // Serialize all events to JSON
    for (int i = 0; i < handle->event_count; i++) {
        webhook_event_t *event = &handle->events[i];
        
        cJSON *event_json = cJSON_CreateObject();
        if (event_json == NULL) continue;
        
        cJSON_AddNumberToObject(event_json, "event_type", event->event_type);
        cJSON_AddStringToObject(event_json, "tag_uid", event->tag_uid);
        cJSON_AddStringToObject(event_json, "device_id", event->device_id);
        cJSON_AddStringToObject(event_json, "tag_type", event->tag_type);
        cJSON_AddNumberToObject(event_json, "timestamp", event->timestamp);
        cJSON_AddBoolToObject(event_json, "sent", event->sent);
        cJSON_AddNumberToObject(event_json, "attempts", event->attempts);
        cJSON_AddNumberToObject(event_json, "next_retry_time_ms", event->next_retry_time_ms);
        
        cJSON_AddItemToArray(log_array, event_json);
    }
    
    int saved_count = handle->event_count;
    xSemaphoreGive(handle->mutex);
    
    // Save to filesystem
    esp_err_t err = fs_tool_save_json_log(handle->fs_handle, "webhook_events.json", log_array);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "✅ Saved %d webhook events to persistent storage", saved_count);
    } else {
        ESP_LOGE(TAG, "Failed to save webhook events: %s", esp_err_to_name(err));
    }
    
    cJSON_Delete(log_array);
    return err;
}


esp_err_t http_tool_set_device_id(http_tool_handle_t handle, const char *device_id) {
    if (handle == NULL || device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "Setting device ID to: %s", device_id);
    strncpy(handle->config.device_id, device_id, sizeof(handle->config.device_id) - 1);
    xSemaphoreGive(handle->mutex);
    
    return ESP_OK;
}

esp_err_t http_tool_check_connectivity(http_tool_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    // First check WiFi connectivity
    if (!handle->wifi_connected) {
        ESP_LOGD(TAG, "WiFi not connected - webhook unreachable");
        handle->webhook_reachable = false;
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    // Create HTTP client for GET request (HEAD not supported by webhook server)
    esp_http_client_config_t config = {
        .url = handle->config.webhook_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 3000, // Short timeout for connectivity check
        .event_handler = http_tool_http_event_handler,
        .user_data = handle,
        .disable_auto_redirect = true,
        .max_redirection_count = 0
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client for connectivity check");
        handle->webhook_reachable = false;
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Perform HEAD request to check webhook reachability
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    
    bool reachable = false;
    if (err == ESP_OK) {
        // Accept any 2xx, 3xx, 4xx as "reachable" - even 404 means server responds
        // Only 5xx and connection errors mean unreachable
        if (status_code >= 200 && status_code < 500) {
            reachable = true;
            ESP_LOGD(TAG, "Webhook endpoint reachable (status: %d)", status_code);
        } else if (status_code >= 500) {
            ESP_LOGW(TAG, "Webhook endpoint server error (status: %d)", status_code);
        }
    } else {
        ESP_LOGW(TAG, "Webhook endpoint unreachable: %s", esp_err_to_name(err));
    }
    
    handle->webhook_reachable = reachable;
    
    esp_http_client_cleanup(client);
    xSemaphoreGive(handle->mutex);
    
    return reachable ? ESP_OK : ESP_FAIL;
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* http_tool_event_to_string(http_tool_event_type_t event_type) {
    switch (event_type) {
        case HTTP_TOOL_EVENT_TRANSMISSION_SUCCESS: return "TRANSMISSION_SUCCESS";
        case HTTP_TOOL_EVENT_TRANSMISSION_FAILED:  return "TRANSMISSION_FAILED";
        case HTTP_TOOL_EVENT_QUEUE_FULL:           return "QUEUE_FULL";
        case HTTP_TOOL_EVENT_CONNECTIVITY_RESTORED: return "CONNECTIVITY_RESTORED";
        case HTTP_TOOL_EVENT_RETRY_EXHAUSTED:      return "RETRY_EXHAUSTED";
        case HTTP_TOOL_EVENT_CONFIG_UPDATED:       return "CONFIG_UPDATED";
        default: return "UNKNOWN";
    }
}

const char* webhook_event_to_string(webhook_event_type_t event_type) {
    switch (event_type) {
        case WEBHOOK_EVENT_TAG_PLACED:  return "TAG_PLACED";
        case WEBHOOK_EVENT_TAG_REMOVED: return "TAG_REMOVED";
        default: return "UNKNOWN";
    }
}

const char* http_tool_http_status_to_string(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

// =============================================================================
// Event Base Definition
// =============================================================================

ESP_EVENT_DEFINE_BASE(HTTP_TOOL_EVENTS);