/**
 * @file rfid_manager.h
 * @brief Common API for RFID modules
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// Event base for RFID events
ESP_EVENT_DECLARE_BASE(RFID_EVENT);

// RFID events
typedef enum {
    RFID_EVENT_TAG_DETECTED,   // Tag detected
    RFID_EVENT_TAG_REMOVED,    // Tag removed
} rfid_event_t;

// Tag structure
typedef struct {
    uint8_t uid[10];           // UID of the tag (up to 10 bytes)
    uint8_t uid_length;        // Length of the UID
    uint8_t sak;               // SAK (Select Acknowledge) value
    uint8_t type;              // Type of the tag
} rfid_tag_t;

// Tag event data
typedef struct {
    rfid_tag_t tag;            // Tag data
    uint32_t timestamp;        // Timestamp when the event occurred
} rfid_tag_event_t;

// RFID handle
typedef struct rfid_manager* rfid_manager_handle_t;

/**
 * @brief Initialize RFID manager
 * 
 * This function initializes the RFID module based on the Kconfig settings.
 * 
 * @return RFID manager handle on success, NULL on failure
 */
rfid_manager_handle_t rfid_manager_init(void);

/**
 * @brief Deinitialize RFID manager
 * 
 * @param handle RFID manager handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rfid_manager_deinit(rfid_manager_handle_t handle);

/**
 * @brief Start scanning for tags
 * 
 * @param handle RFID manager handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rfid_manager_start_scanning(rfid_manager_handle_t handle);

/**
 * @brief Stop scanning for tags
 * 
 * @param handle RFID manager handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rfid_manager_stop_scanning(rfid_manager_handle_t handle);

/**
 * @brief Register event handler for RFID events
 * 
 * @param handle RFID manager handle
 * @param event_id Event ID (RFID_EVENT_TAG_DETECTED or RFID_EVENT_TAG_REMOVED)
 * @param event_handler Event handler function
 * @param event_handler_arg Event handler argument
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rfid_manager_register_event_handler(
    rfid_manager_handle_t handle,
    rfid_event_t event_id,
    esp_event_handler_t event_handler,
    void* event_handler_arg);

/**
 * @brief Unregister event handler for RFID events
 * 
 * @param handle RFID manager handle
 * @param event_id Event ID (RFID_EVENT_TAG_DETECTED or RFID_EVENT_TAG_REMOVED)
 * @param event_handler Event handler function
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rfid_manager_unregister_event_handler(
    rfid_manager_handle_t handle,
    rfid_event_t event_id,
    esp_event_handler_t event_handler);

/**
 * @brief Get string representation of tag UID
 * 
 * @param tag Tag data
 * @param str Output string buffer
 * @param size Size of the output buffer
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rfid_manager_tag_uid_to_string(const rfid_tag_t* tag, char* str, size_t size);

/**
 * @brief Get device UID from chip ID
 * 
 * @param str Output string buffer to store device ID in format "NFC_XXXXXX"
 * @param size Size of the output buffer (should be at least 10 bytes)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rfid_manager_get_device_uid(char* str, size_t size);

#ifdef __cplusplus
}
#endif