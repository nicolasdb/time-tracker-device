/**
 * @file webhook_manager.h
 * @brief Webhook manager for sending events to a remote server with offline storage and retry
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Webhook event types
 */
typedef enum {
    WEBHOOK_EVENT_TAG_PLACED,    ///< Tag placed event
    WEBHOOK_EVENT_TAG_REMOVED,   ///< Tag removed event
} webhook_event_type_t;

/**
 * @brief Webhook event data
 */
typedef struct {
    webhook_event_type_t event_type;    ///< Event type
    char tag_uid[32];                   ///< Tag UID string
    char device_id[32];                 ///< Device ID string
    uint32_t timestamp;                 ///< Event timestamp (seconds since epoch)
    bool sent;                          ///< Whether this event has been successfully sent
    int attempts;                       ///< Number of send attempts made
    char tag_type[16];                  ///< Tag type (if available)
} webhook_event_t;

/**
 * @brief Webhook manager handle
 */
typedef struct webhook_manager* webhook_manager_handle_t;

/**
 * @brief Initialize webhook manager with minimal configuration
 * 
 * Note: This does NOT load configuration or log files to prevent stack issues.
 * Call webhook_manager_load_configuration() and webhook_manager_load_log_file()
 * separately after initialization.
 * 
 * @param config_path Path to the webhook config file (in LittleFS)
 * @param log_path Path to the event log file (in LittleFS)
 * @return Webhook manager handle or NULL on failure
 */
webhook_manager_handle_t webhook_manager_init(const char *config_path, const char *log_path);

/**
 * @brief Load configuration from file
 * 
 * @param handle Webhook manager handle
 * @return ESP_OK on success
 */
esp_err_t webhook_manager_load_configuration(webhook_manager_handle_t handle);

/**
 * @brief Load event log from file
 * 
 * @param handle Webhook manager handle
 * @return ESP_OK on success
 */
esp_err_t webhook_manager_load_log_file(webhook_manager_handle_t handle);

/**
 * @brief Clean up webhook manager
 * 
 * @param handle Webhook manager handle
 * @return ESP_OK on success
 */
esp_err_t webhook_manager_deinit(webhook_manager_handle_t handle);

/**
 * @brief Send event to webhook
 * 
 * This function logs the event in the local storage regardless of whether the
 * webhook is available or not. If WiFi is connected, it will try to send the
 * event immediately. If not, it will be stored for later retry.
 * 
 * @param handle Webhook manager handle
 * @param event_type Event type (tag placed or removed)
 * @param tag_uid Tag UID string
 * @param tag_type Tag type string (can be NULL)
 * @return ESP_OK on success (logging the event, not necessarily sending it)
 */
esp_err_t webhook_manager_send_event(webhook_manager_handle_t handle, 
                                   webhook_event_type_t event_type,
                                   const char *tag_uid,
                                   const char *tag_type);

/**
 * @brief Process all pending events
 * 
 * This should be called periodically, especially after WiFi connection is established.
 * It will try to send all unsent events in the log.
 * 
 * @param handle Webhook manager handle
 * @return ESP_OK on success
 */
esp_err_t webhook_manager_process_pending(webhook_manager_handle_t handle);

/**
 * @brief Get number of pending events
 * 
 * @param handle Webhook manager handle
 * @param pending_count Pointer to store the number of pending events
 * @return ESP_OK on success
 */
esp_err_t webhook_manager_get_pending_count(webhook_manager_handle_t handle, int *pending_count);

/**
 * @brief Get webhook status
 * 
 * @param handle Webhook manager handle
 * @param is_configured Returns true if webhook is properly configured
 * @param is_connected Returns true if webhook was successfully connected at least once
 * @return ESP_OK on success
 */
esp_err_t webhook_manager_get_status(webhook_manager_handle_t handle, 
                                   bool *is_configured, 
                                   bool *is_connected);

/**
 * @brief Set the task handle for the webhook processing task
 * 
 * This allows the webhook manager to properly clean up the task on deinit
 * 
 * @param handle Webhook manager handle
 * @param task_handle The task handle to store
 * @return ESP_OK on success
 */
esp_err_t webhook_manager_set_task_handle(webhook_manager_handle_t handle, TaskHandle_t task_handle);

/**
 * @brief Set the device ID to be used in webhook events
 * 
 * @param handle Webhook manager handle
 * @param device_id Device ID string
 * @return ESP_OK on success
 */
esp_err_t webhook_manager_set_device_id(webhook_manager_handle_t handle, const char *device_id);

/**
 * @brief Check if webhook server is available
 * 
 * This function sends a HEAD request to the webhook URL to check if the server
 * is available and responsive.
 * 
 * @param handle Webhook manager handle
 * @return ESP_OK if server is available, ESP_FAIL otherwise
 */
esp_err_t webhook_manager_check_connectivity(webhook_manager_handle_t handle);

#ifdef __cplusplus
}
#endif