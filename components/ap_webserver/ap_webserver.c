#include "ap_webserver.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "ap_webserver"

// HTML for configuration page
static const char *html_page = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"    <meta http-equiv=\"Cache-Control\" content=\"no-cache, no-store, must-revalidate\" />"
"    <meta http-equiv=\"Pragma\" content=\"no-cache\" />"
"    <meta http-equiv=\"Expires\" content=\"0\" />"
"    <title>Time Tracker Setup</title>"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"    <style>"
"        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; color: #333; }"
"        h1 { color: #2c3e50; }"
"        .container { max-width: 500px; margin: 0 auto; }"
"        .form-group { margin-bottom: 15px; }"
"        label { display: block; margin-bottom: 5px; font-weight: bold; }"
"        input[type=text], input[type=password] { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }"
"        button { background-color: #3498db; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; }"
"        button:hover { background-color: #2980b9; }"
"        .networks { margin-top: 20px; }"
"        .network { background-color: #f9f9f9; border: 1px solid #ddd; border-radius: 4px; padding: 10px; margin-bottom: 10px; }"
"        .delete-btn { background-color: #e74c3c; float: right; }"
"        .delete-btn:hover { background-color: #c0392b; }"
"        .status { margin-top: 20px; padding: 10px; border-radius: 4px; }"
"        .success { background-color: #d4edda; color: #155724; }"
"        .error { background-color: #f8d7da; color: #721c24; }"
"    </style>"
"</head>"
"<body>"
"    <div class=\"container\">"
"        <h1>Time Tracker WiFi Setup</h1>"
"        <div class=\"form-group\">"
"            <label for=\"ssid\">WiFi Network Name (SSID):</label>"
"            <input type=\"text\" id=\"ssid\" name=\"ssid\" required>"
"        </div>"
"        <div class=\"form-group\">"
"            <label for=\"password\">WiFi Password:</label>"
"            <input type=\"password\" id=\"password\" name=\"password\" required>"
"        </div>"
"        <button onclick=\"addNetwork()\">Save Network</button>"
"        <div id=\"status\" class=\"status\" style=\"display: none;\"></div>"
"        <div class=\"networks\" id=\"networks\">"
"            <h2>Saved Networks</h2>"
"            <div id=\"network-list\"></div>"
"        </div>"
"        <div class=\"form-group\" style=\"margin-top: 20px;\">"
"            <button onclick=\"applySettings()\">Apply Settings & Restart</button>"
"        </div>"
"    </div>"
"    <script>"
"        document.addEventListener('DOMContentLoaded', fetchNetworks);"
"        function fetchNetworks() {"
"            fetch('/api/networks')"
"                .then(response => response.json())"
"                .then(data => {"
"                    const networkList = document.getElementById('network-list');"
"                    networkList.innerHTML = '';"
"                    data.networks.forEach((network, index) => {"
"                        const div = document.createElement('div');"
"                        div.className = 'network';"
"                        div.innerHTML = `"
"                            <strong>SSID:</strong> ${network.ssid}"
"                            <button class=\"delete-btn\" onclick=\"deleteNetwork(${index})\">Delete</button>"
"                        `;"
"                        networkList.appendChild(div);"
"                    });"
"                })"
"                .catch(error => {"
"                    console.error('Error fetching networks:', error);"
"                    showStatus('Error loading networks', false);"
"                });"
"        }"
"        function addNetwork() {"
"            const ssid = document.getElementById('ssid').value;"
"            const password = document.getElementById('password').value;"
"            if (!ssid) {"
"                showStatus('SSID is required', false);"
"                return;"
"            }"
"            fetch('/api/networks', {"
"                method: 'POST',"
"                headers: {'Content-Type': 'application/json'},"
"                body: JSON.stringify({ssid, password})"
"            })"
"            .then(response => response.json())"
"            .then(data => {"
"                if (data.success) {"
"                    showStatus('Network added successfully', true);"
"                    document.getElementById('ssid').value = '';"
"                    document.getElementById('password').value = '';"
"                    fetchNetworks();"
"                } else {"
"                    showStatus(`Error: ${data.message}`, false);"
"                }"
"            })"
"            .catch(error => {"
"                console.error('Error adding network:', error);"
"                showStatus('Failed to add network', false);"
"            });"
"        }"
"        function deleteNetwork(index) {"
"            fetch(`/api/networks/${index}`, {method: 'DELETE'})"
"            .then(response => response.json())"
"            .then(data => {"
"                if (data.success) {"
"                    showStatus('Network deleted successfully', true);"
"                    fetchNetworks();"
"                } else {"
"                    showStatus(`Error: ${data.message}`, false);"
"                }"
"            })"
"            .catch(error => {"
"                console.error('Error deleting network:', error);"
"                showStatus('Failed to delete network', false);"
"            });"
"        }"
"        function applySettings() {"
"            if (confirm('Device will restart and try to connect to WiFi. Continue?')) {"
"                fetch('/api/apply', {method: 'POST'})"
"                .then(response => response.json())"
"                .then(data => {"
"                    if (data.success) {"
"                        showStatus('Settings applied, device restarting...', true);"
"                    } else {"
"                        showStatus(`Error: ${data.message}`, false);"
"                    }"
"                })"
"                .catch(error => {"
"                    console.error('Error applying settings:', error);"
"                    showStatus('Failed to apply settings', false);"
"                });"
"            }"
"        }"
"        function showStatus(message, isSuccess) {"
"            const statusDiv = document.getElementById('status');"
"            statusDiv.textContent = message;"
"            statusDiv.className = isSuccess ? 'status success' : 'status error';"
"            statusDiv.style.display = 'block';"
"            setTimeout(() => statusDiv.style.display = 'none', 5000);"
"        }"
"    </script>"
"</body>"
"</html>";

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

// Serve the HTML page
static esp_err_t root_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html_page);
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