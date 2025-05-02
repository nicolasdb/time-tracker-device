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
    ESP_LOGI(TAG, "Initializing WS2812B LED on GPIO %d", led_gpio);
    
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
    led_strip_set_pixel(manager->led_strip, 0, 255, 255, 255); // White
    led_strip_refresh(manager->led_strip);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    led_strip_clear(manager->led_strip);
    led_strip_refresh(manager->led_strip);
    vTaskDelay(pdMS_TO_TICKS(200));
    
    led_strip_set_pixel(manager->led_strip, 0, 255, 255, 255); // White
    led_strip_refresh(manager->led_strip);
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
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Return to original state
        manager->primary_state = original_state;
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
            
        default:
            return COLOR_WHITE;
    }
}

// Breathing effect - returns intensity 0-255
static uint8_t breathing_effect(uint32_t cycle_count, uint32_t period) {
    // Simple sine wave approximation for smooth breathing
    float t = ((float)(cycle_count % period)) / period;
    float value = sinf(t * 2.0f * 3.14159f); // 2π
    value = (value + 1.0f) / 2.0f; // Convert from -1..1 to 0..1
    return (uint8_t)(value * 255.0f);
}

// Feedback manager task
static void feedback_manager_task(void *arg) {
    struct feedback_manager *manager = (struct feedback_manager *)arg;
    
    uint32_t cycle_count = 0;
    const uint32_t BREATHING_PERIOD = 40; // Cycles for one breathing period
    
    while (manager->is_active) {
        // Use primary state if it's not idle, otherwise use background state
        feedback_state_t current_state = manager->primary_state;
        
        if (current_state == FEEDBACK_STATE_IDLE) {
            current_state = manager->background_state;
        }
        
        // Get base color for current state
        rgb_color_t color = get_state_color(current_state);
        rgb_color_t display_color = color;
        
        // Apply different patterns based on the state
        bool led_on = true; // Default for standard LED
        
        switch (current_state) {
            case FEEDBACK_STATE_BOOTING:
                // Quick white flash sequence
                if (cycle_count % 10 < 5) {
                    display_color = COLOR_WHITE;
                    led_on = true;
                } else {
                    display_color = COLOR_OFF;
                    led_on = false;
                }
                break;
                
            case FEEDBACK_STATE_IDLE:
                // Slow breathing effect
                if (manager->use_ws2812) {
                    uint8_t intensity = breathing_effect(cycle_count, BREATHING_PERIOD);
                    display_color.r = (color.r * intensity) / 255;
                    display_color.g = (color.g * intensity) / 255;
                    display_color.b = (color.b * intensity) / 255;
                } else {
                    // For standard LED, approximate breathing with PWM-like pattern
                    uint8_t intensity = breathing_effect(cycle_count, BREATHING_PERIOD);
                    led_on = (rand() % 255) < intensity; // Probability based intensity
                }
                break;
                
            case FEEDBACK_STATE_WIFI_CONNECTING:
                // Fast blue blinking
                if (cycle_count % 6 < 3) {
                    display_color = color;
                    led_on = true;
                } else {
                    display_color = COLOR_OFF;
                    led_on = false;
                }
                break;
                
            case FEEDBACK_STATE_WIFI_AP_MODE:
                // Purple pulse - slow blinking
                if (cycle_count % 30 < 15) {
                    display_color = color;
                    led_on = true;
                } else {
                    display_color = COLOR_OFF;
                    led_on = false;
                }
                break;
                
            case FEEDBACK_STATE_TAG_DETECTED:
                // Solid green - just solid on
                display_color = color;
                led_on = true;
                break;
                
            case FEEDBACK_STATE_RFID_ERROR:
                // Double red flash
                if (cycle_count % 20 < 5) {
                    display_color = color;
                    led_on = true;
                } else if (cycle_count % 20 >= 10 && cycle_count % 20 < 15) {
                    display_color = color;
                    led_on = true;
                } else {
                    display_color = COLOR_OFF;
                    led_on = false;
                }
                break;
                
            case FEEDBACK_STATE_WEBHOOK_ERROR:
                // Triple red flash
                if (cycle_count % 30 < 5) {
                    display_color = color;
                    led_on = true;
                } else if (cycle_count % 30 >= 10 && cycle_count % 30 < 15) {
                    display_color = color;
                    led_on = true;
                } else if (cycle_count % 30 >= 20 && cycle_count % 30 < 25) {
                    display_color = color;
                    led_on = true;
                } else {
                    display_color = COLOR_OFF;
                    led_on = false;
                }
                break;
                
            case FEEDBACK_STATE_WEBHOOK_QUEUED:
                // Yellow pulse with frequency based on queue size - medium blink
                if (cycle_count % 15 < 7) {
                    display_color = color;
                    led_on = true;
                } else {
                    display_color = COLOR_OFF;
                    led_on = false;
                }
                break;
                
            default:
                // For other states, use solid light
                display_color = color;
                led_on = true;
                break;
        }
        
        // Update LED based on type
        if (manager->use_ws2812) {
            led_strip_set_pixel(manager->led_strip, 0, display_color.r, display_color.g, display_color.b);
            led_strip_refresh(manager->led_strip);
        } else {
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
