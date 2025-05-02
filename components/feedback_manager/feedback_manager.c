#include "feedback_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "feedback_manager";

// Feedback manager structure
struct feedback_manager {
    uint8_t led_gpio;                  // GPIO pin for LED
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
    
    // Configure GPIO
    gpio_reset_pin(led_gpio);
    gpio_set_direction(led_gpio, GPIO_MODE_OUTPUT);
    
    // Start feedback task
    ESP_LOGI(TAG, "Starting feedback manager task");
    xTaskCreate(feedback_manager_task, "feedback_task", 2048, manager, 2, &manager->task_handle);
    
    // Show boot pattern
    gpio_set_level(led_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(led_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(led_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(led_gpio, 0);
    
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
    gpio_set_level(manager->led_gpio, 0);
    
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

// Feedback manager task
static void feedback_manager_task(void *arg) {
    struct feedback_manager *manager = (struct feedback_manager *)arg;
    
    uint32_t cycle_count = 0;
    
    while (manager->is_active) {
        // Use primary state if it's not idle, otherwise use background state
        feedback_state_t current_state = manager->primary_state;
        
        if (current_state == FEEDBACK_STATE_IDLE) {
            current_state = manager->background_state;
        }
        
        // Apply different patterns based on the state
        switch (current_state) {
            case FEEDBACK_STATE_BOOTING:
                // Quick white flash sequence
                if (cycle_count % 10 == 0) {
                    gpio_set_level(manager->led_gpio, 1);
                } else if (cycle_count % 10 == 5) {
                    gpio_set_level(manager->led_gpio, 0);
                }
                break;
                
            case FEEDBACK_STATE_IDLE:
                // Slow breathing - approximate with slower blinking
                if (cycle_count % 40 < 20) {
                    gpio_set_level(manager->led_gpio, 1);
                } else {
                    gpio_set_level(manager->led_gpio, 0);
                }
                break;
                
            case FEEDBACK_STATE_WIFI_CONNECTING:
                // Fast blue blinking
                if (cycle_count % 6 < 3) {
                    gpio_set_level(manager->led_gpio, 1);
                } else {
                    gpio_set_level(manager->led_gpio, 0);
                }
                break;
                
            case FEEDBACK_STATE_WIFI_AP_MODE:
                // Purple pulse - approximate with slow blinking
                if (cycle_count % 30 < 15) {
                    gpio_set_level(manager->led_gpio, 1);
                } else {
                    gpio_set_level(manager->led_gpio, 0);
                }
                break;
                
            case FEEDBACK_STATE_TAG_DETECTED:
                // Solid green - just solid on
                gpio_set_level(manager->led_gpio, 1);
                break;
                
            case FEEDBACK_STATE_RFID_ERROR:
                // Double red flash
                if (cycle_count % 20 < 5) {
                    gpio_set_level(manager->led_gpio, 1);
                } else if (cycle_count % 20 >= 10 && cycle_count % 20 < 15) {
                    gpio_set_level(manager->led_gpio, 1);
                } else {
                    gpio_set_level(manager->led_gpio, 0);
                }
                break;
                
            case FEEDBACK_STATE_WEBHOOK_ERROR:
                // Triple red flash
                if (cycle_count % 30 < 5) {
                    gpio_set_level(manager->led_gpio, 1);
                } else if (cycle_count % 30 >= 10 && cycle_count % 30 < 15) {
                    gpio_set_level(manager->led_gpio, 1);
                } else if (cycle_count % 30 >= 20 && cycle_count % 30 < 25) {
                    gpio_set_level(manager->led_gpio, 1);
                } else {
                    gpio_set_level(manager->led_gpio, 0);
                }
                break;
                
            case FEEDBACK_STATE_WEBHOOK_QUEUED:
                // Yellow pulse with frequency based on queue size - medium blink
                if (cycle_count % 15 < 7) {
                    gpio_set_level(manager->led_gpio, 1);
                } else {
                    gpio_set_level(manager->led_gpio, 0);
                }
                break;
                
            default:
                // For other states, use solid light
                gpio_set_level(manager->led_gpio, 1);
                break;
        }
        
        cycle_count++;
        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz update rate
    }
    
    // Turn off LED before exiting
    gpio_set_level(manager->led_gpio, 0);
    
    vTaskDelete(NULL);
}
