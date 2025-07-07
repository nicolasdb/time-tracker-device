/**
 * @file feedback_tool.c
 * @brief Constitutional Feedback Tool Implementation per Process Map 11
 * 
 * Constitutional Authority: Process Map 11 - IDLE { LISTENING → LOOKUP → EXECUTE → LISTENING }
 * "Direct LED control via led_strip, No queues, no priorities, Just execute the recipe"
 * 
 * Implements simple FSM with recipe lookup system instead of complex priority queues.
 */

#include "feedback_tool.h"
#include "event_system.h"   // Event-driven architecture
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <time.h>
#include "esp_event.h"
#include "cJSON.h"

static const char *TAG = "FEEDBACK_TOOL";

// =============================================================================
// MCP Tool Configuration & Constants
// =============================================================================

// Default configuration values
#define DEFAULT_MAX_BRIGHTNESS      CONFIG_FEEDBACK_TOOL_MAX_BRIGHTNESS
#define DEFAULT_BREATHING_PERIOD    (CONFIG_FEEDBACK_TOOL_BREATHING_PERIOD_MS / 50)  // Convert to cycles
#define DEFAULT_CLEANUP_INTERVAL    50      // 50ms task interval
#define FEEDBACK_QUEUE_SIZE         CONFIG_FEEDBACK_TOOL_QUEUE_SIZE
#define TASK_STACK_SIZE             CONFIG_FEEDBACK_TOOL_TASK_STACK_SIZE
#define TASK_PRIORITY               CONFIG_FEEDBACK_TOOL_TASK_PRIORITY

// LED color definitions (RGB format)
typedef struct {
    uint8_t r;
    uint8_t g; 
    uint8_t b;
} rgb_color_t;

// Enhanced color definitions per Feedback_colorMap.md
static const rgb_color_t COLOR_OFF      = {0, 0, 0};
static const rgb_color_t COLOR_RED      = {255, 0, 0};     // Errors & Failures
static const rgb_color_t COLOR_GREEN    = {0, 255, 0};     // Events & Normal operations
static const rgb_color_t COLOR_BLUE     = {0, 0, 255};     // Connectivity & Communication
static const rgb_color_t COLOR_YELLOW   = {255, 255, 0};   // Warnings & Configuration
static const rgb_color_t COLOR_PURPLE   = {128, 0, 128};   // Special states (AP mode, init)
static const rgb_color_t COLOR_WHITE    = {255, 255, 255}; // System states
static const rgb_color_t COLOR_ORANGE   = {255, 165, 0};   // Cognitive wellness & Timing
static const rgb_color_t COLOR_CYAN     = {0, 255, 255};   // Connection success states

// =============================================================================
// Internal Tool Structure (Enhanced from Original)
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
 * @brief MCP-Inspired Feedback Tool Structure
 */
struct feedback_tool {
    // Tool Metadata (MCP Pattern)
    feedback_tool_config_t config;
    feedback_tool_capabilities_t capabilities;
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    
    // Hardware Resources
    led_strip_handle_t led_strip;
    
    // Constitutional Authority: Process Map 11 FSM (LISTENING → LOOKUP → EXECUTE)
    typedef enum {
        FSM_LISTENING,    // Wait for esp_event state changes
        FSM_LOOKUP,       // Match state to recipe
        FSM_EXECUTE       // Execute LED recipe
    } constitutional_fsm_state_t;
    
    constitutional_fsm_state_t fsm_state;
    feedback_state_t pending_state;     // State received in LISTENING
    feedback_state_t current_state;     // State being executed
    
    // Constitutional Authority: Recipe System per Process Map 11
    typedef enum {
        RECIPE_GREEN_STEADY,        // TAG_PRESENT
        RECIPE_BLUE_FADE,          // SYSTEM_IDLE
        RECIPE_CYAN_BLINK,         // WIFI_CONNECTING
        RECIPE_CYAN_STEADY,        // WIFI_CONNECTED
        RECIPE_AP_MODE_SEQUENCE,   // AP_MODE (yellow→blue→purple)
        RECIPE_BLUE_BLINK,         // NTP_CALL
        RECIPE_BLUE_STEADY,        // NTP_SYNC
        RECIPE_GREEN_PULSE,        // HTTP_SENDING
        RECIPE_RED_BLINK,          // ERROR_DETECTED
        RECIPE_ORANGE_FADE,        // FLOW60
        RECIPE_ORANGE_PULSE,       // FLOW90
        RECIPE_WHITE_PULSE         // BOOT
    } led_recipe_type_t;
    
    typedef struct {
        feedback_state_t state;
        led_recipe_type_t recipe;
    } constitutional_recipe_t;
    
    led_recipe_type_t current_recipe;
    
    // Task Management
    TaskHandle_t task_handle;
    SemaphoreHandle_t queue_mutex;
    
    // Animation State
    uint32_t cycle_counter;        // For breathing and pattern animations
    bool led_state;                // Current LED on/off state for blinking
    
    // Dashboard & Status Aggregation (Phase 4.3) - Reduced for stack safety
    char last_dashboard[512];      // Cached ASCII dashboard (reduced)
    char last_json_status[256];    // Cached JSON status (reduced)
    uint32_t last_dashboard_time;  // Last dashboard generation time
    uint32_t dashboard_update_interval; // Dashboard update interval (ms)
    bool system_operational;       // Overall system health
    
    // Flow Awareness Context (Process Map Authority: Constitutional Requirement)
    bool flow_awareness_active;    // 60-minute flow awareness triggered
    bool flow_urgency_active;      // 90-minute flow urgency triggered
};

// =============================================================================
// Constitutional Authority: Recipe Lookup Table per Process Map 11
// =============================================================================

static const constitutional_recipe_t CONSTITUTIONAL_RECIPES[] = {
    {FEEDBACK_STATE_TAG_DETECTED,     RECIPE_GREEN_STEADY},      // TAG_PRESENT → green_steady
    {FEEDBACK_STATE_IDLE,             RECIPE_BLUE_FADE},          // SYSTEM_IDLE → blue.fadeIn→fadeOut
    {FEEDBACK_STATE_WIFI_CONNECTING,  RECIPE_CYAN_BLINK},         // WIFI_CONNECTING → cyan_blink
    {FEEDBACK_STATE_WIFI_CONNECTED,   RECIPE_CYAN_STEADY},        // WIFI_CONNECTED → cyan_steady
    {FEEDBACK_STATE_AP_MODE,          RECIPE_AP_MODE_SEQUENCE},   // AP_MODE → yellow→blue→purple
    {FEEDBACK_STATE_NTP_SYNC_STARTED, RECIPE_BLUE_BLINK},         // NTP_CALL → blue_blink
    {FEEDBACK_STATE_NTP_SYNCED,       RECIPE_BLUE_STEADY},        // NTP_SYNC → blue_steady
    {FEEDBACK_STATE_HTTP_SENDING,     RECIPE_GREEN_PULSE},        // HTTP_SENDING → green_pulse
    {FEEDBACK_STATE_ERROR,            RECIPE_RED_BLINK},          // ERROR_DETECTED → red_blink
    {FEEDBACK_STATE_FLOW_60,          RECIPE_ORANGE_FADE},        // FLOW60 → orange.fadeIn→fadeOut
    {FEEDBACK_STATE_FLOW_90,          RECIPE_ORANGE_PULSE},       // FLOW90 → orange_pulse
    {FEEDBACK_STATE_BOOTING,          RECIPE_WHITE_PULSE}         // BOOT → white_pulse
};

#define CONSTITUTIONAL_RECIPE_COUNT (sizeof(CONSTITUTIONAL_RECIPES) / sizeof(constitutional_recipe_t))

// =============================================================================
// Constitutional Forward Declarations per Process Map 11
// =============================================================================

static void constitutional_feedback_fsm(void *arg);  // LISTENING → LOOKUP → EXECUTE
static led_recipe_type_t constitutional_lookup_recipe(feedback_state_t state);  // LOOKUP phase
static void constitutional_execute_recipe(struct feedback_tool *tool, led_recipe_type_t recipe);  // EXECUTE phase
static rgb_color_t get_state_color(feedback_state_t state);  // Preserved for compatibility

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
        .led_gpio = CONFIG_FEEDBACK_TOOL_LED_GPIO,  // Configurable GPIO
        .max_brightness = DEFAULT_MAX_BRIGHTNESS,   
        .breathing_period_ms = DEFAULT_BREATHING_PERIOD * DEFAULT_CLEANUP_INTERVAL,
        .auto_cleanup_enabled = true,
        .cleanup_interval_ms = DEFAULT_CLEANUP_INTERVAL
    };
    return config;
}

feedback_tool_handle_t feedback_tool_init(const feedback_tool_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Configuration cannot be NULL");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Initializing MCP-inspired feedback tool v%s", FEEDBACK_TOOL_VERSION);
    
    // Allocate tool structure
    struct feedback_tool *tool = calloc(1, sizeof(struct feedback_tool));
    if (!tool) {
        ESP_LOGE(TAG, "Failed to allocate tool structure");
        return NULL;
    }
    
    // Copy configuration
    tool->config = *config;
    tool->uptime_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Constitutional Authority: Set capabilities per Process Map 11
    tool->capabilities = FEEDBACK_CAP_LED_CONTROL | 
                        FEEDBACK_CAP_FSM_EXECUTION |
                        FEEDBACK_CAP_RECIPE_LOOKUP |
                        FEEDBACK_CAP_THREAD_SAFE;
    
    // Initialize queue mutex
    tool->queue_mutex = xSemaphoreCreateMutex();
    if (!tool->queue_mutex) {
        ESP_LOGE(TAG, "Failed to create queue mutex");
        free(tool);
        return NULL;
    }
    
    // Initialize LED strip
    led_strip_config_t strip_config = {
        .strip_gpio_num = config->led_gpio,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };
    
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        }
    };
    
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &tool->led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        vSemaphoreDelete(tool->queue_mutex);
        free(tool);
        return NULL;
    }
    
    // Clear LED
    led_strip_clear(tool->led_strip);
    
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
    
    // Initialize dashboard data (Phase 4.3)
    tool->dashboard_update_interval = 5000; // 5 seconds
    tool->last_dashboard_time = 0;
    tool->system_operational = true;
    memset(tool->last_dashboard, 0, sizeof(tool->last_dashboard));
    memset(tool->last_json_status, 0, sizeof(tool->last_json_status));
    
    // Mark as active before creating task to avoid race condition
    tool->is_active = true;
    
    // Constitutional Authority: Create FSM task per Process Map 11
    BaseType_t task_ret = xTaskCreate(
        constitutional_feedback_fsm,
        "constitutional_fsm",
        TASK_STACK_SIZE,
        tool,
        TASK_PRIORITY,
        &tool->task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create background task");
        led_strip_del(tool->led_strip);
        vSemaphoreDelete(tool->queue_mutex);
        free(tool);
        return NULL;
    }
    
    // Wait for task to start and initialize
    vTaskDelay(pdMS_TO_TICKS(100));
    
    tool->is_initialized = true;
    
    ESP_LOGI(TAG, "Feedback tool initialized successfully");
    ESP_LOGI(TAG, "  GPIO: %d, Brightness: %d, Period: %" PRIu32 "ms", 
             config->led_gpio, config->max_brightness, config->breathing_period_ms);
    ESP_LOGI(TAG, "  Capabilities: 0x%02X", tool->capabilities);
    
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
    
    // Clear LED
    if (tool->led_strip) {
        led_strip_clear(tool->led_strip);
        led_strip_del(tool->led_strip);
    }
    
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
// State Management Implementation (Enhanced from Original)
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
    
    ESP_LOGD(TAG, "Setting state: %s (priority: %d, duration: %" PRIu32 "ms)", 
             feedback_tool_state_to_string(state), priority, duration_ms);
    
    return queue_state_change(tool, state, priority, duration_ms);
}

esp_err_t feedback_tool_set_state_simple(feedback_tool_handle_t handle, feedback_state_t state)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    ESP_LOGI(TAG, "🔄 Constitutional State Change: %s → triggers FSM LISTENING", 
             feedback_tool_state_to_string(state));
    
    // Constitutional Authority: Process Map 11 - esp_event triggers LISTENING state
    // Direct state setting for FSM (no queues, no priorities per Process Map 11)
    xSemaphoreTake(tool->queue_mutex, portMAX_DELAY);
    tool->pending_state = state;
    xSemaphoreGive(tool->queue_mutex);
    
    ESP_LOGI(TAG, "✅ FSM triggered: LISTENING state will detect pending_state change");
    
    return ESP_OK;
}

esp_err_t feedback_tool_flash_event(feedback_tool_handle_t handle, 
                                    feedback_state_t state, 
                                    int count,
                                    uint32_t flash_duration_ms)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (count <= 0) {
        ESP_LOGW(TAG, "Invalid flash count: %d", count);
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    if (!tool->is_initialized) {
        ESP_LOGW(TAG, "Tool not initialized, ignoring flash event");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Flash event: %s (count: %d, duration: %" PRIu32 "ms)", 
             feedback_tool_state_to_string(state), count, flash_duration_ms);
    
    // For now, implement as a single temporary state with total duration
    // This could be enhanced to support multiple flashes in the future
    uint32_t total_duration = count * flash_duration_ms;
    feedback_priority_t priority = get_state_default_priority(state);
    
    return queue_state_change(tool, state, priority, total_duration);
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
        ESP_LOGD(TAG, "Cleared state: %s", feedback_tool_state_to_string(state));
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
// Background Task Implementation (Enhanced from Original)
// =============================================================================

// =============================================================================
// Constitutional Authority: Process Map 11 FSM Implementation
// IDLE { LISTENING → LOOKUP → EXECUTE → LISTENING }
// =============================================================================

static void constitutional_feedback_fsm(void *arg)
{
    struct feedback_tool *tool = (struct feedback_tool*)arg;
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t debug_counter = 0;
    
    ESP_LOGI(TAG, "✅ Constitutional FSM started - Process Map 11 compliance");
    
    // Initialize FSM to LISTENING state
    tool->fsm_state = FSM_LISTENING;
    tool->pending_state = FEEDBACK_STATE_IDLE;
    tool->current_state = FEEDBACK_STATE_IDLE;
    tool->current_recipe = RECIPE_BLUE_FADE;
    
    while (tool->is_active) {
        switch (tool->fsm_state) {
            case FSM_LISTENING:
                // Process Map 11: LISTENING state - wait for state changes
                if (tool->pending_state != tool->current_state) {
                    ESP_LOGI(TAG, "🔄 FSM: LISTENING → LOOKUP (state change detected)");
                    tool->fsm_state = FSM_LOOKUP;
                }
                break;
                
            case FSM_LOOKUP:
                // Process Map 11: LOOKUP state - match state to recipe
                ESP_LOGI(TAG, "🔍 FSM: LOOKUP phase - matching state to recipe");
                led_recipe_type_t new_recipe = constitutional_lookup_recipe(tool->pending_state);
                
                if (new_recipe != tool->current_recipe) {
                    ESP_LOGI(TAG, "🔄 State Transition: %s -> %s", 
                             feedback_tool_state_to_string(tool->current_state),
                             feedback_tool_state_to_string(tool->pending_state));
                    tool->current_state = tool->pending_state;
                    tool->current_recipe = new_recipe;
                    tool->cycle_counter = 0; // Reset recipe cycle
                }
                
                ESP_LOGI(TAG, "✅ FSM: LOOKUP → EXECUTE (recipe matched)");
                tool->fsm_state = FSM_EXECUTE;
                break;
                
            case FSM_EXECUTE:
                // Process Map 11: EXECUTE state - "Just execute the recipe"
                constitutional_execute_recipe(tool, tool->current_recipe);
                
                // Increment cycle for recipe animations
                tool->cycle_counter++;
                debug_counter++;
                
                // Constitutional Authority: Return to LISTENING after execution
                tool->fsm_state = FSM_LISTENING;
                break;
        }
        
        // Debug FSM activity every 5 seconds (reduced logging)
        if (debug_counter % 500 == 0) {
            ESP_LOGI(TAG, "🔄 Constitutional FSM: state=%s, fsm=%s, cycle=%lu", 
                     feedback_tool_state_to_string(tool->current_state),
                     (tool->fsm_state == FSM_LISTENING) ? "LISTENING" :
                     (tool->fsm_state == FSM_LOOKUP) ? "LOOKUP" : "EXECUTE",
                     tool->cycle_counter);
        }
        
        // Constitutional timing: 10ms cycle for responsive feedback
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "Constitutional FSM ended");
    vTaskDelete(NULL);
}

// =============================================================================
// Constitutional Authority: LOOKUP Implementation per Process Map 11
// =============================================================================

static led_recipe_type_t constitutional_lookup_recipe(feedback_state_t state)
{
    // Process Map 11 LOOKUP phase: "Match to recipe"
    for (int i = 0; i < CONSTITUTIONAL_RECIPE_COUNT; i++) {
        if (CONSTITUTIONAL_RECIPES[i].state == state) {
            ESP_LOGD(TAG, "🔍 Constitutional lookup: state=%s → recipe=%d", 
                     feedback_tool_state_to_string(state), CONSTITUTIONAL_RECIPES[i].recipe);
            return CONSTITUTIONAL_RECIPES[i].recipe;
        }
    }
    
    // Default recipe if state not found
    ESP_LOGW(TAG, "⚠️ Constitutional lookup: state=%s not found, using BLUE_FADE default", 
             feedback_tool_state_to_string(state));
    return RECIPE_BLUE_FADE;
}

// =============================================================================
// Constitutional Authority: EXECUTE Implementation per Process Map 11
// =============================================================================

static void constitutional_execute_recipe(struct feedback_tool *tool, led_recipe_type_t recipe)
{
    // Process Map 11 EXECUTE phase: "Direct LED control via led_strip, No queues, no priorities, Just execute the recipe"
    
    switch (recipe) {
        case RECIPE_GREEN_STEADY:
            // TAG_PRESENT → green_steady
            led_strip_set_pixel(tool->led_strip, 0, 0, 255, 0);  // Green
            led_strip_refresh(tool->led_strip);
            break;
            
        case RECIPE_BLUE_FADE:
            // SYSTEM_IDLE → blue.fadeIn → blue.fadeOut
            {
                float phase = (2.0 * M_PI * tool->cycle_counter) / 400.0;  // 4 second cycle
                float intensity = (sin(phase) + 1.0) / 2.0;
                uint8_t blue_value = (uint8_t)(intensity * 255);
                led_strip_set_pixel(tool->led_strip, 0, 0, 0, blue_value);
                led_strip_refresh(tool->led_strip);
            }
            break;
            
        case RECIPE_CYAN_BLINK:
            // WIFI_CONNECTING → cyan_blink (CORRECTED: was blue, now cyan)
            if ((tool->cycle_counter / 50) % 2 == 0) {  // 1 second blink
                led_strip_set_pixel(tool->led_strip, 0, 0, 255, 255);  // Cyan
            } else {
                led_strip_set_pixel(tool->led_strip, 0, 0, 0, 0);  // Off
            }
            led_strip_refresh(tool->led_strip);
            break;
            
        case RECIPE_CYAN_STEADY:
            // WIFI_CONNECTED → cyan_steady
            led_strip_set_pixel(tool->led_strip, 0, 0, 255, 255);  // Cyan
            led_strip_refresh(tool->led_strip);
            break;
            
        case RECIPE_ORANGE_FADE:
            // FLOW60 → orange.fadeIn → orange.fadeOut
            {
                float phase = (2.0 * M_PI * tool->cycle_counter) / 400.0;  // 4 second cycle
                float intensity = (sin(phase) + 1.0) / 2.0;
                uint8_t orange_red = (uint8_t)(intensity * 255);
                uint8_t orange_green = (uint8_t)(intensity * 165);
                led_strip_set_pixel(tool->led_strip, 0, orange_red, orange_green, 0);
                led_strip_refresh(tool->led_strip);
            }
            break;
            
        case RECIPE_ORANGE_PULSE:
            // FLOW90 → orange_pulse
            if ((tool->cycle_counter / 25) % 2 == 0) {  // 0.5 second pulse
                led_strip_set_pixel(tool->led_strip, 0, 255, 165, 0);  // Orange
            } else {
                led_strip_set_pixel(tool->led_strip, 0, 128, 82, 0);   // Dim orange
            }
            led_strip_refresh(tool->led_strip);
            break;
            
        case RECIPE_WHITE_PULSE:
            // BOOT → white_pulse
            if ((tool->cycle_counter / 30) % 2 == 0) {  // ~0.6 second pulse
                led_strip_set_pixel(tool->led_strip, 0, 255, 255, 255);  // White
            } else {
                led_strip_set_pixel(tool->led_strip, 0, 128, 128, 128);   // Dim white
            }
            led_strip_refresh(tool->led_strip);
            break;
            
        case RECIPE_RED_BLINK:
            // ERROR_DETECTED → red_blink
            if ((tool->cycle_counter / 20) % 2 == 0) {  // Fast blink
                led_strip_set_pixel(tool->led_strip, 0, 255, 0, 0);  // Red
            } else {
                led_strip_set_pixel(tool->led_strip, 0, 0, 0, 0);    // Off
            }
            led_strip_refresh(tool->led_strip);
            break;
            
        case RECIPE_GREEN_PULSE:
            // HTTP_SENDING → green_pulse
            if ((tool->cycle_counter / 40) % 2 == 0) {  // 0.8 second pulse
                led_strip_set_pixel(tool->led_strip, 0, 0, 255, 0);  // Green
            } else {
                led_strip_set_pixel(tool->led_strip, 0, 0, 128, 0);   // Dim green
            }
            led_strip_refresh(tool->led_strip);
            break;
            
        case RECIPE_BLUE_BLINK:
            // NTP_CALL → blue_blink
            if ((tool->cycle_counter / 30) % 2 == 0) {  // ~0.6 second blink
                led_strip_set_pixel(tool->led_strip, 0, 0, 0, 255);  // Blue
            } else {
                led_strip_set_pixel(tool->led_strip, 0, 0, 0, 0);    // Off
            }
            led_strip_refresh(tool->led_strip);
            break;
            
        case RECIPE_BLUE_STEADY:
            // NTP_SYNC → blue_steady
            led_strip_set_pixel(tool->led_strip, 0, 0, 0, 255);  // Blue
            led_strip_refresh(tool->led_strip);
            break;
            
        case RECIPE_AP_MODE_SEQUENCE:
            // AP_MODE → yellow_flash0.2 → blue_flash0.3 → purple_steady2.0
            {
                uint32_t sequence_pos = tool->cycle_counter % 300;  // 3 second total cycle
                if (sequence_pos < 20) {  // Yellow flash 0.2s
                    led_strip_set_pixel(tool->led_strip, 0, 255, 255, 0);  // Yellow
                } else if (sequence_pos < 50) {  // Blue flash 0.3s
                    led_strip_set_pixel(tool->led_strip, 0, 0, 0, 255);    // Blue
                } else {  // Purple steady 2.5s
                    led_strip_set_pixel(tool->led_strip, 0, 128, 0, 128);  // Purple
                }
                led_strip_refresh(tool->led_strip);
            }
            break;
            
        default:
            // Fallback to blue fade
            ESP_LOGW(TAG, "⚠️ Unknown recipe %d, using blue fade", recipe);
            led_strip_set_pixel(tool->led_strip, 0, 0, 0, 64);
            led_strip_refresh(tool->led_strip);
            break;
    }
}

// =============================================================================
// DEPRECATED: Old Queue Management (Will be removed)
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
    
    // Find insertion point (sorted by priority, then timestamp)
    uint8_t insert_pos = 0;
    for (uint8_t i = 0; i < tool->queue_count; i++) {
        if (tool->state_queue[i].priority < priority ||
            (tool->state_queue[i].priority == priority && 
             tool->state_queue[i].timestamp > new_entry.timestamp)) {
            insert_pos = i;
            break;
        }
        insert_pos = i + 1;
    }
    
    // Check if queue is full
    if (tool->queue_count >= FEEDBACK_QUEUE_SIZE) {
        // Remove lowest priority item
        if (insert_pos >= FEEDBACK_QUEUE_SIZE) {
            ESP_LOGW(TAG, "Queue full, dropping low priority state");
            xSemaphoreGive(tool->queue_mutex);
            return ESP_ERR_NO_MEM;
        }
        tool->queue_count = FEEDBACK_QUEUE_SIZE - 1;
    }
    
    // Shift elements to make room
    for (uint8_t i = tool->queue_count; i > insert_pos; i--) {
        tool->state_queue[i] = tool->state_queue[i - 1];
    }
    
    // Insert new entry
    tool->state_queue[insert_pos] = new_entry;
    tool->queue_count++;
    
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
    
    // Clean expired entries and find highest priority
    uint8_t write_idx = 0;
    for (uint8_t read_idx = 0; read_idx < tool->queue_count; read_idx++) {
        feedback_state_entry_t *entry = &tool->state_queue[read_idx];
        
        // Check if entry has expired
        bool expired = (entry->duration_ms > 0) && 
                      ((current_time - entry->timestamp) >= entry->duration_ms);
        
        if (!expired) {
            // Keep this entry and update highest priority
            if (write_idx != read_idx) {
                tool->state_queue[write_idx] = *entry;
            }
            
            if (entry->priority > highest_priority) {
                highest_priority = entry->priority;
                highest_state = entry->state;
            }
            
            write_idx++;
        }
    }
    
    tool->queue_count = write_idx;
    
    // Ensure we always have at least IDLE state
    if (tool->queue_count == 0) {
        tool->queue_count = 1;
        tool->state_queue[0] = (feedback_state_entry_t){
            .state = FEEDBACK_STATE_IDLE,
            .priority = FEEDBACK_PRIORITY_LOW,
            .timestamp = current_time,
            .duration_ms = 0,
            .source_tool = "system"
        };
        highest_state = FEEDBACK_STATE_IDLE;
    }
    
    xSemaphoreGive(tool->queue_mutex);
    
    return highest_state;
}

// =============================================================================
// State Mapping & LED Control (Enhanced from Original)
// =============================================================================

static feedback_priority_t get_state_default_priority(feedback_state_t state)
{
    // MCP-inspired state categorization by priority
    switch (state & 0xFF00) { // Check state category
        case 0x0000: // System core states
            switch (state) {
                case FEEDBACK_STATE_ERROR:
                case FEEDBACK_STATE_SHUTDOWN:
                    return FEEDBACK_PRIORITY_CRITICAL;
                case FEEDBACK_STATE_BOOTING:
                    return FEEDBACK_PRIORITY_HIGH;
                default:
                    return FEEDBACK_PRIORITY_LOW;
            }
            
        case 0x0100: // WiFi states
            switch (state) {
                case FEEDBACK_STATE_WIFI_AP_MODE:
                    return FEEDBACK_PRIORITY_HIGH; // AP mode should override connecting state
                default:
                    return FEEDBACK_PRIORITY_MEDIUM;
            }
            
        case 0x0200: // Time states
        case 0x0500: // Webserver states
            return FEEDBACK_PRIORITY_MEDIUM;
            
        case 0x0300: // RFID states
            switch (state) {
                case FEEDBACK_STATE_TAG_DETECTED:
                    return FEEDBACK_PRIORITY_HIGH;
                case FEEDBACK_STATE_TAG_IGNORED:
                    return FEEDBACK_PRIORITY_MEDIUM;  // Quick flash, medium priority
                case FEEDBACK_STATE_RFID_ERROR:
                case FEEDBACK_STATE_TAG_READ_ERROR:
                    return FEEDBACK_PRIORITY_CRITICAL;
                default:
                    return FEEDBACK_PRIORITY_MEDIUM;
            }
            
        case 0x0400: // Webhook states
            switch (state) {
                case FEEDBACK_STATE_WEBHOOK_ERROR:
                    return FEEDBACK_PRIORITY_HIGH;
                default:
                    return FEEDBACK_PRIORITY_MEDIUM;
            }
            
        case 0x1000: // Initialization states
            return FEEDBACK_PRIORITY_MEDIUM;
            
        case 0x2000: // Tool communication states
            return FEEDBACK_PRIORITY_HIGH;
            
        case 0x2100: // Flow awareness states
            switch (state) {
                case FEEDBACK_STATE_FLOW_AWARENESS:
                    return FEEDBACK_PRIORITY_MEDIUM;  // Flow awareness - medium priority
                case FEEDBACK_STATE_FLOW_URGENCY:
                    return FEEDBACK_PRIORITY_HIGH;    // Flow urgency - high priority
                default:
                    return FEEDBACK_PRIORITY_MEDIUM;
            }
            
        default:
            return FEEDBACK_PRIORITY_LOW;
    }
}

static rgb_color_t get_state_color(feedback_state_t state)
{
    switch (state) {
        // System Core States
        case FEEDBACK_STATE_BOOTING:
        case FEEDBACK_STATE_INIT_START:
        case FEEDBACK_STATE_INIT_COMPLETE:
            return COLOR_WHITE;
            
        case FEEDBACK_STATE_IDLE:
            return COLOR_BLUE;
            
        case FEEDBACK_STATE_ERROR:
        case FEEDBACK_STATE_RFID_ERROR:
        case FEEDBACK_STATE_TAG_READ_ERROR:
        case FEEDBACK_STATE_WEBHOOK_ERROR:
        case FEEDBACK_STATE_TOOL_ERROR:
            return COLOR_RED;
            
        // WiFi States
        case FEEDBACK_STATE_WIFI_CONNECTING:
            return COLOR_BLUE; // Blinking blue
            
        case FEEDBACK_STATE_WIFI_CONNECTED:
        case FEEDBACK_STATE_TIME_SYNCED:
        case FEEDBACK_STATE_WEBHOOK_SUCCESS:
            return COLOR_CYAN; // Flash cyan
            
        case FEEDBACK_STATE_WIFI_FAILED:
        case FEEDBACK_STATE_TIME_SYNC_FAILED:
            return COLOR_RED; // Red/orange sequence
            
        case FEEDBACK_STATE_WIFI_AP_MODE:
            return COLOR_YELLOW; // Yellow-blue-purple sequence
            
        // RFID States
        case FEEDBACK_STATE_TAG_DETECTED:
            return COLOR_GREEN; // Solid green
            
        case FEEDBACK_STATE_TAG_IGNORED:
            return COLOR_YELLOW; // Quick yellow flash per process map authority
            
        // Flow Awareness States (Process Map Authority: Constitutional Requirement)
        case FEEDBACK_STATE_FLOW_AWARENESS:
        case FEEDBACK_STATE_FLOW_URGENCY:
            return COLOR_ORANGE; // Orange breathing/pulsing for flow states
            
        case FEEDBACK_STATE_RFID_INITIALIZING:
        case FEEDBACK_STATE_RFID_ACTIVE:
            return COLOR_PURPLE;
            
        // Webhook States
        case FEEDBACK_STATE_WEBHOOK_SENDING:
        case FEEDBACK_STATE_WEBHOOK_QUEUED:
            return COLOR_YELLOW;
            
        // Initialization States
        case FEEDBACK_STATE_INIT_FS:
        case FEEDBACK_STATE_INIT_WIFI_PREP:
        case FEEDBACK_STATE_INIT_TIME:
        case FEEDBACK_STATE_INIT_WEBHOOK:
        case FEEDBACK_STATE_INIT_RFID:
            return COLOR_PURPLE;
            
        // Tool States
        case FEEDBACK_STATE_TOOL_REGISTERED:
            return COLOR_GREEN;
            
        case FEEDBACK_STATE_TOOL_DISCONNECTED:
            return COLOR_ORANGE;
            
        default:
            return COLOR_WHITE;
    }
}

static void update_led_display(struct feedback_tool *tool)
{
    rgb_color_t color = get_state_color(tool->current_state);
    rgb_color_t display_color = COLOR_OFF;
    
    // Debug color mapping every 5 seconds
    static uint32_t last_color_debug = 0;
    if (tool->cycle_counter % 100 == 0 && tool->cycle_counter != last_color_debug) {
        ESP_LOGI(TAG, "🎨 LED Color: state=%s, base_color=(%d,%d,%d)", 
                 feedback_tool_state_to_string(tool->current_state),
                 color.r, color.g, color.b);
        last_color_debug = tool->cycle_counter;
    }
    
    // Apply state-specific animation patterns
    switch (tool->current_state) {
        case FEEDBACK_STATE_IDLE: {
            // Breathing effect (4-second cycle)
            uint32_t breathing_period = tool->config.breathing_period_ms / tool->config.cleanup_interval_ms;
            float phase = (2.0 * M_PI * tool->cycle_counter) / breathing_period;
            float intensity = (sin(phase) + 1.0) / 2.0; // 0.0 to 1.0
            
            display_color.r = (uint8_t)(color.r * intensity * tool->config.max_brightness / 255);
            display_color.g = (uint8_t)(color.g * intensity * tool->config.max_brightness / 255);
            display_color.b = (uint8_t)(color.b * intensity * tool->config.max_brightness / 255);
            
            // Debug breathing every 2 seconds
            if (tool->cycle_counter % (breathing_period / 2) == 0) {
                ESP_LOGI(TAG, "🔵 LED Breathing: intensity=%.2f, RGB=(%d,%d,%d), cycle=%lu", 
                         intensity, display_color.r, display_color.g, display_color.b, tool->cycle_counter);
            }
            break;
        }
        
        case FEEDBACK_STATE_WIFI_CONNECTING: {
            // Fast blinking (500ms on/off)
            uint32_t blink_period = 1000 / tool->config.cleanup_interval_ms; // 1 second period
            bool on = (tool->cycle_counter % blink_period) < (blink_period / 2);
            
            if (on) {
                display_color.r = color.r * tool->config.max_brightness / 255;
                display_color.g = color.g * tool->config.max_brightness / 255;
                display_color.b = color.b * tool->config.max_brightness / 255;
            }
            break;
        }
        
        case FEEDBACK_STATE_TAG_DETECTED: {
            // Process Map Authority: Modify visuals based on flow awareness context
            if (tool->flow_urgency_active) {
                // 90-minute urgency: Orange pulsing (transition invitation)
                uint32_t pulse_period = 2000 / tool->config.cleanup_interval_ms; // 2-second cycle
                float cycle_position = fmod(tool->cycle_counter, pulse_period) / pulse_period;
                float intensity;
                
                if (cycle_position < 0.2) { // Quick rise (20% of cycle)
                    intensity = cycle_position / 0.2; // 0 to 1 quickly
                } else { // Slow fade (80% of cycle)
                    intensity = 1.0 - ((cycle_position - 0.2) / 0.8); // 1 to 0 slowly
                }
                
                intensity = 0.3 + (intensity * 0.7); // 0.3 to 1.0 range for visibility
                
                // Orange color (255, 165, 0) with pulsing intensity
                display_color.r = (uint8_t)(255 * intensity * tool->config.max_brightness / 255);
                display_color.g = (uint8_t)(165 * intensity * tool->config.max_brightness / 255);
                display_color.b = 0;
                
            } else if (tool->flow_awareness_active) {
                // 60-minute awareness: Orange breathing (4:7:8 ratio)
                uint32_t breathing_period = 5000 / tool->config.cleanup_interval_ms; // 5-second cycle
                float cycle_position = fmod(tool->cycle_counter, breathing_period) / breathing_period;
                float intensity;
                
                if (cycle_position < 0.21) { // 4/19 = inhale phase
                    intensity = cycle_position / 0.21; // 0 to 1
                } else if (cycle_position < 0.58) { // 7/19 = hold phase  
                    intensity = 1.0; // sustained
                } else { // 8/19 = exhale phase
                    intensity = 1.0 - ((cycle_position - 0.58) / 0.42); // 1 to 0
                }
                
                // Orange color (255, 165, 0) with breathing intensity
                display_color.r = (uint8_t)(255 * intensity * tool->config.max_brightness / 255);
                display_color.g = (uint8_t)(165 * intensity * tool->config.max_brightness / 255);
                display_color.b = 0;
                
            } else {
                // Normal: Solid green color
                display_color.r = color.r * tool->config.max_brightness / 255;
                display_color.g = color.g * tool->config.max_brightness / 255;
                display_color.b = color.b * tool->config.max_brightness / 255;
            }
            break;
        }
        
        case FEEDBACK_STATE_TAG_IGNORED: {
            // Quick yellow flash (process map authority: "Quick yellow flash")
            uint32_t flash_period = 500 / tool->config.cleanup_interval_ms; // 500ms total flash
            uint32_t phase = tool->cycle_counter % flash_period;
            uint32_t on_phase = 250 / tool->config.cleanup_interval_ms;     // 250ms on
            
            if (phase < on_phase) {
                // Flash on - yellow
                display_color.r = color.r * tool->config.max_brightness / 255;
                display_color.g = color.g * tool->config.max_brightness / 255;
                display_color.b = color.b * tool->config.max_brightness / 255;
            } else {
                // Flash off - dim
                display_color.r = 0;
                display_color.g = 0;
                display_color.b = 0;
            }
            break;
        }
        
        case FEEDBACK_STATE_WIFI_AP_MODE: {
            // Yellow -> Blue -> Purple sequence (0.3s, 0.3s, 2.0s)
            uint32_t sequence_period = 2600 / tool->config.cleanup_interval_ms; // 2.6 second cycle
            uint32_t phase = tool->cycle_counter % sequence_period;
            uint32_t yellow_phase = 300 / tool->config.cleanup_interval_ms;
            uint32_t blue_phase = yellow_phase + (300 / tool->config.cleanup_interval_ms);
            
            if (phase < yellow_phase) {
                display_color = COLOR_YELLOW;
            } else if (phase < blue_phase) {
                display_color = COLOR_BLUE;
            } else {
                display_color = COLOR_PURPLE;
            }
            
            display_color.r = display_color.r * tool->config.max_brightness / 255;
            display_color.g = display_color.g * tool->config.max_brightness / 255;
            display_color.b = display_color.b * tool->config.max_brightness / 255;
            break;
        }
        
        case FEEDBACK_STATE_FLOW_AWARENESS: {
            // Orange breathing (5-second cycle, 4:7:8 ratio per Process Map Authority)
            // Constitutional requirement: Visual flow awareness at 60-minute sessions
            uint32_t breathing_period = 5000 / tool->config.cleanup_interval_ms; // 5-second cycle
            
            // 4:7:8 breathing ratio (inhale:hold:exhale = 4:7:8)
            float cycle_position = fmod(tool->cycle_counter, breathing_period) / breathing_period;
            float intensity;
            
            if (cycle_position < 0.21) { // 4/19 = inhale phase
                intensity = cycle_position / 0.21; // 0 to 1
            } else if (cycle_position < 0.58) { // 7/19 = hold phase  
                intensity = 1.0; // sustained
            } else { // 8/19 = exhale phase
                intensity = 1.0 - ((cycle_position - 0.58) / 0.42); // 1 to 0
            }
            
            // Orange color (255, 165, 0) with breathing intensity
            display_color.r = (uint8_t)(255 * intensity * tool->config.max_brightness / 255);
            display_color.g = (uint8_t)(165 * intensity * tool->config.max_brightness / 255);
            display_color.b = 0;
            
            // Debug breathing every 2.5 seconds
            if (tool->cycle_counter % (breathing_period / 2) == 0) {
                ESP_LOGI(TAG, "🟠 Flow Awareness Breathing: intensity=%.2f, RGB=(%d,%d,%d), cycle=%lu", 
                         intensity, display_color.r, display_color.g, display_color.b, tool->cycle_counter);
            }
            break;
        }
        
        case FEEDBACK_STATE_FLOW_URGENCY: {
            // Orange pulsing (2-second cycle, transition invitation per Process Map Authority)
            // Constitutional requirement: Visual flow urgency at 90-minute sessions
            uint32_t pulse_period = 2000 / tool->config.cleanup_interval_ms; // 2-second cycle
            
            // Sharp pulse: quick rise, slow fade (urgency pattern)
            float cycle_position = fmod(tool->cycle_counter, pulse_period) / pulse_period;
            float intensity;
            
            if (cycle_position < 0.2) { // Quick rise (20% of cycle)
                intensity = cycle_position / 0.2; // 0 to 1 quickly
            } else { // Slow fade (80% of cycle)
                intensity = 1.0 - ((cycle_position - 0.2) / 0.8); // 1 to 0 slowly
            }
            
            // Ensure minimum visibility for urgency
            intensity = 0.3 + (intensity * 0.7); // 0.3 to 1.0 range
            
            // Orange color (255, 165, 0) with pulsing intensity
            display_color.r = (uint8_t)(255 * intensity * tool->config.max_brightness / 255);
            display_color.g = (uint8_t)(165 * intensity * tool->config.max_brightness / 255);
            display_color.b = 0;
            
            // Debug pulsing every second
            if (tool->cycle_counter % (pulse_period / 2) == 0) {
                ESP_LOGI(TAG, "🟠 Flow Urgency Pulsing: intensity=%.2f, RGB=(%d,%d,%d), cycle=%lu", 
                         intensity, display_color.r, display_color.g, display_color.b, tool->cycle_counter);
            }
            break;
        }
        
        default: {
            // Default: solid color or temporary flash
            display_color.r = color.r * tool->config.max_brightness / 255;
            display_color.g = color.g * tool->config.max_brightness / 255;
            display_color.b = color.b * tool->config.max_brightness / 255;
            break;
        }
    }
    
    // Update LED strip
    esp_err_t ret = led_strip_set_pixel(tool->led_strip, 0, display_color.r, display_color.g, display_color.b);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED set_pixel failed: %s", esp_err_to_name(ret));
    }
    
    ret = led_strip_refresh(tool->led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED refresh failed: %s", esp_err_to_name(ret));
    }
    
    // Debug LED hardware calls every 4 seconds for IDLE state
    if (tool->current_state == FEEDBACK_STATE_IDLE && tool->cycle_counter % 80 == 0) {
        ESP_LOGI(TAG, "💡 LED Hardware: GPIO=%d, RGB=(%d,%d,%d), strip=%p", 
                 tool->config.led_gpio, display_color.r, display_color.g, display_color.b, tool->led_strip);
    }
}

// =============================================================================
// Utility Functions Implementation
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
        // System Core States
        case FEEDBACK_STATE_BOOTING: return "BOOTING";
        case FEEDBACK_STATE_IDLE: return "IDLE";
        case FEEDBACK_STATE_ERROR: return "ERROR";
        case FEEDBACK_STATE_SHUTDOWN: return "SHUTDOWN";
        
        // WiFi Tool States (Blue family)
        case FEEDBACK_STATE_WIFI_CONNECTING: return "WIFI_CONNECTING";
        case FEEDBACK_STATE_WIFI_CONNECTED: return "WIFI_CONNECTED";
        case FEEDBACK_STATE_WIFI_FAILED: return "WIFI_FAILED";
        case FEEDBACK_STATE_WIFI_AP_MODE: return "WIFI_AP_MODE";
        
        // RFID Tool States (Green family)
        case FEEDBACK_STATE_RFID_INITIALIZING: return "RFID_INITIALIZING";
        case FEEDBACK_STATE_RFID_ACTIVE: return "RFID_ACTIVE";
        case FEEDBACK_STATE_RFID_ERROR: return "RFID_ERROR";
        case FEEDBACK_STATE_TAG_DETECTED: return "TAG_DETECTED";
        case FEEDBACK_STATE_TAG_IGNORED: return "TAG_IGNORED";
        case FEEDBACK_STATE_TAG_READ_ERROR: return "TAG_READ_ERROR";
        
        // Flow Awareness States (Orange family - Process Map Authority)
        case FEEDBACK_STATE_FLOW_AWARENESS: return "FLOW_AWARENESS";
        case FEEDBACK_STATE_FLOW_URGENCY: return "FLOW_URGENCY";
        
        // Webhook Tool States (Yellow/Green family)
        case FEEDBACK_STATE_WEBHOOK_SENDING: return "WEBHOOK_SENDING";
        case FEEDBACK_STATE_WEBHOOK_SUCCESS: return "WEBHOOK_SUCCESS";
        case FEEDBACK_STATE_WEBHOOK_ERROR: return "WEBHOOK_ERROR";
        case FEEDBACK_STATE_WEBHOOK_QUEUED: return "WEBHOOK_QUEUED";
        
        // Time Tool States
        case FEEDBACK_STATE_TIME_SYNCING: return "TIME_SYNCING";
        case FEEDBACK_STATE_TIME_SYNCED: return "TIME_SYNCED";
        case FEEDBACK_STATE_TIME_SYNC_FAILED: return "TIME_SYNC_FAILED";
        
        // Initialization States (Purple family)
        case FEEDBACK_STATE_INIT_START: return "INIT_START";
        case FEEDBACK_STATE_INIT_FS: return "INIT_FS";
        case FEEDBACK_STATE_INIT_WIFI_PREP: return "INIT_WIFI_PREP";
        case FEEDBACK_STATE_INIT_TIME: return "INIT_TIME";
        case FEEDBACK_STATE_INIT_WEBHOOK: return "INIT_WEBHOOK";
        case FEEDBACK_STATE_INIT_RFID: return "INIT_RFID";
        case FEEDBACK_STATE_INIT_COMPLETE: return "INIT_COMPLETE";
        
        // Tool Communication States
        case FEEDBACK_STATE_TOOL_REGISTERED: return "TOOL_REGISTERED";
        case FEEDBACK_STATE_TOOL_ERROR: return "TOOL_ERROR";
        case FEEDBACK_STATE_TOOL_DISCONNECTED: return "TOOL_DISCONNECTED";
        
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
        .description = FEEDBACK_TOOL_DESCRIPTION,
        .capabilities = FEEDBACK_CAP_LED_CONTROL | 
                       FEEDBACK_CAP_FSM_EXECUTION |
                       FEEDBACK_CAP_RECIPE_LOOKUP |
                       FEEDBACK_CAP_THREAD_SAFE,
        .init_func = feedback_tool_init,
        .deinit_func = feedback_tool_deinit
    };
    
    return &registry_entry;
}

// =============================================================================
// Dashboard & Status Aggregation Implementation (Phase 4.3)
// =============================================================================

static void generate_ascii_dashboard(struct feedback_tool *tool, char* buffer, size_t buffer_size)
{
    // Get current time for uptime calculation
    uint32_t uptime_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - tool->uptime_start;
    uint32_t uptime_min = uptime_ms / 60000;
    
    // Get current state information
    const char* state_str = feedback_tool_state_to_string(tool->current_state);
    const char* priority_str = feedback_tool_priority_to_string(tool->current_priority);
    
    // Debug dashboard generation
    ESP_LOGI(TAG, "📊 Dashboard Generated: state=%s, queue=%d, uptime=%lum", 
             state_str, tool->queue_count, uptime_min);
    
    // Generate compact ASCII dashboard (fits in 512 bytes)
    snprintf(buffer, buffer_size,
        "╔══════════════════════════════╗\n"
        "║     RFID TIME TRACKER        ║\n"
        "╚══════════════════════════════╝\n"
        " 🔄 System: [%s]\n"
        " 💡 LED: [%s] Q:%d\n"
        " ⏱️  Up: %lum | %s\n"
        " 🎯 State: %s\n"
        " [%s%s%s%s%s] %s\n",
        tool->system_operational ? "OK" : "ERR",
        state_str,
        tool->queue_count,
        uptime_min,
        priority_str,
        state_str,
        // Compact progress bar (5 chars)
        tool->system_operational ? "██" : "░░",
        tool->is_active ? "██" : "░░",
        tool->queue_count > 0 ? "██" : "░░",
        tool->current_state != FEEDBACK_STATE_ERROR ? "██" : "░░",
        tool->is_initialized ? "██" : "░░",
        tool->system_operational ? "Ready" : "Error"
    );
}

static void generate_json_status(struct feedback_tool *tool, char* buffer, size_t buffer_size)
{
    uint32_t uptime_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - tool->uptime_start;
    
    cJSON *json = cJSON_CreateObject();
    cJSON *status = cJSON_CreateObject();
    cJSON *led = cJSON_CreateObject();
    cJSON *queue = cJSON_CreateObject();
    
    // System status
    cJSON_AddBoolToObject(status, "operational", tool->system_operational);
    cJSON_AddBoolToObject(status, "initialized", tool->is_initialized);
    cJSON_AddBoolToObject(status, "active", tool->is_active);
    cJSON_AddNumberToObject(status, "uptime_ms", uptime_ms);
    
    // LED status
    cJSON_AddStringToObject(led, "current_state", feedback_tool_state_to_string(tool->current_state));
    cJSON_AddStringToObject(led, "priority", feedback_tool_priority_to_string(tool->current_priority));
    cJSON_AddNumberToObject(led, "cycle_counter", tool->cycle_counter);
    
    // Queue status
    cJSON_AddNumberToObject(queue, "count", tool->queue_count);
    cJSON_AddNumberToObject(queue, "max_size", FEEDBACK_QUEUE_SIZE);
    
    // Add to main JSON
    cJSON_AddItemToObject(json, "system", status);
    cJSON_AddItemToObject(json, "led", led);
    cJSON_AddItemToObject(json, "queue", queue);
    cJSON_AddNumberToObject(json, "timestamp", (uint32_t)time(NULL));
    
    char *json_string = cJSON_Print(json);
    if (json_string) {
        snprintf(buffer, buffer_size, "%s", json_string);
        free(json_string);
    }
    
    cJSON_Delete(json);
}

esp_err_t feedback_tool_generate_dashboard(feedback_tool_handle_t handle, feedback_dashboard_t* dashboard)
{
    if (!handle || !dashboard) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    if (!tool->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Generate ASCII dashboard
    generate_ascii_dashboard(tool, dashboard->ascii_dashboard, sizeof(dashboard->ascii_dashboard));
    
    // Generate JSON status
    generate_json_status(tool, dashboard->json_status, sizeof(dashboard->json_status));
    
    // Update metadata
    dashboard->timestamp = (uint32_t)time(NULL);
    dashboard->is_operational = tool->system_operational;
    
    // Cache the dashboard
    snprintf(tool->last_dashboard, sizeof(tool->last_dashboard), "%s", dashboard->ascii_dashboard);
    snprintf(tool->last_json_status, sizeof(tool->last_json_status), "%s", dashboard->json_status);
    tool->last_dashboard_time = dashboard->timestamp;
    
    return ESP_OK;
}

esp_err_t feedback_tool_get_status_json(feedback_tool_handle_t handle, char* json_buffer, size_t buffer_size)
{
    if (!handle || !json_buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    if (!tool->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    generate_json_status(tool, json_buffer, buffer_size);
    return ESP_OK;
}

esp_err_t feedback_tool_subscribe_to_all_events(feedback_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // For Phase 4.3, we focus on the dashboard generation
    // Event subscription for status aggregation can be added in Phase 4.4
    ESP_LOGI(TAG, "Dashboard functionality enabled - event subscription ready for Phase 4.4");
    
    return ESP_OK;
}

// =============================================================================
// Flow Awareness Context (Process Map Authority: Constitutional Requirement)
// =============================================================================

esp_err_t feedback_tool_set_flow_context(feedback_tool_handle_t handle, 
                                        bool flow_active, 
                                        bool flow_urgent)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    if (!tool->is_initialized) {
        ESP_LOGW(TAG, "Tool not initialized, ignoring flow context update");
        return ESP_ERR_INVALID_STATE;
    }
    
    tool->flow_awareness_active = flow_active;
    tool->flow_urgency_active = flow_urgent;
    
    ESP_LOGI(TAG, "🟠 Flow context updated: awareness=%s, urgency=%s", 
             flow_active ? "active" : "inactive",
             flow_urgent ? "active" : "inactive");
    
    return ESP_OK;
}

// =============================================================================
// Event-Driven Architecture Implementation (Phase 6.0)
// =============================================================================

/**
 * @brief Event handler for session events (flow awareness)
 */
static void feedback_session_event_handler(void* handler_args, esp_event_base_t base,
                                                int32_t id, void* event_data)
{
    struct feedback_tool *tool = (struct feedback_tool*)handler_args;
    
    if (base == SESSION_EVENTS) {
        session_event_data_t* session_data = (session_event_data_t*)event_data;
        
        switch (id) {
            case SESSION_EVENT_STARTED:
                ESP_LOGI(TAG, "📡 Session started event received: tag=%s", session_data->tag_uid);
                
                // Clear flow context for fresh session and show tag detected state
                tool->flow_awareness_active = session_data->flow_awareness_active;  // Should be false
                tool->flow_urgency_active = session_data->flow_urgency_active;      // Should be false
                
                // Set tag detected visual state (will use cleared flow context)
                feedback_tool_set_state_simple((feedback_tool_handle_t)tool, FEEDBACK_STATE_TAG_DETECTED);
                
                ESP_LOGI(TAG, "✅ Fresh session visual feedback set (green, no flow context)");
                break;
                
            case SESSION_EVENT_ENDED:
                ESP_LOGI(TAG, "📡 Session ended event received: tag=%s, duration=%llu ms", 
                         session_data->tag_uid, (unsigned long long)session_data->session_duration_ms);
                
                // Clear flow context and return to idle
                tool->flow_awareness_active = false;
                tool->flow_urgency_active = false;
                
                feedback_tool_set_state_simple((feedback_tool_handle_t)tool, FEEDBACK_STATE_IDLE);
                
                ESP_LOGI(TAG, "✅ Session ended visual feedback set (idle, flow context cleared)");
                break;
                
            case SESSION_EVENT_FLOW_AWARENESS:
                ESP_LOGI(TAG, "📡 Flow awareness event received: %lu minutes", (unsigned long)session_data->session_duration_min);
                
                // Update flow context for orange breathing
                tool->flow_awareness_active = session_data->flow_awareness_active;
                tool->flow_urgency_active = session_data->flow_urgency_active;
                
                // Ensure visual update if tag is currently detected
                if (tool->current_state == FEEDBACK_STATE_TAG_DETECTED) {
                    ESP_LOGI(TAG, "🟠 Tag detected during flow awareness - updating to orange breathing");
                    // State will update on next cycle with new flow context
                }
                
                ESP_LOGI(TAG, "✅ Flow awareness context updated (orange breathing active)");
                break;
                
            case SESSION_EVENT_FLOW_URGENCY:
                ESP_LOGI(TAG, "📡 Flow urgency event received: %lu minutes", (unsigned long)session_data->session_duration_min);
                
                // Update flow context for orange pulsing
                tool->flow_awareness_active = session_data->flow_awareness_active;
                tool->flow_urgency_active = session_data->flow_urgency_active;
                
                // Ensure visual update if tag is currently detected
                if (tool->current_state == FEEDBACK_STATE_TAG_DETECTED) {
                    ESP_LOGI(TAG, "🔥 Tag detected during flow urgency - updating to orange pulsing");
                    // State will update on next cycle with new flow context
                }
                
                ESP_LOGI(TAG, "✅ Flow urgency context updated (orange pulsing active)");
                break;
                
            default:
                break;
        }
    }
}

/**
 * @brief Event handler for RFID events (visual state changes)
 */
static void feedback_rfid_event_handler(void* handler_args, esp_event_base_t base,
                                             int32_t id, void* event_data)
{
    // RAW EVENT DEBUGGING - Log EVERY event that reaches this handler
    ESP_LOGI(TAG, "🔥 FEEDBACK HANDLER ENTRY: base=%s, id=%ld, data=%p", 
             base ? (const char*)base : "NULL", id, event_data);
    
    struct feedback_tool *tool = (struct feedback_tool*)handler_args;
    
    if (base == RFID_EVENTS) {
        switch (id) {
            case RFID_EVENT_TAG_DETECTED:
                ESP_LOGI(TAG, "🏷️ RFID tag detected - setting green state");
                
                // Set solid green for tag detection (highest priority)
                feedback_tool_set_state((feedback_tool_handle_t)tool, FEEDBACK_STATE_TAG_DETECTED, 
                                       FEEDBACK_PRIORITY_HIGH, 0);
                
                ESP_LOGI(TAG, "✅ Tag detected visual feedback set (green solid)");
                break;
                
            case RFID_EVENT_TAG_REMOVED:
                ESP_LOGI(TAG, "📤 RFID tag removed - returning to idle");
                
                // Clear tag detected state and return to idle
                feedback_tool_clear_state((feedback_tool_handle_t)tool, FEEDBACK_STATE_TAG_DETECTED);
                feedback_tool_set_state_simple((feedback_tool_handle_t)tool, FEEDBACK_STATE_IDLE);
                
                ESP_LOGI(TAG, "✅ Tag removed visual feedback set (blue breathing)");
                break;
                
            case RFID_EVENT_TAG_IGNORED:
                ESP_LOGI(TAG, "📡 RFID ignored event received");
                
                // Flash yellow for ignored tags
                feedback_tool_flash_event((feedback_tool_handle_t)tool, FEEDBACK_STATE_TAG_IGNORED, 1, 500);
                
                ESP_LOGI(TAG, "✅ Tag ignored visual feedback set (yellow flash)");
                break;
                
            default:
                ESP_LOGW(TAG, "❓ Unhandled RFID event in feedback tool: event_id=%ld", id);
                break;
        }
    }
}

esp_err_t feedback_tool_start_event_subscription(feedback_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_tool *tool = (struct feedback_tool*)handle;
    
    // Subscribe to session events for flow awareness
    esp_err_t ret = subscribe_to_session_events(feedback_session_event_handler, tool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to subscribe to session events: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Subscribe to RFID events for ignored tags
    ret = subscribe_to_rfid_events(feedback_rfid_event_handler, tool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to subscribe to RFID events: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Feedback tool subscribed to session and RFID events");
    return ESP_OK;
}

esp_err_t feedback_tool_stop_event_subscription(feedback_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Unregister event handlers
    esp_err_t ret1 = esp_event_handler_unregister(SESSION_EVENTS, ESP_EVENT_ANY_ID, 
                                                   feedback_session_event_handler);
    esp_err_t ret2 = esp_event_handler_unregister(RFID_EVENTS, ESP_EVENT_ANY_ID, 
                                                   feedback_rfid_event_handler);
    
    if (ret1 != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unregister session event handler: %s", esp_err_to_name(ret1));
    }
    if (ret2 != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unregister RFID event handler: %s", esp_err_to_name(ret2));
    }
    
    ESP_LOGI(TAG, "✅ Feedback tool event subscription stopped");
    return ESP_OK;
}
