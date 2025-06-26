/**
 * @file system_monitor_tool.c
 * @brief MCP-Inspired Debug Tool Implementation
 * 
 * Centralized debug and monitoring extracted from feedback_tool with enhanced capabilities.
 * Implements ASCII dashboard generation and multi-tool status aggregation.
 */

#include "system_monitor_tool.h"
#include "event_system.h"   // Event-driven architecture
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

static const char *TAG = "system_monitor_tool";

// =============================================================================
// Tool Context Structure
// =============================================================================

#define MAX_REGISTERED_TOOLS 16

typedef struct registered_tool {
    void* tool_handle;
    char tool_id[16];
    char tool_version[16];
    esp_err_t (*get_status_func)(void* handle, void* status);
    size_t status_struct_size;
    void* last_status;
    bool is_active;
} registered_tool_t;

typedef struct system_monitor_tool {
    system_monitor_tool_config_t config;
    bool initialized;
    uint32_t dashboards_generated;
    uint32_t json_reports_generated;
    uint32_t tools_registered;
    system_monitor_system_health_t system_health;
    system_monitor_robot_expression_t current_robot_expr;
    uint32_t last_error_code;
    
    // Tool registry
    registered_tool_t registered_tools[MAX_REGISTERED_TOOLS];
    uint32_t active_tool_count;
    
    // Buffer visualization
    uint32_t buffer_used_slots;
    uint32_t buffer_total_slots;
    system_monitor_buffer_visual_mode_t buffer_visual_mode;
    
    // Retry counter visualization
    uint32_t retry_current;
    uint32_t retry_max;
    
    // System status
    bool is_offline_mode;
    uint64_t init_timestamp_us;
    
    // Session timing (Process Map Authority: Constitutional Requirement)
    session_timing_context_t session_context;
    void* feedback_tool_handle;  // For flow state delegation
} system_monitor_tool_t;

// =============================================================================
// Robot Expression Strings (Process Map Authority)
// =============================================================================

static const char* get_robot_expression_string(system_monitor_robot_expression_t expr)
{
    switch (expr) {
        case SYSTEM_MONITOR_ROBOT_HAPPY:     return "(◕‿◕)っ";
        case SYSTEM_MONITOR_ROBOT_FOCUSED:   return "(•ᴗ•)っ";
        case SYSTEM_MONITOR_ROBOT_CONCERNED: return "(ಠ_ಠ)っ";
        case SYSTEM_MONITOR_ROBOT_ANGRY:     return "(╯°□°)╯";
        case SYSTEM_MONITOR_ROBOT_NEUTRAL:   return "(•_•)っ";
        default:                    return "(•_•)っ";
    }
}

static const char* get_health_status_string(system_monitor_system_health_t health)
{
    switch (health) {
        case SYSTEM_MONITOR_HEALTH_EXCELLENT:  return "EXCELLENT";
        case SYSTEM_MONITOR_HEALTH_GOOD:       return "GOOD";
        case SYSTEM_MONITOR_HEALTH_WARNING:    return "WARNING";
        case SYSTEM_MONITOR_HEALTH_CRITICAL:   return "CRITICAL";
        case SYSTEM_MONITOR_HEALTH_UNKNOWN:    return "UNKNOWN";
        default:                      return "UNKNOWN";
    }
}

// =============================================================================
// MCP Tool Interface Implementation
// =============================================================================

system_monitor_tool_config_t system_monitor_tool_create_default_config(void)
{
    system_monitor_tool_config_t config = {
        .dashboard_buffer_size = 512,
        .json_buffer_size = 256,
        .update_interval_ms = 1000,
        .enable_robot_expressions = true,
        .enable_buffer_visualization = true,
        .enable_real_time_updates = false
    };
    
    return config;
}

system_monitor_tool_handle_t system_monitor_tool_init(const system_monitor_tool_config_t* config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }
    
    system_monitor_tool_t* tool = calloc(1, sizeof(system_monitor_tool_t));
    if (!tool) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return NULL;
    }
    
    // Copy configuration
    memcpy(&tool->config, config, sizeof(system_monitor_tool_config_t));
    
    // Initialize state
    tool->initialized = true;
    tool->system_health = SYSTEM_MONITOR_HEALTH_UNKNOWN;
    tool->current_robot_expr = SYSTEM_MONITOR_ROBOT_NEUTRAL;
    tool->buffer_visual_mode = SYSTEM_MONITOR_BUFFER_DOTS;
    tool->init_timestamp_us = esp_timer_get_time();
    
    // Initialize tool registry
    for (int i = 0; i < MAX_REGISTERED_TOOLS; i++) {
        tool->registered_tools[i].is_active = false;
        tool->registered_tools[i].last_status = NULL;
    }
    
    // Initialize session timing context (Process Map Authority)
    memset(&tool->session_context, 0, sizeof(session_timing_context_t));
    tool->session_context.session_active = false;
    tool->feedback_tool_handle = NULL;
    
    ESP_LOGI(TAG, "Debug tool initialized - Buffer sizes: ASCII=%lu, JSON=%lu",
             tool->config.dashboard_buffer_size, tool->config.json_buffer_size);
    
    return tool;
}

esp_err_t system_monitor_tool_cleanup(system_monitor_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    // Free registered tool status buffers
    for (int i = 0; i < MAX_REGISTERED_TOOLS; i++) {
        if (tool->registered_tools[i].last_status) {
            free(tool->registered_tools[i].last_status);
        }
    }
    
    free(tool);
    
    ESP_LOGI(TAG, "Debug tool cleaned up");
    
    return ESP_OK;
}

const char* system_monitor_tool_get_id(void)
{
    return SYSTEM_MONITOR_TOOL_ID;
}

const char* system_monitor_tool_get_version(void)
{
    return SYSTEM_MONITOR_TOOL_VERSION;
}

system_monitor_tool_capabilities_t system_monitor_tool_get_capabilities(system_monitor_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    system_monitor_tool_capabilities_t caps = SYSTEM_MONITOR_CAP_ASCII_DASHBOARD | 
                                   SYSTEM_MONITOR_CAP_JSON_STATUS |
                                   SYSTEM_MONITOR_CAP_TOOL_MONITORING |
                                   SYSTEM_MONITOR_CAP_THREAD_SAFE;
    
    if (tool->config.enable_robot_expressions) {
        caps |= SYSTEM_MONITOR_CAP_ROBOT_EXPRESSIONS;
    }
    
    if (tool->config.enable_buffer_visualization) {
        caps |= SYSTEM_MONITOR_CAP_BUFFER_VISUAL;
    }
    
    if (tool->config.enable_real_time_updates) {
        caps |= SYSTEM_MONITOR_CAP_REAL_TIME_UPDATE;
    }
    
    return caps;
}

esp_err_t system_monitor_tool_get_status(system_monitor_tool_handle_t handle, system_monitor_tool_status_t* status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    status->is_initialized = tool->initialized;
    status->dashboards_generated = tool->dashboards_generated;
    status->json_reports_generated = tool->json_reports_generated;
    status->tools_registered = tool->tools_registered;
    status->system_health = tool->system_health;
    status->last_error_code = tool->last_error_code;
    
    return ESP_OK;
}

// =============================================================================
// Tool Registration Functions
// =============================================================================

esp_err_t system_monitor_tool_register_tool(system_monitor_tool_handle_t handle,
                                 const system_monitor_tool_registration_t* registration)
{
    if (!handle || !registration) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    // Find available slot
    int slot = -1;
    for (int i = 0; i < MAX_REGISTERED_TOOLS; i++) {
        if (!tool->registered_tools[i].is_active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        ESP_LOGE(TAG, "No available slots for tool registration");
        return ESP_ERR_NO_MEM;
    }
    
    registered_tool_t* reg_tool = &tool->registered_tools[slot];
    
    // Set up registration
    reg_tool->tool_handle = registration->tool_handle;
    strncpy(reg_tool->tool_id, registration->tool_id, sizeof(reg_tool->tool_id) - 1);
    strncpy(reg_tool->tool_version, registration->tool_version, sizeof(reg_tool->tool_version) - 1);
    reg_tool->get_status_func = registration->get_status_func;
    reg_tool->status_struct_size = registration->status_struct_size;
    reg_tool->is_active = true;
    
    // Allocate status buffer
    reg_tool->last_status = malloc(registration->status_struct_size);
    if (!reg_tool->last_status) {
        reg_tool->is_active = false;
        return ESP_ERR_NO_MEM;
    }
    
    tool->tools_registered++;
    tool->active_tool_count++;
    
    ESP_LOGI(TAG, "Registered tool: %s v%s (slot %d)", 
             reg_tool->tool_id, reg_tool->tool_version, slot);
    
    return ESP_OK;
}

esp_err_t system_monitor_tool_unregister_tool(system_monitor_tool_handle_t handle, const char* tool_id)
{
    if (!handle || !tool_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    for (int i = 0; i < MAX_REGISTERED_TOOLS; i++) {
        registered_tool_t* reg_tool = &tool->registered_tools[i];
        if (reg_tool->is_active && strcmp(reg_tool->tool_id, tool_id) == 0) {
            // Free status buffer
            if (reg_tool->last_status) {
                free(reg_tool->last_status);
                reg_tool->last_status = NULL;
            }
            
            reg_tool->is_active = false;
            tool->active_tool_count--;
            
            ESP_LOGI(TAG, "Unregistered tool: %s", tool_id);
            return ESP_OK;
        }
    }
    
    ESP_LOGW(TAG, "Tool not found for unregistration: %s", tool_id);
    return ESP_ERR_NOT_FOUND;
}

// =============================================================================
// ASCII Dashboard Generation (Extracted from feedback_tool)
// =============================================================================

static esp_err_t generate_ascii_dashboard(system_monitor_tool_t* tool, char* buffer, size_t buffer_size)
{
    if (!tool || !buffer || buffer_size < 256) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calculate uptime
    uint64_t uptime_us = esp_timer_get_time() - tool->init_timestamp_us;
    uint32_t uptime_minutes = (uint32_t)(uptime_us / (1000 * 1000 * 60));
    
    // Get robot expression
    const char* robot_expr = get_robot_expression_string(tool->current_robot_expr);
    const char* health_str = get_health_status_string(tool->system_health);
    
    // Generate buffer visualization
    char buffer_visual[32] = {0};
    if (tool->config.enable_buffer_visualization) {
        switch (tool->buffer_visual_mode) {
            case SYSTEM_MONITOR_BUFFER_DOTS: {
                uint32_t dots_used = (tool->buffer_used_slots * 10) / (tool->buffer_total_slots > 0 ? tool->buffer_total_slots : 1);
                for (uint32_t i = 0; i < 10; i++) {
                    strcat(buffer_visual, (i < dots_used) ? "●" : "○");
                }
                break;
            }
            case SYSTEM_MONITOR_BUFFER_BARS: {
                uint32_t bars_used = (tool->buffer_used_slots * 10) / (tool->buffer_total_slots > 0 ? tool->buffer_total_slots : 1);
                strcpy(buffer_visual, "[");
                for (uint32_t i = 0; i < 10; i++) {
                    strcat(buffer_visual, (i < bars_used) ? "█" : "░");
                }
                strcat(buffer_visual, "]");
                break;
            }
            case SYSTEM_MONITOR_BUFFER_NUMERIC:
                snprintf(buffer_visual, sizeof(buffer_visual), "%lu/%lu",
                        tool->buffer_used_slots, tool->buffer_total_slots);
                break;
        }
    }
    
    // Generate retry visualization
    char retry_visual[16] = {0};
    if (tool->retry_max > 0) {
        snprintf(retry_visual, sizeof(retry_visual), "%lu/%lu",
                tool->retry_current, tool->retry_max);
    }
    
    // Collect operational data from tools
    char fs_status[64] = "Unknown";
    char network_status[64] = "Unknown";
    char http_status[96] = "Unknown";
    char rfid_status[64] = "Unknown";
    
    // Query actual tool statuses for operational data
    for (int i = 0; i < MAX_REGISTERED_TOOLS; i++) {
        registered_tool_t* reg_tool = &tool->registered_tools[i];
        if (!reg_tool->is_active) continue;
        
        if (strcmp(reg_tool->tool_id, "fs") == 0) {
            // Get FS tool status for storage info
            if (reg_tool->get_status_func && reg_tool->last_status) {
                reg_tool->get_status_func(reg_tool->tool_handle, reg_tool->last_status);
                // Fixed struct layout to match fs_tool.h exactly (ARCHITECTURAL FIX)
                typedef struct {
                    bool is_initialized;                     ///< Tool initialization status
                    bool is_active;                          ///< Tool active status
                    bool is_mounted;                         ///< Filesystem mount status
                    char mount_point[32];                    ///< Current mount point (FS_TOOL_MAX_MOUNT_LEN)
                    char partition_label[16];                ///< Partition label (FS_TOOL_MAX_LABEL_LEN)
                    uint32_t total_bytes;                    ///< Total filesystem size
                    uint32_t used_bytes;                     ///< Used filesystem space
                    uint32_t available_bytes;                ///< Available filesystem space
                    uint8_t usage_percent;                   ///< Usage percentage
                    uint32_t uptime_ms;                      ///< Tool uptime
                    uint32_t file_operations_count;          ///< Total file operations
                    uint32_t error_count;                    ///< Error count
                    uint32_t capabilities;                   ///< Tool capabilities (fs_tool_capabilities_t)
                } fs_status_corrected_t;
                fs_status_corrected_t* fs = (fs_status_corrected_t*)reg_tool->last_status;
                if (fs->is_mounted) {
                    snprintf(fs_status, sizeof(fs_status), "%u%% (%luK free)", 
                             fs->usage_percent, fs->available_bytes / 1024);
                } else {
                    snprintf(fs_status, sizeof(fs_status), "Not mounted");
                }
            }
        } else if (strcmp(reg_tool->tool_id, "network") == 0) {
            // Get network status for connectivity info
            if (reg_tool->get_status_func && reg_tool->last_status) {
                reg_tool->get_status_func(reg_tool->tool_handle, reg_tool->last_status);
                // Cast to network_tool_status_t and extract real data
                typedef struct {
                    bool is_initialized, is_active, sta_connected, ap_active;
                    char current_ssid[32], ip_address[16];
                    int8_t rssi;
                    uint8_t retry_count;
                    uint32_t uptime_ms;
                    uint8_t ap_client_count;
                    uint32_t capabilities;
                } network_status_t;
                network_status_t* net = (network_status_t*)reg_tool->last_status;
                if (net->sta_connected) {
                    snprintf(network_status, sizeof(network_status), "%s (%ddBm)", 
                             net->current_ssid, net->rssi);
                } else if (net->ap_active) {
                    snprintf(network_status, sizeof(network_status), "AP mode (%u clients)", 
                             net->ap_client_count);
                } else {
                    snprintf(network_status, sizeof(network_status), "Disconnected");
                }
            }
        } else if (strcmp(reg_tool->tool_id, "http") == 0) {
            // Get HTTP tool status for webhook delivery
            if (reg_tool->get_status_func && reg_tool->last_status) {
                reg_tool->get_status_func(reg_tool->tool_handle, reg_tool->last_status);
                // Fixed struct layout to match http_tool.h exactly (ARCHITECTURAL FIX)
                typedef struct {
                    bool is_initialized;                          ///< Tool initialization status
                    bool is_active;                               ///< Tool active status
                    bool wifi_connected;                          ///< WiFi connectivity status
                    bool webhook_reachable;                       ///< Webhook server reachable
                    char current_url[256];                        ///< Current webhook URL (HTTP_TOOL_MAX_URL_LEN)
                    uint32_t pending_count;                       ///< Pending events count
                    uint32_t success_count;                       ///< Successful transmissions
                    uint32_t failed_count;                        ///< Failed transmissions
                    uint32_t uptime_ms;                           ///< Tool uptime
                    uint32_t last_transmission_ms;                ///< Last successful transmission
                    uint32_t capabilities;                        ///< Tool capabilities (http_tool_capabilities_t)
                } http_status_corrected_t;
                http_status_corrected_t* http = (http_status_corrected_t*)reg_tool->last_status;
                if (http->pending_count > 0) {
                    snprintf(http_status, sizeof(http_status), "%lu pending | %lu sent", 
                             http->pending_count, http->success_count);
                } else {
                    snprintf(http_status, sizeof(http_status), "Ready | %lu sent", 
                             http->success_count);
                }
            }
        } else if (strcmp(reg_tool->tool_id, "rfid") == 0) {
            // Get RFID status for tag detection state
            if (reg_tool->get_status_func && reg_tool->last_status) {
                reg_tool->get_status_func(reg_tool->tool_handle, reg_tool->last_status);
                // Simplified RFID status (would need rfid_tool_status_t structure)
                snprintf(rfid_status, sizeof(rfid_status), "Active");
            }
        }
    }
    
    // Create enhanced operational dashboard
    int written = snprintf(buffer, buffer_size,
        "╔══════════════════════════════╗\n"
        "║     RFID TIME TRACKER        ║\n"
        "╚══════════════════════════════╝\n"
        " %s Up: %lu min | %s mode\n"
        " 📂 Storage: %s | Buffer: %s\n"
        " 🌐 Network: %s\n"
        " 📡 Webhook: %s\n"
        " 🏷️  RFID: %s | Events: %s\n"
        " 🎯 Health: %s\n",
        robot_expr,
        uptime_minutes,
        tool->is_offline_mode ? "OFFLINE" : "ONLINE",
        fs_status,
        buffer_visual,
        network_status,
        http_status,
        rfid_status,
        strlen(retry_visual) > 0 ? retry_visual : "None",
        health_str
    );
    
    if (written < 0 || written >= (int)buffer_size) {
        ESP_LOGE(TAG, "Dashboard buffer overflow");
        return ESP_ERR_INVALID_SIZE;
    }
    
    return ESP_OK;
}

esp_err_t system_monitor_tool_generate_dashboard(system_monitor_tool_handle_t handle,
                                      system_monitor_dashboard_result_t* result)
{
    if (!handle || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    // Initialize result
    memset(result, 0, sizeof(system_monitor_dashboard_result_t));
    
    // Allocate dashboard buffer
    result->ascii_dashboard = malloc(tool->config.dashboard_buffer_size);
    if (!result->ascii_dashboard) {
        ESP_LOGE(TAG, "Failed to allocate dashboard buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // Generate dashboard
    esp_err_t ret = generate_ascii_dashboard(tool, result->ascii_dashboard, 
                                           tool->config.dashboard_buffer_size);
    if (ret != ESP_OK) {
        free(result->ascii_dashboard);
        result->ascii_dashboard = NULL;
        return ret;
    }
    
    result->dashboard_length = strlen(result->ascii_dashboard);
    result->robot_expr = tool->current_robot_expr;
    result->system_health = tool->system_health;
    result->update_count = tool->dashboards_generated + 1;
    result->generation_success = true;
    
    tool->dashboards_generated++;
    
    ESP_LOGD(TAG, "Generated ASCII dashboard (%zu bytes)", result->dashboard_length);
    
    return ESP_OK;
}

esp_err_t system_monitor_tool_update_dashboard(system_monitor_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    // Update system health based on registered tools
    system_monitor_tool_calculate_system_health(handle, &tool->system_health);
    
    // Update robot expression based on health
    switch (tool->system_health) {
        case SYSTEM_MONITOR_HEALTH_EXCELLENT:
            tool->current_robot_expr = SYSTEM_MONITOR_ROBOT_HAPPY;
            break;
        case SYSTEM_MONITOR_HEALTH_GOOD:
            tool->current_robot_expr = SYSTEM_MONITOR_ROBOT_FOCUSED;
            break;
        case SYSTEM_MONITOR_HEALTH_WARNING:
            tool->current_robot_expr = SYSTEM_MONITOR_ROBOT_CONCERNED;
            break;
        case SYSTEM_MONITOR_HEALTH_CRITICAL:
            tool->current_robot_expr = SYSTEM_MONITOR_ROBOT_ANGRY;
            break;
        default:
            tool->current_robot_expr = SYSTEM_MONITOR_ROBOT_NEUTRAL;
            break;
    }
    
    return ESP_OK;
}

void system_monitor_tool_free_dashboard_result(system_monitor_dashboard_result_t* result)
{
    if (result && result->ascii_dashboard) {
        free(result->ascii_dashboard);
        result->ascii_dashboard = NULL;
        result->dashboard_length = 0;
        result->generation_success = false;
    }
}

// =============================================================================
// System Health and Expression Functions
// =============================================================================

esp_err_t system_monitor_tool_set_robot_expression(system_monitor_tool_handle_t handle,
                                        system_monitor_robot_expression_t expression)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    tool->current_robot_expr = expression;
    
    ESP_LOGD(TAG, "Robot expression set to: %s", get_robot_expression_string(expression));
    
    return ESP_OK;
}

esp_err_t system_monitor_tool_update_buffer_visualization(system_monitor_tool_handle_t handle,
                                               uint32_t used_slots,
                                               uint32_t total_slots,
                                               system_monitor_buffer_visual_mode_t mode)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    tool->buffer_used_slots = used_slots;
    tool->buffer_total_slots = total_slots;
    tool->buffer_visual_mode = mode;
    
    return ESP_OK;
}

esp_err_t system_monitor_tool_increment_retry_counter(system_monitor_tool_handle_t handle,
                                           uint32_t current_retry,
                                           uint32_t max_retries)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    tool->retry_current = current_retry;
    tool->retry_max = max_retries;
    
    return ESP_OK;
}

esp_err_t system_monitor_tool_set_offline_mode(system_monitor_tool_handle_t handle, bool is_offline)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    tool->is_offline_mode = is_offline;
    
    ESP_LOGD(TAG, "Offline mode set to: %s", is_offline ? "true" : "false");
    
    return ESP_OK;
}

esp_err_t system_monitor_tool_calculate_system_health(system_monitor_tool_handle_t handle,
                                            system_monitor_system_health_t* health)
{
    if (!handle || !health) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    // Simple health calculation based on registered tools
    // In a real implementation, this would query each tool's status
    
    if (tool->active_tool_count == 0) {
        *health = SYSTEM_MONITOR_HEALTH_UNKNOWN;
    } else if (tool->is_offline_mode) {
        *health = SYSTEM_MONITOR_HEALTH_WARNING;
    } else if (tool->retry_current > 0) {
        *health = (tool->retry_current > tool->retry_max / 2) ? 
                  SYSTEM_MONITOR_HEALTH_CRITICAL : SYSTEM_MONITOR_HEALTH_WARNING;
    } else {
        *health = SYSTEM_MONITOR_HEALTH_EXCELLENT;
    }
    
    tool->system_health = *health;
    
    return ESP_OK;
}

// =============================================================================
// JSON Status Generation (Extracted from feedback_tool)
// =============================================================================

esp_err_t system_monitor_tool_generate_json_status(system_monitor_tool_handle_t handle,
                                        system_monitor_json_status_result_t* result)
{
    if (!handle || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    // Initialize result
    memset(result, 0, sizeof(system_monitor_json_status_result_t));
    
    // Allocate JSON buffer
    result->json_status = malloc(tool->config.json_buffer_size);
    if (!result->json_status) {
        ESP_LOGE(TAG, "Failed to allocate JSON buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // Calculate uptime
    uint64_t uptime_us = esp_timer_get_time() - tool->init_timestamp_us;
    uint64_t uptime_ms = uptime_us / 1000;
    
    // Generate JSON status
    int written = snprintf(result->json_status, tool->config.json_buffer_size,
        "{"
        "\"system\":{"
        "\"operational\":true,"
        "\"initialized\":%s,"
        "\"uptime_ms\":%llu,"
        "\"health\":\"%s\","
        "\"offline_mode\":%s"
        "},"
        "\"tools\":{"
        "\"registered\":%lu,"
        "\"active\":%lu"
        "},"
        "\"buffer\":{"
        "\"used\":%lu,"
        "\"total\":%lu"
        "},"
        "\"robot_expression\":\"%s\","
        "\"timestamp\":%llu"
        "}",
        tool->initialized ? "true" : "false",
        uptime_ms,
        get_health_status_string(tool->system_health),
        tool->is_offline_mode ? "true" : "false",
        tool->tools_registered,
        tool->active_tool_count,
        tool->buffer_used_slots,
        tool->buffer_total_slots,
        get_robot_expression_string(tool->current_robot_expr),
        esp_timer_get_time() / 1000
    );
    
    if (written < 0 || written >= (int)tool->config.json_buffer_size) {
        ESP_LOGE(TAG, "JSON buffer overflow");
        free(result->json_status);
        result->json_status = NULL;
        return ESP_ERR_INVALID_SIZE;
    }
    
    result->json_length = strlen(result->json_status);
    result->tools_monitored = tool->active_tool_count;
    result->generation_success = true;
    
    tool->json_reports_generated++;
    
    ESP_LOGD(TAG, "Generated JSON status (%zu bytes)", result->json_length);
    
    return ESP_OK;
}

void system_monitor_tool_free_json_result(system_monitor_json_status_result_t* result)
{
    if (result && result->json_status) {
        free(result->json_status);
        result->json_status = NULL;
        result->json_length = 0;
        result->generation_success = false;
    }
}

// =============================================================================
// Event-Driven Session Timing (Process Map Authority: Constitutional Requirement)
// =============================================================================

/**
 * @brief Event handler for RFID events (session timing)
 */
static void system_monitor_rfid_event_handler(void* handler_args, esp_event_base_t base,
                                                   int32_t id, void* event_data)
{
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handler_args;
    
    if (base == RFID_EVENTS) {
        rfid_event_data_t* rfid_data = (rfid_event_data_t*)event_data;
        session_timing_context_t* ctx = &tool->session_context;
        
        switch (id) {
            case RFID_EVENT_TAG_DETECTED:
                ESP_LOGI(TAG, "🏷️ RFID event received: tag detected - %s", rfid_data->tag_uid);
                
                if (!ctx->session_active) {
                    // Publish session start event with flow context clearing
                    session_event_data_t session_data = {
                        .session_start_time_ms = esp_timer_get_time() / 1000,
                        .session_duration_ms = 0,
                        .session_duration_min = 0,
                        .flow_awareness_active = false,  // Clear flow context
                        .flow_urgency_active = false
                    };
                    snprintf(session_data.tag_uid, sizeof(session_data.tag_uid), "%s", rfid_data->tag_uid);
                    
                    // Update internal session state
                    ctx->session_start_time_ms = session_data.session_start_time_ms;
                    ctx->session_active = true;
                    ctx->session_duration_min = 0;
                    ctx->flow_awareness_triggered = false;
                    ctx->flow_urgency_triggered = false;
                    snprintf(ctx->current_tag_uid, sizeof(ctx->current_tag_uid), "%s", rfid_data->tag_uid);
                    
                    // Publish session started event - feedback_tool will subscribe and clear flow context
                    esp_err_t publish_ret = publish_session_event(SESSION_EVENT_STARTED, &session_data);
                    if (publish_ret == ESP_OK) {
                        ESP_LOGI(TAG, "✅ Session started event published: tag=%s", rfid_data->tag_uid);
                    } else {
                        ESP_LOGW(TAG, "❌ Failed to publish session started event");
                    }
                }
                break;
                
            case RFID_EVENT_TAG_REMOVED:
                ESP_LOGI(TAG, "🏷️ RFID event received: tag removed - %s", rfid_data->tag_uid);
                
                if (ctx->session_active && strcmp(ctx->current_tag_uid, rfid_data->tag_uid) == 0) {
                    // Calculate session duration
                    uint64_t current_time_ms = esp_timer_get_time() / 1000;
                    uint64_t duration_ms = current_time_ms - ctx->session_start_time_ms;
                    uint32_t duration_min = duration_ms / (60 * 1000);
                    
                    // Publish session end event
                    session_event_data_t session_data = {
                        .session_start_time_ms = ctx->session_start_time_ms,
                        .session_duration_ms = duration_ms,
                        .session_duration_min = duration_min,
                        .flow_awareness_active = false,  // Clear flow context
                        .flow_urgency_active = false
                    };
                    snprintf(session_data.tag_uid, sizeof(session_data.tag_uid), "%s", rfid_data->tag_uid);
                    
                    // Clear internal session state
                    ctx->session_active = false;
                    ctx->session_duration_min = 0;
                    ctx->flow_awareness_triggered = false;
                    ctx->flow_urgency_triggered = false;
                    memset(ctx->current_tag_uid, 0, sizeof(ctx->current_tag_uid));
                    
                    // Publish session ended event - feedback_tool will subscribe and clear flow context
                    esp_err_t publish_ret = publish_session_event(SESSION_EVENT_ENDED, &session_data);
                    if (publish_ret == ESP_OK) {
                        ESP_LOGI(TAG, "✅ Session ended event published: tag=%s, duration=%llu ms", 
                                 rfid_data->tag_uid, (unsigned long long)duration_ms);
                    } else {
                        ESP_LOGW(TAG, "❌ Failed to publish session ended event");
                    }
                }
                break;
                
            default:
                break;
        }
    }
}

esp_err_t system_monitor_tool_start_event_subscription(system_monitor_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    
    // Subscribe to RFID events for session timing
    esp_err_t ret = subscribe_to_rfid_events(system_monitor_rfid_event_handler, tool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to subscribe to RFID events: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ System monitor subscribed to RFID events for session timing");
    return ESP_OK;
}

esp_err_t system_monitor_tool_stop_event_subscription(system_monitor_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Unregister event handlers
    esp_err_t ret = esp_event_handler_unregister(RFID_EVENTS, ESP_EVENT_ANY_ID, 
                                                  system_monitor_rfid_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unregister RFID event handler: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "✅ System monitor event subscription stopped");
    return ESP_OK;
}

esp_err_t system_monitor_tool_update_session_timing(system_monitor_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    session_timing_context_t* ctx = &tool->session_context;
    
    if (!ctx->session_active) {
        return ESP_OK; // No active session
    }
    
    // Calculate current session duration
    uint64_t current_time_ms = esp_timer_get_time() / 1000;
    uint32_t duration_min = (current_time_ms - ctx->session_start_time_ms) / (60 * 1000);
    ctx->session_duration_min = duration_min;
    
    // Check for flow awareness triggers (Process Map Authority)
    // Debug mode: Use 60 seconds instead of 60 minutes for testing
    #ifdef DEBUG_FLOW_TESTING
    uint32_t flow_awareness_threshold = 1;  // 1 minute for testing
    uint32_t flow_urgency_threshold = 2;    // 2 minutes for testing
    #else
    uint32_t flow_awareness_threshold = 60; // 60 minutes production
    uint32_t flow_urgency_threshold = 90;   // 90 minutes production
    #endif
    
    if (duration_min >= flow_awareness_threshold && !ctx->flow_awareness_triggered) {
        // Flow awareness: orange breathing - PUBLISH EVENT instead of direct call
        ESP_LOGI(TAG, "🟠 Flow Awareness: %lu-minute session detected, publishing event", flow_awareness_threshold);
        
        session_event_data_t session_data = {
            .session_start_time_ms = ctx->session_start_time_ms,
            .session_duration_ms = current_time_ms - ctx->session_start_time_ms,
            .session_duration_min = duration_min,
            .flow_awareness_active = true,
            .flow_urgency_active = false
        };
        snprintf(session_data.tag_uid, sizeof(session_data.tag_uid), "%s", ctx->current_tag_uid);
        
        esp_err_t publish_ret = publish_session_event(SESSION_EVENT_FLOW_AWARENESS, &session_data);
        if (publish_ret == ESP_OK) {
            ctx->flow_awareness_triggered = true;
            ESP_LOGI(TAG, "✅ Flow awareness event published (orange breathing)");
        } else {
            ESP_LOGW(TAG, "❌ Failed to publish flow awareness event: %s", esp_err_to_name(publish_ret));
        }
    }
    
    if (duration_min >= flow_urgency_threshold && !ctx->flow_urgency_triggered) {
        // Flow urgency: orange pulsing - PUBLISH EVENT instead of direct call
        ESP_LOGI(TAG, "🟠 Flow Urgency: %lu-minute session detected, publishing event", flow_urgency_threshold);
        
        session_event_data_t session_data = {
            .session_start_time_ms = ctx->session_start_time_ms,
            .session_duration_ms = current_time_ms - ctx->session_start_time_ms,
            .session_duration_min = duration_min,
            .flow_awareness_active = true,
            .flow_urgency_active = true
        };
        snprintf(session_data.tag_uid, sizeof(session_data.tag_uid), "%s", ctx->current_tag_uid);
        
        esp_err_t publish_ret = publish_session_event(SESSION_EVENT_FLOW_URGENCY, &session_data);
        if (publish_ret == ESP_OK) {
            ctx->flow_urgency_triggered = true;
            ESP_LOGI(TAG, "✅ Flow urgency event published (orange pulsing)");
        } else {
            ESP_LOGW(TAG, "❌ Failed to publish flow urgency event: %s", esp_err_to_name(publish_ret));
        }
    }
    
    return ESP_OK;
}

esp_err_t system_monitor_tool_get_session_context(system_monitor_tool_handle_t handle,
                                                  session_timing_context_t* context)
{
    if (!handle || !context) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_monitor_tool_t* tool = (system_monitor_tool_t*)handle;
    memcpy(context, &tool->session_context, sizeof(session_timing_context_t));
    
    return ESP_OK;
}