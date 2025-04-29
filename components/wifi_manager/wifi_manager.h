#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"
#include <stdbool.h>

// Maximum number of networks to store
#define WIFI_MANAGER_MAX_NETWORKS 10
// Maximum SSID length (32 bytes as per 802.11 standard + null terminator)
#define WIFI_MANAGER_MAX_SSID_LEN 33
// Maximum password length (64 bytes as per 802.11 standard + null terminator)
#define WIFI_MANAGER_MAX_PASSWORD_LEN 65

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

#endif /* WIFI_MANAGER_H */