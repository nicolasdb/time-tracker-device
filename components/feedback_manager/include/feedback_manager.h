/**
 * @file feedback_manager.h
 * @brief System state feedback manager for visual indicators
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief System state enum for feedback
 */
typedef enum {
    /* System states */
    FEEDBACK_STATE_BOOTING,           /* System is booting up */
    FEEDBACK_STATE_IDLE,              /* System ready, waiting for interaction */
    FEEDBACK_STATE_ERROR,             /* General system error */
    
    /* WiFi states */
    FEEDBACK_STATE_WIFI_CONNECTING,   /* Attempting to connect to WiFi */
    FEEDBACK_STATE_WIFI_CONNECTED,    /* WiFi connected successfully */
    FEEDBACK_STATE_WIFI_FAILED,       /* Failed to connect to WiFi */
    FEEDBACK_STATE_WIFI_AP_MODE,      /* Operating in Access Point mode */
    
    /* Time states */
    FEEDBACK_STATE_TIME_SYNCING,      /* Synchronizing time with NTP */
    FEEDBACK_STATE_TIME_SYNCED,       /* Time synchronized successfully */
    FEEDBACK_STATE_TIME_SYNC_FAILED,  /* Failed to synchronize time */
    
    /* RFID states */
    FEEDBACK_STATE_RFID_INITIALIZING, /* RFID subsystem initializing */
    FEEDBACK_STATE_RFID_ACTIVE,       /* RFID is active and scanning */
    FEEDBACK_STATE_RFID_ERROR,        /* RFID hardware error detected */
    FEEDBACK_STATE_TAG_DETECTED,      /* Tag successfully detected */
    FEEDBACK_STATE_TAG_READ_ERROR,    /* Error reading tag data */
    
    /* Webhook states */
    FEEDBACK_STATE_WEBHOOK_SENDING,   /* Sending data to webhook */
    FEEDBACK_STATE_WEBHOOK_SUCCESS,   /* Data sent successfully */
    FEEDBACK_STATE_WEBHOOK_ERROR,     /* Error sending data to webhook */
    FEEDBACK_STATE_WEBHOOK_QUEUED,    /* Data queued for later sending */
    
    /* Must be last */
    FEEDBACK_STATE_MAX
} feedback_state_t;

/**
 * @brief Feedback manager handle
 */
typedef struct feedback_manager* feedback_manager_handle_t;

/**
 * @brief Initialize feedback manager
 * 
 * @param led_gpio GPIO pin for RGB LED or NeoPixel
 * @return Feedback manager handle or NULL on failure
 */
feedback_manager_handle_t feedback_manager_init(uint8_t led_gpio);

/**
 * @brief Deinitialize feedback manager
 * 
 * @param handle Feedback manager handle
 * @return ESP_OK on success
 */
esp_err_t feedback_manager_deinit(feedback_manager_handle_t handle);

/**
 * @brief Set primary system state
 * 
 * This function sets the primary system state for the feedback manager.
 * The primary state has higher priority and will override the background state.
 * 
 * @param handle Feedback manager handle
 * @param state System state
 * @return ESP_OK on success
 */
esp_err_t feedback_manager_set_state(feedback_manager_handle_t handle, feedback_state_t state);

/**
 * @brief Set background system state
 * 
 * This function sets the background system state for the feedback manager.
 * The background state will be displayed when no primary state is active.
 * 
 * @param handle Feedback manager handle
 * @param state System state
 * @return ESP_OK on success
 */
esp_err_t feedback_manager_set_background_state(feedback_manager_handle_t handle, feedback_state_t state);

/**
 * @brief Flash a temporary state indication
 * 
 * This function flashes a temporary state indication, then returns to the previous state.
 * 
 * @param handle Feedback manager handle
 * @param state State to flash
 * @param count Number of times to flash
 * @return ESP_OK on success
 */
esp_err_t feedback_manager_flash_event(feedback_manager_handle_t handle, feedback_state_t state, int count);

/**
 * @brief Clear all states and reset to default idle state
 * 
 * @param handle Feedback manager handle
 * @return ESP_OK on success
 */
esp_err_t feedback_manager_reset(feedback_manager_handle_t handle);

/**
 * @brief Check if RFID hardware is responding
 * 
 * Tests the RFID hardware by attempting a low-level operation.
 * This can be used to determine if hardware is physically connected.
 * 
 * @param rfid_handle RFID manager handle from rfid_manager
 * @return true if hardware is detected and responding, false otherwise
 */
bool feedback_manager_check_rfid_hardware(void* rfid_handle);

#ifdef __cplusplus
}
#endif
