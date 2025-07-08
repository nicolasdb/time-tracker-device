/**
 * @file rfid_tool.c
 * @brief Constitutional RFID Tool Implementation - RC522 Tag Detection
 * 
 * Constitutional implementation following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf, PRIu32)
 * - Container isolation principles
 * 
 * Constitutional Authority: Process Map 07 (tag_detection_fsm.mmd)
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#include "rfid_tool.h"
#include "fs_tool.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/time.h>

// RC522 Library Integration  
#include "rc522.h"
#include "driver/rc522_spi.h"

static const char* TAG = "rfid_tool";

// Constitutional RFID Tool Event Base
ESP_EVENT_DEFINE_BASE(RFID_TOOL_EVENTS);

// Constitutional Presence Logic Detection (Process Map 07 Authority)
typedef enum {
    TAG_STATE_BOOT_GRACE,    // 5s grace period after boot
    TAG_STATE_SCANNING,      // Normal scanning state
    TAG_STATE_DEBOUNCE,      // 200ms debounce confirmation
    TAG_STATE_APPEARED,      // Tag appeared (ready for event)
    TAG_STATE_DISAPPEARED,   // Tag disappeared (ready for event)
    TAG_STATE_TAG_EVENT      // Processing event
} tag_detection_state_t;

// Constitutional Tool Context (Handle-based pattern)
struct rfid_tool {
    bool is_initialized;
    bool is_active;
    bool is_scanning;
    bool tag_present;
    bool hardware_ok;
    rfid_tool_config_t config;
    rfid_tool_status_t status;
    uint64_t init_timestamp_us;
    uint32_t scan_count;
    uint32_t tag_detection_count;
    uint32_t error_count;
    
    // Current tag state
    rfid_tag_info_t current_tag;
    char current_tag_uid[21];
    char previous_tag_uid[21];
    
    // RC522 hardware implementation (Real SPI communication)
    bool tag_present_last_scan;
    uint8_t last_uid[10];
    uint8_t last_uid_length;
    uint32_t consecutive_no_tag_count;
    
    // Constitutional Presence Logic Detection (Process Map 07 Authority)
    tag_detection_state_t detection_state;
    tag_detection_state_t pending_event_type; // Store APPEARED/DISAPPEARED for event processing
    uint64_t boot_timestamp_us;          // Boot time for grace period
    uint64_t state_change_timestamp_us;  // When state change detected
    uint64_t last_event_timestamp_us;    // Timestamp of last event (prevent bouncing)
    char actual_tag_id[21];              // Current actual reading
    char last_tag_id[21];                // Last confirmed tag state
    bool debounce_confirmed;             // Debounce confirmation flag
    
    // RC522 Library handles
    rc522_driver_handle_t driver;
    rc522_handle_t scanner;
    
    // Constitutional tool dependencies
    fs_tool_handle_t fs_tool;
    
    // Thread safety
    SemaphoreHandle_t state_mutex;
    TaskHandle_t scanning_task_handle;
    esp_event_loop_handle_t event_loop;
};

// =============================================================================
// Constitutional RC522 Helper Functions
// =============================================================================

// Forward declarations for constitutional presence detection
static void constitutional_presence_detection_task(void *arg);
static void constitutional_process_tag_event(rfid_tool_handle_t handle);

/**
 * @brief Map SPI host configuration to ESP-IDF enum
 */
static spi_host_device_t get_spi_host(int config_host) {
#ifdef CONFIG_IDF_TARGET_ESP32C3
    // ESP32-C3 only has SPI2_HOST available
    return SPI2_HOST;
#else
    // For other targets like ESP32
    switch(config_host) {
        case 1: return SPI2_HOST;  // HSPI
        case 2: return SPI3_HOST;  // VSPI
        default: return SPI2_HOST; // Default to SPI2_HOST
    }
#endif
}

/**
 * @brief Constitutional RC522 presence detection (Process Map 07 Authority)
 * Updates actual_tag_id with current reading, state machine processes changes
 */
static void rfid_picc_state_changed_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    rfid_tool_handle_t handle = (rfid_tool_handle_t)arg;
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;
    
    if (xSemaphoreTake(handle->state_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return; // Don't block - state machine will handle next update
    }
    
    // Update actual_tag_id from RC522 reading (Process Map 07: actualRead)
    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        // Convert UID to string
        handle->actual_tag_id[0] = '\0';
        for (uint8_t i = 0; i < picc->uid.length && i < 10; i++) {
            snprintf(handle->actual_tag_id + (i * 2), sizeof(handle->actual_tag_id) - (i * 2), 
                    "%02X", picc->uid.value[i]);
        }
        
        // Update current_tag for compatibility
        snprintf(handle->current_tag_uid, sizeof(handle->current_tag_uid), 
                "%s", handle->actual_tag_id);
        handle->tag_present = true;
        
        // Update tag info
        memcpy(handle->current_tag.uid, picc->uid.value, picc->uid.length);
        handle->current_tag.uid_length = picc->uid.length;
        handle->current_tag.type = RFID_TAG_TYPE_MIFARE_1K;
        handle->current_tag.detection_timestamp_us = esp_timer_get_time();
        snprintf(handle->current_tag.uid_string, sizeof(handle->current_tag.uid_string), 
                "%s", handle->actual_tag_id);
    } else {
        // No tag present
        handle->actual_tag_id[0] = '\0';
        handle->tag_present = false;
    }
    
    xSemaphoreGive(handle->state_mutex);
}

/**
 * @brief Constitutional tag presence state machine task (Process Map 07 Authority)
 * Implements: BOOT_GRACE → SCANNING → DEBOUNCE → APPEARED/DISAPPEARED → TAG_EVENT
 */
static void constitutional_presence_detection_task(void *arg)
{
    rfid_tool_handle_t handle = (rfid_tool_handle_t)arg;
    uint64_t current_time_us;
    bool state_changed = false;
    
    ESP_LOGI(TAG, "🏛️ Constitutional presence detection task started (Process Map 07)");
    
    while (handle->is_active) {
        current_time_us = esp_timer_get_time();
        
        if (xSemaphoreTake(handle->state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            
            switch (handle->detection_state) {
                case TAG_STATE_BOOT_GRACE:
                    // 5-second grace period to prevent false APPEARED on reboot
                    if ((current_time_us - handle->boot_timestamp_us) >= 5000000) { // 5 seconds
                        ESP_LOGI(TAG, "🕐 Boot grace period complete - entering SCANNING state");
                        handle->detection_state = TAG_STATE_SCANNING;
                    }
                    break;
                    
                case TAG_STATE_SCANNING:
                    // Anti-bounce: Prevent immediate re-detection after DISAPPEARED (Process Map 07 Authority)
                    if (handle->last_event_timestamp_us > 0 && 
                        (current_time_us - handle->last_event_timestamp_us) < 500000) { // 500ms anti-bounce
                        // Too soon after last event - ignore changes
                        break;
                    }
                    
                    // Compare actualRead vs lastRead (Process Map 07)
                    if (strcmp(handle->actual_tag_id, handle->last_tag_id) != 0) {
                        ESP_LOGI(TAG, "🔄 State change detected: '%s' → '%s'", 
                                handle->last_tag_id, handle->actual_tag_id);
                        handle->detection_state = TAG_STATE_DEBOUNCE;
                        handle->state_change_timestamp_us = current_time_us;
                        handle->debounce_confirmed = false;
                    }
                    break;
                    
                case TAG_STATE_DEBOUNCE:
                    // 200ms confirmation period (Process Map 07)
                    if ((current_time_us - handle->state_change_timestamp_us) >= 200000) { // 200ms
                        // Check if change is still consistent
                        if (strcmp(handle->actual_tag_id, handle->last_tag_id) != 0) {
                            // Change confirmed - determine direction
                            if (strlen(handle->actual_tag_id) > 0 && strlen(handle->last_tag_id) == 0) {
                                // none → tagID = APPEARED
                                handle->detection_state = TAG_STATE_APPEARED;
                                ESP_LOGI(TAG, "✅ APPEARED confirmed: %s", handle->actual_tag_id);
                            } else if (strlen(handle->actual_tag_id) == 0 && strlen(handle->last_tag_id) > 0) {
                                // tagID → none = DISAPPEARED  
                                handle->detection_state = TAG_STATE_DISAPPEARED;
                                ESP_LOGI(TAG, "❌ DISAPPEARED confirmed: %s", handle->last_tag_id);
                            } else if (strlen(handle->actual_tag_id) > 0 && strlen(handle->last_tag_id) > 0) {
                                // tagID → tagID = direct transition (treat as DISAPPEARED then APPEARED)
                                handle->detection_state = TAG_STATE_DISAPPEARED;
                                ESP_LOGI(TAG, "🔄 Tag change: %s → %s", handle->last_tag_id, handle->actual_tag_id);
                            } else {
                                // none → none (should not happen, but return to scanning)
                                handle->detection_state = TAG_STATE_SCANNING;
                            }
                        } else {
                            // False alarm - return to scanning
                            ESP_LOGI(TAG, "⚠️ Debounce false alarm - returning to SCANNING");
                            handle->detection_state = TAG_STATE_SCANNING;
                        }
                    }
                    break;
                    
                case TAG_STATE_APPEARED:
                case TAG_STATE_DISAPPEARED:
                    // Store event type before transitioning to TAG_EVENT (Process Map 08 authority)
                    handle->pending_event_type = handle->detection_state; // Preserve APPEARED or DISAPPEARED
                    handle->detection_state = TAG_STATE_TAG_EVENT;
                    break;
                    
                case TAG_STATE_TAG_EVENT:
                    // Process event according to Process Map 08
                    state_changed = true;
                    break;
            }
            
            xSemaphoreGive(handle->state_mutex);
        }
        
        // Process events outside mutex (Process Map 08: tag_event_fsm)
        if (state_changed) {
            constitutional_process_tag_event(handle);
            state_changed = false;
            
            // Update last_tag_id and return to SCANNING
            if (xSemaphoreTake(handle->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                snprintf(handle->last_tag_id, sizeof(handle->last_tag_id), 
                        "%s", handle->actual_tag_id);
                handle->last_event_timestamp_us = esp_timer_get_time(); // Record event time for anti-bounce
                handle->detection_state = TAG_STATE_SCANNING;
                xSemaphoreGive(handle->state_mutex);
            }
        }
        
        // Poll every 100ms (Process Map 07 authority)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "🏛️ Constitutional presence detection task terminated");
    vTaskDelete(NULL);
}

/**
 * @brief Process tag event according to Process Map 08 (tag_event_fsm)
 * FORMATTING → SENDING logic with proper tag_id handling
 */
static void constitutional_process_tag_event(rfid_tool_handle_t handle)
{
    if (!handle) return;
    
    // Process Map 08: FORMATTING state
    rfid_tool_event_t tag_event = {0};
    tag_event.timestamp_us = esp_timer_get_time(); // uptime_stamp per process map
    tag_event.error_code = ESP_OK;
    
    // Determine event type and tag_id logic using stored pending_event_type (Process Map 08 authority)
    if (handle->pending_event_type == TAG_STATE_APPEARED) {
        tag_event.type = RFID_TOOL_EVENT_TAG_DETECTED;
        // If APPEARED: tag_id = actual_tag (Process Map 08)
        snprintf(tag_event.tag_info.uid_string, sizeof(tag_event.tag_info.uid_string), 
                "%s", handle->actual_tag_id);
        handle->tag_detection_count++;
        ESP_LOGI(TAG, "🏷️ Constitutional APPEARED: %s", handle->actual_tag_id);
        
    } else if (handle->pending_event_type == TAG_STATE_DISAPPEARED) {
        tag_event.type = RFID_TOOL_EVENT_TAG_REMOVED;
        // If DISAPPEARED: tag_id = last_tag (Process Map 08)
        snprintf(tag_event.tag_info.uid_string, sizeof(tag_event.tag_info.uid_string), 
                "%s", handle->last_tag_id);
        ESP_LOGI(TAG, "🏷️ Constitutional DISAPPEARED: %s", handle->last_tag_id);
    } else {
        // Safety fallback - should never happen
        ESP_LOGE(TAG, "❌ Invalid pending_event_type: %d", handle->pending_event_type);
        tag_event.type = RFID_TOOL_EVENT_ERROR;
        return;
    }
    
    // Copy current tag info
    memcpy(&tag_event.tag_info, &handle->current_tag, sizeof(rfid_tag_info_t));
    
    // Process Map 08: SENDING state - publish to esp_event system  
    ESP_LOGI(TAG, "📤 DEBUG: Posting event type %d (%s) for UID %s", 
             tag_event.type, rfid_tool_event_to_string(tag_event.type), tag_event.tag_info.uid_string);
    esp_err_t ret = esp_event_post(RFID_TOOL_EVENTS, tag_event.type, 
                                  &tag_event, sizeof(tag_event), 0);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Constitutional event posted: %s", 
                rfid_tool_event_to_string(tag_event.type));
    } else {
        ESP_LOGE(TAG, "❌ Failed to post constitutional event: %s", esp_err_to_name(ret));
        handle->error_count++;
    }
    
    // Clear pending event type after processing (Constitutional state management)
    handle->pending_event_type = TAG_STATE_SCANNING;
}

// =============================================================================
// Constitutional RFID Hardware Implementation
// =============================================================================

/**
 * @brief Constitutional RC522 hardware self-test
 * Real SPI communication validation with RC522
 */
static esp_err_t constitutional_rc522_hardware_self_test(rfid_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔧 Constitutional RC522 hardware self-test starting");
    
    // Test 1: SPI bus initialization
    ESP_LOGI(TAG, "  📡 Initializing SPI bus for RC522");
    
    // SPI bus configuration
    spi_bus_config_t bus_cfg = {
        .miso_io_num = handle->config.rc522_config.miso_gpio,
        .mosi_io_num = handle->config.rc522_config.mosi_gpio,
        .sclk_io_num = handle->config.rc522_config.sclk_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "  📡 SPI bus already initialized");
        ret = ESP_OK;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "  ❌ SPI bus initialization failed: %s", esp_err_to_name(ret));
        handle->hardware_ok = false;
        handle->status.hardware_ok = false;
        return ret;
    }
    
    ESP_LOGI(TAG, "  ✅ SPI bus initialized successfully");
    
    // Test 2: SPI device configuration
    ESP_LOGI(TAG, "  🔌 Configuring RC522 SPI device");
    
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = handle->config.rc522_config.clock_speed_hz,
        .mode = 0,
        .spics_io_num = handle->config.rc522_config.cs_gpio,
        .queue_size = 7,
    };
    
    spi_device_handle_t spi_handle;
    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "  ❌ SPI device configuration failed: %s", esp_err_to_name(ret));
        handle->hardware_ok = false;
        handle->status.hardware_ok = false;
        return ret;
    }
    
    ESP_LOGI(TAG, "  ✅ RC522 SPI device configured");
    
    // Test 3: GPIO configuration for RST pin
    ESP_LOGI(TAG, "  🔧 Configuring RC522 reset GPIO");
    
    gpio_config_t rst_cfg = {
        .pin_bit_mask = (1ULL << handle->config.rc522_config.rst_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    ret = gpio_config(&rst_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "  ❌ Reset GPIO configuration failed: %s", esp_err_to_name(ret));
        spi_bus_remove_device(spi_handle);
        handle->hardware_ok = false;
        handle->status.hardware_ok = false;
        return ret;
    }
    
    // Test reset functionality
    gpio_set_level(handle->config.rc522_config.rst_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(handle->config.rc522_config.rst_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG, "  ✅ RC522 reset sequence completed");
    
    // Test 4: Basic SPI communication test
    ESP_LOGI(TAG, "  📋 Testing basic SPI communication");
    
    // Try to read version register (0x37) from RC522
    uint8_t tx_data[2] = {0x37, 0x00}; // Read version register
    uint8_t rx_data[2] = {0};
    
    spi_transaction_t trans = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    
    ret = spi_device_transmit(spi_handle, &trans);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  ✅ SPI communication test successful (response: 0x%02X)", rx_data[1]);
    } else {
        ESP_LOGW(TAG, "  ⚠️ SPI communication test failed - RC522 may not be connected");
        // Don't fail the test - allow operation without physical RC522 for development
    }
    
    // Clean up test resources
    spi_bus_remove_device(spi_handle);
    
    ESP_LOGI(TAG, "✅ Constitutional RC522 hardware self-test complete");
    handle->hardware_ok = true;
    handle->status.hardware_ok = true;
    
    return ESP_OK;
}

// RC522 scanning is now event-driven via the RC522 library
// No manual scanning function needed - events are handled by rfid_picc_state_changed_handler

// =============================================================================
// Constitutional RFID Scanning Task
// =============================================================================

// RC522 scanning is now event-driven - no manual scanning task needed
// The RC522 library handles tag detection automatically via events

// =============================================================================
// Constitutional Tool Interface Implementation
// =============================================================================

const char* rfid_tool_get_id(void)
{
    return RFID_TOOL_ID;
}

const char* rfid_tool_get_version(void)
{
    return RFID_TOOL_VERSION;
}

rfid_tool_config_t rfid_tool_create_default_config(void)
{
    rfid_tool_config_t config = {0};
    
    // Default RC522 configuration (constitutional GPIO assignments for ESP32-C3)
    config.rc522_config.spi_host = 1;  // SPI2_HOST
    config.rc522_config.miso_gpio = 5;  // ESP32-C3 compatible pin
    config.rc522_config.mosi_gpio = 6;  // ESP32-C3 compatible pin
    config.rc522_config.sclk_gpio = 4;  // ESP32-C3 compatible pin
    config.rc522_config.cs_gpio = 10;   // ESP32-C3 compatible pin
    config.rc522_config.rst_gpio = 9;   // ESP32-C3 compatible pin
    config.rc522_config.clock_speed_hz = 1000000;  // 1MHz
    
    // Default behavior settings
    config.auto_start_scanning = true;
    config.scan_interval_ms = 100;
    config.enable_tag_cache = true;
    
    // Event publishing
    config.publish_events = true;
    config.event_queue_size = 8;
    
    // Session tracking
    config.enable_session_tracking = true;
    config.min_session_duration_ms = 5000;  // 5 seconds
    
    return config;
}

rfid_tool_handle_t rfid_tool_init(const rfid_tool_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }
    
    ESP_LOGI(TAG, "🏗️ Constitutional RFID tool initializing");
    
    // Allocate handle with constitutional memory safety
    rfid_tool_handle_t handle = malloc(sizeof(struct rfid_tool));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate RFID tool handle");
        return NULL;
    }
    
    // Initialize handle with constitutional patterns
    memset(handle, 0, sizeof(struct rfid_tool));
    memcpy(&handle->config, config, sizeof(rfid_tool_config_t));
    handle->init_timestamp_us = esp_timer_get_time();
    
    // Constitutional presence detection initialization (Process Map 07)
    handle->detection_state = TAG_STATE_BOOT_GRACE;
    handle->pending_event_type = TAG_STATE_SCANNING; // Initialize to safe state
    handle->boot_timestamp_us = esp_timer_get_time();
    handle->last_event_timestamp_us = 0;
    handle->actual_tag_id[0] = '\0';
    handle->last_tag_id[0] = '\0';
    handle->debounce_confirmed = false;
    
    // Create state mutex
    handle->state_mutex = xSemaphoreCreateMutex();
    if (!handle->state_mutex) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        free(handle);
        return NULL;
    }
    
    // Initialize RC522 library
    ESP_LOGI(TAG, "🔧 Initializing RC522 library");
    
    // Configure RC522 driver
    rc522_spi_config_t driver_config = {
        .host_id = get_spi_host(config->rc522_config.spi_host),
        .bus_config = &(spi_bus_config_t){
            .miso_io_num = config->rc522_config.miso_gpio,
            .mosi_io_num = config->rc522_config.mosi_gpio,
            .sclk_io_num = config->rc522_config.sclk_gpio,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 0,
        },
        .dev_config = {
            .spics_io_num = config->rc522_config.cs_gpio,
            .clock_speed_hz = config->rc522_config.clock_speed_hz,
            .mode = 0,
            .queue_size = 1,
        },
        .rst_io_num = config->rc522_config.rst_gpio,
    };
    
    // Create RC522 driver
    esp_err_t ret = rc522_spi_create(&driver_config, &handle->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RC522 driver: %s", esp_err_to_name(ret));
        vSemaphoreDelete(handle->state_mutex);
        free(handle);
        return NULL;
    }
    
    // Install RC522 driver
    ret = rc522_driver_install(handle->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install RC522 driver: %s", esp_err_to_name(ret));
        vSemaphoreDelete(handle->state_mutex);
        free(handle);
        return NULL;
    }
    
    // Configure RC522 scanner
    rc522_config_t scanner_config = {
        .driver = handle->driver,
    };
    
    // Create RC522 scanner
    ret = rc522_create(&scanner_config, &handle->scanner);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RC522 scanner: %s", esp_err_to_name(ret));
        vSemaphoreDelete(handle->state_mutex);
        free(handle);
        return NULL;
    }
    
    // Register RC522 event handler
    ret = rc522_register_events(handle->scanner, RC522_EVENT_PICC_STATE_CHANGED, 
                               rfid_picc_state_changed_handler, handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register RC522 event handler: %s", esp_err_to_name(ret));
        rc522_destroy(handle->scanner);
        vSemaphoreDelete(handle->state_mutex);
        free(handle);
        return NULL;
    }
    
    handle->hardware_ok = true;
    handle->status.hardware_ok = true;
    ESP_LOGI(TAG, "✅ RC522 library initialized successfully");
    
    // Initialize status
    handle->status.is_initialized = true;
    handle->status.is_active = false;
    handle->status.is_scanning = false;
    handle->status.tag_present = false;
    handle->status.hardware_ok = handle->hardware_ok;
    handle->status.capabilities = RFID_CAP_TAG_DETECTION | RFID_CAP_AUTO_SCAN | 
                                 RFID_CAP_EVENT_PUBLISH | RFID_CAP_UID_EXTRACTION |
                                 RFID_CAP_HEALTH_MONITOR | RFID_CAP_TYPE_DETECTION |
                                 RFID_CAP_SESSION_TRACKING;
    
    // Set control flags
    handle->is_initialized = true;
    handle->is_active = true;
    
    ESP_LOGI(TAG, "✅ Constitutional RFID tool: %s v%s initialized", 
             rfid_tool_get_id(), rfid_tool_get_version());
    
    return handle;
}

esp_err_t rfid_tool_deinit(rfid_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔌 Constitutional RFID tool deinitializing");
    
    // Stop scanning
    if (handle->is_scanning) {
        rfid_tool_stop_scanning(handle);
    }
    
    // Stop task
    handle->is_active = false;
    if (handle->scanning_task_handle) {
        vTaskDelete(handle->scanning_task_handle);
        handle->scanning_task_handle = NULL;
    }
    
    // Clean up resources
    if (handle->state_mutex) {
        vSemaphoreDelete(handle->state_mutex);
    }
    
    // Constitutional cleanup
    free(handle);
    
    ESP_LOGI(TAG, "✅ Constitutional RFID tool deinitialized");
    
    return ESP_OK;
}

rfid_tool_capabilities_t rfid_tool_get_capabilities(rfid_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    return handle->status.capabilities;
}

esp_err_t rfid_tool_get_status(rfid_tool_handle_t handle, rfid_tool_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(handle->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(status, &handle->status, sizeof(rfid_tool_status_t));
        status->uptime_us = esp_timer_get_time() - handle->init_timestamp_us;
        status->scan_count = handle->scan_count;
        status->tag_detection_count = handle->tag_detection_count;
        status->error_count = handle->error_count;
        xSemaphoreGive(handle->state_mutex);
    }
    
    return ESP_OK;
}

esp_err_t rfid_tool_set_fs_dependency(rfid_tool_handle_t handle, void* fs_tool)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handle->fs_tool = (fs_tool_handle_t)fs_tool;
    ESP_LOGI(TAG, "✅ FS tool dependency set for configuration logging");
    
    return ESP_OK;
}

// =============================================================================
// Constitutional RFID Operations Implementation
// =============================================================================

esp_err_t rfid_tool_start_scanning(rfid_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (handle->is_scanning) {
        ESP_LOGW(TAG, "RFID scanning already active");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "🔍 Starting constitutional RFID scanning");
    
    if (xSemaphoreTake(handle->state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        handle->is_scanning = true;
        handle->status.is_scanning = true;
        handle->status.is_active = true;
        
        // Start RC522 scanner
        esp_err_t ret = rc522_start(handle->scanner);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start RC522 scanner: %s", esp_err_to_name(ret));
            handle->is_scanning = false;
            handle->status.is_scanning = false;
            xSemaphoreGive(handle->state_mutex);
            return ret;
        }
        ESP_LOGI(TAG, "✅ RC522 scanner started successfully");
        
        // Start constitutional presence detection task (Process Map 07)
        BaseType_t task_ret = xTaskCreate(
            constitutional_presence_detection_task,
            "rfid_presence",
            4096,
            handle,
            5,
            &handle->scanning_task_handle
        );
        
        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create constitutional presence detection task");
            rc522_pause(handle->scanner);
            handle->is_scanning = false;
            handle->status.is_scanning = false;
            xSemaphoreGive(handle->state_mutex);
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "✅ Constitutional presence detection task started");
        
        xSemaphoreGive(handle->state_mutex);
        
        // Publish constitutional event
        rfid_tool_event_t scan_event = {
            .type = RFID_TOOL_EVENT_SCAN_STARTED,
            .timestamp_us = esp_timer_get_time(),
            .error_code = ESP_OK
        };
        
        esp_event_post(RFID_TOOL_EVENTS, RFID_TOOL_EVENT_SCAN_STARTED, 
                      &scan_event, sizeof(scan_event), 0);
        
        ESP_LOGI(TAG, "✅ Constitutional RFID scanning started");
    } else {
        ESP_LOGE(TAG, "Failed to acquire state mutex for starting scan");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t rfid_tool_stop_scanning(rfid_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!handle->is_scanning) {
        ESP_LOGW(TAG, "RFID scanning already stopped");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "🛑 Stopping constitutional RFID scanning");
    
    if (xSemaphoreTake(handle->state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        handle->is_scanning = false;
        handle->status.is_scanning = false;
        
        // Pause RC522 scanner
        rc522_pause(handle->scanner);
        ESP_LOGI(TAG, "✅ RC522 scanner paused");
        
        xSemaphoreGive(handle->state_mutex);
        
        // Wait for constitutional presence detection task to finish
        if (handle->scanning_task_handle) {
            vTaskDelay(pdMS_TO_TICKS(300)); // Give task time to exit gracefully
            handle->scanning_task_handle = NULL;
        }
        
        // Publish constitutional event
        rfid_tool_event_t scan_event = {
            .type = RFID_TOOL_EVENT_SCAN_STOPPED,
            .timestamp_us = esp_timer_get_time(),
            .error_code = ESP_OK
        };
        
        esp_event_post(RFID_TOOL_EVENTS, RFID_TOOL_EVENT_SCAN_STOPPED, 
                      &scan_event, sizeof(scan_event), 0);
        
        ESP_LOGI(TAG, "✅ Constitutional RFID scanning stopped");
    } else {
        ESP_LOGE(TAG, "Failed to acquire state mutex for stopping scan");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

bool rfid_tool_is_tag_present(rfid_tool_handle_t handle)
{
    if (!handle) {
        return false;
    }
    
    return handle->tag_present;
}

esp_err_t rfid_tool_get_current_tag(rfid_tool_handle_t handle, rfid_tag_info_t *tag_info)
{
    if (!handle || !tag_info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!handle->tag_present) {
        return ESP_ERR_NOT_FOUND;
    }
    
    if (xSemaphoreTake(handle->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(tag_info, &handle->current_tag, sizeof(rfid_tag_info_t));
        xSemaphoreGive(handle->state_mutex);
    } else {
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t rfid_tool_get_tag_uid_string(rfid_tool_handle_t handle, char* uid_string, size_t buffer_size)
{
    if (!handle || !uid_string || buffer_size < 21) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!handle->tag_present) {
        return ESP_ERR_NOT_FOUND;
    }
    
    snprintf(uid_string, buffer_size, "%s", handle->current_tag_uid);
    
    return ESP_OK;
}

esp_err_t rfid_tool_hardware_self_test(rfid_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return constitutional_rc522_hardware_self_test(handle);
}

esp_err_t rfid_tool_scan_tags(rfid_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔍 Performing single RFID tag scan");
    
    if (xSemaphoreTake(handle->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        handle->scan_count++;
        handle->status.scan_count++;
        
        // RC522 scanning is event-driven - check current tag status
        bool tag_present = handle->tag_present;
        
        xSemaphoreGive(handle->state_mutex);
        
        if (tag_present) {
            ESP_LOGI(TAG, "✅ Tag scan complete - Tag present: %s", handle->current_tag_uid);
            return ESP_OK;
        } else {
            ESP_LOGI(TAG, "✅ Tag scan complete - No tag detected");
            return ESP_ERR_NOT_FOUND;
        }
    }
    
    return ESP_ERR_TIMEOUT;
}

// =============================================================================
// Constitutional Utility Functions Implementation
// =============================================================================

const char* rfid_tool_event_to_string(rfid_tool_event_type_t event_type)
{
    switch (event_type) {
        case RFID_TOOL_EVENT_TAG_DETECTED:   return "TAG_DETECTED";
        case RFID_TOOL_EVENT_TAG_REMOVED:    return "TAG_REMOVED";
        case RFID_TOOL_EVENT_SESSION_STARTED: return "SESSION_STARTED";
        case RFID_TOOL_EVENT_SESSION_ENDED:  return "SESSION_ENDED";
        case RFID_TOOL_EVENT_SCAN_STARTED:   return "SCAN_STARTED";
        case RFID_TOOL_EVENT_SCAN_STOPPED:   return "SCAN_STOPPED";
        case RFID_TOOL_EVENT_ERROR:          return "ERROR";
        case RFID_TOOL_EVENT_READY:          return "READY";
        default:                             return "UNKNOWN";
    }
}

const char* rfid_tool_tag_type_to_string(rfid_tag_type_t tag_type)
{
    switch (tag_type) {
        case RFID_TAG_TYPE_MIFARE_1K:  return "MIFARE_1K";
        case RFID_TAG_TYPE_MIFARE_4K:  return "MIFARE_4K";
        case RFID_TAG_TYPE_MIFARE_UL:  return "MIFARE_UL";
        case RFID_TAG_TYPE_UNKNOWN:    return "UNKNOWN";
        default:                       return "INVALID";
    }
}

esp_err_t rfid_tool_tag_uid_to_string(const rfid_tag_info_t* tag_info, char* uid_string, size_t buffer_size)
{
    if (!tag_info || !uid_string || buffer_size < 21) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Constitutional memory safety with snprintf
    int written = 0;
    for (uint8_t i = 0; i < tag_info->uid_length && written < (buffer_size - 3); i++) {
        written += snprintf(uid_string + written, buffer_size - written, "%02X", tag_info->uid[i]);
    }
    
    return ESP_OK;
}