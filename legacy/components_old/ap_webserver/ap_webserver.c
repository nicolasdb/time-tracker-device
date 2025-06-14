#include "ap_webserver.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define TAG "ap_webserver"
#define HTML_FILE_PATH "/littlefs/wifi_setup.html"
#include <sys/stat.h>
#define HTML_FALLBACK "<!DOCTYPE html><html><body><h1>Time Tracker WiFi Setup</h1><p>HTML file not found. Please upload wifi_setup.html to LittleFS.</p></body></html>"

static httpd_handle_t server = NULL;
static char storage_path[256] = {0};
static ap_mode_callback_t exit_ap_mode_callback = NULL;

// Get WiFi networks list
static esp_err_t networks_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /api/networks");
    
    // Read the current configuration
    FILE *f = fopen(storage_path, "r");
    if (f == NULL) {
        // If file doesn't exist, return empty list
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"networks\":[]}");
        return ESP_OK;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Allocate memory for the JSON data
    char *json_data = (char *)malloc(file_size + 1);
    if (json_data == NULL) {
        fclose(f);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Read the file content
    size_t bytes_read = fread(json_data, 1, file_size, f);
    fclose(f);
    
    if (bytes_read != file_size) {
        free(json_data);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Null-terminate the data
    json_data[file_size] = '\0';
    
    // Parse the JSON to mask passwords
    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    
    if (root == NULL) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"networks\":[]}");
        return ESP_OK;
    }
    
    // Get the networks array
    cJSON *networks = cJSON_GetObjectItem(root, "networks");
    if (!cJSON_IsArray(networks)) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"networks\":[]}");
        return ESP_OK;
    }
    
    // Mask passwords
    int network_count = cJSON_GetArraySize(networks);
    for (int i = 0; i < network_count; i++) {
        cJSON *network = cJSON_GetArrayItem(networks, i);
        cJSON *password = cJSON_GetObjectItem(network, "password");
        if (cJSON_IsString(password)) {
            cJSON_DeleteItemFromObject(network, "password");
        }
    }
    
    // Convert to string
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);
    
    return ESP_OK;
}

// Add a new network
static esp_err_t networks_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/networks");
    
    // Get content length
    int total_len = req->content_len;
    if (total_len > 1024) {
        // Too much data
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_FAIL;
    }
    
    // Read the data
    char content[1024] = {0};
    int cur_len = 0;
    int received = 0;
    
    while (cur_len < total_len) {
        received = httpd_req_recv(req, content + cur_len, total_len - cur_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive request");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    content[total_len] = '\0';
    
    // Parse the JSON
    cJSON *root = cJSON_Parse(content);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *password = cJSON_GetObjectItem(root, "password");
    
    if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID or password");
        return ESP_FAIL;
    }
    
    // Read the current configuration
    FILE *f = fopen(storage_path, "r");
    cJSON *config_root = NULL;
    cJSON *networks = NULL;
    
    if (f == NULL) {
        // Create new config
        config_root = cJSON_CreateObject();
        networks = cJSON_CreateArray();
        cJSON_AddItemToObject(config_root, "networks", networks);
    } else {
        // Get file size
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        // Allocate memory for the JSON data
        char *json_data = (char *)malloc(file_size + 1);
        if (json_data == NULL) {
            cJSON_Delete(root);
            fclose(f);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        // Read the file content
        size_t bytes_read = fread(json_data, 1, file_size, f);
        fclose(f);
        
        if (bytes_read != file_size) {
            free(json_data);
            cJSON_Delete(root);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        // Null-terminate the data
        json_data[file_size] = '\0';
        
        // Parse the existing config
        config_root = cJSON_Parse(json_data);
        free(json_data);
        
        if (config_root == NULL) {
            // Invalid JSON, create new config
            config_root = cJSON_CreateObject();
            networks = cJSON_CreateArray();
            cJSON_AddItemToObject(config_root, "networks", networks);
        } else {
            // Get the networks array
            networks = cJSON_GetObjectItem(config_root, "networks");
            if (!cJSON_IsArray(networks)) {
                // Invalid structure, create new networks array
                cJSON_DeleteItemFromObject(config_root, "networks");
                networks = cJSON_CreateArray();
                cJSON_AddItemToObject(config_root, "networks", networks);
            }
        }
    }
    
    // Create new network object
    cJSON *new_network = cJSON_CreateObject();
    cJSON_AddStringToObject(new_network, "ssid", ssid->valuestring);
    cJSON_AddStringToObject(new_network, "password", password->valuestring);
    
    // Add to networks array
    cJSON_AddItemToArray(networks, new_network);
    
    // Save to file
    char *json_str = cJSON_Print(config_root);
    cJSON_Delete(config_root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    f = fopen(storage_path, "w");
    if (f == NULL) {
        free(json_str);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", json_str);
    fclose(f);
    free(json_str);
    
    // Send success response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true}");
    
    return ESP_OK;
}

// Delete a network
static esp_err_t networks_delete_handler(httpd_req_t *req)
{
    int index;
    char index_str[8];
    
    // Get the network index from URL
    if (httpd_req_get_url_query_str(req, index_str, sizeof(index_str)) != ESP_OK ||
        httpd_query_key_value(index_str, "index", index_str, sizeof(index_str)) != ESP_OK) {
        
        // Extract index from URI
        const char *uri = req->uri;
        const char *index_start = strrchr(uri, '/');
        if (index_start == NULL) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing index");
            return ESP_FAIL;
        }
        
        index = atoi(index_start + 1);
    } else {
        index = atoi(index_str);
    }
    
    ESP_LOGI(TAG, "DELETE /api/networks/%d", index);
    
    // Read the current configuration
    FILE *f = fopen(storage_path, "r");
    if (f == NULL) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Configuration not found");
        return ESP_FAIL;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Allocate memory for the JSON data
    char *json_data = (char *)malloc(file_size + 1);
    if (json_data == NULL) {
        fclose(f);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Read the file content
    size_t bytes_read = fread(json_data, 1, file_size, f);
    fclose(f);
    
    if (bytes_read != file_size) {
        free(json_data);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Null-terminate the data
    json_data[file_size] = '\0';
    
    // Parse the JSON
    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON configuration");
        return ESP_FAIL;
    }
    
    // Get the networks array
    cJSON *networks = cJSON_GetObjectItem(root, "networks");
    if (!cJSON_IsArray(networks)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid configuration structure");
        return ESP_FAIL;
    }
    
    // Check if index is valid
    int network_count = cJSON_GetArraySize(networks);
    if (index < 0 || index >= network_count) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Network index out of range");
        return ESP_FAIL;
    }
    
    // Delete the network
    cJSON_DeleteItemFromArray(networks, index);
    
    // Save to file
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    f = fopen(storage_path, "w");
    if (f == NULL) {
        free(json_str);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", json_str);
    fclose(f);
    free(json_str);
    
    // Send success response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true}");
    
    return ESP_OK;
}

// Apply settings and restart
static esp_err_t apply_settings_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/apply");
    
    // Send response before restarting
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true}");
    
    // Call the callback
    if (exit_ap_mode_callback != NULL) {
        exit_ap_mode_callback();
    }
    
    return ESP_OK;
}

// Serve the HTML page from LittleFS
static esp_err_t root_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET / - Serving HTML from LittleFS");
    
    // Try to open the HTML file
    FILE *f = fopen(HTML_FILE_PATH, "r");
    if (f == NULL) {
        // If file not found, serve a simple fallback
        ESP_LOGW(TAG, "HTML file not found, serving fallback");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_sendstr(req, HTML_FALLBACK);
        return ESP_OK;
    }
    
    // Get file size
    struct stat st;
    if (stat(HTML_FILE_PATH, &st) != 0) {
        ESP_LOGE(TAG, "Failed to stat HTML file");
        fclose(f);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Allocate buffer for file content
    char *buffer = malloc(st.st_size + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for HTML file");
        fclose(f);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Read file content
    size_t read_size = fread(buffer, 1, st.st_size, f);
    fclose(f);
    
    if (read_size != st.st_size) {
        ESP_LOGE(TAG, "Failed to read HTML file");
        free(buffer);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Null-terminate the buffer
    buffer[st.st_size] = '\0';
    
    // Send the file content
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, buffer, st.st_size);
    
    // Free the buffer
    free(buffer);
    
    return ESP_OK;
}

esp_err_t ap_webserver_start(const char *config_path)
{
    if (server != NULL) {
        ESP_LOGI(TAG, "Server already started");
        return ESP_OK;
    }
    
    // Store the storage path
    strncpy(storage_path, config_path, sizeof(storage_path) - 1);
    storage_path[sizeof(storage_path) - 1] = '\0';
    
    // Start the HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }
    
    // Register URI handlers
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root);
    
    httpd_uri_t networks_get = {
        .uri = "/api/networks",
        .method = HTTP_GET,
        .handler = networks_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &networks_get);
    
    httpd_uri_t networks_post = {
        .uri = "/api/networks",
        .method = HTTP_POST,
        .handler = networks_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &networks_post);
    
    httpd_uri_t networks_delete = {
        .uri = "/api/networks/*",
        .method = HTTP_DELETE,
        .handler = networks_delete_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &networks_delete);
    
    httpd_uri_t apply_settings = {
        .uri = "/api/apply",
        .method = HTTP_POST,
        .handler = apply_settings_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &apply_settings);
    
    ESP_LOGI(TAG, "HTTP server started successfully");
    return ESP_OK;
}

esp_err_t ap_webserver_stop(void)
{
    if (server == NULL) {
        ESP_LOGI(TAG, "Server not running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping HTTP server");
    httpd_stop(server);
    server = NULL;
    
    return ESP_OK;
}

esp_err_t ap_webserver_set_callback(ap_mode_callback_t callback)
{
    exit_ap_mode_callback = callback;
    return ESP_OK;
}