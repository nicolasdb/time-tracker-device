/**
 * @file system_monitor_tool.c
 * @brief Constitutional System Monitor Tool - Minimal Clean Implementation
 * 
 * Clean implementation for HOST pattern.
 * Provides health reports and dashboard for constitutional orchestrator.
 */

#include "system_monitor_tool.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const char *TAG = "system_monitor_tool";

// =============================================================================
// Minimal Tool Context Structure
// =============================================================================

typedef struct {
    bool is_initialized;
    uint64_t init_timestamp_us;
    uint32_t dashboard_generation_count;
    system_monitor_tool_capabilities_t capabilities;
} system_monitor_tool_context_t;

// =============================================================================
// Tool Configuration and Initialization
// =============================================================================

system_monitor_tool_config_t system_monitor_tool_create_default_config(void)
{
    system_monitor_tool_config_t config = {
        .dashboard_buffer_size = 1024,
        .json_buffer_size = 512,
        .update_interval_ms = 30000,
        .enable_robot_expressions = true,
        .enable_buffer_visualization = false,
        .enable_real_time_updates = true
    };
    return config;
}

system_monitor_tool_handle_t system_monitor_tool_init(const system_monitor_tool_config_t* config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }

    system_monitor_tool_context_t* ctx = calloc(1, sizeof(system_monitor_tool_context_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate context");
        return NULL;
    }

    ctx->is_initialized = true;
    ctx->init_timestamp_us = esp_timer_get_time();
    ctx->dashboard_generation_count = 0;
    ctx->capabilities = SYSTEM_MONITOR_CAP_ASCII_DASHBOARD | 
                       SYSTEM_MONITOR_CAP_JSON_STATUS |
                       SYSTEM_MONITOR_CAP_THREAD_SAFE;

    ESP_LOGI(TAG, "✅ Constitutional system monitor initialized");
    return (system_monitor_tool_handle_t)ctx;
}

esp_err_t system_monitor_tool_cleanup(system_monitor_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    system_monitor_tool_context_t* ctx = (system_monitor_tool_context_t*)handle;
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    free(ctx);
    ESP_LOGI(TAG, "System monitor tool cleaned up");
    return ESP_OK;
}

// =============================================================================
// Tool Metadata Functions
// =============================================================================

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

    system_monitor_tool_context_t* ctx = (system_monitor_tool_context_t*)handle;
    return ctx->capabilities;
}

esp_err_t system_monitor_tool_get_status(system_monitor_tool_handle_t handle, system_monitor_tool_status_t* status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }

    system_monitor_tool_context_t* ctx = (system_monitor_tool_context_t*)handle;
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(status, 0, sizeof(system_monitor_tool_status_t));
    status->is_initialized = ctx->is_initialized;
    status->dashboards_generated = ctx->dashboard_generation_count;
    status->json_reports_generated = 0;
    status->tools_registered = 0;
    status->system_health = SYSTEM_MONITOR_HEALTH_GOOD;

    return ESP_OK;
}

// =============================================================================
// Constitutional Dashboard Generation
// =============================================================================

esp_err_t system_monitor_tool_generate_dashboard(system_monitor_tool_handle_t handle, 
                                                 system_monitor_dashboard_result_t* result)
{
    if (!handle || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    system_monitor_tool_context_t* ctx = (system_monitor_tool_context_t*)handle;
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Generate minimal constitutional dashboard with adequate buffer size
    const size_t dashboard_buffer_size = 1024;
    char* dashboard = malloc(dashboard_buffer_size);
    if (!dashboard) {
        return ESP_ERR_NO_MEM;
    }

    uint64_t uptime_s = (esp_timer_get_time() - ctx->init_timestamp_us) / 1000000;
    uint32_t heap_free = esp_get_free_heap_size();
    uint32_t heap_min = esp_get_minimum_free_heap_size();
    
    snprintf(dashboard, dashboard_buffer_size,
        "┌─ Constitutional HOST Status ──────────────────────┐\n"
        "│ 🏗️  HOST Pattern:    Docker-like orchestrator    │\n"
        "│ ⏱️  Uptime:          %" PRIu64 " seconds                 │\n"
        "│ 💾 Memory:          %" PRIu32 " bytes free              │\n"
        "│ 📊 Memory Low:      %" PRIu32 " bytes minimum           │\n"
        "│ 🔍 Dashboards:      %" PRIu32 " generated               │\n"
        "│ 📋 Status:          Constitutional compliance   │\n"
        "│ 🎯 Next:            Add tools with smart contracts│\n"
        "└───────────────────────────────────────────────────┘\n",
        uptime_s, heap_free, heap_min, ctx->dashboard_generation_count
    );

    result->ascii_dashboard = dashboard;
    result->dashboard_length = strlen(dashboard);
    result->robot_expr = SYSTEM_MONITOR_ROBOT_HAPPY;
    result->system_health = SYSTEM_MONITOR_HEALTH_GOOD;
    result->update_count = ctx->dashboard_generation_count;
    result->generation_success = true;

    ctx->dashboard_generation_count++;

    return ESP_OK;
}

void system_monitor_tool_free_dashboard_result(system_monitor_dashboard_result_t* result)
{
    if (!result || !result->ascii_dashboard) {
        return;
    }

    free(result->ascii_dashboard);
    result->ascii_dashboard = NULL;
    result->dashboard_length = 0;
    result->update_count = 0;
    result->generation_success = false;
}