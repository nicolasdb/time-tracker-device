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

// Configuration with fallbacks
#ifdef CONFIG_FEEDBACK_LED_BRIGHTNESS
#define MAX_BRIGHTNESS CONFIG_FEEDBACK_LED_BRIGHTNESS
#else
#define MAX_BRIGHTNESS 10
#endif

#ifdef CONFIG_FEEDBACK_BREATHING_PERIOD
#define BREATHING_PERIOD CONFIG_FEEDBACK_BREATHING_PERIOD
#else
#define BREATHING_PERIOD 80
#endif

// LED colors for different states (RGB format)
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

// State priority levels for queue management
typedef enum {
    FEEDBACK_PRIORITY_LOW = 0,      // Ambient states (idle breathing)
    FEEDBACK_PRIORITY_MEDIUM = 1,   // Status updates (WiFi, AP mode)
    FEEDBACK_PRIORITY_HIGH = 2,     // Critical events (tag detected, errors)
    FEEDBACK_PRIORITY_CRITICAL = 3  // System errors, validation failures
} feedback_priority_t;

// State queue entry
typedef struct {
    feedback_state_t state;
    feedback_priority_t priority;
    uint32_t timestamp;
    uint32_t duration_ms; // 0 = permanent, >0 = temporary
} feedback_state_entry_t;

#define FEEDBACK_QUEUE_SIZE 8

// Color definitions
static const rgb_color_t COLOR_OFF = {0, 0, 0};
static const rgb_color_t COLOR_RED = {255, 0, 0};
static const rgb_color_t COLOR_GREEN = {0, 255, 0};
static const rgb_color_t COLOR_BLUE = {0, 0, 255};        // Strong blue
static const rgb_color_t COLOR_YELLOW = {255, 255, 0};
static const rgb_color_t COLOR_PURPLE = {128, 0, 128};
static const rgb_color_t COLOR_WHITE = {255, 255, 255};
static const rgb_color_t COLOR_ORANGE = {255, 165, 0};
static const rgb_color_t COLOR_CYAN = {0, 255, 255};

// Feedback manager structure - with state queue
struct feedback_manager {
    uint8_t led_gpio;                  // GPIO pin for LED
    led_strip_handle_t led_strip;      // LED strip handle
    
    // State queue management
    feedback_state_entry_t state_queue[FEEDBACK_QUEUE_SIZE];
    uint8_t queue_head;                // Next write position
    uint8_t queue_count;               // Number of entries in queue
    feedback_state_t current_state;    // Currently active state
    feedback_priority_t current_priority; // Priority of current state
    
    TaskHandle_t task_handle;          // Handle for background task
    bool is_initialized;               // Initialization state
    bool is_active;                    // Whether manager is active
    
    // Synchronization
    SemaphoreHandle_t queue_mutex;     // Protect queue operations
};

// Forward declarations
static void feedback_manager_task(void *arg);
static esp_err_t queue_state_change(struct feedback_manager *manager, feedback_state_t state, feedback_priority_t priority, uint32_t duration_ms);
static feedback_state_t get_highest_priority_state(struct feedback_manager *manager);
static feedback_priority_t get_state_priority(feedback_state_t state);

// Get color for state
static rgb_color_t get_state_color(feedback_state_t state) {
    switch (state) {
        case FEEDBACK_STATE_BOOTING:
            return COLOR_WHITE;
            
        case FEEDBACK_STATE_IDLE:
            return COLOR_BLUE;  // Strong blue when idle
            
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
            return COLOR_WHITE;
            
        case FEEDBACK_STATE_INIT_TIME:
            return COLOR_WHITE;
            
        case FEEDBACK_STATE_INIT_WEBHOOK:
            return COLOR_WHITE;
            
        case FEEDBACK_STATE_INIT_RFID:
            return COLOR_WHITE;
            
        case FEEDBACK_STATE_INIT_COMPLETE:
            return COLOR_GREEN;
            
        default:
            return COLOR_WHITE;
    }
}

// Get priority for state
static feedback_priority_t get_state_priority(feedback_state_t state) {
    switch (state) {
        // Critical - System errors and validation
        case FEEDBACK_STATE_ERROR:
        case FEEDBACK_STATE_WIFI_FAILED:
        case FEEDBACK_STATE_TIME_SYNC_FAILED:
        case FEEDBACK_STATE_RFID_ERROR:
        case FEEDBACK_STATE_TAG_READ_ERROR:
            return FEEDBACK_PRIORITY_CRITICAL;
            
        // High - Tag events and webhook errors  
        case FEEDBACK_STATE_TAG_DETECTED:
        case FEEDBACK_STATE_WEBHOOK_ERROR:
        case FEEDBACK_STATE_WEBHOOK_SUCCESS:
            return FEEDBACK_PRIORITY_HIGH;
            
        // High - Important connection states that should override others
        case FEEDBACK_STATE_WIFI_AP_MODE:
        case FEEDBACK_STATE_WIFI_CONNECTED:
            return FEEDBACK_PRIORITY_HIGH;
            
        // Medium - Status updates and connection states
        case FEEDBACK_STATE_WIFI_CONNECTING:
        case FEEDBACK_STATE_TIME_SYNCING:
        case FEEDBACK_STATE_TIME_SYNCED:
        case FEEDBACK_STATE_WEBHOOK_SENDING:
        case FEEDBACK_STATE_WEBHOOK_QUEUED:
        case FEEDBACK_STATE_RFID_INITIALIZING:
        case FEEDBACK_STATE_RFID_ACTIVE:
        case FEEDBACK_STATE_INIT_COMPLETE:
            return FEEDBACK_PRIORITY_MEDIUM;
            
        // Low - Ambient and idle states
        case FEEDBACK_STATE_IDLE:
        case FEEDBACK_STATE_BOOTING:
        case FEEDBACK_STATE_INIT_START:
        case FEEDBACK_STATE_INIT_FS:
        case FEEDBACK_STATE_INIT_WIFI_PREP:
        case FEEDBACK_STATE_INIT_TIME:
        case FEEDBACK_STATE_INIT_WEBHOOK:
        case FEEDBACK_STATE_INIT_RFID:
        default:
            return FEEDBACK_PRIORITY_LOW;
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

// Breathing effect - simplified
static uint8_t breathing_effect(uint32_t cycle_count) {
    float t = ((float)(cycle_count % BREATHING_PERIOD)) / BREATHING_PERIOD;
    float value = sinf(t * 2.0f * 3.14159f - 3.14159f/2.0f);
    value = (value + 1.0f) / 2.0f;
    value = value * value;
    return (uint8_t)(value * 255);
}

// Queue state change with priority
static esp_err_t queue_state_change(struct feedback_manager *manager, feedback_state_t state, feedback_priority_t priority, uint32_t duration_ms) {
    if (manager == NULL || manager->queue_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Take mutex with timeout
    if (xSemaphoreTake(manager->queue_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take queue mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    // Check if queue is full
    if (manager->queue_count >= FEEDBACK_QUEUE_SIZE) {
        // Remove lowest priority entry to make space
        uint8_t lowest_idx = 0;
        feedback_priority_t lowest_priority = FEEDBACK_PRIORITY_CRITICAL;
        
        for (uint8_t i = 0; i < manager->queue_count; i++) {
            if (manager->state_queue[i].priority < lowest_priority) {
                lowest_priority = manager->state_queue[i].priority;
                lowest_idx = i;
            }
        }
        
        // Only remove if new state has higher priority
        if (priority <= lowest_priority) {
            xSemaphoreGive(manager->queue_mutex);
            return ESP_ERR_NO_MEM;
        }
        
        // Remove lowest priority entry
        for (uint8_t i = lowest_idx; i < manager->queue_count - 1; i++) {
            manager->state_queue[i] = manager->state_queue[i + 1];
        }
        manager->queue_count--;
    }
    
    // Add new entry
    uint8_t insert_idx = manager->queue_count;
    
    // Find insertion point (keep sorted by priority)
    for (uint8_t i = 0; i < manager->queue_count; i++) {
        if (priority > manager->state_queue[i].priority) {
            insert_idx = i;
            break;
        }
    }
    
    // Shift entries to make space
    for (uint8_t i = manager->queue_count; i > insert_idx; i--) {
        manager->state_queue[i] = manager->state_queue[i - 1];
    }
    
    // Insert new entry
    manager->state_queue[insert_idx].state = state;
    manager->state_queue[insert_idx].priority = priority;
    manager->state_queue[insert_idx].timestamp = esp_timer_get_time() / 1000; // Convert to ms
    manager->state_queue[insert_idx].duration_ms = duration_ms;
    manager->queue_count++;
    
    ESP_LOGD(TAG, "Queued state %d (priority %d, duration %lums), queue count: %d", 
             (int)state, (int)priority, (unsigned long)duration_ms, (int)manager->queue_count);
    
    xSemaphoreGive(manager->queue_mutex);
    return ESP_OK;
}

// Get highest priority state from queue
static feedback_state_t get_highest_priority_state(struct feedback_manager *manager) {
    if (manager == NULL) {
        ESP_LOGE(TAG, "get_highest_priority_state: manager is NULL");
        return FEEDBACK_STATE_IDLE;
    }
    
    if (manager->queue_mutex == NULL) {
        ESP_LOGE(TAG, "get_highest_priority_state: mutex is NULL");
        return FEEDBACK_STATE_IDLE;
    }
    
    if (manager->queue_count == 0) {
        ESP_LOGD(TAG, "get_highest_priority_state: queue is empty, returning IDLE");
        return FEEDBACK_STATE_IDLE;
    }
    
    if (xSemaphoreTake(manager->queue_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return manager->current_state; // Return current if can't take mutex
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    feedback_state_t result_state = FEEDBACK_STATE_IDLE;
    
    // Clean expired entries and find highest priority
    uint8_t write_idx = 0;
    for (uint8_t read_idx = 0; read_idx < manager->queue_count; read_idx++) {
        feedback_state_entry_t *entry = &manager->state_queue[read_idx];
        
        // Check if entry has expired
        bool expired = (entry->duration_ms > 0) && 
                      ((current_time - entry->timestamp) >= entry->duration_ms);
        
        if (!expired) {
            // Keep this entry
            if (write_idx != read_idx) {
                manager->state_queue[write_idx] = *entry;
            }
            
            // First non-expired entry is highest priority (queue is sorted)
            if (write_idx == 0) {
                result_state = entry->state;
                manager->current_priority = entry->priority;
                ESP_LOGD(TAG, "Selected highest priority state: %d (priority %d)", 
                         (int)result_state, (int)manager->current_priority);
            }
            
            write_idx++;
        }
    }
    
    manager->queue_count = write_idx;
    
    // If no entries, default to idle
    if (manager->queue_count == 0) {
        result_state = FEEDBACK_STATE_IDLE;
        manager->current_priority = FEEDBACK_PRIORITY_LOW;
    }
    
    xSemaphoreGive(manager->queue_mutex);
    return result_state;
}

// Initialize feedback manager - WS2812 only
feedback_manager_handle_t feedback_manager_init(uint8_t led_gpio) {
    ESP_LOGI(TAG, "Initializing on GPIO %d", led_gpio);
    
    struct feedback_manager *manager = calloc(1, sizeof(struct feedback_manager));
    if (manager == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }
    
    manager->led_gpio = led_gpio;
    
    // Initialize state queue
    manager->queue_head = 0;
    manager->queue_count = 0;
    manager->current_state = FEEDBACK_STATE_BOOTING;
    manager->current_priority = FEEDBACK_PRIORITY_LOW;
    
    // Create mutex for queue synchronization
    manager->queue_mutex = xSemaphoreCreateMutex();
    if (manager->queue_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create queue mutex");
        free(manager);
        return NULL;
    }
    
    // Set active state BEFORE creating task to avoid race condition
    manager->is_initialized = false; // Will be set true after LED init
    manager->is_active = true;       // Must be true before task starts
    
    // Initialize WS2812B LED
    led_strip_config_t strip_config = {
        .strip_gpio_num = led_gpio,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &manager->led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        free(manager);
        return NULL;
    }
    
    // Start task first, then do boot pattern
    BaseType_t task_result = xTaskCreate(feedback_manager_task, "feedback_task", 3072, manager, 2, &manager->task_handle);
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create feedback task");
        free(manager);
        return NULL;
    }
    ESP_LOGI(TAG, "Feedback task created successfully");
    
    // Now mark as fully initialized
    manager->is_initialized = true;
    
    // Initialize with booting state, then permanent idle state
    vTaskDelay(pdMS_TO_TICKS(100)); // Let task start
    queue_state_change(manager, FEEDBACK_STATE_IDLE, FEEDBACK_PRIORITY_LOW, 0); // Permanent idle baseline
    queue_state_change(manager, FEEDBACK_STATE_BOOTING, FEEDBACK_PRIORITY_LOW, 300); // Brief boot indication
    
    ESP_LOGI(TAG, "Initialized successfully");
    return manager;
}

// Deinitialize feedback manager
esp_err_t feedback_manager_deinit(feedback_manager_handle_t handle) {
    if (handle == NULL) return ESP_ERR_INVALID_ARG;
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    
    manager->is_active = false;
    if (manager->task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(manager->task_handle);
    }
    
    if (manager->led_strip != NULL) {
        led_strip_clear(manager->led_strip);
        led_strip_refresh(manager->led_strip);
        led_strip_del(manager->led_strip);
    }
    
    // Clean up mutex
    if (manager->queue_mutex != NULL) {
        vSemaphoreDelete(manager->queue_mutex);
    }
    
    free(manager);
    return ESP_OK;
}

// Set primary system state - now uses priority queue
esp_err_t feedback_manager_set_state(feedback_manager_handle_t handle, feedback_state_t state) {
    if (handle == NULL || state >= FEEDBACK_STATE_MAX) return ESP_ERR_INVALID_ARG;
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    
    // Get priority for this state
    feedback_priority_t priority = get_state_priority(state);
    
    // Queue the state change with appropriate duration
    uint32_t duration_ms = 0; // Permanent by default
    
    // Some states are temporary and should auto-expire
    switch (state) {
        case FEEDBACK_STATE_TAG_DETECTED:
        case FEEDBACK_STATE_WEBHOOK_SUCCESS:
            duration_ms = 2000; // 2 seconds
            break;
        case FEEDBACK_STATE_WIFI_CONNECTED:
            duration_ms = 1000; // 1 second - show connected briefly then return to idle
            break;
        case FEEDBACK_STATE_INIT_COMPLETE:
            duration_ms = 1000; // 1 second
            break;
        default:
            duration_ms = 0; // Permanent
            break;
    }
    
    return queue_state_change(manager, state, priority, duration_ms);
}

// Flash a temporary state indication - now uses queue
esp_err_t feedback_manager_flash_event(feedback_manager_handle_t handle, feedback_state_t state, int count) {
    if (handle == NULL || state >= FEEDBACK_STATE_MAX || count <= 0) return ESP_ERR_INVALID_ARG;
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    
    // Queue multiple flash events with high priority and short duration
    feedback_priority_t priority = get_state_priority(state);
    if (priority < FEEDBACK_PRIORITY_HIGH) {
        priority = FEEDBACK_PRIORITY_HIGH; // Flash events should have high priority
    }
    
    // Schedule multiple flash events with gaps
    for (int i = 0; i < count; i++) {
        // Schedule the flash state
        esp_err_t ret = queue_state_change(manager, state, priority, 200); // 200ms duration
        if (ret != ESP_OK) {
            return ret;
        }
        
        // Small delay to ensure states are queued in order
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    return ESP_OK;
}

// Clear all states and reset to default idle state
esp_err_t feedback_manager_reset(feedback_manager_handle_t handle) {
    if (handle == NULL) return ESP_ERR_INVALID_ARG;
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    
    // Clear queue and reset to idle state
    if (manager->queue_mutex != NULL && xSemaphoreTake(manager->queue_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        manager->queue_count = 0;
        xSemaphoreGive(manager->queue_mutex);
    }
    
    // Queue idle state
    return queue_state_change(manager, FEEDBACK_STATE_IDLE, FEEDBACK_PRIORITY_LOW, 0);
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

// Validate an initialization step - non-blocking queue-based
esp_err_t feedback_manager_validate_init_step(feedback_manager_handle_t handle, bool success) {
    if (handle == NULL) return ESP_ERR_INVALID_ARG;
    
    struct feedback_manager *manager = (struct feedback_manager *)handle;
    
    if (success) {
        // Queue a brief green flash for success
        return queue_state_change(manager, FEEDBACK_STATE_WEBHOOK_SUCCESS, FEEDBACK_PRIORITY_HIGH, 100);
    } else {
        // Queue error flash sequence for failure
        feedback_manager_flash_event(handle, FEEDBACK_STATE_ERROR, 3);
        return ESP_OK;
    }
}

// Feedback manager task - queue-based WS2812B control
static void feedback_manager_task(void *arg) {
    struct feedback_manager *manager = (struct feedback_manager *)arg;
    uint32_t cycle_count = 0;

    ESP_LOGI(TAG, "Feedback task started, manager=%p", manager);
    ESP_LOGI(TAG, "Task initial state: is_active=%d, led_strip=%p, queue_mutex=%p", 
             manager->is_active, manager->led_strip, manager->queue_mutex);

    while (manager->is_active) {
        // Get the highest priority state from queue
        feedback_state_t current_state = get_highest_priority_state(manager);
        
        // Update current state if it changed
        if (current_state != manager->current_state) {
            manager->current_state = current_state;
            ESP_LOGI(TAG, "LED state changed to: %d", current_state);
        }
        
        if (manager->led_strip == NULL) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        
        // Single point of WS2812B LED control
        switch (current_state) {
            case FEEDBACK_STATE_IDLE:
                // Blue breathing effect - apply brightness properly
                {
                    uint8_t intensity = breathing_effect(cycle_count);
                    // Apply both breathing intensity AND configured brightness
                    uint8_t final_brightness = (MAX_BRIGHTNESS * intensity) / 255;
                    uint8_t blue_intensity = (COLOR_BLUE.b * final_brightness) / 255;
                    led_strip_set_pixel(manager->led_strip, 0, 0, 0, blue_intensity);
                    led_strip_refresh(manager->led_strip);
                }
                break;
                
            case FEEDBACK_STATE_WIFI_CONNECTING:
                // Fast blue blinking
                if (cycle_count % 6 < 3) {
                    set_led_color(manager->led_strip, COLOR_BLUE, MAX_BRIGHTNESS);
                } else {
                    set_led_color(manager->led_strip, COLOR_OFF, MAX_BRIGHTNESS);
                }
                break;
                
            case FEEDBACK_STATE_WIFI_AP_MODE:
                // AP mode sequence: Y→B→P (0.3s,0.3s,2.0s)
                {
                    uint32_t ap_cycle = cycle_count % 52;  // 2.6s total cycle
                    if (ap_cycle < 6) {
                        set_led_color(manager->led_strip, COLOR_YELLOW, MAX_BRIGHTNESS);
                    } else if (ap_cycle < 12) {
                        set_led_color(manager->led_strip, COLOR_BLUE, MAX_BRIGHTNESS);
                    } else {
                        set_led_color(manager->led_strip, COLOR_PURPLE, MAX_BRIGHTNESS);
                    }
                }
                break;
                
            case FEEDBACK_STATE_WEBHOOK_ERROR:
                // Critical webhook error: R→R→O (0.2s,0.2s,0.6s) - double red flash + orange
                {
                    uint32_t error_cycle = cycle_count % 20;  // 1.0s total cycle
                    if (error_cycle < 4) {
                        set_led_color(manager->led_strip, COLOR_RED, MAX_BRIGHTNESS);
                    } else if (error_cycle < 8) {
                        set_led_color(manager->led_strip, COLOR_RED, MAX_BRIGHTNESS);
                    } else {
                        set_led_color(manager->led_strip, COLOR_ORANGE, MAX_BRIGHTNESS);
                    }
                }
                break;
                
            case FEEDBACK_STATE_WEBHOOK_QUEUED:
                // Webhook queued: Y→G (0.5s,0.5s) - yellow/green alternating
                if (cycle_count % 20 < 10) {
                    set_led_color(manager->led_strip, COLOR_YELLOW, MAX_BRIGHTNESS);
                } else {
                    set_led_color(manager->led_strip, COLOR_GREEN, MAX_BRIGHTNESS);
                }
                break;
                
            case FEEDBACK_STATE_WIFI_FAILED:
                // WiFi failed: R→O→R (0.3s,0.4s,0.3s) - red/orange/red pattern
                {
                    uint32_t fail_cycle = cycle_count % 20;  // 1.0s total cycle
                    if (fail_cycle < 6) {
                        set_led_color(manager->led_strip, COLOR_RED, MAX_BRIGHTNESS);
                    } else if (fail_cycle < 14) {
                        set_led_color(manager->led_strip, COLOR_ORANGE, MAX_BRIGHTNESS);
                    } else {
                        set_led_color(manager->led_strip, COLOR_RED, MAX_BRIGHTNESS);
                    }
                }
                break;
                
            case FEEDBACK_STATE_RFID_ERROR:
                // RFID error: R→W→R (0.2s,0.6s,0.2s) - red/white/red pattern
                {
                    uint32_t rfid_cycle = cycle_count % 20;  // 1.0s total cycle
                    if (rfid_cycle < 4) {
                        set_led_color(manager->led_strip, COLOR_RED, MAX_BRIGHTNESS);
                    } else if (rfid_cycle < 16) {
                        set_led_color(manager->led_strip, COLOR_WHITE, MAX_BRIGHTNESS);
                    } else {
                        set_led_color(manager->led_strip, COLOR_RED, MAX_BRIGHTNESS);
                    }
                }
                break;
                
            default:
                // All other states display their static color
                {
                    rgb_color_t color = get_state_color(current_state);
                    set_led_color(manager->led_strip, color, MAX_BRIGHTNESS);
                }
                break;
        }
        
        cycle_count++;
        
        // Debug log every 5 seconds to show task is running
        if (cycle_count % 100 == 0) {
            ESP_LOGI(TAG, "Task running, cycle=%lu, current_state=%d, queue_count=%d", 
                     (unsigned long)cycle_count, (int)manager->current_state, (int)manager->queue_count);
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Cleanup
    led_strip_clear(manager->led_strip);
    led_strip_refresh(manager->led_strip);
    vTaskDelete(NULL);
}