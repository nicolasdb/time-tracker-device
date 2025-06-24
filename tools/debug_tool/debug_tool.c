/**
 * @file debug_tool.c
 * @brief MCP-Inspired Debug Tool Implementation
 * 
 * Centralized debug and monitoring extracted from feedback_tool with enhanced capabilities.
 * Implements ASCII dashboard generation and multi-tool status aggregation.
 */

#include "debug_tool.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "debug_tool";

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

typedef struct debug_tool {
    debug_tool_config_t config;
    bool initialized;
    uint32_t dashboards_generated;
    uint32_t json_reports_generated;
    uint32_t tools_registered;
    debug_system_health_t system_health;
    debug_robot_expression_t current_robot_expr;
    uint32_t last_error_code;
    
    // Tool registry
    registered_tool_t registered_tools[MAX_REGISTERED_TOOLS];
    uint32_t active_tool_count;
    
    // Buffer visualization
    uint32_t buffer_used_slots;
    uint32_t buffer_total_slots;
    debug_buffer_visual_mode_t buffer_visual_mode;
    
    // Retry counter visualization
    uint32_t retry_current;
    uint32_t retry_max;
    
    // System status
    bool is_offline_mode;
    uint64_t init_timestamp_us;
} debug_tool_t;

// =============================================================================
// Robot Expression Strings (Process Map Authority)
// =============================================================================

static const char* get_robot_expression_string(debug_robot_expression_t expr)
{
    switch (expr) {
        case DEBUG_ROBOT_HAPPY:     return "(◕‿◕)っ";
        case DEBUG_ROBOT_FOCUSED:   return "(•ᴗ•)っ";
        case DEBUG_ROBOT_CONCERNED: return "(ಠ_ಠ)っ";
        case DEBUG_ROBOT_ANGRY:     return "(╯°□°)╯";
        case DEBUG_ROBOT_NEUTRAL:   return "(•_•)っ";
        default:                    return "(•_•)っ";
    }
}

static const char* get_health_status_string(debug_system_health_t health)
{
    switch (health) {
        case DEBUG_HEALTH_EXCELLENT:  return "EXCELLENT";
        case DEBUG_HEALTH_GOOD:       return "GOOD";
        case DEBUG_HEALTH_WARNING:    return "WARNING";
        case DEBUG_HEALTH_CRITICAL:   return "CRITICAL";
        case DEBUG_HEALTH_UNKNOWN:    return "UNKNOWN";
        default:                      return "UNKNOWN";
    }
}

// =============================================================================
// MCP Tool Interface Implementation
// =============================================================================

debug_tool_config_t debug_tool_create_default_config(void)
{
    debug_tool_config_t config = {
        .dashboard_buffer_size = 512,
        .json_buffer_size = 256,
        .update_interval_ms = 1000,
        .enable_robot_expressions = true,
        .enable_buffer_visualization = true,
        .enable_real_time_updates = false
    };
    
    return config;
}

debug_tool_handle_t debug_tool_init(const debug_tool_config_t* config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }
    
    debug_tool_t* tool = calloc(1, sizeof(debug_tool_t));
    if (!tool) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return NULL;
    }
    
    // Copy configuration
    memcpy(&tool->config, config, sizeof(debug_tool_config_t));
    
    // Initialize state
    tool->initialized = true;
    tool->system_health = DEBUG_HEALTH_UNKNOWN;
    tool->current_robot_expr = DEBUG_ROBOT_NEUTRAL;
    tool->buffer_visual_mode = DEBUG_BUFFER_DOTS;
    tool->init_timestamp_us = esp_timer_get_time();
    
    // Initialize tool registry
    for (int i = 0; i < MAX_REGISTERED_TOOLS; i++) {
        tool->registered_tools[i].is_active = false;
        tool->registered_tools[i].last_status = NULL;
    }
    
    ESP_LOGI(TAG, "Debug tool initialized - Buffer sizes: ASCII=%lu, JSON=%lu",
             tool->config.dashboard_buffer_size, tool->config.json_buffer_size);
    
    return tool;
}

esp_err_t debug_tool_cleanup(debug_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
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

const char* debug_tool_get_id(void)
{
    return DEBUG_TOOL_ID;
}

const char* debug_tool_get_version(void)
{
    return DEBUG_TOOL_VERSION;
}

debug_tool_capabilities_t debug_tool_get_capabilities(debug_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
    debug_tool_capabilities_t caps = DEBUG_CAP_ASCII_DASHBOARD | 
                                   DEBUG_CAP_JSON_STATUS |
                                   DEBUG_CAP_TOOL_MONITORING |
                                   DEBUG_CAP_THREAD_SAFE;
    
    if (tool->config.enable_robot_expressions) {
        caps |= DEBUG_CAP_ROBOT_EXPRESSIONS;
    }
    
    if (tool->config.enable_buffer_visualization) {
        caps |= DEBUG_CAP_BUFFER_VISUAL;
    }
    
    if (tool->config.enable_real_time_updates) {
        caps |= DEBUG_CAP_REAL_TIME_UPDATE;
    }
    
    return caps;
}

esp_err_t debug_tool_get_status(debug_tool_handle_t handle, debug_tool_status_t* status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
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

esp_err_t debug_tool_register_tool(debug_tool_handle_t handle,
                                 const debug_tool_registration_t* registration)
{
    if (!handle || !registration) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
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

esp_err_t debug_tool_unregister_tool(debug_tool_handle_t handle, const char* tool_id)
{
    if (!handle || !tool_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
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

static esp_err_t generate_ascii_dashboard(debug_tool_t* tool, char* buffer, size_t buffer_size)
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
            case DEBUG_BUFFER_DOTS: {
                uint32_t dots_used = (tool->buffer_used_slots * 10) / (tool->buffer_total_slots > 0 ? tool->buffer_total_slots : 1);
                for (uint32_t i = 0; i < 10; i++) {
                    strcat(buffer_visual, (i < dots_used) ? "●" : "○");
                }
                break;
            }
            case DEBUG_BUFFER_BARS: {
                uint32_t bars_used = (tool->buffer_used_slots * 10) / (tool->buffer_total_slots > 0 ? tool->buffer_total_slots : 1);
                strcpy(buffer_visual, "[");
                for (uint32_t i = 0; i < 10; i++) {
                    strcat(buffer_visual, (i < bars_used) ? "█" : "░");
                }
                strcat(buffer_visual, "]");
                break;
            }
            case DEBUG_BUFFER_NUMERIC:
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
    
    // Create ASCII dashboard
    int written = snprintf(buffer, buffer_size,
        "╔══════════════════════════════╗\n"
        "║     RFID TIME TRACKER        ║\n"
        "╚══════════════════════════════╝\n"
        " %s System: %s\n"
        " 🔧 Tools: %lu registered\n"
        " ⏱️  Up: %lu min | %s mode\n"
        " 📊 Buffer: %s\n"
        " 🔄 Retry: %s\n"
        " 🎯 Health: %s\n",
        robot_expr,
        health_str,
        tool->active_tool_count,
        uptime_minutes,
        tool->is_offline_mode ? "OFFLINE" : "ONLINE",
        buffer_visual,
        strlen(retry_visual) > 0 ? retry_visual : "None",
        health_str
    );
    
    if (written < 0 || written >= (int)buffer_size) {
        ESP_LOGE(TAG, "Dashboard buffer overflow");
        return ESP_ERR_INVALID_SIZE;
    }
    
    return ESP_OK;
}

esp_err_t debug_tool_generate_dashboard(debug_tool_handle_t handle,
                                      debug_dashboard_result_t* result)
{
    if (!handle || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
    // Initialize result
    memset(result, 0, sizeof(debug_dashboard_result_t));
    
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

esp_err_t debug_tool_update_dashboard(debug_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
    // Update system health based on registered tools
    debug_tool_calculate_system_health(handle, &tool->system_health);
    
    // Update robot expression based on health
    switch (tool->system_health) {
        case DEBUG_HEALTH_EXCELLENT:
            tool->current_robot_expr = DEBUG_ROBOT_HAPPY;
            break;
        case DEBUG_HEALTH_GOOD:
            tool->current_robot_expr = DEBUG_ROBOT_FOCUSED;
            break;
        case DEBUG_HEALTH_WARNING:
            tool->current_robot_expr = DEBUG_ROBOT_CONCERNED;
            break;
        case DEBUG_HEALTH_CRITICAL:
            tool->current_robot_expr = DEBUG_ROBOT_ANGRY;
            break;
        default:
            tool->current_robot_expr = DEBUG_ROBOT_NEUTRAL;
            break;
    }
    
    return ESP_OK;
}

void debug_tool_free_dashboard_result(debug_dashboard_result_t* result)
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

esp_err_t debug_tool_set_robot_expression(debug_tool_handle_t handle,
                                        debug_robot_expression_t expression)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    tool->current_robot_expr = expression;
    
    ESP_LOGD(TAG, "Robot expression set to: %s", get_robot_expression_string(expression));
    
    return ESP_OK;
}

esp_err_t debug_tool_update_buffer_visualization(debug_tool_handle_t handle,
                                               uint32_t used_slots,
                                               uint32_t total_slots,
                                               debug_buffer_visual_mode_t mode)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
    tool->buffer_used_slots = used_slots;
    tool->buffer_total_slots = total_slots;
    tool->buffer_visual_mode = mode;
    
    return ESP_OK;
}

esp_err_t debug_tool_increment_retry_counter(debug_tool_handle_t handle,
                                           uint32_t current_retry,
                                           uint32_t max_retries)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
    tool->retry_current = current_retry;
    tool->retry_max = max_retries;
    
    return ESP_OK;
}

esp_err_t debug_tool_set_offline_mode(debug_tool_handle_t handle, bool is_offline)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    tool->is_offline_mode = is_offline;
    
    ESP_LOGD(TAG, "Offline mode set to: %s", is_offline ? "true" : "false");
    
    return ESP_OK;
}

esp_err_t debug_tool_calculate_system_health(debug_tool_handle_t handle,
                                            debug_system_health_t* health)
{
    if (!handle || !health) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
    // Simple health calculation based on registered tools
    // In a real implementation, this would query each tool's status
    
    if (tool->active_tool_count == 0) {
        *health = DEBUG_HEALTH_UNKNOWN;
    } else if (tool->is_offline_mode) {
        *health = DEBUG_HEALTH_WARNING;
    } else if (tool->retry_current > 0) {
        *health = (tool->retry_current > tool->retry_max / 2) ? 
                  DEBUG_HEALTH_CRITICAL : DEBUG_HEALTH_WARNING;
    } else {
        *health = DEBUG_HEALTH_EXCELLENT;
    }
    
    tool->system_health = *health;
    
    return ESP_OK;
}

// =============================================================================
// JSON Status Generation (Extracted from feedback_tool)
// =============================================================================

esp_err_t debug_tool_generate_json_status(debug_tool_handle_t handle,
                                        debug_json_status_result_t* result)
{
    if (!handle || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    debug_tool_t* tool = (debug_tool_t*)handle;
    
    // Initialize result
    memset(result, 0, sizeof(debug_json_status_result_t));
    
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

void debug_tool_free_json_result(debug_json_status_result_t* result)
{
    if (result && result->json_status) {
        free(result->json_status);
        result->json_status = NULL;
        result->json_length = 0;
        result->generation_success = false;
    }
}