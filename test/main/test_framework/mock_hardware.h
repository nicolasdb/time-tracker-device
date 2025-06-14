/*
 * Mock Hardware Framework for Component Isolation Testing
 * Phase 0: Hardware abstraction for testing without physical devices
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// LED Mock Hardware
// =============================================================================

/**
 * @brief LED pattern types for validation
 */
typedef enum {
    LED_PATTERN_OFF = 0,
    LED_PATTERN_SOLID,
    LED_PATTERN_BLINK,
    LED_PATTERN_BREATHING,
    LED_PATTERN_SEQUENCE
} led_pattern_type_t;

/**
 * @brief LED mock state for validation
 */
typedef struct {
    uint8_t red;                        // Red color component (0-255)
    uint8_t green;                      // Green color component (0-255) 
    uint8_t blue;                       // Blue color component (0-255)
    uint32_t timestamp_us;              // Timestamp when state was set
    led_pattern_type_t pattern;         // Current LED pattern
    uint32_t pattern_period_ms;         // Pattern period (for breathing/blinking)
    bool is_active;                     // Whether LED is currently on
} led_mock_state_t;

/**
 * @brief Initialize LED mock hardware
 * @return ESP_OK on success
 */
esp_err_t mock_led_init(void);

/**
 * @brief Cleanup LED mock hardware
 * @return ESP_OK on success
 */
esp_err_t mock_led_cleanup(void);

/**
 * @brief Get current LED state
 * @param state Pointer to store current state
 * @return ESP_OK on success
 */
esp_err_t mock_led_get_current_state(led_mock_state_t *state);

/**
 * @brief Get LED state history
 * @param history Pointer to array of historical states
 * @param count Pointer to store number of history entries
 * @return ESP_OK on success
 */
esp_err_t mock_led_get_state_history(led_mock_state_t **history, size_t *count);

/**
 * @brief Validate breathing pattern timing
 * @param expected_period_ms Expected breathing period in milliseconds
 * @param tolerance_percent Acceptable timing tolerance (0-100%)
 * @return true if pattern matches expected timing
 */
bool mock_led_validate_breathing_pattern(uint32_t expected_period_ms, uint8_t tolerance_percent);

/**
 * @brief Get total number of LED events (for stability testing)
 * @return Number of LED state changes
 */
size_t mock_led_get_event_count(void);

/**
 * @brief Reset LED mock state and history
 * @return ESP_OK on success
 */
esp_err_t mock_led_reset(void);

// =============================================================================
// WiFi Mock Hardware  
// =============================================================================

/**
 * @brief WiFi connection simulation states
 */
typedef enum {
    WIFI_MOCK_DISCONNECTED = 0,
    WIFI_MOCK_CONNECTING,
    WIFI_MOCK_CONNECTED,
    WIFI_MOCK_AP_MODE,
    WIFI_MOCK_CONNECTION_FAILED
} wifi_mock_state_t;

/**
 * @brief WiFi mock configuration
 */
typedef struct {
    wifi_mock_state_t state;            // Current WiFi state
    char ip_address[16];                // Simulated IP address
    int8_t rssi;                        // Simulated signal strength
    uint32_t connection_time_ms;        // Simulated connection time
    bool ntp_synced;                    // NTP synchronization status
    char ssid[32];                      // Connected SSID
} wifi_mock_config_t;

/**
 * @brief Initialize WiFi mock hardware
 * @return ESP_OK on success
 */
esp_err_t mock_wifi_init(void);

/**
 * @brief Cleanup WiFi mock hardware
 * @return ESP_OK on success
 */
esp_err_t mock_wifi_cleanup(void);

/**
 * @brief Simulate WiFi connection
 * @param ssid Network SSID to connect to
 * @param password Network password
 * @param ip_address Simulated IP address to assign
 * @param connection_time_ms Simulated connection delay
 * @return ESP_OK on success
 */
esp_err_t mock_wifi_simulate_connection(const char *ssid, const char *password, 
                                       const char *ip_address, uint32_t connection_time_ms);

/**
 * @brief Simulate WiFi disconnection
 * @return ESP_OK on success
 */
esp_err_t mock_wifi_simulate_disconnection(void);

/**
 * @brief Simulate WiFi AP mode
 * @param ap_ip IP address for AP mode
 * @return ESP_OK on success
 */
esp_err_t mock_wifi_simulate_ap_mode(const char *ap_ip);

/**
 * @brief Simulate NTP synchronization
 * @param success Whether NTP sync should succeed
 * @return ESP_OK on success
 */
esp_err_t mock_wifi_simulate_ntp_sync(bool success);

/**
 * @brief Get current WiFi mock state
 * @param config Pointer to store current configuration
 * @return ESP_OK on success
 */
esp_err_t mock_wifi_get_state(wifi_mock_config_t *config);

/**
 * @brief Check if mock WiFi is connected
 * @return true if connected
 */
bool mock_wifi_is_connected(void);

// =============================================================================
// HTTP Mock Server
// =============================================================================

/**
 * @brief HTTP mock response configuration
 */
typedef struct {
    int status_code;                    // HTTP status code to return
    const char *response_body;          // Response body content
    const char *content_type;           // Content-Type header
    uint32_t response_delay_ms;         // Simulated response delay
    bool simulate_timeout;              // Whether to simulate timeout
    bool simulate_connection_error;     // Whether to simulate connection error
} http_mock_response_t;

/**
 * @brief HTTP request log entry
 */
typedef struct {
    char method[8];                     // HTTP method (GET, POST, etc.)
    char url[256];                      // Request URL
    char headers[512];                  // Request headers
    char body[1024];                    // Request body
    uint32_t timestamp_us;              // Request timestamp
} http_mock_request_t;

/**
 * @brief Initialize HTTP mock server
 * @param port Port number to listen on
 * @return ESP_OK on success
 */
esp_err_t mock_http_server_start(int port);

/**
 * @brief Stop HTTP mock server
 * @return ESP_OK on success
 */
esp_err_t mock_http_server_stop(void);

/**
 * @brief Set mock response for specific endpoint
 * @param endpoint Endpoint path (e.g., "/api/webhook")
 * @param response Response configuration
 * @return ESP_OK on success
 */
esp_err_t mock_http_server_set_response(const char *endpoint, const http_mock_response_t *response);

/**
 * @brief Get request history
 * @param requests Pointer to array of request logs
 * @param count Pointer to store number of requests
 * @return ESP_OK on success
 */
esp_err_t mock_http_server_get_request_history(http_mock_request_t **requests, size_t *count);

/**
 * @brief Clear request history
 * @return ESP_OK on success
 */
esp_err_t mock_http_server_clear_history(void);

/**
 * @brief Validate that expected webhook was called
 * @param expected_endpoint Expected endpoint path
 * @param expected_method Expected HTTP method
 * @param expected_body Expected request body (NULL to skip validation)
 * @return true if request was found
 */
bool mock_http_server_validate_webhook_call(const char *expected_endpoint, 
                                           const char *expected_method,
                                           const char *expected_body);

// =============================================================================
// RFID Mock Hardware
// =============================================================================

/**
 * @brief RFID tag simulation
 */
typedef struct {
    char uid[32];                       // Tag UID string
    uint8_t uid_bytes[10];              // Raw UID bytes
    size_t uid_length;                  // UID length in bytes
    bool is_present;                    // Whether tag is currently present
    uint32_t detection_time_ms;         // Time when tag was detected
} rfid_mock_tag_t;

/**
 * @brief Initialize RFID mock hardware
 * @return ESP_OK on success
 */
esp_err_t mock_rfid_init(void);

/**
 * @brief Cleanup RFID mock hardware
 * @return ESP_OK on success
 */
esp_err_t mock_rfid_cleanup(void);

/**
 * @brief Simulate tag placement
 * @param tag Tag information to simulate
 * @return ESP_OK on success
 */
esp_err_t mock_rfid_simulate_tag_placed(const rfid_mock_tag_t *tag);

/**
 * @brief Simulate tag removal
 * @return ESP_OK on success
 */
esp_err_t mock_rfid_simulate_tag_removed(void);

/**
 * @brief Get currently present tag
 * @param tag Pointer to store current tag information
 * @return ESP_OK if tag present, ESP_ERR_NOT_FOUND if no tag
 */
esp_err_t mock_rfid_get_current_tag(rfid_mock_tag_t *tag);

/**
 * @brief Check if any tag is currently present
 * @return true if tag is present
 */
bool mock_rfid_is_tag_present(void);

#ifdef __cplusplus
}
#endif