/**
 * @file ntp_tool.c
 * @brief Constitutional NTP Tool Implementation - Time Synchronization
 * 
 * Constitutional implementation following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf, PRIu32)
 * - Container isolation principles
 * 
 * Constitutional Authority: Process Map 13 dependency - "wait for NTP_SYNC" → "clock synced"
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#include "ntp_tool.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/time.h>
#include <time.h>

static const char* TAG = "ntp_tool";

// Constitutional NTP Tool Event Base
ESP_EVENT_DEFINE_BASE(NTP_TOOL_EVENTS);

// Forward declaration for network_tool events
ESP_EVENT_DECLARE_BASE(NETWORK_TOOL_EVENTS);

// Constitutional NTP Tool Context (Handle-based pattern)
struct ntp_tool {
    bool is_initialized;
    bool is_active;
    bool sntp_initialized;
    bool time_valid;
    ntp_tool_config_t config;
    ntp_tool_status_t status;
    uint64_t init_timestamp_us;
    uint32_t sync_attempts;
    uint32_t sync_failures;
    
    // Current synchronization state
    ntp_state_t current_state;
    time_t last_sync_time;
    uint64_t last_sync_esp_timer;  // Issue #6: esp_timer value at sync for correlation
    time_t system_boot_time;
    int64_t time_offset_us;
    char current_server[64];
    
    // Constitutional task management
    TaskHandle_t sync_task_handle;
    TimerHandle_t periodic_timer;
    esp_event_loop_handle_t event_loop;
    
    // Synchronization control
    bool sync_in_progress;
    bool wifi_connected;
    uint32_t current_server_index;
};

// =============================================================================
// Constitutional Timezone Auto-Detection (Issue #6)
// =============================================================================

/**
 * @brief Use configured timezone (simplified implementation)
 * 
 * @param handle NTP tool handle
 * @return esp_err_t ESP_OK on success
 */
static esp_err_t constitutional_use_configured_timezone(ntp_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🌍 Using configured timezone: %s", handle->config.timezone);
    return ESP_OK;
}

// =============================================================================
// Constitutional NTP Event Handlers
// =============================================================================

/**
 * @brief Constitutional network event handler
 * Responds to WiFi connection for automatic sync
 */
static void constitutional_network_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ntp_tool_handle_t handle = (ntp_tool_handle_t)arg;
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in network event handler");
        return;
    }
    
    // Listen for network tool connection events
    if (event_base == NETWORK_TOOL_EVENTS) {
        switch (event_id) {
            case 1: // NETWORK_TOOL_EVENT_CONNECTED
                ESP_LOGI(TAG, "📡 WiFi connected - triggering NTP sync");
                handle->wifi_connected = true;
                
                if (handle->config.wifi_triggered_sync && !handle->sync_in_progress) {
                    // Trigger sync via task notification (non-blocking)
                    if (handle->sync_task_handle) {
                        xTaskNotify(handle->sync_task_handle, 1, eSetBits);
                    }
                }
                break;
                
            case 2: // NETWORK_TOOL_EVENT_DISCONNECTED
                ESP_LOGW(TAG, "📡 WiFi disconnected - NTP sync disabled");
                handle->wifi_connected = false;
                break;
                
            default:
                break;
        }
    }
}

// =============================================================================
// Constitutional NTP Synchronization
// =============================================================================

/**
 * @brief SNTP notification callback
 * Called when SNTP sync completes (success or failure)
 */
static void constitutional_sntp_sync_callback(struct timeval *tv)
{
    // Note: This runs in SNTP context, keep minimal
    ESP_LOGI(TAG, "🕐 SNTP sync notification received");
}

/**
 * @brief Constitutional NTP synchronization implementation
 */
static esp_err_t constitutional_perform_ntp_sync(ntp_tool_handle_t handle)
{
    if (!handle || !handle->wifi_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (handle->sync_in_progress) {
        ESP_LOGW(TAG, "NTP sync already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    handle->sync_in_progress = true;
    handle->current_state = NTP_STATE_SYNCING;
    handle->status.sync_state = NTP_STATE_SYNCING;
    handle->sync_attempts++;
    
    ESP_LOGI(TAG, "🕐 Starting NTP synchronization");
    
    // Initialize SNTP if not already done
    if (!handle->sntp_initialized) {
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        
        // Configure NTP servers
        uint32_t server_count = 0;
        for (int i = 0; i < 4; i++) {
            if (strlen(handle->config.servers[i].hostname) > 0) {
                ESP_LOGI(TAG, "📡 Configuring NTP server %d: %s", i, handle->config.servers[i].hostname);
                esp_sntp_setservername(i, handle->config.servers[i].hostname);
                server_count++;
            }
        }
        
        if (server_count == 0) {
            ESP_LOGE(TAG, "No NTP servers configured");
            handle->sync_in_progress = false;
            handle->current_state = NTP_STATE_FAILED;
            handle->status.sync_state = NTP_STATE_FAILED;
            return ESP_ERR_INVALID_ARG;
        }
        
        // Set sync notification callback
        esp_sntp_set_time_sync_notification_cb(constitutional_sntp_sync_callback);
        
        // Initialize SNTP
        esp_sntp_init();
        handle->sntp_initialized = true;
        
        ESP_LOGI(TAG, "✅ SNTP initialized with %" PRIu32 " servers", server_count);
    }
    
    // Publish sync started event
    ntp_tool_event_t sync_event = {
        .state = NTP_STATE_SYNCING,
        .timestamp_us = esp_timer_get_time()
    };
    esp_event_post(NTP_TOOL_EVENTS, NTP_TOOL_EVENT_SYNC_STARTED, &sync_event, sizeof(sync_event), 0);
    
    // Wait for synchronization with timeout
    uint32_t timeout_ticks = pdMS_TO_TICKS(handle->config.sync_timeout_ms);
    uint32_t start_time = xTaskGetTickCount();
    
    while ((xTaskGetTickCount() - start_time) < timeout_ticks) {
        time_t now = 0;
        time(&now);
        
        // Check if time is reasonable (after year 2020)
        if (now > 1577836800) { // Jan 1, 2020
            // Successful sync
            handle->current_state = NTP_STATE_SYNCED;
            handle->status.sync_state = NTP_STATE_SYNCED;
            handle->status.time_valid = true;
            handle->time_valid = true;
            handle->last_sync_time = now;
            handle->last_sync_esp_timer = esp_timer_get_time();  // Issue #6: correlation storage
            handle->status.last_sync_time = now;
            handle->status.system_time = now;
            handle->status.sync_count++;
            handle->status.last_sync_timestamp_us = handle->last_sync_esp_timer;
            handle->sync_in_progress = false;
            
            // Store current server (simplified - use first configured server)
            if (strlen(handle->config.servers[0].hostname) > 0) {
                snprintf(handle->current_server, sizeof(handle->current_server), 
                        "%s", handle->config.servers[0].hostname);
                snprintf(handle->status.current_server, sizeof(handle->status.current_server), 
                        "%s", handle->config.servers[0].hostname);
            }
            
            ESP_LOGI(TAG, "✅ NTP sync successful - Time: %" PRIu32, (uint32_t)now);
            
            // Use configured timezone (Issue #6)
            constitutional_use_configured_timezone(handle);
            
            // Set timezone from configuration
            if (strlen(handle->config.timezone) > 0) {
                setenv("TZ", handle->config.timezone, 1);
                tzset();
                ESP_LOGI(TAG, "🌍 Timezone set: %s", handle->config.timezone);
            }
            
            // Publish success event
            ntp_tool_event_t success_event = {
                .state = NTP_STATE_SYNCED,
                .system_time = now,
                .timestamp_us = esp_timer_get_time()
            };
            snprintf(success_event.server_used, sizeof(success_event.server_used), "%s", handle->current_server);
            esp_event_post(NTP_TOOL_EVENTS, NTP_TOOL_EVENT_SYNC_SUCCESS, &success_event, sizeof(success_event), 0);
            esp_event_post(NTP_TOOL_EVENTS, NTP_TOOL_EVENT_TIME_UPDATED, &success_event, sizeof(success_event), 0);
            
            return ESP_OK;
        }
        
        // Constitutional task yield
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Timeout or failure
    handle->current_state = NTP_STATE_FAILED;
    handle->status.sync_state = NTP_STATE_FAILED;
    handle->status.sync_failures++;
    handle->sync_failures++;
    handle->sync_in_progress = false;
    
    ESP_LOGE(TAG, "❌ NTP sync failed - timeout after %" PRIu32 "ms", handle->config.sync_timeout_ms);
    
    // Publish failure event
    ntp_tool_event_t failure_event = {
        .state = NTP_STATE_FAILED,
        .timestamp_us = esp_timer_get_time()
    };
    esp_event_post(NTP_TOOL_EVENTS, NTP_TOOL_EVENT_SYNC_FAILED, &failure_event, sizeof(failure_event), 0);
    
    return ESP_ERR_TIMEOUT;
}

// =============================================================================
// Constitutional NTP Tasks
// =============================================================================

/**
 * @brief Constitutional NTP synchronization task
 */
static void constitutional_ntp_sync_task(void *arg)
{
    ntp_tool_handle_t handle = (ntp_tool_handle_t)arg;
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in NTP sync task");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "🕐 Constitutional NTP sync task started");
    
    while (handle->is_active) {
        uint32_t notification_value = 0;
        
        // Wait for sync trigger (manual or automatic)
        BaseType_t result = xTaskNotifyWait(0, UINT32_MAX, &notification_value, 
                                          pdMS_TO_TICKS(handle->config.sync_interval_s * 1000));
        
        if (!handle->is_active) {
            break;
        }
        
        if (result == pdTRUE || (handle->config.auto_sync_enabled && handle->wifi_connected)) {
            // Perform NTP synchronization
            esp_err_t sync_result = constitutional_perform_ntp_sync(handle);
            
            if (sync_result != ESP_OK && handle->config.retry_attempts > 1) {
                // Retry logic
                for (uint32_t retry = 1; retry < handle->config.retry_attempts; retry++) {
                    ESP_LOGW(TAG, "🔄 NTP sync retry %" PRIu32 "/%" PRIu32, retry, handle->config.retry_attempts);
                    vTaskDelay(pdMS_TO_TICKS(2000)); // 2 second delay between retries
                    
                    sync_result = constitutional_perform_ntp_sync(handle);
                    if (sync_result == ESP_OK) {
                        break;
                    }
                }
            }
        }
        
        // Constitutional task yield
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "🕐 Constitutional NTP sync task ended");
    vTaskDelete(NULL);
}

/**
 * @brief Periodic timer callback for automatic sync
 */
static void constitutional_periodic_timer_callback(TimerHandle_t timer)
{
    ntp_tool_handle_t handle = (ntp_tool_handle_t)pvTimerGetTimerID(timer);
    
    if (handle && handle->is_active && handle->config.auto_sync_enabled) {
        ESP_LOGI(TAG, "⏰ Periodic NTP sync triggered");
        
        if (handle->sync_task_handle) {
            xTaskNotify(handle->sync_task_handle, 2, eSetBits);
        }
    }
}

// =============================================================================
// Constitutional NTP Tool Interface Implementation
// =============================================================================

const char* ntp_tool_get_id(void)
{
    return "ntp_tool";
}

const char* ntp_tool_get_version(void)
{
    return "6.1.0";
}

ntp_tool_config_t ntp_tool_create_default_config(void)
{
    ntp_tool_config_t config = {
        .sync_interval_s = 3600,      // 1 hour
        .sync_timeout_ms = 10000,     // 10 seconds
        .retry_attempts = 3,
        .auto_sync_enabled = true,
        .wifi_triggered_sync = true,
        .publish_events = true
    };
    
    // Default NTP servers (80/20 rule - simple and reliable)
    snprintf(config.servers[0].hostname, sizeof(config.servers[0].hostname), "pool.ntp.org");
    config.servers[0].priority = 100;
    
    snprintf(config.servers[1].hostname, sizeof(config.servers[1].hostname), "time.nist.gov");
    config.servers[1].priority = 90;
    
    snprintf(config.servers[2].hostname, sizeof(config.servers[2].hostname), "time.google.com");
    config.servers[2].priority = 80;
    
    // Default timezone from Kconfig (Issue #6)
    snprintf(config.timezone, sizeof(config.timezone), CONFIG_HTTP_TOOL_TIMEZONE);
    
    return config;
}

ntp_tool_handle_t ntp_tool_init(const ntp_tool_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }
    
    ESP_LOGI(TAG, "🏗️ Constitutional NTP tool initializing");
    
    // Allocate handle with constitutional memory safety
    ntp_tool_handle_t handle = malloc(sizeof(struct ntp_tool));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate NTP tool handle");
        return NULL;
    }
    
    // Initialize handle with constitutional patterns
    memset(handle, 0, sizeof(struct ntp_tool));
    memcpy(&handle->config, config, sizeof(ntp_tool_config_t));
    handle->init_timestamp_us = esp_timer_get_time();
    handle->current_state = NTP_STATE_NOT_SYNCED;
    handle->status.sync_state = NTP_STATE_NOT_SYNCED;
    
    // Store boot time for uptime calculations
    time(&handle->system_boot_time);
    
    // Register network event handlers for WiFi triggers
    esp_err_t ret = esp_event_handler_register(NETWORK_TOOL_EVENTS, ESP_EVENT_ANY_ID, 
                                             &constitutional_network_event_handler, handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register network event handler: %s", esp_err_to_name(ret));
        // Continue initialization - network integration is optional
    }
    
    // Set control flags BEFORE task creation (constitutional race condition fix)
    handle->is_active = true;
    handle->is_initialized = true;
    handle->status.is_initialized = true;
    handle->status.is_active = true;
    
    // Create constitutional sync task
    BaseType_t task_ret = xTaskCreate(constitutional_ntp_sync_task,
                                     "ntp_sync",
                                     4096,
                                     handle,
                                     5,
                                     &handle->sync_task_handle);
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create NTP sync task");
        esp_event_handler_unregister(NETWORK_TOOL_EVENTS, ESP_EVENT_ANY_ID, &constitutional_network_event_handler);
        free(handle);
        return NULL;
    }
    
    // Create periodic timer for automatic sync
    if (handle->config.auto_sync_enabled) {
        handle->periodic_timer = xTimerCreate("ntp_periodic",
                                            pdMS_TO_TICKS(handle->config.sync_interval_s * 1000),
                                            pdTRUE,  // Auto-reload
                                            handle,  // Timer ID
                                            constitutional_periodic_timer_callback);
        
        if (handle->periodic_timer) {
            xTimerStart(handle->periodic_timer, 0);
            ESP_LOGI(TAG, "⏰ Periodic sync timer started (interval: %" PRIu32 "s)", handle->config.sync_interval_s);
        }
    }
    
    ESP_LOGI(TAG, "✅ Constitutional NTP tool: %s v%s initialized", 
             ntp_tool_get_id(), ntp_tool_get_version());
    ESP_LOGI(TAG, "🕐 Servers: %s, %s, %s", 
             handle->config.servers[0].hostname, 
             handle->config.servers[1].hostname, 
             handle->config.servers[2].hostname);
    
    return handle;
}

esp_err_t ntp_tool_deinit(ntp_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🕐 Constitutional NTP tool deinitializing");
    
    // Stop sync task
    handle->is_active = false;
    if (handle->sync_task_handle) {
        vTaskDelete(handle->sync_task_handle);
        handle->sync_task_handle = NULL;
    }
    
    // Stop periodic timer
    if (handle->periodic_timer) {
        xTimerDelete(handle->periodic_timer, pdMS_TO_TICKS(1000));
        handle->periodic_timer = NULL;
    }
    
    // Deinitialize SNTP
    if (handle->sntp_initialized) {
        esp_sntp_stop();
        handle->sntp_initialized = false;
    }
    
    // Unregister event handlers
    esp_event_handler_unregister(NETWORK_TOOL_EVENTS, ESP_EVENT_ANY_ID, &constitutional_network_event_handler);
    
    // Constitutional cleanup
    free(handle);
    
    ESP_LOGI(TAG, "✅ Constitutional NTP tool deinitialized");
    
    return ESP_OK;
}

esp_err_t ntp_tool_get_status(ntp_tool_handle_t handle, ntp_tool_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Update current system time
    time(&handle->status.system_time);
    
    memcpy(status, &handle->status, sizeof(ntp_tool_status_t));
    
    return ESP_OK;
}

esp_err_t ntp_tool_sync_now(ntp_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🕐 Manual NTP sync requested");
    
    if (handle->sync_task_handle) {
        xTaskNotify(handle->sync_task_handle, 3, eSetBits);
        return ESP_OK;
    }
    
    return ESP_ERR_INVALID_STATE;
}

bool ntp_tool_is_time_valid(ntp_tool_handle_t handle)
{
    if (!handle) {
        return false;
    }
    
    return handle->time_valid && (handle->current_state == NTP_STATE_SYNCED);
}

esp_err_t ntp_tool_get_time(ntp_tool_handle_t handle, time_t *current_time)
{
    if (!handle || !current_time) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!ntp_tool_is_time_valid(handle)) {
        return ESP_ERR_INVALID_STATE;
    }
    
    time(current_time);
    return ESP_OK;
}

esp_err_t ntp_tool_get_precise_timestamp(ntp_tool_handle_t handle, uint64_t *timestamp_us)
{
    if (!handle || !timestamp_us) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!ntp_tool_is_time_valid(handle)) {
        ESP_LOGE(TAG, "Time not valid - cannot provide precise timestamp");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get high-precision timestamp for payload creation (esp_timer uptime)
    *timestamp_us = esp_timer_get_time();
    
    return ESP_OK;
}

esp_err_t ntp_tool_get_sync_correlation(ntp_tool_handle_t handle, 
                                       time_t *real_time_at_sync, 
                                       uint64_t *esp_timer_at_sync)
{
    if (!handle || !real_time_at_sync || !esp_timer_at_sync) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!ntp_tool_is_time_valid(handle) || handle->last_sync_time == 0) {
        ESP_LOGW(TAG, "No valid NTP sync correlation available");
        return ESP_ERR_INVALID_STATE;
    }
    
    *real_time_at_sync = handle->last_sync_time;
    *esp_timer_at_sync = handle->last_sync_esp_timer;
    
    ESP_LOGD(TAG, "✅ NTP sync correlation: real_time=%" PRIu32 ", esp_timer=%" PRIu64, 
             (uint32_t)*real_time_at_sync, *esp_timer_at_sync);
    
    return ESP_OK;
}

esp_err_t ntp_tool_set_timezone(ntp_tool_handle_t handle, const char* timezone)
{
    if (!handle || !timezone) {
        return ESP_ERR_INVALID_ARG;
    }
    
    snprintf(handle->config.timezone, sizeof(handle->config.timezone), "%s", timezone);
    
    // Apply timezone immediately if time is valid
    if (handle->time_valid) {
        setenv("TZ", timezone, 1);
        tzset();
        ESP_LOGI(TAG, "🌍 Timezone updated: %s", timezone);
        
        // Publish timezone change event
        ntp_tool_event_t tz_event = {
            .state = handle->current_state,
            .timestamp_us = esp_timer_get_time()
        };
        esp_event_post(NTP_TOOL_EVENTS, NTP_TOOL_EVENT_TIMEZONE_CHANGED, &tz_event, sizeof(tz_event), 0);
    }
    
    return ESP_OK;
}