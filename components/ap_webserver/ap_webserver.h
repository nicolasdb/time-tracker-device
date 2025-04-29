#ifndef AP_WEBSERVER_H
#define AP_WEBSERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Initialize and start the web server
 * @param storage_path Path to the storage for WiFi configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ap_webserver_start(const char *storage_path);

/**
 * @brief Stop the web server
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ap_webserver_stop(void);

/**
 * @brief Set the AP mode callback
 * @param callback Function to call when WiFi credentials are updated
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*ap_mode_callback_t)(void);
esp_err_t ap_webserver_set_callback(ap_mode_callback_t callback);

#endif /* AP_WEBSERVER_H */