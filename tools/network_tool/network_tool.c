/**
 * @file network_tool.c
 * @brief MCP-Inspired WiFi Tool Implementation
 * 
 * Transformed from wifi_manager to follow MCP tool composition patterns.
 * Breaks coupling with ap_webserver by using event-driven communication.
 */

#include "network_tool.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <string.h>
#include <sys/stat.h>

// Forward declaration for fs_tool functions (MCP dependency injection pattern)
esp_err_t fs_tool_save_json_config(void* fs_handle, const char* filename, const cJSON* json_data);
esp_err_t fs_tool_load_json_config(void* fs_handle, const char* filename, cJSON** json_data);

static const char *TAG = "NETWORK_TOOL";

// =============================================================================
// MCP Tool Event System
// =============================================================================

ESP_EVENT_DEFINE_BASE(NETWORK_TOOL_EVENTS);

// =============================================================================
// MCP Tool Configuration & Constants
// =============================================================================

#define NETWORK_TOOL_TASK_STACK_SIZE    4096
#define NETWORK_TOOL_TASK_PRIORITY      5
// Use Kconfig values with fallback defaults
#ifndef CONFIG_NETWORK_TOOL_MAX_RETRY_ATTEMPTS
#define CONFIG_NETWORK_TOOL_MAX_RETRY_ATTEMPTS 3
#endif
#ifndef CONFIG_NETWORK_TOOL_RETRY_DELAY_MS
#define CONFIG_NETWORK_TOOL_RETRY_DELAY_MS 2000
#endif
#ifndef CONFIG_NETWORK_TOOL_CONNECT_TIMEOUT_MS
#define CONFIG_NETWORK_TOOL_CONNECT_TIMEOUT_MS 10000
#endif

// WiFi event group bits
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_FAIL_BIT         BIT1
#define WIFI_AP_STARTED_BIT   BIT2

// =============================================================================
// Internal Tool Structure (Enhanced from wifi_manager)
// =============================================================================

/**
 * @brief MCP-Inspired WiFi Tool Context
 */
struct network_tool_context {
    // Tool Metadata (MCP Pattern)
    network_tool_config_t config;
    network_tool_capabilities_t capabilities;
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    
    // WiFi State Management
    bool sta_connected;
    bool ap_active;
    char current_ssid[NETWORK_TOOL_MAX_SSID_LEN];
    char ip_address[16];
    int8_t rssi;
    uint8_t retry_count;
    uint8_t current_network_index;
    uint8_t ap_client_count;
    
    // ESP-IDF Resources
    esp_netif_t *sta_netif;
    esp_netif_t *ap_netif;
    EventGroupHandle_t wifi_event_group;
    SemaphoreHandle_t config_mutex;
    
    // MCP Tool Dependencies
    fs_tool_handle_t fs_tool;
    
    // Event Publishing
    TaskHandle_t event_task;
    bool publish_events;
};

// =============================================================================
// Forward Declarations
// =============================================================================

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t publish_wifi_event(struct network_tool_context *ctx, network_tool_event_type_t type, void* data);
static esp_err_t load_networks_from_config(struct network_tool_context *ctx);
static esp_err_t load_networks_from_json(struct network_tool_context *ctx, const cJSON *wifi_config);
static esp_err_t try_connect_next_network(struct network_tool_context *ctx);
static esp_err_t start_sta_mode(struct network_tool_context *ctx);
static esp_err_t start_ap_mode(struct network_tool_context *ctx);

// =============================================================================
// MCP Tool Interface Implementation
// =============================================================================

const char* network_tool_get_id(void)
{
    return NETWORK_TOOL_ID;
}

const char* network_tool_get_version(void)
{
    return NETWORK_TOOL_VERSION;
}

network_tool_config_t network_tool_create_default_config(void)
{
    network_tool_config_t config = {0};
    
    // Default AP configuration (Proof of Concept - Low Risk Scenario)
    strncpy(config.ap_config.ssid, "TimeTracker-Setup", sizeof(config.ap_config.ssid) - 1);
    config.ap_config.password[0] = '\0';  // Open network for simple PoC setup
    config.ap_config.channel = 1;
    config.ap_config.max_connections = 4;
    config.ap_config.auth_mode = WIFI_AUTH_OPEN;
    config.ap_config.ssid_hidden = false;
    strncpy(config.ap_config.ip_address, "192.168.4.1", sizeof(config.ap_config.ip_address) - 1);
    strncpy(config.ap_config.gateway, "192.168.4.1", sizeof(config.ap_config.gateway) - 1);
    strncpy(config.ap_config.netmask, "255.255.255.0", sizeof(config.ap_config.netmask) - 1);
    
    // Default behavior settings (from Kconfig)
    config.connect_timeout_ms = CONFIG_NETWORK_TOOL_CONNECT_TIMEOUT_MS;
    config.max_retry_attempts = CONFIG_NETWORK_TOOL_MAX_RETRY_ATTEMPTS;
    config.retry_delay_ms = CONFIG_NETWORK_TOOL_RETRY_DELAY_MS;
    config.auto_reconnect = true;
    config.enable_ap_fallback = true;
    
    // Default configuration persistence
    strncpy(config.config_file_path, "/littlefs/wifi.json", sizeof(config.config_file_path) - 1);
    config.auto_save_config = true;
    
    // Default event publishing
    config.publish_events = true;
    config.event_stack_size = NETWORK_TOOL_TASK_STACK_SIZE;
    
    return config;
}

network_tool_handle_t network_tool_init(const network_tool_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Configuration cannot be NULL");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Initializing MCP-inspired WiFi tool v%s", NETWORK_TOOL_VERSION);
    
    // Allocate tool context
    struct network_tool_context *ctx = calloc(1, sizeof(struct network_tool_context));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate tool context");
        return NULL;
    }
    
    // Copy configuration
    ctx->config = *config;
    ctx->uptime_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ctx->publish_events = config->publish_events;
    
    // Set capabilities
    ctx->capabilities = NETWORK_CAP_STA_MODE | 
                       NETWORK_CAP_AP_MODE |
                       NETWORK_CAP_MULTI_NETWORK |
                       NETWORK_CAP_AUTO_RECONNECT |
                       NETWORK_CAP_CONFIG_PERSIST |
                       NETWORK_CAP_EVENT_PUBLISH |
                       NETWORK_CAP_HEALTH_MONITOR;
    
    // Initialize synchronization
    ctx->config_mutex = xSemaphoreCreateMutex();
    if (!ctx->config_mutex) {
        ESP_LOGE(TAG, "Failed to create config mutex");
        free(ctx);
        return NULL;
    }
    
    ctx->wifi_event_group = xEventGroupCreate();
    if (!ctx->wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        vSemaphoreDelete(ctx->config_mutex);
        free(ctx);
        return NULL;
    }
    
    // Initialize ESP networking
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "netif_init returned: %s", esp_err_to_name(ret));
    }
    
    // Create event loop if needed
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "event_loop_create returned: %s", esp_err_to_name(ret));
    }
    
    // Create network interfaces
    ctx->sta_netif = esp_netif_create_default_wifi_sta();
    ctx->ap_netif = esp_netif_create_default_wifi_ap();
    
    if (!ctx->sta_netif || !ctx->ap_netif) {
        ESP_LOGE(TAG, "Failed to create network interfaces");
        if (ctx->sta_netif) esp_netif_destroy(ctx->sta_netif);
        if (ctx->ap_netif) esp_netif_destroy(ctx->ap_netif);
        vEventGroupDelete(ctx->wifi_event_group);
        vSemaphoreDelete(ctx->config_mutex);
        free(ctx);
        return NULL;
    }
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        esp_netif_destroy(ctx->sta_netif);
        esp_netif_destroy(ctx->ap_netif);
        vEventGroupDelete(ctx->wifi_event_group);
        vSemaphoreDelete(ctx->config_mutex);
        free(ctx);
        return NULL;
    }
    
    // Register event handlers
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        esp_netif_destroy(ctx->sta_netif);
        esp_netif_destroy(ctx->ap_netif);
        vEventGroupDelete(ctx->wifi_event_group);
        vSemaphoreDelete(ctx->config_mutex);
        free(ctx);
        return NULL;
    }
    
    ret = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
        esp_wifi_deinit();
        esp_netif_destroy(ctx->sta_netif);
        esp_netif_destroy(ctx->ap_netif);
        vEventGroupDelete(ctx->wifi_event_group);
        vSemaphoreDelete(ctx->config_mutex);
        free(ctx);
        return NULL;
    }
    
    // Load network configuration
    load_networks_from_config(ctx);
    
    ctx->is_initialized = true;
    ctx->is_active = true;
    
    ESP_LOGI(TAG, "WiFi tool initialized successfully");
    ESP_LOGI(TAG, "  Networks: %d, AP fallback: %s, Events: %s", 
             ctx->config.network_count,
             ctx->config.enable_ap_fallback ? "enabled" : "disabled",
             ctx->publish_events ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Capabilities: 0x%02X", ctx->capabilities);
    
    return ctx;
}

esp_err_t network_tool_deinit(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    ESP_LOGI(TAG, "Deinitializing WiFi tool");
    
    // Stop WiFi operations
    ctx->is_active = false;
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // Unregister event handlers
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler);
    
    // Cleanup network interfaces
    if (ctx->sta_netif) esp_netif_destroy(ctx->sta_netif);
    if (ctx->ap_netif) esp_netif_destroy(ctx->ap_netif);
    
    // Cleanup synchronization
    if (ctx->wifi_event_group) vEventGroupDelete(ctx->wifi_event_group);
    if (ctx->config_mutex) vSemaphoreDelete(ctx->config_mutex);
    
    // Free context
    free(ctx);
    
    ESP_LOGI(TAG, "WiFi tool deinitialized");
    return ESP_OK;
}

network_tool_capabilities_t network_tool_get_capabilities(network_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    return ctx->capabilities;
}

esp_err_t network_tool_get_status(network_tool_handle_t handle, network_tool_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    status->is_initialized = ctx->is_initialized;
    status->is_active = ctx->is_active;
    status->sta_connected = ctx->sta_connected;
    status->ap_active = ctx->ap_active;
    snprintf(status->current_ssid, sizeof(status->current_ssid), "%s", ctx->current_ssid);
    snprintf(status->ip_address, sizeof(status->ip_address), "%s", ctx->ip_address);
    status->rssi = ctx->rssi;
    status->retry_count = ctx->retry_count;
    status->uptime_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - ctx->uptime_start;
    status->ap_client_count = ctx->ap_client_count;
    status->capabilities = ctx->capabilities;
    
    return ESP_OK;
}

esp_err_t network_tool_set_fs_dependency(network_tool_handle_t handle, fs_tool_handle_t fs_handle)
{
    if (!handle || !fs_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ctx->fs_tool = fs_handle;
    
    ESP_LOGI(TAG, "Filesystem dependency set (use network_tool_load_networks_from_json to load config)");
    
    return ESP_OK;
}

esp_err_t network_tool_start_auto_connection(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (ctx->config.network_count == 0) {
        ESP_LOGW(TAG, "No WiFi networks configured, cannot start auto connection");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Starting automatic WiFi connection (%d networks available)", ctx->config.network_count);
    
    // Reset network index to start from first network (essential for multi-network iteration)
    ctx->current_network_index = 0;
    ctx->retry_count = 0;
    
    // Start STA mode which will automatically trigger connection attempt
    return network_tool_start_sta(handle);
}

esp_err_t network_tool_load_networks_from_json(network_tool_handle_t handle, const cJSON *wifi_config)
{
    if (!handle || !wifi_config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return load_networks_from_json(ctx, wifi_config);
}

// =============================================================================
// WiFi Operations Implementation
// =============================================================================

esp_err_t network_tool_start_sta(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting WiFi STA mode");
    
    return start_sta_mode(ctx);
}

esp_err_t network_tool_start_ap(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting WiFi AP mode");
    
    return start_ap_mode(ctx);
}

esp_err_t network_tool_stop(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    ESP_LOGI(TAG, "Stopping WiFi operations");
    
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_OK) {
        ctx->sta_connected = false;
        ctx->ap_active = false;
        ctx->current_ssid[0] = '\0';
        ctx->ip_address[0] = '\0';
        ctx->retry_count = 0;
    }
    
    return ret;
}

bool network_tool_is_connected(network_tool_handle_t handle)
{
    if (!handle) {
        return false;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    return ctx->sta_connected;
}

esp_err_t network_tool_get_ip_address(network_tool_handle_t handle, char* ip_str, size_t len)
{
    if (!handle || !ip_str || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->sta_connected || strlen(ctx->ip_address) == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    strncpy(ip_str, ctx->ip_address, len - 1);
    ip_str[len - 1] = '\0';
    
    return ESP_OK;
}

// =============================================================================
// Event Handlers (Enhanced from Original)
// =============================================================================

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    struct network_tool_context *ctx = (struct network_tool_context*)arg;
    
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi STA started");
            try_connect_next_network(ctx);
            break;
            
        case WIFI_EVENT_STA_CONNECTED: {
            wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*)event_data;
            ESP_LOGI(TAG, "Connected to SSID: %s", event->ssid);
            
            ctx->sta_connected = true;
            snprintf(ctx->current_ssid, sizeof(ctx->current_ssid), "%s", (char*)event->ssid);
            ctx->retry_count = 0;
            
            // Publish event for other tools
            network_tool_event_t wifi_event = {
                .type = NETWORK_TOOL_EVENT_STA_CONNECTED
            };
            snprintf(wifi_event.data.sta_info.ssid, sizeof(wifi_event.data.sta_info.ssid), "%s", (char*)event->ssid);
            memcpy(wifi_event.data.sta_info.bssid, event->bssid, 6);
            publish_wifi_event(ctx, NETWORK_TOOL_EVENT_STA_CONNECTED, &wifi_event);
            
            xEventGroupSetBits(ctx->wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        }
        
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
            ESP_LOGW(TAG, "Disconnected from WiFi (reason: %d)", event->reason);
            
            ctx->sta_connected = false;
            ctx->ip_address[0] = '\0';
            
            // Publish disconnect event
            network_tool_event_t wifi_event = {
                .type = NETWORK_TOOL_EVENT_STA_DISCONNECTED,
                .data.error_info.reason = event->reason,
                .data.error_info.retry_count = ctx->retry_count
            };
            publish_wifi_event(ctx, NETWORK_TOOL_EVENT_STA_DISCONNECTED, &wifi_event);
            
            // Handle reconnection - try same network again
            if (ctx->config.auto_reconnect && ctx->retry_count < ctx->config.max_retry_attempts) {
                ctx->retry_count++;
                ESP_LOGI(TAG, "Retrying connection (%d/%d)", ctx->retry_count, ctx->config.max_retry_attempts);
                vTaskDelay(pdMS_TO_TICKS(ctx->config.retry_delay_ms));
                esp_wifi_connect();
            } else {
                // Max retries for current network reached - try next network
                ctx->current_network_index++;
                ctx->retry_count = 0; // Reset retry count for new network
                
                if (ctx->current_network_index < ctx->config.network_count) {
                    // Try next network in the list
                    ESP_LOGI(TAG, "Trying next network (%d/%d)", 
                             ctx->current_network_index + 1, ctx->config.network_count);
                    vTaskDelay(pdMS_TO_TICKS(ctx->config.retry_delay_ms));
                    try_connect_next_network(ctx);
                } else if (ctx->config.enable_ap_fallback) {
                    // All networks exhausted - start AP mode
                    ESP_LOGI(TAG, "All networks failed, starting AP mode");
                    xEventGroupSetBits(ctx->wifi_event_group, WIFI_FAIL_BIT);
                    start_ap_mode(ctx);
                } else {
                    // No AP fallback - reset to first network and stop
                    ESP_LOGI(TAG, "All networks failed, no AP fallback configured");
                    ctx->current_network_index = 0;
                    xEventGroupSetBits(ctx->wifi_event_group, WIFI_FAIL_BIT);
                }
            }
            break;
        }
        
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "WiFi AP started");
            ctx->ap_active = true;
            
            // Publish AP started event for webserver tool
            network_tool_event_t ap_event = {
                .type = NETWORK_TOOL_EVENT_AP_STARTED
            };
            snprintf(ap_event.data.ap_info.ap_ssid, sizeof(ap_event.data.ap_info.ap_ssid), "%s", ctx->config.ap_config.ssid);
            snprintf(ap_event.data.ap_info.ip_address, sizeof(ap_event.data.ap_info.ip_address), "%s", ctx->config.ap_config.ip_address);
            ap_event.data.ap_info.channel = ctx->config.ap_config.channel;
            publish_wifi_event(ctx, NETWORK_TOOL_EVENT_AP_STARTED, &ap_event);
            
            xEventGroupSetBits(ctx->wifi_event_group, WIFI_AP_STARTED_BIT);
            break;
            
        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG, "WiFi AP stopped");
            ctx->ap_active = false;
            ctx->ap_client_count = 0;
            publish_wifi_event(ctx, NETWORK_TOOL_EVENT_AP_STOPPED, NULL);
            break;
            
        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
            ESP_LOGI(TAG, "Client connected to AP (MAC: %02x:%02x:%02x:%02x:%02x:%02x)", 
                     event->mac[0], event->mac[1], event->mac[2], 
                     event->mac[3], event->mac[4], event->mac[5]);
            ctx->ap_client_count++;
            publish_wifi_event(ctx, NETWORK_TOOL_EVENT_AP_CLIENT_CONNECTED, NULL);
            break;
        }
        
        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
            ESP_LOGI(TAG, "Client disconnected from AP (MAC: %02x:%02x:%02x:%02x:%02x:%02x)", 
                     event->mac[0], event->mac[1], event->mac[2], 
                     event->mac[3], event->mac[4], event->mac[5]);
            if (ctx->ap_client_count > 0) ctx->ap_client_count--;
            publish_wifi_event(ctx, NETWORK_TOOL_EVENT_AP_CLIENT_DISCONNECTED, NULL);
            break;
        }
    }
}

static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    struct network_tool_context *ctx = (struct network_tool_context*)arg;
    
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // Store IP address
        snprintf(ctx->ip_address, sizeof(ctx->ip_address), IPSTR, IP2STR(&event->ip_info.ip));
        
        // Publish IP acquired event
        network_tool_event_t wifi_event = {
            .type = NETWORK_TOOL_EVENT_IP_ACQUIRED
        };
        snprintf(wifi_event.data.ip_info.ip_address, sizeof(wifi_event.data.ip_info.ip_address), "%s", ctx->ip_address);
        snprintf(wifi_event.data.ip_info.gateway, sizeof(wifi_event.data.ip_info.gateway), 
                IPSTR, IP2STR(&event->ip_info.gw));
        snprintf(wifi_event.data.ip_info.netmask, sizeof(wifi_event.data.ip_info.netmask), 
                IPSTR, IP2STR(&event->ip_info.netmask));
        
        publish_wifi_event(ctx, NETWORK_TOOL_EVENT_IP_ACQUIRED, &wifi_event);
    }
}

// =============================================================================
// Internal Helper Functions
// =============================================================================

static esp_err_t publish_wifi_event(struct network_tool_context *ctx, network_tool_event_type_t type, void* data)
{
    if (!ctx->publish_events) {
        return ESP_OK;
    }
    
    network_tool_event_t event = {.type = type};
    if (data) {
        memcpy(&event, data, sizeof(network_tool_event_t));
    }
    
    ESP_LOGD(TAG, "Publishing WiFi event: %s", network_tool_event_to_string(type));
    
    return esp_event_post(NETWORK_TOOL_EVENTS, type, &event, sizeof(event), 0);
}

static esp_err_t load_networks_from_config(struct network_tool_context *ctx)
{
    ESP_LOGD(TAG, "Loading networks from config (placeholder - will be set via dependency injection)");
    ctx->config.network_count = 0;
    return ESP_OK;
}

static esp_err_t load_networks_from_json(struct network_tool_context *ctx, const cJSON *wifi_config)
{
    if (!wifi_config) {
        ESP_LOGE(TAG, "WiFi config JSON is NULL");
        ctx->config.network_count = 0;
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Loading WiFi networks from JSON configuration");
    
    // Parse networks array
    cJSON *networks_array = cJSON_GetObjectItem(wifi_config, "networks");
    if (!networks_array || !cJSON_IsArray(networks_array)) {
        ESP_LOGE(TAG, "wifi.json missing 'networks' array");
        ctx->config.network_count = 0;
        return ESP_ERR_INVALID_ARG;
    }
    
    int network_count = cJSON_GetArraySize(networks_array);
    if (network_count > NETWORK_TOOL_MAX_NETWORKS) {
        ESP_LOGW(TAG, "Too many networks in config (%d), limiting to %d", 
                 network_count, NETWORK_TOOL_MAX_NETWORKS);
        network_count = NETWORK_TOOL_MAX_NETWORKS;
    }
    
    // Parse each network entry
    ctx->config.network_count = 0;
    for (int i = 0; i < network_count; i++) {
        cJSON *network_item = cJSON_GetArrayItem(networks_array, i);
        if (!network_item) continue;
        
        cJSON *ssid_item = cJSON_GetObjectItem(network_item, "ssid");
        cJSON *password_item = cJSON_GetObjectItem(network_item, "password");
        
        if (!ssid_item || !cJSON_IsString(ssid_item)) {
            ESP_LOGW(TAG, "Network %d missing SSID, skipping", i);
            continue;
        }
        
        wifi_network_config_t *network = &ctx->config.networks[ctx->config.network_count];
        
        // Copy SSID
        snprintf(network->ssid, sizeof(network->ssid), "%s", cJSON_GetStringValue(ssid_item));
        
        // Copy password (if present)
        if (password_item && cJSON_IsString(password_item)) {
            snprintf(network->password, sizeof(network->password), "%s", cJSON_GetStringValue(password_item));
        } else {
            network->password[0] = '\0';  // Open network
        }
        
        // Set defaults
        network->priority = 1;
        network->auth_mode = (strlen(network->password) > 0) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
        network->hidden = false;
        
        ESP_LOGI(TAG, "Loaded network %d: '%s' (auth: %s)", 
                 ctx->config.network_count,
                 network->ssid,
                 strlen(network->password) > 0 ? "WPA2" : "Open");
        
        ctx->config.network_count++;
    }
    
    ESP_LOGI(TAG, "Successfully loaded %d WiFi networks from JSON config", ctx->config.network_count);
    
    return ESP_OK;
}

static esp_err_t try_connect_next_network(struct network_tool_context *ctx)
{
    if (ctx->config.network_count == 0) {
        ESP_LOGW(TAG, "No networks configured");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Simple implementation - connect to first available network
    wifi_network_config_t *network = &ctx->config.networks[ctx->current_network_index];
    
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, network->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, network->password, sizeof(wifi_config.sta.password) - 1);
    
    ESP_LOGI(TAG, "Connecting to network: %s", network->ssid);
    
    // Publish connecting event for feedback coordination
    network_tool_event_t connecting_event = {
        .type = NETWORK_TOOL_EVENT_STA_CONNECTING
    };
    snprintf(connecting_event.data.sta_info.ssid, sizeof(connecting_event.data.sta_info.ssid), "%s", network->ssid);
    publish_wifi_event(ctx, NETWORK_TOOL_EVENT_STA_CONNECTING, &connecting_event);
    
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        return ret;
    }
    
    return esp_wifi_connect();
}

static esp_err_t start_sta_mode(struct network_tool_context *ctx)
{
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        return ret;
    }
    
    return esp_wifi_start();
}

static esp_err_t start_ap_mode(struct network_tool_context *ctx)
{
    wifi_config_t wifi_config = {0};
    
    snprintf((char*)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "%s", ctx->config.ap_config.ssid);
    snprintf((char*)wifi_config.ap.password, sizeof(wifi_config.ap.password), "%s", ctx->config.ap_config.password);
    wifi_config.ap.ssid_len = strlen(ctx->config.ap_config.ssid);
    wifi_config.ap.channel = ctx->config.ap_config.channel;
    wifi_config.ap.max_connection = ctx->config.ap_config.max_connections;
    wifi_config.ap.authmode = ctx->config.ap_config.auth_mode;
    
    if (strlen(ctx->config.ap_config.password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        return ret;
    }
    
    return esp_wifi_start();
}

// =============================================================================
// Missing Network Management Functions Implementation
// =============================================================================

esp_err_t network_tool_connect(network_tool_handle_t handle, const char* ssid, const char* password)
{
    if (!handle || !ssid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Connecting to network: %s", ssid);
    
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        return ret;
    }
    
    return esp_wifi_connect();
}

esp_err_t network_tool_disconnect(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Disconnecting from network");
    
    return esp_wifi_disconnect();
}

esp_err_t network_tool_add_network(network_tool_handle_t handle, const wifi_network_config_t *network)
{
    if (!handle || !network) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(ctx->config_mutex, portMAX_DELAY);
    
    if (ctx->config.network_count >= NETWORK_TOOL_MAX_NETWORKS) {
        ESP_LOGW(TAG, "Maximum number of networks reached (%d)", NETWORK_TOOL_MAX_NETWORKS);
        xSemaphoreGive(ctx->config_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Check if network already exists
    for (int i = 0; i < ctx->config.network_count; i++) {
        if (strcmp(ctx->config.networks[i].ssid, network->ssid) == 0) {
            ESP_LOGW(TAG, "Network '%s' already exists, updating", network->ssid);
            memcpy(&ctx->config.networks[i], network, sizeof(wifi_network_config_t));
            xSemaphoreGive(ctx->config_mutex);
            
            // Publish config changed event
            publish_wifi_event(ctx, NETWORK_TOOL_EVENT_CONFIG_CHANGED, NULL);
            return ESP_OK;
        }
    }
    
    // Add new network
    memcpy(&ctx->config.networks[ctx->config.network_count], network, sizeof(wifi_network_config_t));
    ctx->config.network_count++;
    
    ESP_LOGI(TAG, "Added network '%s' (%d/%d)", network->ssid, ctx->config.network_count, NETWORK_TOOL_MAX_NETWORKS);
    
    xSemaphoreGive(ctx->config_mutex);
    
    // Publish config changed event
    publish_wifi_event(ctx, NETWORK_TOOL_EVENT_CONFIG_CHANGED, NULL);
    
    return ESP_OK;
}

esp_err_t network_tool_remove_network(network_tool_handle_t handle, const char* ssid)
{
    if (!handle || !ssid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(ctx->config_mutex, portMAX_DELAY);
    
    // Find network by SSID
    int found_index = -1;
    for (int i = 0; i < ctx->config.network_count; i++) {
        if (strcmp(ctx->config.networks[i].ssid, ssid) == 0) {
            found_index = i;
            break;
        }
    }
    
    if (found_index == -1) {
        ESP_LOGW(TAG, "Network '%s' not found in configuration", ssid);
        xSemaphoreGive(ctx->config_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Removing network '%s' at index %d", ssid, found_index);
    
    // Shift remaining networks down
    for (int i = found_index; i < ctx->config.network_count - 1; i++) {
        memcpy(&ctx->config.networks[i], &ctx->config.networks[i + 1], sizeof(wifi_network_config_t));
    }
    
    // Clear the last entry
    memset(&ctx->config.networks[ctx->config.network_count - 1], 0, sizeof(wifi_network_config_t));
    ctx->config.network_count--;
    
    // Reset current network index if needed
    if (ctx->current_network_index >= ctx->config.network_count) {
        ctx->current_network_index = 0;
    }
    
    ESP_LOGI(TAG, "Network removed successfully. Remaining networks: %d", ctx->config.network_count);
    
    xSemaphoreGive(ctx->config_mutex);
    
    // Publish config changed event
    publish_wifi_event(ctx, NETWORK_TOOL_EVENT_CONFIG_CHANGED, NULL);
    
    return ESP_OK;
}

esp_err_t network_tool_save_config(network_tool_handle_t handle, const char* file_path)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized || !ctx->fs_tool) {
        return ESP_ERR_INVALID_STATE;
    }
    
    const char* config_file = file_path ? file_path : ctx->config.config_file_path;
    
    xSemaphoreTake(ctx->config_mutex, portMAX_DELAY);
    
    // Create JSON configuration
    cJSON *wifi_config = cJSON_CreateObject();
    if (!wifi_config) {
        xSemaphoreGive(ctx->config_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    cJSON *networks_array = cJSON_CreateArray();
    if (!networks_array) {
        cJSON_Delete(wifi_config);
        xSemaphoreGive(ctx->config_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Add all networks to JSON array
    for (int i = 0; i < ctx->config.network_count; i++) {
        cJSON *network_obj = cJSON_CreateObject();
        if (!network_obj) {
            cJSON_Delete(wifi_config);
            xSemaphoreGive(ctx->config_mutex);
            return ESP_ERR_NO_MEM;
        }
        
        cJSON_AddStringToObject(network_obj, "ssid", ctx->config.networks[i].ssid);
        cJSON_AddStringToObject(network_obj, "password", ctx->config.networks[i].password);
        
        cJSON_AddItemToArray(networks_array, network_obj);
    }
    
    cJSON_AddItemToObject(wifi_config, "networks", networks_array);
    
    // Save to filesystem
    esp_err_t ret = fs_tool_save_json_config(ctx->fs_tool, config_file, wifi_config);
    
    cJSON_Delete(wifi_config);
    xSemaphoreGive(ctx->config_mutex);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration saved successfully to %s", config_file);
    } else {
        ESP_LOGE(TAG, "Failed to save configuration: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t network_tool_load_config(network_tool_handle_t handle, const char* file_path)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized || !ctx->fs_tool) {
        return ESP_ERR_INVALID_STATE;
    }
    
    const char* config_file = file_path ? file_path : ctx->config.config_file_path;
    
    cJSON *wifi_config = NULL;
    esp_err_t ret = fs_tool_load_json_config(ctx->fs_tool, config_file, &wifi_config);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load configuration from %s: %s", config_file, esp_err_to_name(ret));
        return ret;
    }
    
    ret = load_networks_from_json(ctx, wifi_config);
    cJSON_Delete(wifi_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration reloaded from %s", config_file);
        // Publish config changed event
        publish_wifi_event(ctx, NETWORK_TOOL_EVENT_CONFIG_CHANGED, NULL);
    }
    
    return ret;
}

esp_err_t network_tool_update_config(network_tool_handle_t handle, const network_tool_config_t *config)
{
    if (!handle || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct network_tool_context *ctx = (struct network_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(ctx->config_mutex, portMAX_DELAY);
    
    // Update configuration
    memcpy(&ctx->config, config, sizeof(network_tool_config_t));
    
    xSemaphoreGive(ctx->config_mutex);
    
    ESP_LOGI(TAG, "Configuration updated");
    
    // Publish config changed event
    publish_wifi_event(ctx, NETWORK_TOOL_EVENT_CONFIG_CHANGED, NULL);
    
    return ESP_OK;
}

// =============================================================================
// Utility Functions Implementation
// =============================================================================

const char* network_tool_event_to_string(network_tool_event_type_t event_type)
{
    switch (event_type) {
        case NETWORK_TOOL_EVENT_STA_CONNECTING: return "STA_CONNECTING";
        case NETWORK_TOOL_EVENT_STA_CONNECTED: return "STA_CONNECTED";
        case NETWORK_TOOL_EVENT_STA_DISCONNECTED: return "STA_DISCONNECTED";
        case NETWORK_TOOL_EVENT_STA_FAILED: return "STA_FAILED";
        case NETWORK_TOOL_EVENT_AP_STARTED: return "AP_STARTED";
        case NETWORK_TOOL_EVENT_AP_STOPPED: return "AP_STOPPED";
        case NETWORK_TOOL_EVENT_AP_CLIENT_CONNECTED: return "AP_CLIENT_CONNECTED";
        case NETWORK_TOOL_EVENT_AP_CLIENT_DISCONNECTED: return "AP_CLIENT_DISCONNECTED";
        case NETWORK_TOOL_EVENT_CONFIG_CHANGED: return "CONFIG_CHANGED";
        case NETWORK_TOOL_EVENT_IP_ACQUIRED: return "IP_ACQUIRED";
        case NETWORK_TOOL_EVENT_IP_LOST: return "IP_LOST";
        default: return "UNKNOWN";
    }
}

const char* network_tool_auth_mode_to_string(wifi_auth_mode_t auth_mode)
{
    switch (auth_mode) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK: return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2_WPA3_PSK";
        default: return "UNKNOWN";
    }
}

const char* network_tool_error_to_string(wifi_err_reason_t reason)
{
    switch (reason) {
        case WIFI_REASON_UNSPECIFIED: return "UNSPECIFIED";
        case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE: return "AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE";
        case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY";
        case WIFI_REASON_NOT_AUTHED: return "NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED: return "NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE: return "ASSOC_LEAVE";
        case WIFI_REASON_ASSOC_NOT_AUTHED: return "ASSOC_NOT_AUTHED";
        case WIFI_REASON_DISASSOC_PWRCAP_BAD: return "DISASSOC_PWRCAP_BAD";
        case WIFI_REASON_DISASSOC_SUPCHAN_BAD: return "DISASSOC_SUPCHAN_BAD";
        case WIFI_REASON_BSS_TRANSITION_DISASSOC: return "BSS_TRANSITION_DISASSOC";
        case WIFI_REASON_IE_INVALID: return "IE_INVALID";
        case WIFI_REASON_MIC_FAILURE: return "MIC_FAILURE";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GROUP_KEY_UPDATE_TIMEOUT";
        case WIFI_REASON_IE_IN_4WAY_DIFFERS: return "IE_IN_4WAY_DIFFERS";
        case WIFI_REASON_GROUP_CIPHER_INVALID: return "GROUP_CIPHER_INVALID";
        case WIFI_REASON_PAIRWISE_CIPHER_INVALID: return "PAIRWISE_CIPHER_INVALID";
        case WIFI_REASON_AKMP_INVALID: return "AKMP_INVALID";
        case WIFI_REASON_UNSUPP_RSN_IE_VERSION: return "UNSUPP_RSN_IE_VERSION";
        case WIFI_REASON_INVALID_RSN_IE_CAP: return "INVALID_RSN_IE_CAP";
        case WIFI_REASON_802_1X_AUTH_FAILED: return "802_1X_AUTH_FAILED";
        case WIFI_REASON_CIPHER_SUITE_REJECTED: return "CIPHER_SUITE_REJECTED";
        case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
        case WIFI_REASON_CONNECTION_FAIL: return "CONNECTION_FAIL";
        case WIFI_REASON_AP_TSF_RESET: return "AP_TSF_RESET";
        case WIFI_REASON_ROAMING: return "ROAMING";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// Tool Registry Implementation (MCP Pattern)
// =============================================================================

const network_tool_registry_t* network_tool_get_registry_entry(void)
{
    static const network_tool_registry_t registry_entry = {
        .tool_id = NETWORK_TOOL_ID,
        .version = NETWORK_TOOL_VERSION,
        .description = NETWORK_TOOL_DESCRIPTION,
        .capabilities = NETWORK_CAP_STA_MODE | 
                       NETWORK_CAP_AP_MODE |
                       NETWORK_CAP_MULTI_NETWORK |
                       NETWORK_CAP_AUTO_RECONNECT |
                       NETWORK_CAP_CONFIG_PERSIST |
                       NETWORK_CAP_EVENT_PUBLISH |
                       NETWORK_CAP_HEALTH_MONITOR,
        .init_func = network_tool_init,
        .deinit_func = network_tool_deinit
    };
    
    return &registry_entry;
}