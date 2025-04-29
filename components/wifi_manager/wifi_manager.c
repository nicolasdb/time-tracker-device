#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "cJSON.h"
#include "../ap_webserver/ap_webserver.h"
#include "esp_system.h"
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

#define TAG "wifi_manager"

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

// The event group allows multiple bits for each event, but we only care about these events:
// - we are connected to the AP with an IP
// - we failed to connect after the maximum amount of retries
// - AP mode is active
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_AP_STARTED_BIT BIT2

// Maximum number of connection attempts per network
#define MAX_RETRY_COUNT 3

// Access Point configuration
#define AP_SSID "TimeTracker-Setup"
#define AP_PASSWORD NULL  // NULL for open AP
#define AP_MAX_CONNECTIONS 4

static int s_retry_count = 0;
static int s_current_network_index = 0;
static bool s_is_connected = false;
static bool s_ap_mode_active = false;
static wifi_networks_config_t s_wifi_config = {0};
static char s_wifi_json_path[256] = {0};
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;

// Forward declarations
static esp_err_t wifi_manager_try_connect_to_next_network(void);
static esp_err_t wifi_manager_start_ap_mode(void);
static esp_err_t wifi_manager_exit_ap_mode_callback(void);
static void start_ap_delayed_task(void *pvParameters);

static void sta_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < MAX_RETRY_COUNT) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "Retry %d to connect to the AP", s_retry_count);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_is_connected = false;
            
            // Try the next network if available
            ESP_LOGI(TAG, "Failed to connect to network %d", s_current_network_index - 1);
            
            // Stop WiFi before trying next network or starting AP mode
            esp_wifi_stop();
            
            // Small delay to ensure clean WiFi state
            vTaskDelay(500 / portTICK_PERIOD_MS);
            
            wifi_manager_try_connect_to_next_network();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        s_is_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void ap_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station %s connected to AP", event->mac ? "client" : "unknown");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station %s disconnected from AP", event->mac ? "client" : "unknown");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "AP mode started");
        s_ap_mode_active = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_AP_STARTED_BIT);
        
        // Start the web server
        ap_webserver_start(s_wifi_json_path);
        ap_webserver_set_callback(wifi_manager_exit_ap_mode_callback);
    }
}

esp_err_t wifi_manager_init(const char *json_path)
{
    ESP_LOGI(TAG, "Initializing WiFi manager");
    
    // Store the JSON path
    if (json_path) {
        strncpy(s_wifi_json_path, json_path, sizeof(s_wifi_json_path) - 1);
        s_wifi_json_path[sizeof(s_wifi_json_path) - 1] = '\0';
    }
    
    // Parse WiFi configuration
    esp_err_t ret = wifi_manager_parse_config(json_path, &s_wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse WiFi configuration");
        return ret;
    }
    
    // Print the parsed configuration
    wifi_manager_print_config(&s_wifi_config);
    
    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    
    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default STA interface
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create STA interface");
        return ESP_FAIL;
    }
    
    // Create default AP interface
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (s_ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create AP interface");
        return ESP_FAIL;
    }
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register STA event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &sta_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &sta_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &sta_event_handler, NULL));
    
    // Register AP event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &ap_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &ap_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &ap_event_handler, NULL));
    
    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    ESP_LOGI(TAG, "Starting WiFi manager");
    
    // Reset retry count and network index
    s_retry_count = 0;
    s_current_network_index = 0;
    
    // If no networks configured, start AP mode immediately
    if (s_wifi_config.count == 0) {
        ESP_LOGW(TAG, "No networks configured, starting AP mode");
        return wifi_manager_start_ap_mode();
    }
    
    // Try to connect to the first network
    return wifi_manager_try_connect_to_next_network();
}

static esp_err_t wifi_manager_try_connect_to_next_network(void)
{
    // Check if there are more networks to try
    if (s_current_network_index >= s_wifi_config.count) {
        ESP_LOGW(TAG, "No more networks to try, starting AP mode");
        
        // Create a task to start AP mode after a short delay
        // This ensures proper cleanup and state transition
        xTaskCreate(
            start_ap_delayed_task,
            "start_ap_task",
            4096,
            NULL,
            5,
            NULL
        );
        
        return ESP_OK;
    }
    
    // Reset retry count
    s_retry_count = 0;
    
    // Get the current network
    wifi_network_t *network = &s_wifi_config.networks[s_current_network_index];
    
    ESP_LOGI(TAG, "Trying to connect to network %d: %s", s_current_network_index, network->ssid);
    
    // Configure WiFi
    wifi_config_t wifi_cfg = {
        .sta = {
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    // Copy network credentials
    strncpy((char *)wifi_cfg.sta.ssid, network->ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, network->password, sizeof(wifi_cfg.sta.password) - 1);
    
    // Set WiFi mode and config
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    
    // Start WiFi if it's not already started
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi connecting to %s...", network->ssid);
    
    // Increment the network index for next attempt
    s_current_network_index++;
    
    return ESP_OK;
}

static void start_ap_delayed_task(void *pvParameters)
{
    // Small delay to ensure clean WiFi state
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    wifi_manager_start_ap_mode();
    
    // Delete the task when done
    vTaskDelete(NULL);
}

static esp_err_t wifi_manager_start_ap_mode(void)
{
    ESP_LOGI(TAG, "Starting AP mode");
    
    // Set AP configuration
    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN
        },
    };
    
    // Set SSID
    strncpy((char *)wifi_config.ap.ssid, AP_SSID, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(AP_SSID);
    
    // Set password if provided
    // AP_PASSWORD is defined as NULL, so we'll use an open network
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    // Zero out the password buffer for safety
    memset(wifi_config.ap.password, 0, sizeof(wifi_config.ap.password));
    
    // Set WiFi mode and config
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    
    // Start WiFi
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "Failed to start WiFi AP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi AP \"%s\" started", AP_SSID);
    
    // Wait for AP to start
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_AP_STARTED_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          portMAX_DELAY);
    
    if (bits & WIFI_AP_STARTED_BIT) {
        ESP_LOGI(TAG, "AP mode active");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to start AP mode");
        return ESP_FAIL;
    }
}

static esp_err_t wifi_manager_exit_ap_mode_callback(void)
{
    ESP_LOGI(TAG, "Exiting AP mode");
    
    // Stop the web server
    ap_webserver_stop();
    
    // Stop WiFi
    ESP_ERROR_CHECK(esp_wifi_stop());
    
    // Reset AP mode flag
    s_ap_mode_active = false;
    
    // Clear event flags
    xEventGroupClearBits(s_wifi_event_group, WIFI_AP_STARTED_BIT);
    
    // Parse the updated configuration
    esp_err_t ret = wifi_manager_parse_config(s_wifi_json_path, &s_wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse updated WiFi configuration");
        return ret;
    }
    
    // Print the new configuration
    wifi_manager_print_config(&s_wifi_config);
    
    // Reset connection counters
    s_retry_count = 0;
    s_current_network_index = 0;
    
    // Try to connect to the first network
    ret = wifi_manager_try_connect_to_next_network();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to restart WiFi connection");
        return ret;
    }
    
    // Restart the device after a short delay to ensure settings are applied
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
    
    return ESP_OK;
}

esp_err_t wifi_manager_parse_config(const char *json_path, wifi_networks_config_t *config)
{
    ESP_LOGI(TAG, "Parsing WiFi configuration from %s", json_path);
    
    // Initialize the configuration
    memset(config, 0, sizeof(wifi_networks_config_t));
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
    
    // Reset connection state
    s_retry_count = 0;
    s_current_network_index = 0;
    
    // Try to connect to the first network
    return wifi_manager_try_connect_to_next_network();
}

esp_err_t wifi_manager_start_ap(const char *ap_ssid, const char *ap_password)
{
    // Stop any existing WiFi
    ESP_ERROR_CHECK(esp_wifi_stop());
    
    // Set AP configuration
    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN
        },
    };
    
    // Set SSID
    if (ap_ssid) {
        strncpy((char *)wifi_config.ap.ssid, ap_ssid, sizeof(wifi_config.ap.ssid) - 1);
        wifi_config.ap.ssid_len = strlen(ap_ssid);
    } else {
        strncpy((char *)wifi_config.ap.ssid, AP_SSID, sizeof(wifi_config.ap.ssid) - 1);
        wifi_config.ap.ssid_len = strlen(AP_SSID);
    }
    
    // Set password if provided
    if (ap_password != NULL) {
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        strncpy((char *)wifi_config.ap.password, ap_password, sizeof(wifi_config.ap.password) - 1);
    } else {
        // Ensure authmode is open if no password
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    // Set WiFi mode and config
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    
    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Started AP mode with SSID: %s", wifi_config.ap.ssid);
    
    return ESP_OK;
}

esp_err_t wifi_manager_save_config(const char *json_path, const wifi_networks_config_t *config)
{
    // Create the JSON object
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }
    
    // Create the networks array
    cJSON *networks = cJSON_CreateArray();
    if (networks == NULL) {
        ESP_LOGE(TAG, "Failed to create networks array");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // Add networks to the array
    for (int i = 0; i < config->count; i++) {
        cJSON *network = cJSON_CreateObject();
        if (network == NULL) {
            ESP_LOGE(TAG, "Failed to create network object");
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        
        cJSON_AddStringToObject(network, "ssid", config->networks[i].ssid);
        cJSON_AddStringToObject(network, "password", config->networks[i].password);
        
        cJSON_AddItemToArray(networks, network);
    }
    
    // Add the networks array to the root object
    cJSON_AddItemToObject(root, "networks", networks);
    
    // Convert to string
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        ESP_LOGE(TAG, "Failed to convert JSON to string");
        return ESP_FAIL;
    }
    
    // Write to file
    FILE *f = fopen(json_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", json_path);
        free(json_str);
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", json_str);
    fclose(f);
    free(json_str);
    
    ESP_LOGI(TAG, "Configuration saved to %s", json_path);
    
    return ESP_OK;
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
    
    if (s_sta_netif == NULL) {
        return ESP_FAIL;
    }
    
    ESP_ERROR_CHECK(esp_netif_get_ip_info(s_sta_netif, &ip_info));
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