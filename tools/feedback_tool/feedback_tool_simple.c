/**
 * @file feedback_tool_simple.c
 * @brief MCP-Inspired Feedback Tool - Simplified GPIO Implementation
 * 
 * Phase 1 simplified implementation using GPIO instead of LED strip
 * to avoid dependency issues during initial integration.
 */

#include "feedback_tool.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "FEEDBACK_TOOL";

// =============================================================================
// Simplified Tool Structure for Phase 1
// =============================================================================

/**
 * @brief State queue entry with enhanced MCP metadata
 */
typedef struct {
    feedback_state_t state;
    feedback_priority_t priority;
    uint32_t timestamp;
    uint32_t duration_ms;          // 0 = permanent, >0 = temporary
    const char* source_tool;       // Which tool set this state (Phase 2)
} feedback_state_entry_t;

/**
 * @brief Simplified Feedback Tool Structure
 */
struct feedback_tool {
    // Tool Metadata (MCP Pattern)
    feedback_tool_config_t config;
    feedback_tool_capabilities_t capabilities;
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    
    // Hardware Resources (Simplified)
    gpio_num_t led_gpio;
    
    // Priority Queue System (Preserved from Original)
    feedback_state_entry_t state_queue[8]; // FEEDBACK_QUEUE_SIZE
    uint8_t queue_count;
    feedback_state_t current_state;
    feedback_priority_t current_priority;
    
    // Task Management
    TaskHandle_t task_handle;
    SemaphoreHandle_t queue_mutex;
    
    // Animation State
    uint32_t cycle_counter;        // For breathing and pattern animations
    bool led_state;                // Current LED on/off state for blinking
};

// =============================================================================
// Forward Declarations
// =============================================================================

static void feedback_tool_task(void *arg);
static esp_err_t queue_state_change(struct feedback_tool *tool, 
                                   feedback_state_t state, 
                                   feedback_priority_t priority, 
                                   uint32_t duration_ms);
static feedback_state_t get_highest_priority_state(struct feedback_tool *tool);
static feedback_priority_t get_state_default_priority(feedback_state_t state);
static void update_led_display(struct feedback_tool *tool);

// =============================================================================
// MCP Tool Interface Implementation
// =============================================================================

const char* feedback_tool_get_id(void)
{
    return FEEDBACK_TOOL_ID;
}

const char* feedback_tool_get_version(void)
{
    return FEEDBACK_TOOL_VERSION;
}

feedback_tool_config_t feedback_tool_create_default_config(void)
{
    feedback_tool_config_t config = {
        .led_gpio = 5,                              // Default GPIO
        .max_brightness = 10,   
        .breathing_period_ms = 4000,
        .auto_cleanup_enabled = true,
        .cleanup_interval_ms = 50
    };
    return config;
}

feedback_tool_handle_t feedback_tool_init(const feedback_tool_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Configuration cannot be NULL");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Initializing MCP-inspired feedback tool v%s (simplified)", FEEDBACK_TOOL_VERSION);
    
    // Allocate tool structure
    struct feedback_tool *tool = calloc(1, sizeof(struct feedback_tool));
    if (!tool) {
        ESP_LOGE(TAG, "Failed to allocate tool structure");
        return NULL;
    }
    
    // Copy configuration
    tool->config = *config;
    tool->uptime_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    tool->led_gpio = (gpio_num_t)config->led_gpio;
    
    // Set capabilities (simplified)
    tool->capabilities = FEEDBACK_CAP_PRIORITY_QUEUE |
                        FEEDBACK_CAP_AUTO_EXPIRE |
                        FEEDBACK_CAP_THREAD_SAFE;
    
    // Initialize queue mutex
    tool->queue_mutex = xSemaphoreCreateMutex();
    if (!tool->queue_mutex) {
        ESP_LOGE(TAG, "Failed to create queue mutex");
        free(tool);
        return NULL;
    }
    
    // Initialize GPIO
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << tool->led_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        vSemaphoreDelete(tool->queue_mutex);
        free(tool);
        return NULL;
    }
    
    // Turn off LED initially
    gpio_set_level(tool->led_gpio, 0);
    
    // Initialize state queue with IDLE state
    tool->queue_count = 1;
    tool->state_queue[0] = (feedback_state_entry_t){
        .state = FEEDBACK_STATE_IDLE,
        .priority = FEEDBACK_PRIORITY_LOW,
        .timestamp = tool->uptime_start,
        .duration_ms = 0, // Permanent
        .source_tool = "system"
    };
    tool->current_state = FEEDBACK_STATE_IDLE;
    tool->current_priority = FEEDBACK_PRIORITY_LOW;
    
    // Mark as active before creating task to avoid race condition
    tool->is_active = true;
    
    // Create background task
    BaseType_t task_ret = xTaskCreate(
        feedback_tool_task,
        "feedback_tool",
        4096,
        tool,
        5,
        &tool->task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create background task");
        vSemaphoreDelete(tool->queue_mutex);
        free(tool);
        return NULL;
    }
    
    // Wait for task to start and initialize
    vTaskDelay(pdMS_TO_TICKS(100));
    
    tool->is_initialized = true;
    
    ESP_LOGI(TAG, "Feedback tool initialized successfully (GPIO: %d)", config->led_gpio);
    ESP_LOGI(TAG, "Capabilities: 0x%02X", tool->capabilities);
    
    return tool;
}

esp_err_t feedback_tool_deinit(feedback_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    ESP_LOGI(TAG, "Deinitializing feedback tool");
    
    // Stop background task
    tool->is_active = false;
    if (tool->task_handle) {
        vTaskDelete(tool->task_handle);
        tool->task_handle = NULL;
    }
    
    // Turn off LED
    gpio_set_level(tool->led_gpio, 0);
    
    // Cleanup synchronization
    if (tool->queue_mutex) {
        vSemaphoreDelete(tool->queue_mutex);
    }
    
    // Free tool structure
    free(tool);
    
    ESP_LOGI(TAG, "Feedback tool deinitialized");
    return ESP_OK;
}

feedback_tool_capabilities_t feedback_tool_get_capabilities(feedback_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    return tool->capabilities;
}

esp_err_t feedback_tool_get_status(feedback_tool_handle_t handle, feedback_tool_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    status->is_initialized = tool->is_initialized;
    status->is_active = tool->is_active;
    status->queue_count = tool->queue_count;
    status->current_state = tool->current_state;
    status->uptime_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - tool->uptime_start;
    status->capabilities = tool->capabilities;
    
    return ESP_OK;
}

// =============================================================================
// State Management Implementation
// =============================================================================

esp_err_t feedback_tool_set_state(feedback_tool_handle_t handle, 
                                  feedback_state_t state,
                                  feedback_priority_t priority,
                                  uint32_t duration_ms)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    if (!tool->is_initialized) {
        ESP_LOGW(TAG, "Tool not initialized, ignoring state change");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Setting state: %d (priority: %d, duration: %ldms)", 
             state, priority, (long)duration_ms);
    
    return queue_state_change(tool, state, priority, duration_ms);
}

esp_err_t feedback_tool_set_state_simple(feedback_tool_handle_t handle, feedback_state_t state)
{
    feedback_priority_t priority = get_state_default_priority(state);
    uint32_t duration = 0; // Permanent by default
    
    // Some states are naturally temporary
    switch (state) {
        case FEEDBACK_STATE_WIFI_CONNECTED:
        case FEEDBACK_STATE_TIME_SYNCED:
        case FEEDBACK_STATE_WEBHOOK_SUCCESS:
            duration = 1000; // Flash for 1 second
            break;
        default:
            break;
    }
    
    return feedback_tool_set_state(handle, state, priority, duration);
}

feedback_state_t feedback_tool_get_current_state(feedback_tool_handle_t handle)
{
    if (!handle) {
        return FEEDBACK_STATE_ERROR;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    return tool->current_state;
}

uint8_t feedback_tool_get_queue_count(feedback_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    return tool->queue_count;
}

esp_err_t feedback_tool_clear_state(feedback_tool_handle_t handle, feedback_state_t state)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    if (xSemaphoreTake(tool->queue_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for state clear");
        return ESP_ERR_TIMEOUT;
    }
    
    // Remove all instances of the specified state
    uint8_t write_idx = 0;
    bool state_found = false;
    
    for (uint8_t read_idx = 0; read_idx < tool->queue_count; read_idx++) {
        if (tool->state_queue[read_idx].state != state) {
            if (write_idx != read_idx) {
                tool->state_queue[write_idx] = tool->state_queue[read_idx];
            }
            write_idx++;
        } else {
            state_found = true;
        }
    }
    
    tool->queue_count = write_idx;
    
    xSemaphoreGive(tool->queue_mutex);
    
    if (state_found) {
        ESP_LOGD(TAG, "Cleared state: %d", state);
    }
    
    return ESP_OK;
}

esp_err_t feedback_tool_reset(feedback_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    if (xSemaphoreTake(tool->queue_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Clear all states and reset to IDLE
    tool->queue_count = 1;
    tool->state_queue[0] = (feedback_state_entry_t){
        .state = FEEDBACK_STATE_IDLE,
        .priority = FEEDBACK_PRIORITY_LOW,
        .timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS,
        .duration_ms = 0,
        .source_tool = "system"
    };
    
    xSemaphoreGive(tool->queue_mutex);
    
    ESP_LOGI(TAG, "Tool reset to IDLE state");
    return ESP_OK;
}

// =============================================================================
// Background Task Implementation (Simplified)
// =============================================================================

static void feedback_tool_task(void *arg)
{
    struct feedback_tool *tool = (struct feedback_tool*)arg;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "Feedback tool task started");
    
    while (tool->is_active) {
        // Process state queue and update current state
        feedback_state_t new_state = get_highest_priority_state(tool);
        
        if (new_state != tool->current_state) {
            ESP_LOGD(TAG, "State transition: %d -> %d", tool->current_state, new_state);
            tool->current_state = new_state;
            tool->cycle_counter = 0; // Reset animation cycle
        }
        
        // Update LED display based on current state
        update_led_display(tool);
        
        // Increment cycle counter for animations
        tool->cycle_counter++;
        
        // Wait for next cycle
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(tool->config.cleanup_interval_ms));
    }
    
    ESP_LOGI(TAG, "Feedback tool task ended");
    vTaskDelete(NULL);
}

// =============================================================================
// State Queue Management (Simplified)
// =============================================================================

static esp_err_t queue_state_change(struct feedback_tool *tool, 
                                   feedback_state_t state, 
                                   feedback_priority_t priority, 
                                   uint32_t duration_ms)
{
    if (xSemaphoreTake(tool->queue_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for state queue");
        return ESP_ERR_TIMEOUT;
    }
    
    // Create new state entry
    feedback_state_entry_t new_entry = {
        .state = state,
        .priority = priority,
        .timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS,
        .duration_ms = duration_ms,
        .source_tool = "unknown" // TODO: Phase 2 - track source tool
    };
    
    // Simple append (simplified for Phase 1)
    if (tool->queue_count < 8) {
        tool->state_queue[tool->queue_count] = new_entry;
        tool->queue_count++;
    }
    
    xSemaphoreGive(tool->queue_mutex);
    
    return ESP_OK;
}

static feedback_state_t get_highest_priority_state(struct feedback_tool *tool)
{
    if (xSemaphoreTake(tool->queue_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return tool->current_state; // Keep current state if can't acquire mutex
    }
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    feedback_state_t highest_state = FEEDBACK_STATE_IDLE;
    feedback_priority_t highest_priority = FEEDBACK_PRIORITY_LOW;
    
    // Find highest priority non-expired state
    for (uint8_t i = 0; i < tool->queue_count; i++) {
        feedback_state_entry_t *entry = &tool->state_queue[i];
        
        // Check if entry has expired
        bool expired = (entry->duration_ms > 0) && 
                      ((current_time - entry->timestamp) >= entry->duration_ms);
        
        if (!expired && entry->priority >= highest_priority) {
            highest_priority = entry->priority;
            highest_state = entry->state;
        }
    }
    
    xSemaphoreGive(tool->queue_mutex);
    
    return highest_state;
}

// =============================================================================
// Simplified LED Control
// =============================================================================

static feedback_priority_t get_state_default_priority(feedback_state_t state)
{
    // Simplified priority assignment
    switch (state & 0xFF00) {
        case 0x0000: // System core states
            return FEEDBACK_PRIORITY_LOW;
        case 0x0300: // RFID states
            return FEEDBACK_PRIORITY_HIGH;
        default:
            return FEEDBACK_PRIORITY_MEDIUM;
    }
}

static void update_led_display(struct feedback_tool *tool)
{
    // Simplified LED control - just blink for different states
    bool led_on = false;
    
    switch (tool->current_state) {
        case FEEDBACK_STATE_IDLE:
            // Slow breathing
            led_on = (tool->cycle_counter % 80) < 40;
            break;
            
        case FEEDBACK_STATE_WIFI_CONNECTING:
            // Fast blinking
            led_on = (tool->cycle_counter % 20) < 10;
            break;
            
        case FEEDBACK_STATE_TAG_DETECTED:
            // Solid on
            led_on = true;
            break;
            
        case FEEDBACK_STATE_ERROR:
        case FEEDBACK_STATE_WEBHOOK_ERROR:
            // Very fast blinking
            led_on = (tool->cycle_counter % 4) < 2;
            break;
            
        default:
            // Medium blinking
            led_on = (tool->cycle_counter % 40) < 20;
            break;
    }
    
    gpio_set_level(tool->led_gpio, led_on ? 1 : 0);
}

// =============================================================================
// Utility Functions Implementation (Simplified)
// =============================================================================

esp_err_t feedback_tool_validate_init_step(feedback_tool_handle_t handle, 
                                           const char* step_name,
                                           bool success)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Init step '%s': %s", step_name ? step_name : "unknown", 
             success ? "SUCCESS" : "FAILED");
    
    feedback_state_t flash_state = success ? FEEDBACK_STATE_INIT_COMPLETE : FEEDBACK_STATE_ERROR;
    return feedback_tool_set_state(handle, flash_state, FEEDBACK_PRIORITY_HIGH, 300);
}

const char* feedback_tool_state_to_string(feedback_state_t state)
{
    switch (state) {
        case FEEDBACK_STATE_BOOTING: return "BOOTING";
        case FEEDBACK_STATE_IDLE: return "IDLE";
        case FEEDBACK_STATE_ERROR: return "ERROR";
        case FEEDBACK_STATE_WIFI_CONNECTING: return "WIFI_CONNECTING";
        case FEEDBACK_STATE_WIFI_CONNECTED: return "WIFI_CONNECTED";
        case FEEDBACK_STATE_TAG_DETECTED: return "TAG_DETECTED";
        default: return "UNKNOWN";
    }
}

const char* feedback_tool_priority_to_string(feedback_priority_t priority)
{
    switch (priority) {
        case FEEDBACK_PRIORITY_LOW: return "LOW";
        case FEEDBACK_PRIORITY_MEDIUM: return "MEDIUM";
        case FEEDBACK_PRIORITY_HIGH: return "HIGH";
        case FEEDBACK_PRIORITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// Tool Registry Implementation (Phase 2 Target)
// =============================================================================

const feedback_tool_registry_t* feedback_tool_get_registry_entry(void)
{
    static const feedback_tool_registry_t registry_entry = {
        .tool_id = FEEDBACK_TOOL_ID,
        .version = FEEDBACK_TOOL_VERSION,
        .description = FEEDBACK_TOOL_DESCRIPTION " (simplified)",
        .capabilities = FEEDBACK_CAP_PRIORITY_QUEUE |
                       FEEDBACK_CAP_AUTO_EXPIRE |
                       FEEDBACK_CAP_THREAD_SAFE,
        .init_func = feedback_tool_init,
        .deinit_func = feedback_tool_deinit
    };
    
    return &registry_entry;
}