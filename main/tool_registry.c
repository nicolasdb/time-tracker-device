/**
 * @file tool_registry.c
 * @brief Constitutional Tool Registry Implementation
 * 
 * ESP_EVENT-based container orchestration.
 * Constitutional Authority: Process Map 01
 */

#include "tool_registry.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "TOOL_REGISTRY";

// =============================================================================
// Constitutional Registry Storage
// =============================================================================

static struct {
    tool_registration_entry_t tools[TOOL_REGISTRY_MAX_TOOLS];
    uint32_t tool_count;
    uint64_t init_timestamp_us;
    bool is_initialized;
} tool_registry_context = {0};

// =============================================================================
// Constitutional Tool Registry Implementation
// =============================================================================

esp_err_t tool_registry_init(void)
{
    if (tool_registry_context.is_initialized) {
        ESP_LOGW(TAG, "Tool registry already initialized");
        return ESP_OK;
    }

    memset(&tool_registry_context, 0, sizeof(tool_registry_context));
    tool_registry_context.init_timestamp_us = esp_timer_get_time();
    tool_registry_context.is_initialized = true;

    ESP_LOGI(TAG, "✅ Constitutional tool registry initialized");
    return ESP_OK;
}

esp_err_t tool_registry_register(const char* tool_id,
                                void* tool_handle,
                                const tool_interface_t* interface,
                                bool is_critical)
{
    if (!tool_registry_context.is_initialized) {
        ESP_LOGE(TAG, "Registry not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!tool_id || !tool_handle || !interface) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (tool_registry_context.tool_count >= TOOL_REGISTRY_MAX_TOOLS) {
        ESP_LOGE(TAG, "Registry full");
        return ESP_ERR_NO_MEM;
    }

    // Check for duplicate registration
    for (uint32_t i = 0; i < tool_registry_context.tool_count; i++) {
        if (strcmp(tool_registry_context.tools[i].tool_id, tool_id) == 0) {
            ESP_LOGW(TAG, "Tool '%s' already registered", tool_id);
            return ESP_ERR_INVALID_STATE;
        }
    }

    // Register new tool
    tool_registration_entry_t* entry = &tool_registry_context.tools[tool_registry_context.tool_count];
    
    snprintf(entry->tool_id, TOOL_REGISTRY_MAX_ID_LENGTH, "%s", tool_id);
    
    if (interface->get_version) {
        snprintf(entry->tool_version, TOOL_REGISTRY_MAX_VERSION_LENGTH, "%s", interface->get_version());
    }

    entry->tool_handle = tool_handle;
    entry->interface = interface;
    entry->state = TOOL_STATE_INITIALIZED;
    entry->init_timestamp_us = esp_timer_get_time();
    entry->error_count = 0;
    entry->is_critical = is_critical;

    if (interface->get_capabilities) {
        entry->capabilities = interface->get_capabilities(tool_handle);
    }

    tool_registry_context.tool_count++;

    ESP_LOGI(TAG, "✅ Registered tool '%s' v%s (critical: %s)", 
             tool_id, entry->tool_version, is_critical ? "yes" : "no");

    return ESP_OK;
}

esp_err_t tool_registry_get_handle(const char* tool_id, void** tool_handle)
{
    if (!tool_registry_context.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!tool_id || !tool_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < tool_registry_context.tool_count; i++) {
        if (strcmp(tool_registry_context.tools[i].tool_id, tool_id) == 0) {
            *tool_handle = tool_registry_context.tools[i].tool_handle;
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t tool_registry_set_state(const char* tool_id, tool_state_t state)
{
    if (!tool_registry_context.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!tool_id) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < tool_registry_context.tool_count; i++) {
        if (strcmp(tool_registry_context.tools[i].tool_id, tool_id) == 0) {
            tool_registry_context.tools[i].state = state;
            ESP_LOGI(TAG, "Tool '%s' state changed to %d", tool_id, state);
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t tool_registry_get_stats(tool_registry_stats_t* stats)
{
    if (!tool_registry_context.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(stats, 0, sizeof(tool_registry_stats_t));
    stats->total_tools = tool_registry_context.tool_count;
    stats->registry_init_timestamp_us = tool_registry_context.init_timestamp_us;

    for (uint32_t i = 0; i < tool_registry_context.tool_count; i++) {
        tool_registration_entry_t* entry = &tool_registry_context.tools[i];
        
        if (entry->state == TOOL_STATE_INITIALIZED) {
            stats->initialized_tools++;
        } else if (entry->state == TOOL_STATE_RUNNING) {
            stats->running_tools++;
        } else if (entry->state == TOOL_STATE_ERROR) {
            stats->error_tools++;
        }

        if (entry->is_critical) {
            stats->critical_tools++;
        }
    }

    return ESP_OK;
}

esp_err_t tool_registry_boot_sequence_init(boot_sequence_entry_t* sequence,
                                          uint32_t sequence_length)
{
    if (!sequence || sequence_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "🎯 Validating boot sequence per Process Map 01");

    uint32_t success_count = 0;
    for (uint32_t i = 0; i < sequence_length; i++) {
        boot_sequence_entry_t* entry = &sequence[i];
        
        // Check if tool is registered
        void* tool_handle = NULL;
        esp_err_t result = tool_registry_get_handle(entry->tool_id, &tool_handle);
        
        if (result == ESP_OK && tool_handle != NULL) {
            entry->is_initialized = true;
            entry->init_result = ESP_OK;
            success_count++;
            ESP_LOGI(TAG, "✅ Boot sequence validation: %s (order %d)", 
                     entry->tool_id, entry->order);
        } else {
            entry->is_initialized = false;
            entry->init_result = result;
            ESP_LOGW(TAG, "⚠️ Boot sequence validation: %s MISSING (order %d)", 
                     entry->tool_id, entry->order);
        }
    }

    ESP_LOGI(TAG, "🎯 Boot sequence validation: %lu/%lu tools validated", 
             success_count, sequence_length);

    return (success_count == sequence_length) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t tool_registry_cleanup(void)
{
    if (!tool_registry_context.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Cleanup registered tools in reverse order
    for (int32_t i = tool_registry_context.tool_count - 1; i >= 0; i--) {
        tool_registration_entry_t* entry = &tool_registry_context.tools[i];
        
        if (entry->interface && entry->interface->cleanup) {
            esp_err_t result = entry->interface->cleanup(entry->tool_handle);
            if (result != ESP_OK) {
                ESP_LOGW(TAG, "Tool '%s' cleanup failed: %s", 
                         entry->tool_id, esp_err_to_name(result));
            } else {
                ESP_LOGI(TAG, "✅ Tool '%s' cleaned up", entry->tool_id);
            }
        }
    }

    memset(&tool_registry_context, 0, sizeof(tool_registry_context));
    ESP_LOGI(TAG, "✅ Constitutional tool registry cleaned up");

    return ESP_OK;
}