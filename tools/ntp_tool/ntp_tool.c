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
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
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
    
    // SNTP State Management
    bool sntp_initialized;
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
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ntp_sync_notification_cb(struct timeval *tv);

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
    
    // Timezone (UTC by default, can be customized per location)
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
    
    // Register WiFi event handler for automatic sync triggers
    if (ctx->config.wifi_triggered_sync) {
        esp_err_t ret = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &wifi_event_handler, ctx);
        if (ret == ESP_OK) {
            ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, ctx);
        }
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register WiFi event handlers: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "📶 WiFi-triggered sync enabled");
        }
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
    
    // Cleanup SNTP service
    if (ctx->sntp_initialized) {
        esp_netif_sntp_deinit();
        ctx->sntp_initialized = false;
    }
    
    // Unregister WiFi event handlers
    if (ctx->config.wifi_triggered_sync) {
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &wifi_event_handler);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);
    }
    
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
    
    // Skip manual sync if we recently succeeded
    if (ctx->sync_status == NTP_STATUS_SYNCED && ctx->last_sync_time > 0) {
        time_t now = time(NULL);
        if ((now - ctx->last_sync_time) < 60) { // Within last 60 seconds
            ESP_LOGI(TAG, "⏭️ Skipping manual sync - recently synced %" PRId64 "s ago", (int64_t)(now - ctx->last_sync_time));
            return ESP_OK;
        }
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
    
    ESP_LOGI(TAG, "🔄 Starting real NTP synchronization...");
    
    // Publish sync started event
    ntp_tool_event_t event = {
        .type = NTP_TOOL_EVENT_SYNC_STARTED,
        .data.sync_info = {
            .response_time_ms = 0
        }
    };
    ntp_tool_publish_event(handle, NTP_TOOL_EVENT_SYNC_STARTED, &event);
    
    // Store old time for offset calculation
    time_t old_time = time(NULL);
    
    // Initialize basic SNTP configuration
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.start = false;
    config.server_from_dhcp = false;
    config.renew_servers_after_new_IP = false;
    
    // Set sync notification callback
    esp_sntp_set_time_sync_notification_cb(ntp_sync_notification_cb);
    
    // Initialize SNTP only if not already initialized
    esp_err_t ret = ESP_OK;
    if (!ctx->sntp_initialized) {
        ret = esp_netif_sntp_init(&config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to initialize SNTP: %s", esp_err_to_name(ret));
            ctx->sync_status = NTP_STATUS_SYNC_FAILED;
            ctx->failed_syncs++;
            return ret;
        }
        ctx->sntp_initialized = true;
        ESP_LOGI(TAG, "📅 SNTP service initialized");
        
        // Add multiple NTP servers for fallback
        uint8_t server_index = 0;
        for (uint8_t i = 0; i < ctx->config.server_count && server_index < SNTP_MAX_SERVERS; i++) {
            if (ctx->config.servers[i].enabled) {
                esp_sntp_setservername(server_index, ctx->config.servers[i].hostname);
                ESP_LOGI(TAG, "📡 NTP server %d: %s", server_index, ctx->config.servers[i].hostname);
                server_index++;
            }
        }
        
        if (server_index == 0) {
            ESP_LOGW(TAG, "⚠️  No custom servers configured, using default pool.ntp.org");
        }
    }
    
    esp_netif_sntp_start();
    
    // Wait for synchronization with timeout
    int retry = 0;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(1000)) == ESP_ERR_TIMEOUT && ++retry < (ctx->config.sync_timeout_ms / 1000)) {
        ESP_LOGI(TAG, "⏳ Waiting for NTP sync... (%d/%" PRIu32 ")", retry, ctx->config.sync_timeout_ms / 1000);
    }
    
    if (retry >= (ctx->config.sync_timeout_ms / 1000)) {
        ESP_LOGE(TAG, "❌ NTP sync timeout after %" PRIu32 "ms", ctx->config.sync_timeout_ms);
        ctx->sync_status = NTP_STATUS_SYNC_FAILED;
        ctx->failed_syncs++;
        
        // Publish failure event
        ntp_tool_event_t fail_event = {
            .type = NTP_TOOL_EVENT_SYNC_FAILED,
            .data.error_info = {
                .error_code = ESP_ERR_TIMEOUT,
                .retry_count = ctx->failed_syncs
            }
        };
        ntp_tool_publish_event(handle, NTP_TOOL_EVENT_SYNC_FAILED, &fail_event);
        
        return ESP_ERR_TIMEOUT;
    }
    
    // Sync successful
    time_t new_time = time(NULL);
    int64_t offset_us = (new_time - old_time) * 1000000LL;
    
    ctx->sync_status = NTP_STATUS_SYNCED;
    ctx->successful_syncs++;
    ctx->last_sync_time = new_time;
    ctx->last_offset_us = offset_us;
    ctx->next_sync_time = new_time + ctx->config.sync_interval_s;
    
    // Store active server (use first configured server)
    if (ctx->config.server_count > 0) {
        snprintf(ctx->active_server, sizeof(ctx->active_server), "%s", ctx->config.servers[0].hostname);
    }
    
    ESP_LOGI(TAG, "✅ NTP sync successful - Time: %" PRId64 ", Offset: %" PRId64 " μs", (int64_t)new_time, offset_us);
    
    // Publish success event
    ntp_tool_event_t success_event = {
        .type = NTP_TOOL_EVENT_SYNC_SUCCESS,
        .data.time_info = {
            .old_time = old_time,
            .new_time = new_time,
            .offset_us = offset_us
        }
    };
    ntp_tool_publish_event(handle, NTP_TOOL_EVENT_SYNC_SUCCESS, &success_event);
    
    return ESP_OK;
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ntp_tool_context_t *ctx = (ntp_tool_context_t *)arg;
    
    if (!ctx || !ctx->is_active) {
        return;
    }
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "📶 WiFi connected - preparing for NTP sync");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "🌐 IP acquired - triggering NTP sync");
        
        // Trigger immediate sync on WiFi connection
        esp_err_t ret = ntp_tool_perform_sync((ntp_tool_handle_t)ctx);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "WiFi-triggered NTP sync failed: %s", esp_err_to_name(ret));
        }
    }
}

static void ntp_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "📅 SNTP notification: time is set to %" PRId64 ".%06ld", (int64_t)tv->tv_sec, tv->tv_usec);
}

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