#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "cJSON.h"
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

#define TAG "wifi_manager"

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

// The event group allows multiple bits for each event, but we only care about two events:
// - we are connected to the AP with an IP
// - we failed to connect after the maximum amount of retries
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Maximum number of connection attempts
#define MAX_RETRY_COUNT 5
static int s_retry_count = 0;
static bool s_is_connected = false;
static wifi_networks_config_t s_wifi_config = {0};

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < MAX_RETRY_COUNT) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_is_connected = false;
        }
        ESP_LOGI(TAG,"Connect to AP failed");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        s_is_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_init(const char *json_path)
{
    ESP_LOGI(TAG, "Initializing WiFi manager");
    
    // Parse WiFi configuration
    esp_err_t ret = wifi_manager_parse_config(json_path, &s_wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse WiFi configuration");
        return ret;
    }
    
    // Print the parsed configuration
    wifi_manager_print_config(&s_wifi_config);
    
    return ESP_OK;
}

esp_err_t wifi_manager_parse_config(const char *json_path, wifi_networks_config_t *config)
{
    ESP_LOGI(TAG, "Parsing WiFi configuration from %s", json_path);
    
    // Initialize the configuration
    memset(config, 0, sizeof(wifi_config_t));
    config->count = 0;
    
    // Check if the file exists
    struct stat st;
    if (stat(json_path, &st) != 0) {
        ESP_LOGE(TAG, "Configuration file not found: %s", json_path);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Read the file
    FILE *f = fopen(json_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", json_path);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Allocate memory for the JSON data
    char *json_data = (char *)malloc(file_size + 1);
    if (json_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON data");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    // Read the file content
    size_t bytes_read = fread(json_data, 1, file_size, f);
    fclose(f);
    
    if (bytes_read != file_size) {
        ESP_LOGE(TAG, "Failed to read file: %s", json_path);
        free(json_data);
        return ESP_FAIL;
    }
    
    // Null-terminate the data
    json_data[file_size] = '\0';
    
    // Parse the JSON data
    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON data: %s", cJSON_GetErrorPtr());
        return ESP_FAIL;
    }
    
    // Get the networks array
    cJSON *networks = cJSON_GetObjectItem(root, "networks");
    if (!cJSON_IsArray(networks)) {
        ESP_LOGE(TAG, "JSON data does not contain a 'networks' array");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // Parse each network
    int network_count = cJSON_GetArraySize(networks);
    ESP_LOGI(TAG, "Found %d networks in config", network_count);
    
    if (network_count > WIFI_MANAGER_MAX_NETWORKS) {
        ESP_LOGW(TAG, "Network count exceeds maximum (%d > %d), truncating", 
                 network_count, WIFI_MANAGER_MAX_NETWORKS);
        network_count = WIFI_MANAGER_MAX_NETWORKS;
    }
    
    // Process each network
    for (int i = 0; i < network_count; i++) {
        cJSON *network = cJSON_GetArrayItem(networks, i);
        if (!cJSON_IsObject(network)) {
            ESP_LOGW(TAG, "Network %d is not an object, skipping", i);
            continue;
        }
        
        cJSON *ssid = cJSON_GetObjectItem(network, "ssid");
        cJSON *password = cJSON_GetObjectItem(network, "password");
        
        if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
            ESP_LOGW(TAG, "Network %d has invalid SSID or password, skipping", i);
            continue;
        }
        
        // Copy the credentials to the configuration
        strncpy(config->networks[config->count].ssid, ssid->valuestring, WIFI_MANAGER_MAX_SSID_LEN - 1);
        strncpy(config->networks[config->count].password, password->valuestring, WIFI_MANAGER_MAX_PASSWORD_LEN - 1);
        
        // Ensure null-termination
        config->networks[config->count].ssid[WIFI_MANAGER_MAX_SSID_LEN - 1] = '\0';
        config->networks[config->count].password[WIFI_MANAGER_MAX_PASSWORD_LEN - 1] = '\0';
        
        config->count++;
    }
    
    // Clean up
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Successfully parsed %d networks", config->count);
    return ESP_OK;
}

void wifi_manager_print_config(const wifi_networks_config_t *config)
{
    ESP_LOGI(TAG, "WiFi Configuration:");
    ESP_LOGI(TAG, "  Network count: %d", config->count);
    
    for (int i = 0; i < config->count; i++) {
        ESP_LOGI(TAG, "  Network %d:", i);
        ESP_LOGI(TAG, "    SSID: %s", config->networks[i].ssid);
        ESP_LOGI(TAG, "    Password: %s", "********"); // Don't log actual passwords
    }
}

esp_err_t wifi_manager_connect(const wifi_networks_config_t *config)
{
    if (config->count == 0) {
        ESP_LOGE(TAG, "No networks configured");
        return ESP_ERR_INVALID_STATE;
    }
    
    s_wifi_event_group = xEventGroupCreate();
    
    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    
    // Try to connect to the first network
    wifi_config_t wifi_cfg = {
        .sta = {
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    // Copy the first network's credentials
    strncpy((char *)wifi_cfg.sta.ssid, config->networks[0].ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char *)wifi_cfg.sta.password, config->networks[0].password, sizeof(wifi_cfg.sta.password));
    
    ESP_LOGI(TAG, "Connecting to SSID: %s", wifi_cfg.sta.ssid);
    
    // Set WiFi mode and config
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi connecting...");
    
    // Wait for the connection to complete
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    
    // Check the result
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID: %s", wifi_cfg.sta.ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID: %s", wifi_cfg.sta.ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Unexpected event");
        return ESP_FAIL;
    }
}

bool wifi_manager_is_connected(void)
{
    return s_is_connected;
}

esp_err_t wifi_manager_get_ip(char *ip_str, size_t len)
{
    if (!s_is_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    
    if (netif == NULL) {
        return ESP_FAIL;
    }
    
    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));
    snprintf(ip_str, len, IPSTR, IP2STR(&ip_info.ip));
    
    return ESP_OK;
}

esp_err_t wifi_manager_get_rssi(int8_t *rssi)
{
    if (!s_is_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret == ESP_OK) {
        *rssi = ap_info.rssi;
    }
    
    return ret;
}