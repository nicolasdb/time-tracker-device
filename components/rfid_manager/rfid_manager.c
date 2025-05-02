#include "rfid_manager.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "rfid_manager";

// Event base for RFID events
ESP_EVENT_DEFINE_BASE(RFID_EVENT);

// Using the abobija rc522 library
#if CONFIG_RFID_MODULE_RC522
#include <rc522.h>
#include <driver/rc522_spi.h>
#include <rc522_picc.h>
#endif

// RFID manager structure
struct rfid_manager {
#if CONFIG_RFID_MODULE_RC522
    rc522_driver_handle_t driver;
    rc522_handle_t scanner;
#endif
    esp_event_loop_handle_t event_loop;
    bool scanning_active;
};

// Forward declarations
#if CONFIG_RFID_MODULE_RC522
static void rfid_manager_on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data);
#endif

/**
 * @brief Map Kconfig SPI host value to ESP-IDF SPI host device enum
 * @param config_host SPI host value from Kconfig (1 or 2)
 * @return spi_host_device_t ESP-IDF SPI host device enum
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

// Initialize RFID manager
rfid_manager_handle_t rfid_manager_init(void) {
    ESP_LOGI(TAG, "Initializing RFID manager");
    
    // Allocate memory for the manager
    struct rfid_manager *manager = calloc(1, sizeof(struct rfid_manager));
    if (manager == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for manager");
        return NULL;
    }
    
    // Initialize event loop
    esp_event_loop_args_t event_loop_args = {
        .queue_size = 5,
        .task_name = "rfid_event_loop",
        .task_priority = 5,
        .task_stack_size = 4096,  // Doubled from 2048 to 4096
        .task_core_id = tskNO_AFFINITY
    };
    
    esp_err_t ret = esp_event_loop_create(&event_loop_args, &manager->event_loop);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        free(manager);
        return NULL;
    }
    
#if CONFIG_RFID_MODULE_RC522
    ESP_LOGI(TAG, "Initializing RC522 module");
    
    // Configure RC522 driver
    rc522_spi_config_t driver_config = {
        .host_id = get_spi_host(CONFIG_RFID_RC522_SPI_HOST),
        .bus_config = &(spi_bus_config_t){
            .miso_io_num = CONFIG_RFID_RC522_SPI_MISO,
            .mosi_io_num = CONFIG_RFID_RC522_SPI_MOSI,
            .sclk_io_num = CONFIG_RFID_RC522_SPI_SCLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 0,
        },
        .dev_config = {
            .spics_io_num = CONFIG_RFID_RC522_SPI_CS,
            .clock_speed_hz = 1000000,  // 1MHz clock speed
            .mode = 0,                  // SPI mode 0
            .queue_size = 1,
        },
        .rst_io_num = CONFIG_RFID_RST_GPIO,
    };
    
    // Create RC522 driver
    ESP_LOGI(TAG, "Creating RC522 SPI driver with MISO=%d, MOSI=%d, SCK=%d, SS=%d, RST=%d", 
             CONFIG_RFID_RC522_SPI_MISO, CONFIG_RFID_RC522_SPI_MOSI, 
             CONFIG_RFID_RC522_SPI_SCLK, CONFIG_RFID_RC522_SPI_CS, CONFIG_RFID_RST_GPIO);
    ret = rc522_spi_create(&driver_config, &manager->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RC522 driver: %s", esp_err_to_name(ret));
        esp_event_loop_delete(manager->event_loop);
        free(manager);
        return NULL;
    }
    ESP_LOGI(TAG, "RC522 SPI driver created successfully");
    
    // Install RC522 driver
    ESP_LOGI(TAG, "Installing RC522 driver");
    ret = rc522_driver_install(manager->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install RC522 driver: %s", esp_err_to_name(ret));
        // TODO: Missing cleanup of the driver, but API doesn't have a "delete" function
        esp_event_loop_delete(manager->event_loop);
        free(manager);
        return NULL;
    }
    ESP_LOGI(TAG, "RC522 driver installed successfully");
    
    // Configure RC522 scanner
    rc522_config_t scanner_config = {
        .driver = manager->driver,
    };
    
    // Create RC522 scanner
    ESP_LOGI(TAG, "Creating RC522 scanner");
    ret = rc522_create(&scanner_config, &manager->scanner);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RC522 scanner: %s", esp_err_to_name(ret));
        esp_event_loop_delete(manager->event_loop);
        free(manager);
        return NULL;
    }
    ESP_LOGI(TAG, "RC522 scanner created successfully");
    
    // Register RC522 event handler
    ESP_LOGI(TAG, "Registering RC522 event handler");
    ret = rc522_register_events(manager->scanner, RC522_EVENT_PICC_STATE_CHANGED, 
                               rfid_manager_on_picc_state_changed, manager);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register RC522 event handler: %s", esp_err_to_name(ret));
        rc522_destroy(manager->scanner);
        esp_event_loop_delete(manager->event_loop);
        free(manager);
        return NULL;
    }
    ESP_LOGI(TAG, "RC522 event handler registered successfully");
    
    ESP_LOGI(TAG, "RC522 initialization complete");
#endif
    
    return manager;
}

// Deinitialize RFID manager
esp_err_t rfid_manager_deinit(rfid_manager_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_manager *manager = (struct rfid_manager *)handle;
    
    // Stop scanning if active
    if (manager->scanning_active) {
        rfid_manager_stop_scanning(handle);
    }
    
#if CONFIG_RFID_MODULE_RC522
    // Destroy RC522 scanner
    rc522_destroy(manager->scanner);
    
    // Driver can't be deinitialized with current API
#endif
    
    // Delete event loop
    esp_err_t ret = esp_event_loop_delete(manager->event_loop);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete event loop: %s", esp_err_to_name(ret));
    }
    
    // Free memory
    free(manager);
    
    return ESP_OK;
}

// Start scanning for tags
esp_err_t rfid_manager_start_scanning(rfid_manager_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_manager *manager = (struct rfid_manager *)handle;
    
    // Check if already scanning
    if (manager->scanning_active) {
        ESP_LOGW(TAG, "Already scanning for tags");
        return ESP_OK;
    }
    
    esp_err_t ret = ESP_OK;
    
#if CONFIG_RFID_MODULE_RC522
    // Start RC522 scanner
    ESP_LOGI(TAG, "Starting RC522 scanner");
    ret = rc522_start(manager->scanner);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start RC522 scanner: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "RC522 scanner started successfully");
#endif
    
    manager->scanning_active = true;
    ESP_LOGI(TAG, "Started scanning for tags");
    
    return ESP_OK;
}

// Stop scanning for tags
esp_err_t rfid_manager_stop_scanning(rfid_manager_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_manager *manager = (struct rfid_manager *)handle;
    
    // Check if scanning
    if (!manager->scanning_active) {
        ESP_LOGW(TAG, "Not scanning for tags");
        return ESP_OK;
    }
    
    esp_err_t ret = ESP_OK;
    
#if CONFIG_RFID_MODULE_RC522
    // Use rc522_pause instead of rc522_stop
    ret = rc522_pause(manager->scanner);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to pause RC522 scanner: %s", esp_err_to_name(ret));
        return ret;
    }
#endif
    
    manager->scanning_active = false;
    ESP_LOGI(TAG, "Stopped scanning for tags");
    
    return ESP_OK;
}

// Register event handler
esp_err_t rfid_manager_register_event_handler(
    rfid_manager_handle_t handle,
    rfid_event_t event_id,
    esp_event_handler_t event_handler,
    void* event_handler_arg) {
    
    if (handle == NULL || event_handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_manager *manager = (struct rfid_manager *)handle;
    
    return esp_event_handler_register_with(manager->event_loop, 
                                          RFID_EVENT, 
                                          event_id, 
                                          event_handler, 
                                          event_handler_arg);
}

// Unregister event handler
esp_err_t rfid_manager_unregister_event_handler(
    rfid_manager_handle_t handle,
    rfid_event_t event_id,
    esp_event_handler_t event_handler) {
    
    if (handle == NULL || event_handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_manager *manager = (struct rfid_manager *)handle;
    
    return esp_event_handler_unregister_with(manager->event_loop, 
                                           RFID_EVENT, 
                                           event_id, 
                                           event_handler);
}

// Get tag UID as string
esp_err_t rfid_manager_tag_uid_to_string(const rfid_tag_t* tag, char* str, size_t size) {
    if (tag == NULL || str == NULL || size < (tag->uid_length * 2 + 1)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < tag->uid_length; i++) {
        snprintf(str + (i * 2), 3, "%02X", tag->uid[i]);
    }
    
    str[tag->uid_length * 2] = '\0';
    
    return ESP_OK;
}

// Get device UID from chip ID
esp_err_t rfid_manager_get_device_uid(char* str, size_t size) {
    if (str == NULL || size < 13) {  // 12 hex chars + null terminator
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t chipid[6];
    esp_efuse_mac_get_default(chipid);
    
    // Use all 6 bytes of MAC address for device ID
    // This gives us a globally unique identifier
    sprintf(str, "%02X%02X%02X%02X%02X%02X", 
           chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5]);
    
    return ESP_OK;
}

#if CONFIG_RFID_MODULE_RC522
// RC522 event handler
static void rfid_manager_on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    struct rfid_manager *manager = (struct rfid_manager *)arg;
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;
    
    ESP_LOGI(TAG, "RC522 event received: picc state changed from %d to %d", 
             event->old_state, picc->state);
    
    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        // Tag detected
        ESP_LOGI(TAG, "Tag detected");
        rc522_picc_print(picc);
        
        // Create tag data
        rfid_tag_t tag;
        memset(&tag, 0, sizeof(tag));
        
        // Copy the UID from rc522_picc_uid_t structure
        tag.uid_length = picc->uid.length <= 10 ? picc->uid.length : 10;
        memcpy(tag.uid, picc->uid.value, tag.uid_length);
        
        // Copy SAK
        tag.sak = picc->sak;
        
        // Determine tag type (simplified)
        if (picc->sak == 0x08) {
            tag.type = 0; // MIFARE 1K
        } else if (picc->sak == 0x18) {
            tag.type = 1; // MIFARE 4K
        } else if (picc->sak == 0x00) {
            tag.type = 2; // MIFARE Ultralight or NTAG
        } else {
            tag.type = 255; // Unknown
        }
        
        // Create event data
        rfid_tag_event_t event_data;
        memcpy(&event_data.tag, &tag, sizeof(tag));
        event_data.timestamp = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); // Current time in ms
        
        // Post event with timeout to prevent blocking
        esp_err_t ret = esp_event_post_to(manager->event_loop, 
                         RFID_EVENT, 
                         RFID_EVENT_TAG_DETECTED, 
                         &event_data, 
                         sizeof(event_data), 
                         pdMS_TO_TICKS(100)); // 100ms timeout instead of portMAX_DELAY
        
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to post tag detected event: %s", esp_err_to_name(ret));
        }
    }
    else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        // Tag removed
        ESP_LOGI(TAG, "Tag removed");
        
        // Create event data (with empty tag data)
        rfid_tag_event_t event_data;
        memset(&event_data, 0, sizeof(event_data));
        event_data.timestamp = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); // Current time in ms
        
        // Post event with timeout to prevent blocking
        esp_err_t ret = esp_event_post_to(manager->event_loop, 
                         RFID_EVENT, 
                         RFID_EVENT_TAG_REMOVED, 
                         &event_data, 
                         sizeof(event_data), 
                         pdMS_TO_TICKS(100)); // 100ms timeout instead of portMAX_DELAY
        
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to post tag removed event: %s", esp_err_to_name(ret));
        }
    }
}
#endif