#include "feedback_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "led_strip.h"
#include <string.h>
#include <math.h>
#include "sdkconfig.h"

static const char *TAG = "feedback_manager";

// Maximum brightness (0-255)
#ifdef CONFIG_FEEDBACK_LED_BRIGHTNESS
#define MAX_BRIGHTNESS CONFIG_FEEDBACK_LED_BRIGHTNESS
#else
#define MAX_BRIGHTNESS 100
#endif

// LED colors for different states (RGB format)
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

// Color definitions
static const rgb_color_t COLOR_OFF = {0, 0, 0};
static const rgb_color_t COLOR_RED = {255, 0, 0};
static const rgb_color_t COLOR_GREEN = {0, 255, 0};
static const rgb_color_t COLOR_BLUE = {0, 0, 255};
static const rgb_color_t COLOR_YELLOW = {255, 255, 0};
static const rgb_color_t COLOR_PURPLE = {128, 0, 128};
static const rgb_color_t COLOR_WHITE = {255, 255, 255};
static const rgb_color_t COLOR_ORANGE = {255, 165, 0};
static const rgb_color_t COLOR_CYAN = {0, 255, 255};

// Feedback manager structure
struct feedback_manager {
    uint8_t led_gpio;                  // GPIO pin for LED
    bool use_ws2812;                   // Whether to use WS2812B LED
    led_strip_handle_t led_strip;      // LED strip handle (when using WS2812B)
    feedback_state_t primary_state;    // Current primary state
    feedback_state_t background_state; // Background state
    TaskHandle_t task_handle;          // Handle for background task
    bool is_initialized;               // Initialization state
    bool is_active;                    // Whether manager is active
};

// Forward declarations
static void feedback_manager_task(void *arg);

// Get color for state
static rgb_color_t get_state_color(feedback_state_t state) {
    switch (state) {
        case FEEDBACK_STATE_BOOTING:
            return COLOR_WHITE;
            
        case FEEDBACK_STATE_IDLE:
            return COLOR_CYAN;  // Soft cyan when idle
            
        case FEEDBACK_STATE_ERROR:
            return COLOR_RED;
            
        case FEEDBACK_STATE_WIFI_CONNECTING:
            return COLOR_BLUE;
            
        case FEEDBACK_STATE_WIFI_CONNECTED:
            return COLOR_CYAN;
            
        case FEEDBACK_STATE_WIFI_FAILED:
            return COLOR_RED;
            
        case FEEDBACK_STATE_WIFI_AP_MODE:
            return COLOR_PURPLE;
            
        case FEEDBACK_STATE_TIME_SYNCING:
            return COLOR_YELLOW;
            
        case FEEDBACK_STATE_TIME_SYNCED:
            return COLOR_GREEN;
            
        case FEEDBACK_STATE_TIME_SYNC_FAILED:
            return COLOR_RED;
            
        case FEEDBACK_STATE_RFID_INITIALIZING:
            return COLOR_ORANGE;
            
        case FEEDBACK_STATE_RFID_ACTIVE:
            return COLOR_BLUE;
            
        case FEEDBACK_STATE_RFID_ERROR:
            return COLOR_RED;
            
        case FEEDBACK_STATE_TAG_DETECTED:
            return COLOR_GREEN;
            
        case FEEDBACK_STATE_TAG_READ_ERROR:
            return COLOR_RED;
            
        case FEEDBACK_STATE_WEBHOOK_SENDING:
            return COLOR_YELLOW;
            
        case FEEDBACK_STATE_WEBHOOK_SUCCESS:
            return COLOR_GREEN;
            
        case FEEDBACK_STATE_WEBHOOK_ERROR:
            return COLOR_RED;
            
        case FEEDBACK_STATE_WEBHOOK_QUEUED:
            return COLOR_ORANGE;
            
        /* Initialization sequence states */
        case FEEDBACK_STATE_INIT_START:
            return COLOR_WHITE;
            
        case FEEDBACK_STATE_INIT_FS:
            return COLOR_WHITE;
            
        case FEEDBACK_STATE_INIT_WIFI_PREP:
            return COLOR_BLUE;
            
        case FEEDBACK_STATE_INIT_TIME:
            return COLOR_CYAN;
            
        case FEEDBACK_STATE_INIT_WEBHOOK:
            return COLOR_YELLOW;
            
        case FEEDBACK_STATE_INIT_RFID:
            return COLOR_ORANGE;
            
        case FEEDBACK_STATE_INIT_COMPLETE:
            return COLOR_GREEN;
            
        default:
            return COLOR_WHITE;
    }
}

// Set a specific color to the WS2812B LED
static void set_led_color(led_strip_handle_t led_strip, rgb_color_t color, uint8_t brightness) {
    if (led_strip == NULL) {
        return;
    }
    
    // Apply brightness
    uint8_t r = (color.r * brightness) / 255;
    uint8_t g = (color.g * brightness) / 255;
    uint8_t b = (color.b * brightness) / 255;
    
    led_strip_set_pixel(led_strip, 0, r, g, b);
    esp_err_t ret = led_strip_refresh(led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(ret));
    }
}

// Apply brightness to a color
static rgb_color_t apply_brightness(rgb_color_t color, uint8_t brightness) {
    rgb_color_t result;
    result.r = (color.r * brightness) / 255;
    result.g = (color.g * brightness) / 255;
    result.b = (color.b * brightness) / 255;
    return result;
}

// Breathing effect - returns intensity 0-255
static uint8_t breathing_effect(uint32_t cycle_count, uint32_t period) {
    // Enhanced sine wave approximation for more natural breathing
    float t = ((float)(cycle_count % period)) / period;
    
    // Adjusted sine wave with phase shift for more natural curve
    float value = sinf(t * 2.0f * 3.14159f - 3.14159f/2.0f); // Phase shifted sine
    value = (value + 1.0f) / 2.0f; // Convert from -1..1 to 0..1
    
    // Apply squaring for more prominent peaks and smoother valleys
    value = value * value;
    
    return (uint8_t)(value * MAX_BRIGHTNESS);
}

// Initialize feedback manager
feedback_manager_handle_t feedback_manager_init(uint8_t led_gpio) {
    ESP_LOGI(TAG, "Initializing feedback manager on GPIO %d", led_gpio);
    
    // Allocate memory for the manager
    struct feedback_manager *manager = calloc(1, sizeof(struct feedback_manager));
    if (manager == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for manager");
        return NULL;
    }
    
    // Store configuration
    manager->led_gpio = led_gpio;
    manager->primary_state = FEEDBACK_STATE_BOOTING;
    manager->background_state = FEEDBACK_STATE_IDLE;
    manager->is_initialized = false;
    manager->is_active = false;
    
#ifdef CONFIG_FEEDBACK_USE_WS2812
    manager->use_ws2812 = true;
    
    // Initialize WS2812B LED
    ESP_LOGI(TAG, "Initializing WS2812B LED on GPIO %d with brightness %d", led_gpio, MAX_BRIGHTNESS);
    
    led_strip_config_t strip_config = {
        .strip_gpio_num = led_gpio,
        .max_leds = 1,  // Single LED
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // Most WS2812B use GRB format
        .flags.invert_out = false,
    };
    
    // Use RMT driver for WS2812
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &manager->led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip driver: %s", esp_err_to_name(ret));
        free(manager);
        return NULL;
    }
    
    // Boot pattern with WS2812
    // White blink
    led_strip_clear(manager->led_strip);
    led_strip_refresh(manager->led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Show boot pattern sequence
    rgb_color_t boot_color = COLOR_WHITE;
    set_led_color(manager->led_strip, boot_color, MAX_BRIGHTNESS);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    led_strip_clear(manager->led_strip);
    led_strip_refresh(manager->led_strip);
    vTaskDelay(pdMS_TO_TICKS(200));
    
    set_led_color(manager->led_strip, boot_color, MAX_BRIGHTNESS);
    vTaskDelay(pdMS_TO_TICKS(200));
    
    led_strip_clear(manager->led_strip);
    led_strip_refresh(manager->led_strip);
#else
    manager->use_ws2812 = false;
    
    // Configure standard GPIO for LED
    gpio_reset_pin(led_gpio);
    gpio_set_direction(led_gpio, GPIO_MODE_OUTPUT);
    
    // Show boot pattern with standard LED
    gpio_set_level(led_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(led_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(led_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(led_gpio, 0);
#endif
    
    // Start feedback task
    ESP_LOGI(TAG, "Starting feedback manager task");
    xTaskCreate(feedback_manager_task, "feedback_task", 3072, manager, 2, &manager->task_handle);
    
    manager->is_initialized = true;
    manager->is_active = true;
    
    ESP_LOGI(TAG, "Feedback manager initialized successfully");
    
    return manager;
}

// Deinitialize feedback manager
esp_err_t feedback_manager_deinit(feedback_manager_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    
    // Stop task
    manager->is_active = false;
    if (manager->task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Give time for task to exit
        vTaskDelete(manager->task_handle);
        manager->task_handle = NULL;
    }
    
    // Turn off LED
    if (manager->use_ws2812 && manager->led_strip != NULL) {
        led_strip_clear(manager->led_strip);
        led_strip_refresh(manager->led_strip);
        led_strip_del(manager->led_strip);
    } else {
        gpio_set_level(manager->led_gpio, 0);
    }
    
    // Free memory
    free(manager);
    
    return ESP_OK;
}

// Set primary system state
esp_err_t feedback_manager_set_state(feedback_manager_handle_t handle, feedback_state_t state) {
    if (handle == NULL || state >= FEEDBACK_STATE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    
    ESP_LOGI(TAG, "Setting primary state to %d", state);
    manager->primary_state = state;
    
    // Immediately update LED color based on the new state
    if (manager->use_ws2812 && manager->led_strip != NULL) {
        rgb_color_t color = get_state_color(state);
        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
    }
    
    return ESP_OK;
}

// Set background system state
esp_err_t feedback_manager_set_background_state(feedback_manager_handle_t handle, feedback_state_t state) {
    if (handle == NULL || state >= FEEDBACK_STATE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    
    ESP_LOGI(TAG, "Setting background state to %d", state);
    manager->background_state = state;
    
    // If primary state is idle, immediately update LED color based on the new background state
    if (manager->primary_state == FEEDBACK_STATE_IDLE && manager->use_ws2812 && manager->led_strip != NULL) {
        rgb_color_t color = get_state_color(state);
        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
    }
    
    return ESP_OK;
}

// Flash a temporary state indication
esp_err_t feedback_manager_flash_event(feedback_manager_handle_t handle, feedback_state_t state, int count) {
    if (handle == NULL || state >= FEEDBACK_STATE_MAX || count <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    
    // Store original primary state
    feedback_state_t original_state = manager->primary_state;
    
    // Flash the new state
    for (int i = 0; i < count; i++) {
        // Set new state
        manager->primary_state = state;
        
        // Get color for flash state
        if (manager->use_ws2812 && manager->led_strip != NULL) {
            rgb_color_t color = get_state_color(state);
            set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
        } else {
            gpio_set_level(manager->led_gpio, 1);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Return to original state
        manager->primary_state = original_state;
        
        // Restore original color
        if (manager->use_ws2812 && manager->led_strip != NULL) {
            rgb_color_t color;
            
            if (original_state == FEEDBACK_STATE_IDLE) {
                color = get_state_color(manager->background_state);
            } else {
                color = get_state_color(original_state);
            }
            
            set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
        } else {
            // For standard LED, approximate with on/off
            gpio_set_level(manager->led_gpio, 0);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    return ESP_OK;
}

// Clear all states and reset to default idle state
esp_err_t feedback_manager_reset(feedback_manager_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    
    manager->primary_state = FEEDBACK_STATE_IDLE;
    manager->background_state = FEEDBACK_STATE_IDLE;
    
    // Update LED color
    if (manager->use_ws2812 && manager->led_strip != NULL) {
        rgb_color_t color = get_state_color(FEEDBACK_STATE_IDLE);
        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
    }
    
    return ESP_OK;
}

// Check if RFID hardware is responding
bool feedback_manager_check_rfid_hardware(void* rfid_handle) {
    if (rfid_handle == NULL) {
        return false;
    }
    
    // For now, just return true if the handle is not NULL
    // In the future, this could be expanded to actually check hardware status
    return true;
}

// Validate an initialization step
esp_err_t feedback_manager_validate_init_step(feedback_manager_handle_t handle, bool success) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    feedback_state_t current_state = manager->primary_state;
    
    ESP_LOGI(TAG, "Validating initialization step (state: %d), success: %s", 
            current_state, success ? "true" : "false");
    
    if (success) {
        // Flash green briefly to indicate success
        if (manager->use_ws2812 && manager->led_strip != NULL) {
            // Store current color
            rgb_color_t current_color = get_state_color(current_state);
            
            // Flash green briefly
            set_led_color(manager->led_strip, COLOR_GREEN, MAX_BRIGHTNESS);
            vTaskDelay(pdMS_TO_TICKS(100));
            
            // Return to current state color
            set_led_color(manager->led_strip, current_color, MAX_BRIGHTNESS);
        } else {
            // Standard LED approximation - triple quick flash
            for (int i = 0; i < 3; i++) {
                gpio_set_level(manager->led_gpio, 1);
                vTaskDelay(pdMS_TO_TICKS(50));
                gpio_set_level(manager->led_gpio, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            gpio_set_level(manager->led_gpio, 1); // Back to on state
        }
    } else {
        // Flash red for failure
        if (manager->use_ws2812 && manager->led_strip != NULL) {
            // Store current color
            rgb_color_t current_color = get_state_color(current_state);
            
            // Flash red three times
            for (int i = 0; i < 3; i++) {
                set_led_color(manager->led_strip, COLOR_RED, MAX_BRIGHTNESS);
                vTaskDelay(pdMS_TO_TICKS(100));
                set_led_color(manager->led_strip, COLOR_OFF, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            
            // Return to current state color
            set_led_color(manager->led_strip, current_color, MAX_BRIGHTNESS);
        } else {
            // Standard LED approximation - longer flashes
            for (int i = 0; i < 3; i++) {
                gpio_set_level(manager->led_gpio, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(manager->led_gpio, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
    }
    
    return ESP_OK;
}

// Feedback manager task
static void feedback_manager_task(void *arg) {
    struct feedback_manager *manager = (struct feedback_manager *)arg;
    
    uint32_t cycle_count = 0;
    const uint32_t BREATHING_PERIOD = 40; // Cycles for one breathing period
    
    // Debug log to show we're starting the task
    ESP_LOGI(TAG, "Feedback manager task started. Use WS2812: %d, Max Brightness: %d", 
             manager->use_ws2812, MAX_BRIGHTNESS);

    while (manager->is_active) {
        // Use primary state if it's not idle, otherwise use background state
        feedback_state_t current_state = manager->primary_state;
        
        if (current_state == FEEDBACK_STATE_IDLE) {
            current_state = manager->background_state;
        }
        
        // Get base color for current state
        rgb_color_t color = get_state_color(current_state);
        
        // Apply different patterns based on the state
        if (manager->use_ws2812) {
            // Special effects for certain states
            switch (current_state) {
                case FEEDBACK_STATE_IDLE:
                    // Slow breathing effect
                    {
                        uint8_t intensity = breathing_effect(cycle_count, BREATHING_PERIOD);
                        rgb_color_t adjusted_color = apply_brightness(color, intensity);
                        set_led_color(manager->led_strip, adjusted_color, MAX_BRIGHTNESS);
                    }
                    break;
                    
                case FEEDBACK_STATE_WIFI_CONNECTING:
                    // Fast blue blinking
                    if (cycle_count % 6 < 3) {
                        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
                    } else {
                        set_led_color(manager->led_strip, COLOR_OFF, MAX_BRIGHTNESS);
                    }
                    break;
                    
                case FEEDBACK_STATE_WIFI_AP_MODE:
                    // Purple pulse - slow blinking
                    if (cycle_count % 30 < 15) {
                        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
                    } else {
                        set_led_color(manager->led_strip, COLOR_OFF, MAX_BRIGHTNESS);
                    }
                    break;
                    
                case FEEDBACK_STATE_RFID_ERROR:
                    // Double red flash
                    if (cycle_count % 20 < 5) {
                        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
                    } else if (cycle_count % 20 >= 10 && cycle_count % 20 < 15) {
                        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
                    } else {
                        set_led_color(manager->led_strip, COLOR_OFF, MAX_BRIGHTNESS);
                    }
                    break;
                    
                case FEEDBACK_STATE_WEBHOOK_ERROR:
                    // Triple red flash
                    if (cycle_count % 30 < 5) {
                        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
                    } else if (cycle_count % 30 >= 10 && cycle_count % 30 < 15) {
                        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
                    } else if (cycle_count % 30 >= 20 && cycle_count % 30 < 25) {
                        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
                    } else {
                        set_led_color(manager->led_strip, COLOR_OFF, MAX_BRIGHTNESS);
                    }
                    break;
                    
                case FEEDBACK_STATE_WEBHOOK_QUEUED:
                    // Yellow pulse with frequency based on queue size - medium blink
                    if (cycle_count % 15 < 7) {
                        set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
                    } else {
                        set_led_color(manager->led_strip, COLOR_OFF, MAX_BRIGHTNESS);
                    }
                    break;
                    
                default:
                    // For other states, no need to update since they're handled by set_state
                    break;
            }
        } else {
            // Standard GPIO LED - approximate with blinking patterns
            bool led_on = false;
            
            switch (current_state) {
                case FEEDBACK_STATE_IDLE:
                    // Breathing effect approximation
                    {
                        uint8_t intensity = breathing_effect(cycle_count, BREATHING_PERIOD);
                        led_on = (rand() % 255) < intensity; // Probability based intensity
                    }
                    break;
                    
                case FEEDBACK_STATE_WIFI_CONNECTING:
                    // Fast blinking
                    led_on = (cycle_count % 6 < 3);
                    break;
                    
                case FEEDBACK_STATE_WIFI_AP_MODE:
                    // Slow blinking
                    led_on = (cycle_count % 30 < 15);
                    break;
                    
                case FEEDBACK_STATE_TAG_DETECTED:
                    // Solid on
                    led_on = true;
                    break;
                    
                case FEEDBACK_STATE_RFID_ERROR:
                    // Double flash
                    led_on = (cycle_count % 20 < 5) || (cycle_count % 20 >= 10 && cycle_count % 20 < 15);
                    break;
                    
                case FEEDBACK_STATE_WEBHOOK_ERROR:
                    // Triple flash
                    led_on = (cycle_count % 30 < 5) || 
                             (cycle_count % 30 >= 10 && cycle_count % 30 < 15) || 
                             (cycle_count % 30 >= 20 && cycle_count % 30 < 25);
                    break;
                    
                case FEEDBACK_STATE_WEBHOOK_QUEUED:
                    // Medium blink
                    led_on = (cycle_count % 15 < 7);
                    break;
                    
                default:
                    // For other states, solid on
                    led_on = true;
                    break;
            }
            
            gpio_set_level(manager->led_gpio, led_on ? 1 : 0);
        }
        
        cycle_count++;
        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz update rate
    }
    
    // Turn off LED before exiting
    if (manager->use_ws2812) {
        led_strip_clear(manager->led_strip);
        led_strip_refresh(manager->led_strip);
    } else {
        gpio_set_level(manager->led_gpio, 0);
    }
    
    vTaskDelete(NULL);
}