/**
 * @file rfid_tool.h
 * @brief Constitutional RFID Tool Interface - RC522 Tag Detection
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

#ifndef RFID_TOOL_H
#define RFID_TOOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constitutional Tool Metadata
// =============================================================================

#define RFID_TOOL_ID            "rfid_tool"
#define RFID_TOOL_VERSION       "6.1.0"
#define RFID_TOOL_DESCRIPTION   "Constitutional RFID tag detection with RC522 support"

#define RFID_TOOL_MAX_UID_LEN   10
#define RFID_TOOL_MAX_TAGS      8

// =============================================================================
// Constitutional ESP_EVENT System
// =============================================================================

ESP_EVENT_DECLARE_BASE(RFID_TOOL_EVENTS);

/**
 * @brief Constitutional RFID Tool Event Types
 */
typedef enum {
    RFID_TOOL_EVENT_TAG_DETECTED = 0,      ///< New tag placed on reader
    RFID_TOOL_EVENT_TAG_REMOVED,           ///< Tag removed from reader
    RFID_TOOL_EVENT_SESSION_STARTED,       ///< Work session started (time tracking)
    RFID_TOOL_EVENT_SESSION_ENDED,         ///< Work session ended (time tracking)
    RFID_TOOL_EVENT_SCAN_STARTED,          ///< RFID scanning activated
    RFID_TOOL_EVENT_SCAN_STOPPED,          ///< RFID scanning deactivated
    RFID_TOOL_EVENT_ERROR,                 ///< Hardware or communication error
    RFID_TOOL_EVENT_READY,                 ///< Tool ready for scanning
} rfid_tool_event_type_t;

/**
 * @brief Constitutional RFID Tag Types
 */
typedef enum {
    RFID_TAG_TYPE_MIFARE_1K = 0,           ///< MIFARE Classic 1K
    RFID_TAG_TYPE_MIFARE_4K,               ///< MIFARE Classic 4K  
    RFID_TAG_TYPE_MIFARE_UL,               ///< MIFARE Ultralight/NTAG
    RFID_TAG_TYPE_UNKNOWN = 255            ///< Unknown tag type
} rfid_tag_type_t;

/**
 * @brief Constitutional RFID Tag Information
 */
typedef struct {
    uint8_t uid[RFID_TOOL_MAX_UID_LEN];    ///< Tag UID (up to 10 bytes)
    uint8_t uid_length;                    ///< Length of UID in bytes
    uint8_t sak;                           ///< SAK (Select Acknowledge) value
    rfid_tag_type_t type;                  ///< Tag type
    uint64_t detection_timestamp_us;       ///< Constitutional timestamp (microseconds)
    char uid_string[21];                   ///< UID as hex string (10*2+1)
} rfid_tag_info_t;

/**
 * @brief Constitutional RFID Tool Event Data Structure
 */
typedef struct {
    rfid_tool_event_type_t type;
    rfid_tag_info_t tag_info;
    uint64_t timestamp_us;
    esp_err_t error_code;
    char error_message[64];
    char session_id[37];                   ///< Session UUID for event pairing
} rfid_tool_event_t;

// =============================================================================
// Constitutional Tool Configuration
// =============================================================================

/**
 * @brief Constitutional RFID Tool Capabilities (Bitmask)
 */
typedef enum {
    RFID_CAP_TAG_DETECTION    = (1 << 0),  ///< Tag detection support
    RFID_CAP_AUTO_SCAN        = (1 << 1),  ///< Automatic scanning
    RFID_CAP_EVENT_PUBLISH    = (1 << 2),  ///< Event publishing to other tools
    RFID_CAP_UID_EXTRACTION   = (1 << 3),  ///< UID string extraction
    RFID_CAP_HEALTH_MONITOR   = (1 << 4),  ///< Hardware health monitoring
    RFID_CAP_TYPE_DETECTION   = (1 << 5),  ///< Tag type classification
    RFID_CAP_SESSION_TRACKING = (1 << 6),  ///< Time tracking session detection
} rfid_tool_capabilities_t;

/**
 * @brief Constitutional RFID Tool RC522 Configuration
 */
typedef struct {
    int spi_host;                          ///< SPI host (1=SPI2, 2=SPI3)
    int miso_gpio;                         ///< MISO GPIO pin
    int mosi_gpio;                         ///< MOSI GPIO pin  
    int sclk_gpio;                         ///< Clock GPIO pin
    int cs_gpio;                           ///< Chip select GPIO pin
    int rst_gpio;                          ///< Reset GPIO pin
    uint32_t clock_speed_hz;               ///< SPI clock speed
} rfid_tool_rc522_config_t;

/**
 * @brief Constitutional RFID Tool Configuration
 */
typedef struct {
    // Hardware Configuration
    rfid_tool_rc522_config_t rc522_config; ///< RC522 SPI configuration
    
    // Behavior Settings
    bool auto_start_scanning;              ///< Start scanning on init
    uint32_t scan_interval_ms;             ///< Scanning interval
    bool enable_tag_cache;                 ///< Cache detected tags
    
    // Event Publishing
    bool publish_events;                   ///< Enable event publishing
    uint32_t event_queue_size;             ///< Event queue size
    
    // Session Tracking (Constitutional Time Tracking)
    bool enable_session_tracking;          ///< Enable work session detection
    uint32_t min_session_duration_ms;      ///< Minimum valid session duration (5s)
} rfid_tool_config_t;

// =============================================================================
// Constitutional Tool Types & Handles
// =============================================================================

/**
 * @brief Opaque Constitutional RFID Tool Handle
 */
typedef struct rfid_tool* rfid_tool_handle_t;

/**
 * @brief Constitutional RFID Tool Status Information
 */
typedef struct {
    bool is_initialized;                   ///< Tool initialization status
    bool is_active;                        ///< Tool active status
    bool is_scanning;                      ///< Scanning active status
    bool tag_present;                      ///< Current tag presence
    bool hardware_ok;                      ///< RC522 hardware status
    rfid_tag_info_t current_tag;           ///< Currently detected tag
    uint32_t scan_count;                   ///< Total scans performed
    uint32_t tag_detection_count;          ///< Total tags detected
    uint32_t error_count;                  ///< Total errors encountered
    uint64_t uptime_us;                    ///< Tool uptime in microseconds
    rfid_tool_capabilities_t capabilities; ///< Tool capabilities
} rfid_tool_status_t;

// =============================================================================
// Constitutional Tool Interface Functions
// =============================================================================

/**
 * @brief Get RFID tool identifier
 * @return Tool ID string
 */
const char* rfid_tool_get_id(void);

/**
 * @brief Get RFID tool version
 * @return Version string
 */
const char* rfid_tool_get_version(void);

/**
 * @brief Create default RFID tool configuration
 * @return Default configuration structure
 */
rfid_tool_config_t rfid_tool_create_default_config(void);

/**
 * @brief Initialize constitutional RFID tool with configuration
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
rfid_tool_handle_t rfid_tool_init(const rfid_tool_config_t *config);

/**
 * @brief Deinitialize constitutional RFID tool and free resources
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rfid_tool_deinit(rfid_tool_handle_t handle);

/**
 * @brief Get constitutional RFID tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
rfid_tool_capabilities_t rfid_tool_get_capabilities(rfid_tool_handle_t handle);

/**
 * @brief Get constitutional RFID tool status
 * @param handle Tool handle
 * @param status Pointer to status structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rfid_tool_get_status(rfid_tool_handle_t handle, rfid_tool_status_t *status);

/**
 * @brief Set filesystem tool dependency for configuration loading
 * @param handle RFID tool handle
 * @param fs_tool Filesystem tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rfid_tool_set_fs_dependency(rfid_tool_handle_t handle, void* fs_tool);

// =============================================================================
// Constitutional RFID Operations Interface
// =============================================================================

/**
 * @brief Start constitutional RFID tag scanning
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rfid_tool_start_scanning(rfid_tool_handle_t handle);

/**
 * @brief Stop constitutional RFID tag scanning  
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rfid_tool_stop_scanning(rfid_tool_handle_t handle);

/**
 * @brief Check if tag is currently present
 * @param handle Tool handle
 * @return true if tag present, false otherwise
 */
bool rfid_tool_is_tag_present(rfid_tool_handle_t handle);

/**
 * @brief Get current tag information
 * @param handle Tool handle
 * @param tag_info Pointer to tag info structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rfid_tool_get_current_tag(rfid_tool_handle_t handle, rfid_tag_info_t *tag_info);

/**
 * @brief Get tag UID as hex string
 * @param handle Tool handle
 * @param uid_string Buffer for UID string (minimum 21 chars)
 * @param buffer_size Size of buffer
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rfid_tool_get_tag_uid_string(rfid_tool_handle_t handle, char* uid_string, size_t buffer_size);

/**
 * @brief Constitutional hardware self-test for RC522
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rfid_tool_hardware_self_test(rfid_tool_handle_t handle);

/**
 * @brief Scan for available RFID tags (single scan operation)
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rfid_tool_scan_tags(rfid_tool_handle_t handle);

// =============================================================================
// Constitutional Utility Functions
// =============================================================================

/**
 * @brief Convert RFID tool event type to string
 * @param event_type Event type
 * @return String representation
 */
const char* rfid_tool_event_to_string(rfid_tool_event_type_t event_type);

/**
 * @brief Convert tag type to string
 * @param tag_type Tag type
 * @return String representation  
 */
const char* rfid_tool_tag_type_to_string(rfid_tag_type_t tag_type);

/**
 * @brief Convert tag UID to hex string with constitutional memory safety
 * @param tag_info Tag information
 * @param uid_string Output buffer (minimum 21 chars)
 * @param buffer_size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rfid_tool_tag_uid_to_string(const rfid_tag_info_t* tag_info, char* uid_string, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // RFID_TOOL_H