#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"
#include <stdbool.h>
#include <time.h>

// Maximum number of networks to store
#define WIFI_MANAGER_MAX_NETWORKS 10
// Maximum SSID length (32 bytes as per 802.11 standard + null terminator)
#define WIFI_MANAGER_MAX_SSID_LEN 33
// Maximum password length (64 bytes as per 802.11 standard + null terminator)
#define WIFI_MANAGER_MAX_PASSWORD_LEN 65

// NTP time synchronization configuration
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600      // Brussels timezone: UTC+1
#define DAYLIGHT_OFFSET_SEC 3600 // Additional hour for DST

/**
 * @brief WiFi network credentials structure
 */
typedef struct {
    char ssid[WIFI_MANAGER_MAX_SSID_LEN];
    char password[WIFI_MANAGER_MAX_PASSWORD_LEN];
} wifi_network_t;

/**
 * @brief WiFi configuration structure
 */
typedef struct {
    wifi_network_t networks[WIFI_MANAGER_MAX_NETWORKS];
    int count;
} wifi_networks_config_t;

/**
 * @brief Initialize the WiFi manager
 * @param json_path Path to the JSON configuration file
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_init(const char *json_path);

/**
 * @brief Start the WiFi manager
 * This will attempt to connect to known networks, and if unsuccessful,
 * it will start in AP mode with a configuration web server
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief Start the device in Access Point mode
 * @param ap_ssid SSID for the access point
 * @param ap_password Password for the access point (NULL for open AP)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_start_ap(const char *ap_ssid, const char *ap_password);

/**
 * @brief Save WiFi configuration to JSON file
 * @param json_path Path to the JSON configuration file
 * @param config Pointer to the wifi_networks_config_t structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_save_config(const char *json_path, const wifi_networks_config_t *config);

/**
 * @brief Parse WiFi configuration from JSON file
 * @param json_path Path to the JSON configuration file
 * @param config Pointer to the wifi_networks_config_t structure to store the configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_parse_config(const char *json_path, wifi_networks_config_t *config);

/**
 * @brief Display the WiFi configuration
 * @param config Pointer to the wifi_networks_config_t structure
 */
void wifi_manager_print_config(const wifi_networks_config_t *config);

/**
 * @brief Connect to a WiFi network from the configuration
 * @param config Pointer to the wifi_networks_config_t structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_connect(const wifi_networks_config_t *config);

/**
 * @brief Check if currently connected to WiFi
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get current IP address as string
 * @param ip_str Buffer to store the IP address string
 * @param len Length of the buffer
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_get_ip(char *ip_str, size_t len);

/**
 * @brief Get RSSI of the connected network
 * @param rssi Pointer to store the RSSI value
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_get_rssi(int8_t *rssi);

/**
 * @brief Initialize and synchronize time with NTP server
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_sync_time(void);

/**
 * @brief Check if time is synchronized
 * @return true if time is synchronized, false otherwise
 */
bool wifi_manager_is_time_synced(void);

/**
 * @brief Get current time as Unix timestamp
 * @return Current time as Unix timestamp, 0 if not synchronized
 */
time_t wifi_manager_get_time(void);

/**
 * @brief Get current time as formatted string
 * @param time_str Buffer to store formatted time string
 * @param len Length of the buffer
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_get_formatted_time(char *time_str, size_t len);

/**
 * @brief Check if Daylight Saving Time is active for a given time
 * @param timeinfo Pointer to struct tm with time information
 * @return true if DST is active, false otherwise
 */
bool wifi_manager_is_dst(struct tm *timeinfo);

#endif /* WIFI_MANAGER_H */