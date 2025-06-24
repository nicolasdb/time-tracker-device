/**
 * @file tool_registry.c
 * @brief MCP-Inspired Tool Registry Implementation
 * 
 * Centralized tool management system replacing global handles.
 * Supports process map boot sequence and dependency management.
 */

#include "tool_registry.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "tool_registry";

// =============================================================================
// Registry State
// =============================================================================

typedef struct {
    tool_registration_entry_t tools[TOOL_REGISTRY_MAX_TOOLS];
    uint32_t tool_count;
    bool is_initialized;
    uint64_t init_timestamp_us;
} tool_registry_t;

static tool_registry_t* g_registry = NULL;

// =============================================================================
// Registry Management Functions
// =============================================================================

esp_err_t tool_registry_init(void)
{
    if (g_registry != NULL) {
        ESP_LOGW(TAG, "Tool registry already initialized");
        return ESP_OK;
    }
    
    g_registry = calloc(1, sizeof(tool_registry_t));
    if (!g_registry) {
        ESP_LOGE(TAG, "Failed to allocate tool registry");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize registry state
    g_registry->tool_count = 0;
    g_registry->is_initialized = true;
    g_registry->init_timestamp_us = esp_timer_get_time();
    
    // Initialize all tool entries
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        memset(entry, 0, sizeof(tool_registration_entry_t));
        entry->state = TOOL_STATE_UNINITIALIZED;
        entry->tool_handle = NULL;
        entry->interface = NULL;
    }
    
    ESP_LOGI(TAG, "Tool registry initialized (max tools: %d)", TOOL_REGISTRY_MAX_TOOLS);
    
    return ESP_OK;
}

esp_err_t tool_registry_cleanup(void)
{
    if (!g_registry) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Cleanup all registered tools
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->state != TOOL_STATE_UNINITIALIZED && 
            entry->interface && 
            entry->interface->cleanup &&
            entry->tool_handle) {
            
            ESP_LOGI(TAG, "Cleaning up tool: %s", entry->tool_id);
            entry->interface->cleanup(entry->tool_handle);
        }
    }
    
    free(g_registry);
    g_registry = NULL;
    
    ESP_LOGI(TAG, "Tool registry cleaned up");
    
    return ESP_OK;
}

esp_err_t tool_registry_register(const char* tool_id,
                                void* tool_handle,
                                const tool_interface_t* interface,
                                bool is_critical)
{
    if (!g_registry || !tool_id || !tool_handle || !interface) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if tool already registered
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        if (g_registry->tools[i].state != TOOL_STATE_UNINITIALIZED &&
            strcmp(g_registry->tools[i].tool_id, tool_id) == 0) {
            ESP_LOGW(TAG, "Tool already registered: %s", tool_id);
            return ESP_ERR_INVALID_STATE;
        }
    }
    
    // Find available slot
    int slot = -1;
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        if (g_registry->tools[i].state == TOOL_STATE_UNINITIALIZED) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        ESP_LOGE(TAG, "No available slots for tool registration");
        return ESP_ERR_NO_MEM;
    }
    
    // Register tool
    tool_registration_entry_t* entry = &g_registry->tools[slot];
    
    strncpy(entry->tool_id, tool_id, TOOL_REGISTRY_MAX_ID_LENGTH - 1);
    entry->tool_id[TOOL_REGISTRY_MAX_ID_LENGTH - 1] = '\0';
    
    // Get version from tool interface
    if (interface->get_version) {
        const char* version = interface->get_version();
        if (version) {
            strncpy(entry->tool_version, version, TOOL_REGISTRY_MAX_VERSION_LENGTH - 1);
            entry->tool_version[TOOL_REGISTRY_MAX_VERSION_LENGTH - 1] = '\0';
        }
    }
    
    entry->tool_handle = tool_handle;
    entry->interface = interface;
    entry->is_critical = is_critical;
    entry->state = TOOL_STATE_INITIALIZED;
    entry->init_timestamp_us = esp_timer_get_time();
    entry->error_count = 0;
    
    // Get capabilities if available
    if (interface->get_capabilities) {
        entry->capabilities = interface->get_capabilities(tool_handle);
    }
    
    g_registry->tool_count++;
    
    ESP_LOGI(TAG, "Registered tool: %s v%s (slot %d, %s)", 
             entry->tool_id, 
             entry->tool_version,
             slot,
             is_critical ? "critical" : "optional");
    
    return ESP_OK;
}

esp_err_t tool_registry_unregister(const char* tool_id)
{
    if (!g_registry || !tool_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->state != TOOL_STATE_UNINITIALIZED &&
            strcmp(entry->tool_id, tool_id) == 0) {
            
            // Cleanup tool if possible
            if (entry->interface && entry->interface->cleanup && entry->tool_handle) {
                entry->interface->cleanup(entry->tool_handle);
            }
            
            // Clear entry
            memset(entry, 0, sizeof(tool_registration_entry_t));
            entry->state = TOOL_STATE_UNINITIALIZED;
            
            g_registry->tool_count--;
            
            ESP_LOGI(TAG, "Unregistered tool: %s", tool_id);
            return ESP_OK;
        }
    }
    
    ESP_LOGW(TAG, "Tool not found for unregistration: %s", tool_id);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t tool_registry_get_handle(const char* tool_id, void** tool_handle)
{
    if (!g_registry || !tool_id || !tool_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->state != TOOL_STATE_UNINITIALIZED &&
            strcmp(entry->tool_id, tool_id) == 0) {
            *tool_handle = entry->tool_handle;
            return ESP_OK;
        }
    }
    
    ESP_LOGD(TAG, "Tool handle not found: %s", tool_id);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t tool_registry_get_state(const char* tool_id, tool_state_t* state)
{
    if (!g_registry || !tool_id || !state) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->state != TOOL_STATE_UNINITIALIZED &&
            strcmp(entry->tool_id, tool_id) == 0) {
            *state = entry->state;
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t tool_registry_set_state(const char* tool_id, tool_state_t state)
{
    if (!g_registry || !tool_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->state != TOOL_STATE_UNINITIALIZED &&
            strcmp(entry->tool_id, tool_id) == 0) {
            entry->state = state;
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t tool_registry_get_capabilities(const char* tool_id, tool_capabilities_t* capabilities)
{
    if (!g_registry || !tool_id || !capabilities) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->state != TOOL_STATE_UNINITIALIZED &&
            strcmp(entry->tool_id, tool_id) == 0) {
            *capabilities = entry->capabilities;
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t tool_registry_get_stats(tool_registry_stats_t* stats)
{
    if (!g_registry || !stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(stats, 0, sizeof(tool_registry_stats_t));
    stats->registry_init_timestamp_us = g_registry->init_timestamp_us;
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->state != TOOL_STATE_UNINITIALIZED) {
            stats->total_tools++;
            
            switch (entry->state) {
                case TOOL_STATE_INITIALIZED:
                    stats->initialized_tools++;
                    break;
                case TOOL_STATE_RUNNING:
                    stats->running_tools++;
                    break;
                case TOOL_STATE_ERROR:
                    stats->error_tools++;
                    break;
                default:
                    break;
            }
            
            if (entry->is_critical) {
                stats->critical_tools++;
            }
        }
    }
    
    return ESP_OK;
}

esp_err_t tool_registry_check_critical_tools(bool* all_critical_running)
{
    if (!g_registry || !all_critical_running) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *all_critical_running = true;
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->is_critical && 
            entry->state != TOOL_STATE_UNINITIALIZED &&
            entry->state != TOOL_STATE_RUNNING) {
            *all_critical_running = false;
            ESP_LOGW(TAG, "Critical tool not running: %s (state: %d)", 
                     entry->tool_id, entry->state);
        }
    }
    
    return ESP_OK;
}

esp_err_t tool_registry_list_tools(char tool_ids[][TOOL_REGISTRY_MAX_ID_LENGTH],
                                  uint32_t max_tools,
                                  uint32_t* actual_count)
{
    if (!g_registry || !tool_ids || !actual_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS && *actual_count < max_tools; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->state != TOOL_STATE_UNINITIALIZED) {
            strncpy(tool_ids[*actual_count], entry->tool_id, TOOL_REGISTRY_MAX_ID_LENGTH - 1);
            tool_ids[*actual_count][TOOL_REGISTRY_MAX_ID_LENGTH - 1] = '\0';
            (*actual_count)++;
        }
    }
    
    return ESP_OK;
}

esp_err_t tool_registry_increment_error_count(const char* tool_id)
{
    if (!g_registry || !tool_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->state != TOOL_STATE_UNINITIALIZED &&
            strcmp(entry->tool_id, tool_id) == 0) {
            entry->error_count++;
            ESP_LOGD(TAG, "Error count incremented for %s: %lu", tool_id, entry->error_count);
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t tool_registry_get_entry(const char* tool_id, tool_registration_entry_t* entry)
{
    if (!g_registry || !tool_id || !entry) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* reg_entry = &g_registry->tools[i];
        if (reg_entry->state != TOOL_STATE_UNINITIALIZED &&
            strcmp(reg_entry->tool_id, tool_id) == 0) {
            memcpy(entry, reg_entry, sizeof(tool_registration_entry_t));
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

// =============================================================================
// Process Map Boot Sequence Support
// =============================================================================

esp_err_t tool_registry_boot_sequence_init(boot_sequence_entry_t* sequence,
                                          uint32_t sequence_length)
{
    if (!g_registry || !sequence || sequence_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Starting boot sequence initialization (%lu tools)", sequence_length);
    
    // Sort sequence by init order (simple bubble sort for small arrays)
    for (uint32_t i = 0; i < sequence_length - 1; i++) {
        for (uint32_t j = 0; j < sequence_length - i - 1; j++) {
            if (sequence[j].order > sequence[j + 1].order) {
                boot_sequence_entry_t temp = sequence[j];
                sequence[j] = sequence[j + 1];
                sequence[j + 1] = temp;
            }
        }
    }
    
    // Initialize tools in order
    esp_err_t overall_result = ESP_OK;
    for (uint32_t i = 0; i < sequence_length; i++) {
        boot_sequence_entry_t* entry = &sequence[i];
        
        ESP_LOGI(TAG, "Boot sequence step %lu: %s (order %d)", 
                 i + 1, entry->tool_id, entry->order);
        
        // Check if tool is registered
        tool_state_t state;
        esp_err_t state_ret = tool_registry_get_state(entry->tool_id, &state);
        
        if (state_ret == ESP_OK) {
            if (state == TOOL_STATE_INITIALIZED || state == TOOL_STATE_RUNNING) {
                // Set tool to running state
                tool_registry_set_state(entry->tool_id, TOOL_STATE_RUNNING);
                entry->is_initialized = true;
                entry->init_result = ESP_OK;
                ESP_LOGI(TAG, "✅ Tool %s: OK", entry->tool_id);
            } else {
                entry->is_initialized = false;
                entry->init_result = ESP_ERR_INVALID_STATE;
                overall_result = ESP_ERR_INVALID_STATE;
                ESP_LOGW(TAG, "⚠️  Tool %s: Invalid state (%d)", entry->tool_id, state);
            }
        } else {
            entry->is_initialized = false;
            entry->init_result = ESP_ERR_NOT_FOUND;
            overall_result = ESP_ERR_NOT_FOUND;
            ESP_LOGW(TAG, "❌ Tool %s: Not registered", entry->tool_id);
        }
        
        // Small delay between tool initializations
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Boot sequence initialization %s", 
             (overall_result == ESP_OK) ? "COMPLETED" : "COMPLETED WITH WARNINGS");
    
    return overall_result;
}

esp_err_t tool_registry_validate_boot_sequence(bool* all_initialized)
{
    if (!g_registry || !all_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *all_initialized = true;
    uint32_t total_tools = 0;
    uint32_t running_tools = 0;
    
    for (int i = 0; i < TOOL_REGISTRY_MAX_TOOLS; i++) {
        tool_registration_entry_t* entry = &g_registry->tools[i];
        if (entry->state != TOOL_STATE_UNINITIALIZED) {
            total_tools++;
            if (entry->state == TOOL_STATE_RUNNING) {
                running_tools++;
            } else {
                *all_initialized = false;
                ESP_LOGW(TAG, "Tool not running: %s (state: %d)", entry->tool_id, entry->state);
            }
        }
    }
    
    ESP_LOGI(TAG, "Boot sequence validation: %lu/%lu tools running", running_tools, total_tools);
    
    return ESP_OK;
}