/**
 * @file dns_server.h
 * @brief Simple DNS server for captive portal
 * 
 * Responds to all DNS queries with the AP IP address (192.168.4.1)
 * to force browsers to connect to the configuration interface.
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start DNS server for captive portal
 * @param ap_ip AP IP address in network byte order
 * @return ESP_OK on success
 */
esp_err_t dns_server_start(uint32_t ap_ip);

/**
 * @brief Stop DNS server
 * @return ESP_OK on success
 */
esp_err_t dns_server_stop(void);

/**
 * @brief Check if DNS server is running
 * @return true if running, false otherwise
 */
bool dns_server_is_running(void);

#ifdef __cplusplus
}
#endif