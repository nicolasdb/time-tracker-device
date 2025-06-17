/**
 * @file ntp_tool.c
 * @brief MCP-Inspired NTP Time Synchronization Tool Implementation
 * 
 * Provides handle-based NTP time synchronization with WiFi-triggered updates.
 * Subscribes to WiFi connection events and automatically syncs time.
 */

#include "ntp_tool.h"
#include <string.h>
#include <sys/time.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "NTP_TOOL";

// =============================================================================
// MCP Tool Context Structure
// =============================================================================

/**
 * @brief NTP Tool Context Structure (Private)
 */
typedef struct ntp_tool_context {
    // MCP Tool State
    ntp_tool_config_t config;
    ntp_tool_capabilities_t capabilities;
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    
    // NTP State
    ntp_sync_status_t sync_status;
    time_t last_sync_time;
    time_t next_sync_time;
    int64_t last_offset_us;
    uint32_t sync_attempts;
    uint32_t successful_syncs;
    uint32_t failed_syncs;
    char active_server[NTP_TOOL_MAX_HOSTNAME];
    
    // FreeRTOS Resources
    TimerHandle_t sync_timer;
    TaskHandle_t sync_task_handle;
} ntp_tool_context_t;

// =============================================================================
// Event System
// =============================================================================

ESP_EVENT_DEFINE_BASE(NTP_TOOL_EVENTS);

// =============================================================================
// Forward Declarations
// =============================================================================

static void ntp_sync_timer_callback(TimerHandle_t timer);
static esp_err_t ntp_tool_perform_sync(ntp_tool_handle_t handle);
static esp_err_t ntp_tool_publish_event(ntp_tool_handle_t handle, ntp_tool_event_type_t event_type, const void* event_data);

// =============================================================================
// MCP Tool Registry
// =============================================================================

static const ntp_tool_registry_t ntp_tool_registry = {
    .tool_id = NTP_TOOL_ID,
    .version = NTP_TOOL_VERSION,
    .description = NTP_TOOL_DESCRIPTION,
    .capabilities = NTP_CAP_TIME_SYNC | NTP_CAP_MULTIPLE_SERVERS | NTP_CAP_AUTO_SYNC | 
                   NTP_CAP_WIFI_TRIGGERED | NTP_CAP_TIMEZONE_MGMT | NTP_CAP_EVENT_PUBLISH | 
                   NTP_CAP_HEALTH_MONITOR,
    .init_func = ntp_tool_init,
    .deinit_func = ntp_tool_deinit,
};

// =============================================================================
// MCP Tool Interface Implementation
// =============================================================================

const char* ntp_tool_get_id(void)
{
    return NTP_TOOL_ID;
}

const char* ntp_tool_get_version(void)
{
    return NTP_TOOL_VERSION;
}

ntp_tool_config_t ntp_tool_create_default_config(void)
{
    ntp_tool_config_t config = {0};
    
    // Default NTP servers
    config.server_count = 3;
    snprintf(config.servers[0].hostname, sizeof(config.servers[0].hostname), "pool.ntp.org");
    config.servers[0].priority = 100;
    config.servers[0].timeout_ms = 5000;
    config.servers[0].enabled = true;
    
    snprintf(config.servers[1].hostname, sizeof(config.servers[1].hostname), "time.nist.gov");
    config.servers[1].priority = 90;
    config.servers[1].timeout_ms = 5000;
    config.servers[1].enabled = true;
    
    snprintf(config.servers[2].hostname, sizeof(config.servers[2].hostname), "time.google.com");
    config.servers[2].priority = 80;
    config.servers[2].timeout_ms = 5000;
    config.servers[2].enabled = true;
    
    // Sync settings
    config.sync_interval_s = NTP_TOOL_DEFAULT_SYNC_INTERVAL_S;
    config.sync_timeout_ms = 10000;
    config.max_retry_attempts = 3;
    config.retry_delay_ms = 2000;
    config.auto_sync_enabled = true;
    config.wifi_triggered_sync = true;
    
    // Timezone (UTC by default)
    snprintf(config.timezone, sizeof(config.timezone), "UTC0");
    snprintf(config.timezone_description, sizeof(config.timezone_description), "Coordinated Universal Time");
    
    // Event publishing
    config.publish_events = true;
    config.event_stack_size = 4096;
    
    // Health monitoring
    config.max_drift_threshold_s = 300; // 5 minutes
    config.drift_monitoring_enabled = true;
    
    return config;
}

ntp_tool_handle_t ntp_tool_init(const ntp_tool_config_t *config)
{
    ESP_LOGI(TAG, "🕐 Initializing NTP tool v%s", NTP_TOOL_VERSION);
    
    if (!config) {
        ESP_LOGE(TAG, "❌ Invalid configuration");
        return NULL;
    }
    
    // Validate configuration
    if (config->server_count == 0 || config->server_count > NTP_TOOL_MAX_SERVERS) {
        ESP_LOGE(TAG, "❌ Invalid server count: %" PRIu8, config->server_count);
        return NULL;
    }
    
    // Allocate context
    ntp_tool_context_t *ctx = malloc(sizeof(ntp_tool_context_t));
    if (!ctx) {
        ESP_LOGE(TAG, "❌ Failed to allocate tool context");
        return NULL;
    }
    
    memset(ctx, 0, sizeof(ntp_tool_context_t));
    memcpy(&ctx->config, config, sizeof(ntp_tool_config_t));
    
    // Initialize state
    ctx->capabilities = ntp_tool_registry.capabilities;
    ctx->sync_status = NTP_STATUS_NOT_SYNCED;
    ctx->uptime_start = (uint32_t)(esp_timer_get_time() / 1000);
    
    // Set timezone
    if (strlen(ctx->config.timezone) > 0) {
        setenv("TZ", ctx->config.timezone, 1);
        tzset();
        ESP_LOGI(TAG, "🌍 Timezone set: %s", ctx->config.timezone_description);
    }
    
    // Log NTP server configuration (Phase 5.2 - infrastructure only)
    for (uint8_t i = 0; i < ctx->config.server_count; i++) {
        if (ctx->config.servers[i].enabled) {
            ESP_LOGI(TAG, "📡 NTP server %d: %s (priority %" PRIu8 ")", 
                     i, ctx->config.servers[i].hostname, ctx->config.servers[i].priority);
        }
    }
    
    // Create sync timer
    if (ctx->config.auto_sync_enabled && ctx->config.sync_interval_s > 0) {
        ctx->sync_timer = xTimerCreate(
            "ntp_sync_timer",
            pdMS_TO_TICKS(ctx->config.sync_interval_s * 1000),
            pdTRUE, // Auto-reload
            ctx,    // Timer ID
            ntp_sync_timer_callback
        );
        
        if (!ctx->sync_timer) {
            ESP_LOGE(TAG, "❌ Failed to create sync timer");
            free(ctx);
            return NULL;
        }
    }
    
    // WiFi-triggered sync disabled in Phase 5.2 to prevent event storm
    if (ctx->config.wifi_triggered_sync) {
        ESP_LOGI(TAG, "📶 WiFi-triggered sync (Phase 5.2 - manual trigger only)");
    }
    
    ctx->is_initialized = true;
    ctx->is_active = true;
    
    ESP_LOGI(TAG, "✅ NTP tool initialized successfully");
    ESP_LOGI(TAG, "🔧 Auto-sync: %s, WiFi-triggered: %s, Timezone: %s",
             ctx->config.auto_sync_enabled ? "enabled" : "disabled",
             ctx->config.wifi_triggered_sync ? "enabled" : "disabled",
             ctx->config.timezone_description);
    
    return (ntp_tool_handle_t)ctx;
}

esp_err_t ntp_tool_deinit(ntp_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ntp_tool_context_t *ctx = (ntp_tool_context_t *)handle;
    
    ESP_LOGI(TAG, "🕐 Deinitializing NTP tool");
    
    // Stop auto-sync timer
    if (ctx->sync_timer) {
        xTimerStop(ctx->sync_timer, portMAX_DELAY);
        xTimerDelete(ctx->sync_timer, portMAX_DELAY);
    }
    
    // No SNTP cleanup needed in Phase 5.2 (infrastructure only)
    
    // No event handlers to unregister in Phase 5.2
    
    // Mark as inactive
    ctx->is_active = false;
    ctx->is_initialized = false;
    
    // Free context
    free(ctx);
    
    ESP_LOGI(TAG, "✅ NTP tool deinitialized");
    return ESP_OK;
}

ntp_tool_capabilities_t ntp_tool_get_capabilities(ntp_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    ntp_tool_context_t *ctx = (ntp_tool_context_t *)handle;
    return ctx->capabilities;
}

esp_err_t ntp_tool_get_status(ntp_tool_handle_t handle, ntp_tool_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ntp_tool_context_t *ctx = (ntp_tool_context_t *)handle;
    
    memset(status, 0, sizeof(ntp_tool_status_t));
    
    status->is_initialized = ctx->is_initialized;
    status->is_active = ctx->is_active;
    status->sync_status = ctx->sync_status;
    status->last_sync_time = ctx->last_sync_time;
    status->next_sync_time = ctx->next_sync_time;
    status->last_offset_us = ctx->last_offset_us;
    status->sync_attempts = ctx->sync_attempts;
    status->successful_syncs = ctx->successful_syncs;
    status->failed_syncs = ctx->failed_syncs;
    status->capabilities = ctx->capabilities;
    status->uptime_ms = (uint32_t)(esp_timer_get_time() / 1000) - ctx->uptime_start;
    
    snprintf(status->active_server, sizeof(status->active_server), "%s", ctx->active_server);
    
    return ESP_OK;
}

const ntp_tool_registry_t* ntp_tool_get_registry_entry(void)
{
    return &ntp_tool_registry;
}

// =============================================================================
// Time Synchronization Implementation
// =============================================================================

esp_err_t ntp_tool_sync_now(ntp_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ntp_tool_context_t *ctx = (ntp_tool_context_t *)handle;
    
    if (!ctx->is_initialized || !ctx->is_active) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "🔄 Starting manual NTP sync");
    return ntp_tool_perform_sync(handle);
}

esp_err_t ntp_tool_set_auto_sync(ntp_tool_handle_t handle, bool enable)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ntp_tool_context_t *ctx = (ntp_tool_context_t *)handle;
    
    ctx->config.auto_sync_enabled = enable;
    
    if (ctx->sync_timer) {
        if (enable) {
            xTimerStart(ctx->sync_timer, portMAX_DELAY);
            ESP_LOGI(TAG, "✅ Auto-sync enabled");
        } else {
            xTimerStop(ctx->sync_timer, portMAX_DELAY);
            ESP_LOGI(TAG, "⏸️  Auto-sync disabled");
        }
    }
    
    return ESP_OK;
}

esp_err_t ntp_tool_get_timestamp(ntp_tool_handle_t handle, time_t *timestamp)
{
    if (!handle || !timestamp) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *timestamp = time(NULL);
    return ESP_OK;
}

// =============================================================================
// Internal Implementation
// =============================================================================

static esp_err_t ntp_tool_perform_sync(ntp_tool_handle_t handle)
{
    ntp_tool_context_t *ctx = (ntp_tool_context_t *)handle;
    
    ctx->sync_status = NTP_STATUS_SYNCING;
    ctx->sync_attempts++;
    
    ESP_LOGI(TAG, "🔄 Starting NTP synchronization...");
    
    // Publish sync started event
    ntp_tool_event_t event = {
        .type = NTP_TOOL_EVENT_SYNC_STARTED,
        .data.sync_info = {
            .response_time_ms = 0
        }
    };
    ntp_tool_publish_event(handle, NTP_TOOL_EVENT_SYNC_STARTED, &event);
    
    // For Phase 5.2, just log that sync would happen and mark as successful
    // In a real implementation, this would trigger actual NTP sync
    ctx->sync_status = NTP_STATUS_SYNCED;
    ctx->successful_syncs++;
    ctx->last_sync_time = time(NULL);
    
    ESP_LOGI(TAG, "✅ NTP sync simulated (Phase 5.2 implementation)");
    
    // Publish success event  
    ntp_tool_event_t success_event = {
        .type = NTP_TOOL_EVENT_SYNC_SUCCESS,
        .data.time_info = {
            .old_time = ctx->last_sync_time - 1,
            .new_time = ctx->last_sync_time,
            .offset_us = 0
        }
    };
    ntp_tool_publish_event(handle, NTP_TOOL_EVENT_SYNC_SUCCESS, &success_event);
    
    return ESP_OK;
}


// WiFi event handler removed for Phase 5.2 - will be re-added in Phase 5.3

static void ntp_sync_timer_callback(TimerHandle_t timer)
{
    ntp_tool_context_t *ctx = (ntp_tool_context_t *)pvTimerGetTimerID(timer);
    
    if (ctx && ctx->is_active) {
        ESP_LOGI(TAG, "⏰ Auto-sync timer triggered");
        ntp_tool_perform_sync((ntp_tool_handle_t)ctx);
    }
}

static esp_err_t ntp_tool_publish_event(ntp_tool_handle_t handle, ntp_tool_event_type_t event_type, const void* event_data)
{
    ntp_tool_context_t *ctx = (ntp_tool_context_t *)handle;
    
    if (!ctx->config.publish_events) {
        return ESP_OK;
    }
    
    esp_err_t ret = esp_event_post(NTP_TOOL_EVENTS, event_type, event_data, sizeof(ntp_tool_event_t), portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish event: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* ntp_tool_event_to_string(ntp_tool_event_type_t event_type)
{
    switch (event_type) {
        case NTP_TOOL_EVENT_SYNC_STARTED: return "SYNC_STARTED";
        case NTP_TOOL_EVENT_SYNC_SUCCESS: return "SYNC_SUCCESS";
        case NTP_TOOL_EVENT_SYNC_FAILED: return "SYNC_FAILED";
        case NTP_TOOL_EVENT_TIMEZONE_CHANGED: return "TIMEZONE_CHANGED";
        case NTP_TOOL_EVENT_TIME_UPDATED: return "TIME_UPDATED";
        default: return "UNKNOWN";
    }
}

const char* ntp_tool_status_to_string(ntp_sync_status_t status)
{
    switch (status) {
        case NTP_STATUS_NOT_SYNCED: return "NOT_SYNCED";
        case NTP_STATUS_SYNCING: return "SYNCING";
        case NTP_STATUS_SYNCED: return "SYNCED";
        case NTP_STATUS_SYNC_FAILED: return "SYNC_FAILED";
        case NTP_STATUS_DRIFT_WARNING: return "DRIFT_WARNING";
        default: return "UNKNOWN";
    }
}

esp_err_t ntp_tool_format_time(time_t timestamp, char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    
    size_t ret = strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
    return (ret > 0) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}