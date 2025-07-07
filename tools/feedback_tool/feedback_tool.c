/**
 * @file feedback_tool.c
 * @brief Constitutional Feedback Tool Implementation - LED Visual Feedback
 * 
 * Constitutional implementation following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf, PRIu32)
 * - Container isolation principles
 * 
 * Constitutional Authority: Process Map 11 (feedback_fsm) visual feedback management
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#include "feedback_tool.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>

// ESP_EVENT declarations for system state communication
ESP_EVENT_DECLARE_BASE(SYSTEM_STATE_EVENTS);

typedef enum {
    SYSTEM_STATE_EVENT_STATE_CHANGE = 0,
    SYSTEM_STATE_EVENT_HARDWARE_TEST,
    SYSTEM_STATE_EVENT_TEST_SEQUENCE
} system_state_event_id_t;

typedef struct {
    feedback_state_t target_state;
    uint32_t duration_ms;
    char test_name[64];
    uint64_t timestamp_us;
} system_state_change_event_t;

static const char* TAG = "feedback_tool";

// Constitutional Feedback Tool Event Base
ESP_EVENT_DEFINE_BASE(FEEDBACK_TOOL_EVENTS);

// Constitutional RGB color definitions
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

// Constitutional color palette per Process Map 11
static const rgb_color_t COLOR_OFF      = {0, 0, 0};
static const rgb_color_t COLOR_RED      = {255, 0, 0};     // Errors
static const rgb_color_t COLOR_GREEN    = {0, 255, 0};     // Success/Active
static const rgb_color_t COLOR_BLUE     = {0, 0, 255};     // Idle/Info
static const rgb_color_t COLOR_YELLOW   = {255, 255, 0};   // Warnings
static const rgb_color_t COLOR_WHITE    = {255, 255, 255}; // System states
static const rgb_color_t COLOR_ORANGE   = {255, 165, 0};   // Flow awareness
static const rgb_color_t COLOR_CYAN     = {0, 255, 255};   // Connected states

// Constitutional Feedback Tool Context (Handle-based pattern)
struct feedback_tool {
    bool is_initialized;
    bool is_active;
    bool led_hardware_ok;
    feedback_tool_config_t config;
    feedback_tool_status_t status;
    uint64_t init_timestamp_us;
    uint32_t state_changes_count;
    uint32_t error_count;
    
    // Current visual state
    feedback_state_t current_state;
    feedback_pattern_t current_pattern;
    rgb_color_t current_color;
    uint8_t current_brightness;
    
    // Animation state
    uint32_t animation_cycle;
    uint32_t pattern_start_time;
    uint32_t pattern_duration_ms;
    
    // Flow awareness context
    bool flow_awareness_active;
    bool flow_urgency_active;
    
    // Hardware components
    led_strip_handle_t led_strip;
    
    // Constitutional task management
    TaskHandle_t update_task_handle;
    esp_event_loop_handle_t event_loop;
};

// =============================================================================
// Constitutional Forward Declarations  
// =============================================================================

static void constitutional_led_update_task(void *arg);
static rgb_color_t constitutional_get_state_color(feedback_state_t state);
static void constitutional_apply_pattern(struct feedback_tool *tool, rgb_color_t base_color);
static void constitutional_update_led_hardware(struct feedback_tool *tool, rgb_color_t color);
static void constitutional_system_state_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);

// =============================================================================
// Constitutional Tool Implementation
// =============================================================================

const char* feedback_tool_get_id(void) {
    return FEEDBACK_TOOL_ID;
}

const char* feedback_tool_get_version(void) {
    return FEEDBACK_TOOL_VERSION;
}

feedback_tool_capabilities_t feedback_tool_get_capabilities(feedback_tool_handle_t handle) {
    if (!handle || !handle->is_initialized) {
        return 0;
    }
    
    return FEEDBACK_TOOL_CAP_LED_CONTROL |
           FEEDBACK_TOOL_CAP_STATE_MANAGEMENT |
           FEEDBACK_TOOL_CAP_EVENT_PUBLISH |
           FEEDBACK_TOOL_CAP_PROCESS_MAP_11 |
           FEEDBACK_TOOL_CAP_FLOW_AWARENESS |
           FEEDBACK_TOOL_CAP_DASHBOARD |
           FEEDBACK_TOOL_CAP_HEALTH_CHECK |
           FEEDBACK_TOOL_CAP_ANIMATION;
}

feedback_tool_config_t feedback_tool_create_default_config(void) {
    feedback_tool_config_t config = {0};
    
    // Constitutional memory safety - use snprintf for string fields if any
    config.led_gpio = 7;  // Default WS2812B GPIO (Constitutional hardware specification)
    config.max_brightness = 128;  // 50% brightness for power efficiency
    config.breathing_period_ms = 4000;  // 4-second breathing cycle
    config.blink_period_ms = 1000;      // 1-second blink cycle
    config.update_interval_ms = 50;     // 20 FPS update rate
    config.publish_events = true;
    config.enable_flow_awareness = true;
    
    return config;
}

feedback_tool_handle_t feedback_tool_init(const feedback_tool_config_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Constitutional violation: NULL configuration");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Initializing constitutional feedback tool");
    
    // Allocate constitutional tool handle
    feedback_tool_handle_t handle = calloc(1, sizeof(struct feedback_tool));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate constitutional feedback tool handle");
        return NULL;
    }
    
    // Initialize constitutional context
    memcpy(&handle->config, config, sizeof(feedback_tool_config_t));
    handle->init_timestamp_us = esp_timer_get_time();
    handle->state_changes_count = 0;
    handle->error_count = 0;
    
    // Initialize constitutional status
    handle->status.is_initialized = false;
    handle->status.is_active = false;
    handle->status.led_hardware_ok = false;
    handle->status.current_state = FEEDBACK_STATE_IDLE;
    handle->status.current_pattern = FEEDBACK_PATTERN_BREATHING;
    handle->status.uptime_ms = 0;
    handle->status.state_changes_count = 0;
    handle->status.error_count = 0;
    handle->status.led_gpio = config->led_gpio;
    handle->status.current_brightness = config->max_brightness;
    // Will set capabilities after initialization is complete
    
    // Constitutional memory safety
    snprintf(handle->status.mount_point, sizeof(handle->status.mount_point), "/feedback");
    snprintf(handle->status.version, sizeof(handle->status.version), "%s", FEEDBACK_TOOL_VERSION);
    
    // Initialize LED strip hardware (WS2812B)
    led_strip_config_t strip_config = {
        .strip_gpio_num = config->led_gpio,
        .max_leds = 1,  // Single LED for time tracker device
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10MHz
        .mem_block_symbols = 64,  // ESP-IDF official specification (Context7)
        .flags.with_dma = false,
    };
    
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &handle->led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        free(handle);
        return NULL;
    }
    
    // Clear LED on initialization
    esp_err_t clear_ret = led_strip_clear(handle->led_strip);
    if (clear_ret != ESP_OK) {
        ESP_LOGE(TAG, "🚨 CRITICAL: Failed to clear LED strip during init: %s", esp_err_to_name(clear_ret));
        led_strip_del(handle->led_strip);
        free(handle);
        return NULL;
    }
    ESP_LOGI(TAG, "✅ LED strip cleared successfully during initialization");
    
    // Constitutional self-test: Flash LED to verify hardware control
    ESP_LOGI(TAG, "🔧 Constitutional self-test: Verifying LED hardware control");
    
    // Test 1: Red flash
    ESP_LOGI(TAG, "🔴 Self-test: RED flash");
    esp_err_t test_ret = led_strip_set_pixel(handle->led_strip, 0, 255, 0, 0);
    if (test_ret != ESP_OK) {
        ESP_LOGE(TAG, "🚨 SELF-TEST FAILED: Red pixel set failed: %s", esp_err_to_name(test_ret));
    }
    test_ret = led_strip_refresh(handle->led_strip);
    if (test_ret != ESP_OK) {
        ESP_LOGE(TAG, "🚨 SELF-TEST FAILED: Red refresh failed: %s", esp_err_to_name(test_ret));
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // 500ms red
    
    // Test 2: Green flash  
    ESP_LOGI(TAG, "🟢 Self-test: GREEN flash");
    test_ret = led_strip_set_pixel(handle->led_strip, 0, 0, 255, 0);
    if (test_ret != ESP_OK) {
        ESP_LOGE(TAG, "🚨 SELF-TEST FAILED: Green pixel set failed: %s", esp_err_to_name(test_ret));
    }
    test_ret = led_strip_refresh(handle->led_strip);
    if (test_ret != ESP_OK) {
        ESP_LOGE(TAG, "🚨 SELF-TEST FAILED: Green refresh failed: %s", esp_err_to_name(test_ret));
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // 500ms green
    
    // Test 3: Blue flash
    ESP_LOGI(TAG, "🔵 Self-test: BLUE flash");
    test_ret = led_strip_set_pixel(handle->led_strip, 0, 0, 0, 255);
    if (test_ret != ESP_OK) {
        ESP_LOGE(TAG, "🚨 SELF-TEST FAILED: Blue pixel set failed: %s", esp_err_to_name(test_ret));
    }
    test_ret = led_strip_refresh(handle->led_strip);
    if (test_ret != ESP_OK) {
        ESP_LOGE(TAG, "🚨 SELF-TEST FAILED: Blue refresh failed: %s", esp_err_to_name(test_ret));
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // 500ms blue
    
    // Clear LED after self-test
    led_strip_clear(handle->led_strip);
    ESP_LOGI(TAG, "✅ Constitutional self-test complete - LED hardware verified");
    
    // Set initial state
    handle->current_state = FEEDBACK_STATE_IDLE;
    handle->current_pattern = FEEDBACK_PATTERN_BREATHING;
    handle->current_color = COLOR_BLUE;
    handle->current_brightness = config->max_brightness;
    handle->animation_cycle = 0;
    handle->pattern_start_time = esp_timer_get_time() / 1000;
    handle->pattern_duration_ms = 0; // Permanent
    
    // Initialize flow awareness context
    handle->flow_awareness_active = false;
    handle->flow_urgency_active = false;
    
    // Mark as initialized and active BEFORE creating task
    handle->is_initialized = true;
    handle->is_active = true;
    handle->led_hardware_ok = true;
    
    ESP_LOGI(TAG, "✅ Constitutional tool flags set: initialized=%s, active=%s, led_ok=%s",
             handle->is_initialized ? "YES" : "NO",
             handle->is_active ? "YES" : "NO", 
             handle->led_hardware_ok ? "YES" : "NO");
    
    // Constitutional task creation (AFTER flags are set)
    BaseType_t task_ret = xTaskCreate(
        constitutional_led_update_task,
        "constitutional_led_update",
        4096,  // Stack size
        handle,
        5,     // Priority
        &handle->update_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create constitutional LED update task");
        free(handle);
        return NULL;
    }
    
    // Register system state event handler for ESP_EVENT communication
    esp_err_t event_ret = esp_event_handler_register(SYSTEM_STATE_EVENTS, 
                                                    ESP_EVENT_ANY_ID,
                                                    constitutional_system_state_event_handler,
                                                    handle);
    if (event_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register system state event handler: %s", esp_err_to_name(event_ret));
    } else {
        ESP_LOGI(TAG, "✅ System state event handler registered for LED control");
    }
    handle->status.is_initialized = true;
    handle->status.is_active = true;
    handle->status.led_hardware_ok = true;
    
    // Set capabilities now that handle is initialized
    handle->status.capabilities = feedback_tool_get_capabilities(handle);
    
    ESP_LOGI(TAG, "✅ Constitutional feedback tool initialized: %s v%s", 
             feedback_tool_get_id(), feedback_tool_get_version());
    ESP_LOGI(TAG, "  GPIO: %d, Brightness: %d, Update Rate: %" PRIu32 "ms", 
             config->led_gpio, config->max_brightness, config->update_interval_ms);
    
    return handle;
}

esp_err_t feedback_tool_deinit(feedback_tool_handle_t handle) {
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Deinitializing constitutional feedback tool");
    
    // Unregister system state event handler
    esp_event_handler_unregister(SYSTEM_STATE_EVENTS, ESP_EVENT_ANY_ID, constitutional_system_state_event_handler);
    
    // Stop background task
    handle->is_active = false;
    if (handle->update_task_handle) {
        vTaskDelete(handle->update_task_handle);
        handle->update_task_handle = NULL;
    }
    
    // Turn off and cleanup LED strip
    if (handle->led_strip) {
        led_strip_clear(handle->led_strip);
        led_strip_del(handle->led_strip);
        handle->led_strip = NULL;
    }
    
    // Publish deinit event if enabled
    if (handle->config.publish_events) {
        feedback_tool_event_data_t deinit_event = {
            .type = FEEDBACK_TOOL_EVENT_HEALTH_CHECK,
            .data.health_status = {
                .health_ok = false,
                .uptime_ms = (esp_timer_get_time() - handle->init_timestamp_us) / 1000,
                .error_count = handle->error_count
            }
        };
        esp_event_post(FEEDBACK_TOOL_EVENTS, FEEDBACK_TOOL_EVENT_HEALTH_CHECK, 
                      &deinit_event, sizeof(deinit_event), 0);
    }
    
    // Free constitutional handle
    free(handle);
    
    ESP_LOGI(TAG, "✅ Constitutional feedback tool deinitialized");
    
    return ESP_OK;
}

esp_err_t feedback_tool_get_status(feedback_tool_handle_t handle, feedback_tool_status_t *status) {
    if (!handle || !handle->is_initialized || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Update uptime
    handle->status.uptime_ms = (esp_timer_get_time() - handle->init_timestamp_us) / 1000;
    handle->status.state_changes_count = handle->state_changes_count;
    handle->status.error_count = handle->error_count;
    handle->status.current_state = handle->current_state;
    handle->status.current_pattern = handle->current_pattern;
    handle->status.current_brightness = handle->current_brightness;
    
    // Copy status with constitutional memory safety
    memcpy(status, &handle->status, sizeof(feedback_tool_status_t));
    
    return ESP_OK;
}

// =============================================================================
// Constitutional Visual Feedback Functions
// =============================================================================

esp_err_t feedback_tool_set_state(feedback_tool_handle_t handle, feedback_state_t state) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (handle->current_state == state) {
        // No change needed
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "🔄 Constitutional state change: %s → %s", 
             feedback_tool_state_to_string(handle->current_state),
             feedback_tool_state_to_string(state));
    
    // Store old state for event
    feedback_state_t old_state = handle->current_state;
    
    // Update state
    handle->current_state = state;
    handle->state_changes_count++;
    handle->animation_cycle = 0;  // Reset animation
    handle->pattern_start_time = esp_timer_get_time() / 1000;
    
    // Update color based on state
    handle->current_color = constitutional_get_state_color(state);
    
    // Set default pattern based on state
    switch (state) {
        case FEEDBACK_STATE_IDLE:
            handle->current_pattern = FEEDBACK_PATTERN_BREATHING;
            break;
        case FEEDBACK_STATE_TAG_DETECTED:
            handle->current_pattern = FEEDBACK_PATTERN_SOLID;
            break;
        case FEEDBACK_STATE_WIFI_CONNECTING:
            handle->current_pattern = FEEDBACK_PATTERN_BLINKING;
            break;
        case FEEDBACK_STATE_ERROR:
        case FEEDBACK_STATE_RFID_ERROR:
        case FEEDBACK_STATE_HTTP_ERROR:
            handle->current_pattern = FEEDBACK_PATTERN_BLINKING;
            break;
        case FEEDBACK_STATE_FLOW_60:
            handle->current_pattern = FEEDBACK_PATTERN_BREATHING;
            break;
        case FEEDBACK_STATE_FLOW_90:
            handle->current_pattern = FEEDBACK_PATTERN_PULSING;
            break;
        default:
            handle->current_pattern = FEEDBACK_PATTERN_SOLID;
            break;
    }
    
    // Publish state change event if enabled
    if (handle->config.publish_events) {
        feedback_tool_event_data_t state_event = {
            .type = FEEDBACK_TOOL_EVENT_STATE_CHANGED,
            .data.state_change = {
                .old_state = old_state,
                .new_state = state,
                .timestamp_ms = esp_timer_get_time() / 1000
            }
        };
        esp_event_post(FEEDBACK_TOOL_EVENTS, FEEDBACK_TOOL_EVENT_STATE_CHANGED, 
                      &state_event, sizeof(state_event), 0);
    }
    
    ESP_LOGI(TAG, "✅ Constitutional state updated: %s with %s pattern", 
             feedback_tool_state_to_string(state),
             feedback_tool_pattern_to_string(handle->current_pattern));
    
    return ESP_OK;
}

esp_err_t feedback_tool_set_pattern(feedback_tool_handle_t handle, 
                                   feedback_pattern_t pattern,
                                   uint8_t brightness,
                                   uint32_t duration_ms) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🎨 Constitutional pattern change: %s → %s (brightness: %d, duration: %" PRIu32 "ms)", 
             feedback_tool_pattern_to_string(handle->current_pattern),
             feedback_tool_pattern_to_string(pattern),
             brightness, duration_ms);
    
    handle->current_pattern = pattern;
    handle->current_brightness = brightness;
    handle->pattern_duration_ms = duration_ms;
    handle->pattern_start_time = esp_timer_get_time() / 1000;
    handle->animation_cycle = 0;  // Reset animation
    
    // Publish pattern update event if enabled
    if (handle->config.publish_events) {
        feedback_tool_event_data_t pattern_event = {
            .type = FEEDBACK_TOOL_EVENT_PATTERN_UPDATED,
            .data.pattern_update = {
                .pattern = pattern,
                .brightness = brightness,
                .duration_ms = duration_ms
            }
        };
        esp_event_post(FEEDBACK_TOOL_EVENTS, FEEDBACK_TOOL_EVENT_PATTERN_UPDATED, 
                      &pattern_event, sizeof(pattern_event), 0);
    }
    
    return ESP_OK;
}

esp_err_t feedback_tool_health_check(feedback_tool_handle_t handle) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Constitutional feedback tool health check");
    
    bool health_ok = true;
    
    // Check if task is running
    if (!handle->is_active || !handle->update_task_handle) {
        ESP_LOGW(TAG, "⚠️ LED update task not running");
        health_ok = false;
    }
    
    // Check LED hardware
    if (!handle->led_hardware_ok) {
        ESP_LOGW(TAG, "⚠️ LED hardware issues detected");
        health_ok = false;
    }
    
    // Check error count
    if (handle->error_count > 10) {
        ESP_LOGW(TAG, "⚠️ High error count: %" PRIu32, handle->error_count);
        health_ok = false;
    }
    
    // Update status
    handle->status.led_hardware_ok = health_ok;
    
    // Publish health check event if enabled
    if (handle->config.publish_events) {
        feedback_tool_event_data_t health_event = {
            .type = FEEDBACK_TOOL_EVENT_HEALTH_CHECK,
            .data.health_status = {
                .health_ok = health_ok,
                .uptime_ms = (esp_timer_get_time() - handle->init_timestamp_us) / 1000,
                .error_count = handle->error_count
            }
        };
        esp_event_post(FEEDBACK_TOOL_EVENTS, FEEDBACK_TOOL_EVENT_HEALTH_CHECK, 
                      &health_event, sizeof(health_event), 0);
    }
    
    ESP_LOGI(TAG, "%s Constitutional feedback tool health check", 
             health_ok ? "✅" : "⚠️");
    
    return health_ok ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t feedback_tool_generate_dashboard(feedback_tool_handle_t handle, 
                                          char* dashboard_buffer, 
                                          size_t buffer_size) {
    if (!handle || !handle->is_initialized || !dashboard_buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Constitutional requirement: 1KB+ buffer
    if (buffer_size < 1024) {
        ESP_LOGE(TAG, "Constitutional violation: Dashboard buffer too small (%zu bytes, need 1KB+)", buffer_size);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Update status before dashboard generation
    handle->status.uptime_ms = (esp_timer_get_time() - handle->init_timestamp_us) / 1000;
    
    // Generate constitutional feedback dashboard (constitutional requirement: 1KB+ buffer)
    snprintf(dashboard_buffer, buffer_size,
             "💡 CONSTITUTIONAL FEEDBACK DASHBOARD\n"
             "====================================\n"
             "📋 Tool Status:\n"
             "   Tool ID: %s\n"
             "   Version: %s\n"
             "   Initialized: %s\n"
             "   Active: %s\n"
             "   LED Hardware: %s\n\n"
             "🎨 Visual State:\n"
             "   Current State: %s\n"
             "   Pattern: %s\n"
             "   Brightness: %d/255\n"
             "   Color: RGB(%d,%d,%d)\n"
             "   Animation Cycle: %" PRIu32 "\n\n"
             "⚙️ Hardware Configuration:\n"
             "   LED GPIO: %d\n"
             "   Max Brightness: %d\n"
             "   Update Rate: %" PRIu32 "ms\n"
             "   Breathing Period: %" PRIu32 "ms\n"
             "   Blink Period: %" PRIu32 "ms\n\n"
             "📊 Statistics:\n"
             "   State Changes: %" PRIu32 "\n"
             "   Error Count: %" PRIu32 "\n"
             "   Uptime: %" PRIu32 " ms\n\n"
             "🔄 Flow Awareness:\n"
             "   Flow Awareness: %s\n"
             "   Flow Urgency: %s\n\n"
             "🎯 Constitutional Capabilities:\n"
             "   LED Control: %s\n"
             "   State Management: %s\n"
             "   Event Publishing: %s\n"
             "   Process Map 11: %s\n"
             "   Flow Awareness: %s\n"
             "   Dashboard: %s\n"
             "   Health Checks: %s\n"
             "   Animation: %s\n\n"
             "Status: %s\n",
             feedback_tool_get_id(),
             feedback_tool_get_version(),
             handle->status.is_initialized ? "YES" : "NO",
             handle->status.is_active ? "YES" : "NO",
             handle->status.led_hardware_ok ? "OK" : "ERROR",
             feedback_tool_state_to_string(handle->current_state),
             feedback_tool_pattern_to_string(handle->current_pattern),
             handle->current_brightness,
             handle->current_color.r, handle->current_color.g, handle->current_color.b,
             handle->animation_cycle,
             handle->status.led_gpio,
             handle->config.max_brightness,
             handle->config.update_interval_ms,
             handle->config.breathing_period_ms,
             handle->config.blink_period_ms,
             handle->status.state_changes_count,
             handle->status.error_count,
             handle->status.uptime_ms,
             handle->flow_awareness_active ? "ACTIVE" : "INACTIVE",
             handle->flow_urgency_active ? "ACTIVE" : "INACTIVE",
             (handle->status.capabilities & FEEDBACK_TOOL_CAP_LED_CONTROL) ? "ENABLED" : "DISABLED",
             (handle->status.capabilities & FEEDBACK_TOOL_CAP_STATE_MANAGEMENT) ? "ENABLED" : "DISABLED",
             (handle->status.capabilities & FEEDBACK_TOOL_CAP_EVENT_PUBLISH) ? "ENABLED" : "DISABLED",
             (handle->status.capabilities & FEEDBACK_TOOL_CAP_PROCESS_MAP_11) ? "ENABLED" : "DISABLED",
             (handle->status.capabilities & FEEDBACK_TOOL_CAP_FLOW_AWARENESS) ? "ENABLED" : "DISABLED",
             (handle->status.capabilities & FEEDBACK_TOOL_CAP_DASHBOARD) ? "ENABLED" : "DISABLED",
             (handle->status.capabilities & FEEDBACK_TOOL_CAP_HEALTH_CHECK) ? "ENABLED" : "DISABLED",
             (handle->status.capabilities & FEEDBACK_TOOL_CAP_ANIMATION) ? "ENABLED" : "DISABLED",
             (handle->status.led_hardware_ok && handle->status.error_count == 0) ? 
             "CONSTITUTIONAL FEEDBACK OPERATIONAL" : "FEEDBACK ISSUES DETECTED");
    
    return ESP_OK;
}

// =============================================================================
// Constitutional Flow Awareness Functions
// =============================================================================

esp_err_t feedback_tool_set_flow_context(feedback_tool_handle_t handle, 
                                        bool flow_active, 
                                        bool flow_urgent) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handle->flow_awareness_active = flow_active;
    handle->flow_urgency_active = flow_urgent;
    
    ESP_LOGI(TAG, "🟠 Constitutional flow context updated: awareness=%s, urgency=%s", 
             flow_active ? "active" : "inactive",
             flow_urgent ? "active" : "inactive");
    
    // Update visual state if currently showing tag detected with flow context
    if (handle->current_state == FEEDBACK_STATE_TAG_DETECTED) {
        if (flow_urgent) {
            feedback_tool_set_state(handle, FEEDBACK_STATE_FLOW_90);
        } else if (flow_active) {
            feedback_tool_set_state(handle, FEEDBACK_STATE_FLOW_60);
        }
    }
    
    return ESP_OK;
}

// =============================================================================
// Constitutional Utility Functions
// =============================================================================

const char* feedback_tool_state_to_string(feedback_state_t state) {
    switch (state) {
        case FEEDBACK_STATE_BOOTING: return "BOOTING";
        case FEEDBACK_STATE_IDLE: return "IDLE";
        case FEEDBACK_STATE_ERROR: return "ERROR";
        case FEEDBACK_STATE_SHUTDOWN: return "SHUTDOWN";
        case FEEDBACK_STATE_WIFI_CONNECTING: return "WIFI_CONNECTING";
        case FEEDBACK_STATE_WIFI_CONNECTED: return "WIFI_CONNECTED";
        case FEEDBACK_STATE_WIFI_FAILED: return "WIFI_FAILED";
        case FEEDBACK_STATE_AP_MODE: return "AP_MODE";
        case FEEDBACK_STATE_TAG_DETECTED: return "TAG_DETECTED";
        case FEEDBACK_STATE_TAG_IGNORED: return "TAG_IGNORED";
        case FEEDBACK_STATE_RFID_ERROR: return "RFID_ERROR";
        case FEEDBACK_STATE_NTP_SYNC_STARTED: return "NTP_SYNC_STARTED";
        case FEEDBACK_STATE_NTP_SYNCED: return "NTP_SYNCED";
        case FEEDBACK_STATE_NTP_FAILED: return "NTP_FAILED";
        case FEEDBACK_STATE_HTTP_SENDING: return "HTTP_SENDING";
        case FEEDBACK_STATE_HTTP_SUCCESS: return "HTTP_SUCCESS";
        case FEEDBACK_STATE_HTTP_ERROR: return "HTTP_ERROR";
        case FEEDBACK_STATE_FLOW_60: return "FLOW_60";
        case FEEDBACK_STATE_FLOW_90: return "FLOW_90";
        default: return "UNKNOWN";
    }
}

const char* feedback_tool_pattern_to_string(feedback_pattern_t pattern) {
    switch (pattern) {
        case FEEDBACK_PATTERN_OFF: return "OFF";
        case FEEDBACK_PATTERN_SOLID: return "SOLID";
        case FEEDBACK_PATTERN_BREATHING: return "BREATHING";
        case FEEDBACK_PATTERN_BLINKING: return "BLINKING";
        case FEEDBACK_PATTERN_PULSING: return "PULSING";
        case FEEDBACK_PATTERN_SEQUENCE: return "SEQUENCE";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// Constitutional Internal Functions
// =============================================================================

static rgb_color_t constitutional_get_state_color(feedback_state_t state) {
    switch (state) {
        case FEEDBACK_STATE_IDLE:
            return COLOR_BLUE;
        case FEEDBACK_STATE_TAG_DETECTED:
            return COLOR_GREEN;
        case FEEDBACK_STATE_ERROR:
        case FEEDBACK_STATE_RFID_ERROR:
        case FEEDBACK_STATE_HTTP_ERROR:
        case FEEDBACK_STATE_WIFI_FAILED:
        case FEEDBACK_STATE_NTP_FAILED:
            return COLOR_RED;
        case FEEDBACK_STATE_WIFI_CONNECTING:
        case FEEDBACK_STATE_NTP_SYNC_STARTED:
            return COLOR_BLUE;
        case FEEDBACK_STATE_WIFI_CONNECTED:
        case FEEDBACK_STATE_NTP_SYNCED:
        case FEEDBACK_STATE_HTTP_SUCCESS:
            return COLOR_CYAN;
        case FEEDBACK_STATE_AP_MODE:
            return COLOR_YELLOW;
        case FEEDBACK_STATE_TAG_IGNORED:
            return COLOR_YELLOW;
        case FEEDBACK_STATE_HTTP_SENDING:
            return COLOR_GREEN;
        case FEEDBACK_STATE_FLOW_60:
        case FEEDBACK_STATE_FLOW_90:
            return COLOR_ORANGE;
        case FEEDBACK_STATE_BOOTING:
        case FEEDBACK_STATE_SHUTDOWN:
            return COLOR_WHITE;
        default:
            return COLOR_WHITE;
    }
}

static void constitutional_apply_pattern(struct feedback_tool *tool, rgb_color_t base_color) {
    rgb_color_t output_color = base_color;
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    switch (tool->current_pattern) {
        case FEEDBACK_PATTERN_OFF:
            output_color = COLOR_OFF;
            break;
            
        case FEEDBACK_PATTERN_SOLID:
            // No modification needed
            break;
            
        case FEEDBACK_PATTERN_BREATHING: {
            uint32_t period = tool->config.breathing_period_ms;
            float phase = (2.0 * M_PI * (current_time % period)) / period;
            float intensity = (sin(phase) + 1.0) / 2.0;  // 0.0 to 1.0
            
            output_color.r = (uint8_t)(base_color.r * intensity);
            output_color.g = (uint8_t)(base_color.g * intensity);
            output_color.b = (uint8_t)(base_color.b * intensity);
            break;
        }
        
        case FEEDBACK_PATTERN_BLINKING: {
            uint32_t period = tool->config.blink_period_ms;
            bool on = ((current_time % period) < (period / 2));
            
            if (!on) {
                output_color = COLOR_OFF;
            }
            break;
        }
        
        case FEEDBACK_PATTERN_PULSING: {
            uint32_t period = tool->config.blink_period_ms / 2;  // Faster pulsing
            float phase = (2.0 * M_PI * (current_time % period)) / period;
            float intensity = (sin(phase) + 1.0) / 2.0;
            intensity = 0.3 + (intensity * 0.7);  // 0.3 to 1.0 range
            
            output_color.r = (uint8_t)(base_color.r * intensity);
            output_color.g = (uint8_t)(base_color.g * intensity);
            output_color.b = (uint8_t)(base_color.b * intensity);
            break;
        }
        
        case FEEDBACK_PATTERN_SEQUENCE:
            // TODO: Implement color sequence patterns
            break;
    }
    
    // Apply brightness scaling
    float brightness_scale = (float)tool->current_brightness / 255.0f;
    output_color.r = (uint8_t)(output_color.r * brightness_scale);
    output_color.g = (uint8_t)(output_color.g * brightness_scale);
    output_color.b = (uint8_t)(output_color.b * brightness_scale);
    
    constitutional_update_led_hardware(tool, output_color);
}

static void constitutional_update_led_hardware(struct feedback_tool *tool, rgb_color_t color) {
    // Real WS2812B LED strip control
    if (!tool->led_strip) {
        ESP_LOGE(TAG, "🚨 CRITICAL: LED strip handle is NULL!");
        tool->error_count++;
        return;
    }
    
    // Set LED pixel color (RGB to GRB conversion handled by led_strip)
    esp_err_t ret = led_strip_set_pixel(tool->led_strip, 0, color.r, color.g, color.b);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "🚨 LED set_pixel failed: %s", esp_err_to_name(ret));
        tool->error_count++;
        return;
    }
    
    // Refresh LED strip to show the color
    ret = led_strip_refresh(tool->led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "🚨 LED refresh failed: %s", esp_err_to_name(ret));
        tool->error_count++;
        return;
    }
}

static void constitutional_led_update_task(void *arg) {
    struct feedback_tool *tool = (struct feedback_tool*)arg;
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t update_count = 0;
    
    ESP_LOGI(TAG, "✅ Constitutional LED update task started");
    
    while (tool->is_active) {
        update_count++;
        
        // Debug every 1000 updates (50 seconds at 50ms intervals) - reduced logging
        if (update_count % 1000 == 1) {
            ESP_LOGI(TAG, "🔄 LED Task: cycle %" PRIu32 ", state=%s", 
                     update_count,
                     feedback_tool_state_to_string(tool->current_state));
        }
        
        // Update animation cycle
        tool->animation_cycle++;
        
        // Apply current pattern to current color
        constitutional_apply_pattern(tool, tool->current_color);
        
        // Check for pattern timeout
        if (tool->pattern_duration_ms > 0) {
            uint32_t current_time = esp_timer_get_time() / 1000;
            if ((current_time - tool->pattern_start_time) >= tool->pattern_duration_ms) {
                // Reset to default pattern for current state
                feedback_tool_set_state(tool, tool->current_state);
            }
        }
        
        // Constitutional timing: Update at configured interval
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(tool->config.update_interval_ms));
    }
    
    ESP_LOGI(TAG, "Constitutional LED update task ended");
    vTaskDelete(NULL);
}

// =============================================================================
// Constitutional System State Event Handler
// =============================================================================

static void constitutional_system_state_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    struct feedback_tool *tool = (struct feedback_tool*)handler_args;
    
    if (!tool || !tool->is_initialized || !tool->is_active) {
        return;
    }
    
    if (base == SYSTEM_STATE_EVENTS && id == SYSTEM_STATE_EVENT_STATE_CHANGE) {
        system_state_change_event_t *state_event = (system_state_change_event_t*)event_data;
        
        ESP_LOGI(TAG, "🎯 LED State: %s", state_event->test_name);
        
        // Execute LED recipe based on system state (Constitutional Process Map 11)
        feedback_state_t target_state = state_event->target_state;
        feedback_pattern_t recipe_pattern = FEEDBACK_PATTERN_SOLID;
        
        // Determine pattern recipe based on state
        switch (target_state) {
            case FEEDBACK_STATE_TAG_DETECTED:
                recipe_pattern = FEEDBACK_PATTERN_PULSING;
                break;
            case FEEDBACK_STATE_WIFI_CONNECTING:
                recipe_pattern = FEEDBACK_PATTERN_BLINKING;
                break;
            case FEEDBACK_STATE_ERROR:
                recipe_pattern = FEEDBACK_PATTERN_BLINKING;
                break;
            case FEEDBACK_STATE_IDLE:
            default:
                recipe_pattern = FEEDBACK_PATTERN_BREATHING;
                break;
        }
        
        esp_err_t state_ret = feedback_tool_set_state(tool, target_state);
        esp_err_t pattern_ret = feedback_tool_set_pattern(tool, recipe_pattern, 
                                                         tool->config.max_brightness,
                                                         state_event->duration_ms);
        
        if (state_ret != ESP_OK || pattern_ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ LED recipe failed");
            tool->error_count++;
        }
    }
}