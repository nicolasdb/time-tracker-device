/**
 * @file webserver_tool.c
 * @brief MCP-Inspired Webserver Tool Implementation for AP Mode Configuration
 * 
 * Provides HTTP server with REST API for WiFi configuration during AP mode.
 * Integrates with fs_tool for persistent storage and wifi_tool for configuration.
 */

#include "webserver_tool.h"
#include "dns_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_event.h"
#include "cJSON.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static const char *TAG = "WEBSERVER_TOOL";

// =============================================================================
// MCP Tool Event System
// =============================================================================

ESP_EVENT_DEFINE_BASE(WEBSERVER_TOOL_EVENTS);

// =============================================================================
// Internal Tool Structure
// =============================================================================

/**
 * @brief MCP-Inspired Webserver Tool Context
 */
struct webserver_tool_context {
    // Tool Metadata (MCP Pattern)
    webserver_tool_config_t config;
    webserver_tool_capabilities_t capabilities;
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    
    // HTTP Server State
    httpd_handle_t server;
    bool server_running;
    uint16_t port;
    
    // Captive Portal DNS
    bool dns_running;
    
    // MCP Tool Dependencies
    fs_tool_handle_t fs_tool;
    
    // Event Publishing
    bool publish_events;
};

// =============================================================================
// Forward Declarations
// =============================================================================

static esp_err_t publish_webserver_event(struct webserver_tool_context *ctx, webserver_tool_event_type_t type, void* data);
static esp_err_t start_http_server(struct webserver_tool_context *ctx);
static esp_err_t stop_http_server(struct webserver_tool_context *ctx);
static esp_err_t start_captive_portal(struct webserver_tool_context *ctx);
static esp_err_t stop_captive_portal(struct webserver_tool_context *ctx);

// HTTP Handlers
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t api_networks_get_handler(httpd_req_t *req);
static esp_err_t api_networks_post_handler(httpd_req_t *req);
static esp_err_t api_networks_delete_handler(httpd_req_t *req);
static esp_err_t api_apply_handler(httpd_req_t *req);
static esp_err_t wifi_setup_html_handler(httpd_req_t *req);
static esp_err_t cors_options_handler(httpd_req_t *req);

// =============================================================================
// MCP Tool Interface Implementation
// =============================================================================

const char* webserver_tool_get_id(void)
{
    return WEBSERVER_TOOL_ID;
}

const char* webserver_tool_get_version(void)
{
    return WEBSERVER_TOOL_VERSION;
}

webserver_tool_config_t webserver_tool_create_default_config(void)
{
    webserver_tool_config_t config = {0};
    
    config.port = 80;
    config.max_connections = 4;
    config.backlog_conn = 5;
    config.max_uri_handlers = 8;
    config.max_resp_headers = 8;
    config.max_open_sockets = 7;
    config.stack_size = 8192;
    config.task_priority = 5;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    
    // Default behavior
    config.auto_start = false;  // Only start when needed (AP mode)
    config.publish_events = true;
    config.enable_cors = true;
    
    return config;
}

webserver_tool_handle_t webserver_tool_init(const webserver_tool_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Config cannot be NULL");
        return NULL;
    }
    
    struct webserver_tool_context *ctx = calloc(1, sizeof(struct webserver_tool_context));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate context");
        return NULL;
    }
    
    // Copy configuration
    memcpy(&ctx->config, config, sizeof(webserver_tool_config_t));
    
    // Set MCP metadata
    ctx->capabilities = WEBSERVER_CAP_HTTP_SERVER | 
                       WEBSERVER_CAP_REST_API |
                       WEBSERVER_CAP_STATIC_FILES |
                       WEBSERVER_CAP_WIFI_CONFIG |
                       WEBSERVER_CAP_CAPTIVE_PORTAL |
                       WEBSERVER_CAP_EVENT_PUBLISH;
    ctx->is_initialized = true;
    ctx->is_active = false;
    ctx->uptime_start = esp_log_timestamp();
    ctx->publish_events = config->publish_events;
    
    ESP_LOGI(TAG, "✅ webserver_tool initialized (port=%d)", config->port);
    
    // Note: Webserver starts only when needed (AP mode events)
    // Event-driven architecture: NETWORK_TOOL_EVENT_AP_STARTED triggers start
    
    return (webserver_tool_handle_t)ctx;
}

esp_err_t webserver_tool_deinit(webserver_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)handle;
    
    // Stop server if running
    if (ctx->server_running) {
        stop_http_server(ctx);
    }
    
    ESP_LOGI(TAG, "webserver_tool deinitialized");
    free(ctx);
    
    return ESP_OK;
}

esp_err_t webserver_tool_set_fs_dependency(webserver_tool_handle_t handle, fs_tool_handle_t fs_tool)
{
    if (!handle || !fs_tool) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)handle;
    ctx->fs_tool = fs_tool;
    
    ESP_LOGI(TAG, "✅ fs_tool dependency injected");
    return ESP_OK;
}

esp_err_t webserver_tool_start(webserver_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)handle;
    
    if (ctx->server_running) {
        ESP_LOGW(TAG, "HTTP server already running");
        return ESP_OK;
    }
    
    esp_err_t ret = start_http_server(ctx);
    if (ret == ESP_OK) {
        // Start captive portal DNS if requested
        if (ctx->config.enable_cors) {  // Using CORS flag as captive portal enable
            start_captive_portal(ctx);
        }
        
        ctx->is_active = true;
        publish_webserver_event(ctx, WEBSERVER_TOOL_EVENT_STARTED, NULL);
    }
    
    return ret;
}

esp_err_t webserver_tool_stop(webserver_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)handle;
    
    // Stop captive portal first
    stop_captive_portal(ctx);
    
    esp_err_t ret = stop_http_server(ctx);
    if (ret == ESP_OK) {
        ctx->is_active = false;
        publish_webserver_event(ctx, WEBSERVER_TOOL_EVENT_STOPPED, NULL);
    }
    
    return ret;
}

webserver_tool_capabilities_t webserver_tool_get_capabilities(webserver_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)handle;
    return ctx->capabilities;
}

esp_err_t webserver_tool_get_status(webserver_tool_handle_t handle, webserver_tool_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)handle;
    
    status->is_initialized = ctx->is_initialized;
    status->is_active = ctx->is_active;
    status->server_running = ctx->server_running;
    status->port = ctx->port;
    status->uptime_ms = esp_log_timestamp() - ctx->uptime_start;
    
    return ESP_OK;
}

// =============================================================================
// HTTP Server Implementation
// =============================================================================

static esp_err_t start_http_server(struct webserver_tool_context *ctx)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = ctx->config.port;
    config.max_open_sockets = ctx->config.max_open_sockets;
    config.max_uri_handlers = ctx->config.max_uri_handlers;
    config.max_resp_headers = ctx->config.max_resp_headers;
    config.backlog_conn = ctx->config.backlog_conn;
    config.lru_purge_enable = ctx->config.lru_purge_enable;
    config.recv_wait_timeout = ctx->config.recv_wait_timeout;
    config.send_wait_timeout = ctx->config.send_wait_timeout;
    config.stack_size = ctx->config.stack_size;
    config.task_priority = ctx->config.task_priority;
    
    esp_err_t ret = httpd_start(&ctx->server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register URI handlers
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(ctx->server, &root_uri);
    
    httpd_uri_t wifi_setup_uri = {
        .uri = "/wifi_setup.html",
        .method = HTTP_GET,
        .handler = wifi_setup_html_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(ctx->server, &wifi_setup_uri);
    
    httpd_uri_t api_networks_get_uri = {
        .uri = "/api/networks",
        .method = HTTP_GET,
        .handler = api_networks_get_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(ctx->server, &api_networks_get_uri);
    
    httpd_uri_t api_networks_post_uri = {
        .uri = "/api/networks",
        .method = HTTP_POST,
        .handler = api_networks_post_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(ctx->server, &api_networks_post_uri);
    
    httpd_uri_t api_networks_delete_uri = {
        .uri = "/api/networks",  // Same endpoint as POST
        .method = HTTP_DELETE,
        .handler = api_networks_delete_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(ctx->server, &api_networks_delete_uri);
    
    httpd_uri_t api_apply_uri = {
        .uri = "/api/apply",
        .method = HTTP_POST,
        .handler = api_apply_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(ctx->server, &api_apply_uri);
    
    // Add CORS preflight handler for DELETE requests
    httpd_uri_t cors_options_uri = {
        .uri = "/api/*",
        .method = HTTP_OPTIONS,
        .handler = cors_options_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(ctx->server, &cors_options_uri);
    
    ctx->server_running = true;
    ctx->port = ctx->config.port;
    
    ESP_LOGI(TAG, "✅ HTTP server started on port %d", ctx->config.port);
    return ESP_OK;
}

static esp_err_t stop_http_server(struct webserver_tool_context *ctx)
{
    if (!ctx->server) {
        return ESP_OK;
    }
    
    esp_err_t ret = httpd_stop(ctx->server);
    ctx->server = NULL;
    ctx->server_running = false;
    
    ESP_LOGI(TAG, "HTTP server stopped");
    return ret;
}

// =============================================================================
// HTTP Request Handlers
// =============================================================================

static esp_err_t root_handler(httpd_req_t *req)
{
    // Redirect to wifi_setup.html
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/wifi_setup.html");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t wifi_setup_html_handler(httpd_req_t *req)
{
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)req->user_ctx;
    
    if (!ctx->fs_tool) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fs_tool not available");
        return ESP_FAIL;
    }
    
    // Load HTML file from filesystem
    FILE *f = fopen("/littlefs/wifi_setup.html", "r");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "wifi_setup.html not found");
        return ESP_FAIL;
    }
    
    // Set content type
    httpd_resp_set_type(req, "text/html");
    
    // Send file content
    char buffer[512];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);  // End response
    
    return ESP_OK;
}

static esp_err_t api_networks_get_handler(httpd_req_t *req)
{
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)req->user_ctx;
    
    if (!ctx->fs_tool) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fs_tool not available");
        return ESP_FAIL;
    }
    
    // Load current WiFi configuration
    cJSON *wifi_config = NULL;
    esp_err_t ret = fs_tool_load_json_config(ctx->fs_tool, "wifi.json", &wifi_config);
    
    if (ret != ESP_OK || !wifi_config) {
        // Return empty networks array if no config exists
        wifi_config = cJSON_CreateObject();
        cJSON *networks_array = cJSON_CreateArray();
        cJSON_AddItemToObject(wifi_config, "networks", networks_array);
    }
    
    // Set headers
    httpd_resp_set_type(req, "application/json");
    if (ctx->config.enable_cors) {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    }
    
    // Send JSON response
    char *json_string = cJSON_Print(wifi_config);
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(wifi_config);
    
    return ESP_OK;
}

static esp_err_t api_networks_post_handler(httpd_req_t *req)
{
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)req->user_ctx;
    
    if (!ctx->fs_tool) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fs_tool not available");
        return ESP_FAIL;
    }
    
    // Read request body
    char buffer[512];
    int total_len = req->content_len;
    int cur_len = 0;
    
    if (total_len >= sizeof(buffer)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too large");
        return ESP_FAIL;
    }
    
    while (cur_len < total_len) {
        int received = httpd_req_recv(req, buffer + cur_len, total_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buffer[total_len] = '\0';
    
    // Parse JSON request
    cJSON *request_json = cJSON_Parse(buffer);
    if (!request_json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Check if this is a delete action
    cJSON *action_item = cJSON_GetObjectItem(request_json, "action");
    if (action_item && cJSON_IsString(action_item) && strcmp(cJSON_GetStringValue(action_item), "delete") == 0) {
        // Handle delete action
        cJSON *index_item = cJSON_GetObjectItem(request_json, "index");
        if (!index_item || !cJSON_IsNumber(index_item)) {
            cJSON_Delete(request_json);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Index required for delete");
            return ESP_FAIL;
        }
        
        int index = cJSON_GetNumberValue(index_item);
        cJSON_Delete(request_json);
        
        // Load existing configuration
        cJSON *wifi_config = NULL;
        esp_err_t ret = fs_tool_load_json_config(ctx->fs_tool, "wifi.json", &wifi_config);
        if (ret != ESP_OK || !wifi_config) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Configuration not found");
            return ESP_FAIL;
        }
        
        cJSON *networks_array = cJSON_GetObjectItem(wifi_config, "networks");
        if (!networks_array || !cJSON_IsArray(networks_array)) {
            cJSON_Delete(wifi_config);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Networks array not found");
            return ESP_FAIL;
        }
        
        // Delete network at index
        cJSON *deleted_item = cJSON_DetachItemFromArray(networks_array, index);
        if (!deleted_item) {
            cJSON_Delete(wifi_config);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Network index not found");
            return ESP_FAIL;
        }
        cJSON_Delete(deleted_item);
        
        // Save updated configuration
        ret = fs_tool_save_json_config(ctx->fs_tool, "wifi.json", wifi_config);
        cJSON_Delete(wifi_config);
        
        // Send response
        cJSON *response = cJSON_CreateObject();
        if (ret == ESP_OK) {
            cJSON_AddBoolToObject(response, "success", true);
            cJSON_AddStringToObject(response, "message", "Network deleted successfully");
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "Failed to save configuration");
        }
        
        const char *response_str = cJSON_Print(response);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, response_str);
        
        free((void*)response_str);
        cJSON_Delete(response);
        return ESP_OK;
    }
    
    // Handle add network action (original logic)
    cJSON *ssid_item = cJSON_GetObjectItem(request_json, "ssid");
    cJSON *password_item = cJSON_GetObjectItem(request_json, "password");
    
    if (!ssid_item || !cJSON_IsString(ssid_item)) {
        cJSON_Delete(request_json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid SSID");
        return ESP_FAIL;
    }
    
    // Load existing WiFi configuration
    cJSON *wifi_config = NULL;
    fs_tool_load_json_config(ctx->fs_tool, "wifi.json", &wifi_config);
    
    if (!wifi_config) {
        wifi_config = cJSON_CreateObject();
        cJSON *networks_array = cJSON_CreateArray();
        cJSON_AddItemToObject(wifi_config, "networks", networks_array);
    }
    
    cJSON *networks_array = cJSON_GetObjectItem(wifi_config, "networks");
    if (!networks_array) {
        networks_array = cJSON_CreateArray();
        cJSON_AddItemToObject(wifi_config, "networks", networks_array);
    }
    
    // Create new network entry
    cJSON *new_network = cJSON_CreateObject();
    cJSON_AddStringToObject(new_network, "ssid", cJSON_GetStringValue(ssid_item));
    if (password_item && cJSON_IsString(password_item)) {
        cJSON_AddStringToObject(new_network, "password", cJSON_GetStringValue(password_item));
    } else {
        cJSON_AddStringToObject(new_network, "password", "");
    }
    
    // Add to networks array
    cJSON_AddItemToArray(networks_array, new_network);
    
    // Save updated configuration
    esp_err_t ret = fs_tool_save_json_config(ctx->fs_tool, "wifi.json", wifi_config);
    
    // Prepare response
    cJSON *response = cJSON_CreateObject();
    if (ret == ESP_OK) {
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Network added successfully");
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Failed to save configuration");
    }
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    if (ctx->config.enable_cors) {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    }
    
    char *response_string = cJSON_Print(response);
    httpd_resp_send(req, response_string, strlen(response_string));
    
    // Cleanup
    free(response_string);
    cJSON_Delete(response);
    cJSON_Delete(wifi_config);
    cJSON_Delete(request_json);
    
    return ESP_OK;
}

static esp_err_t api_networks_delete_handler(httpd_req_t *req)
{
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)req->user_ctx;
    
    if (!ctx->fs_tool) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fs_tool not available");
        return ESP_FAIL;
    }
    
    // Read request body (same pattern as POST handler)
    char content[128];
    int content_len = httpd_req_recv(req, content, sizeof(content) - 1);
    if (content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request body required");
        return ESP_FAIL;
    }
    content[content_len] = '\0';
    
    // Parse JSON body
    cJSON *request_json = cJSON_Parse(content);
    if (!request_json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *index_json = cJSON_GetObjectItem(request_json, "index");
    if (!index_json || !cJSON_IsNumber(index_json)) {
        cJSON_Delete(request_json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid index");
        return ESP_FAIL;
    }
    
    int index = cJSON_GetNumberValue(index_json);
    cJSON_Delete(request_json);
    
    // Load existing WiFi configuration  
    cJSON *wifi_config = NULL;
    esp_err_t ret = fs_tool_load_json_config(ctx->fs_tool, "wifi.json", &wifi_config);
    
    if (ret != ESP_OK || !wifi_config) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Configuration not found");
        return ESP_FAIL;
    }
    
    cJSON *networks_array = cJSON_GetObjectItem(wifi_config, "networks");
    if (!networks_array || !cJSON_IsArray(networks_array)) {
        cJSON_Delete(wifi_config);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Networks array not found");
        return ESP_FAIL;
    }
    
    // Delete network at index
    cJSON *deleted_item = cJSON_DetachItemFromArray(networks_array, index);
    if (!deleted_item) {
        cJSON_Delete(wifi_config);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Network index not found");
        return ESP_FAIL;
    }
    cJSON_Delete(deleted_item);
    
    // Save updated configuration
    ret = fs_tool_save_json_config(ctx->fs_tool, "wifi.json", wifi_config);
    
    // Prepare response
    cJSON *response = cJSON_CreateObject();
    if (ret == ESP_OK) {
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Network deleted successfully");
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Failed to save configuration");
    }
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    if (ctx->config.enable_cors) {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    }
    
    char *response_string = cJSON_Print(response);
    httpd_resp_send(req, response_string, strlen(response_string));
    
    // Cleanup
    free(response_string);
    cJSON_Delete(response);
    cJSON_Delete(wifi_config);
    
    return ESP_OK;
}

static esp_err_t api_apply_handler(httpd_req_t *req)
{
    struct webserver_tool_context *ctx = (struct webserver_tool_context*)req->user_ctx;
    
    // Prepare response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Settings will be applied, device restarting...");
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    if (ctx->config.enable_cors) {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    }
    
    char *response_string = cJSON_Print(response);
    httpd_resp_send(req, response_string, strlen(response_string));
    
    // Cleanup
    free(response_string);
    cJSON_Delete(response);
    
    // Publish restart event
    publish_webserver_event(ctx, WEBSERVER_TOOL_EVENT_RESTART_REQUESTED, NULL);
    
    ESP_LOGI(TAG, "Restart requested via API - restarting in 2 seconds");
    
    // Give HTTP response time to send, then restart
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    
    return ESP_OK;
}

static esp_err_t cors_options_handler(httpd_req_t *req)
{
    // Set CORS headers for preflight requests
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
    
    // Send empty response with 200 OK
    httpd_resp_send(req, NULL, 0);
    
    return ESP_OK;
}

// =============================================================================
// Event Publishing
// =============================================================================

static esp_err_t publish_webserver_event(struct webserver_tool_context *ctx, webserver_tool_event_type_t type, void* data)
{
    if (!ctx->publish_events) {
        return ESP_OK;
    }
    
    webserver_tool_event_t event = {.type = type};
    if (data) {
        memcpy(&event, data, sizeof(webserver_tool_event_t));
    }
    
    ESP_LOGD(TAG, "Publishing webserver event: %d", type);
    
    return esp_event_post(WEBSERVER_TOOL_EVENTS, type, &event, sizeof(event), 0);
}

// =============================================================================
// Captive Portal DNS Server
// =============================================================================

static esp_err_t start_captive_portal(struct webserver_tool_context *ctx)
{
    // Convert AP IP to network byte order (192.168.4.1)
    struct in_addr ap_addr;
    inet_aton("192.168.4.1", &ap_addr);
    
    esp_err_t ret = dns_server_start(ap_addr.s_addr);
    if (ret == ESP_OK) {
        ctx->dns_running = true;
        ESP_LOGI(TAG, "✅ Captive portal DNS started");
    } else {
        ESP_LOGW(TAG, "Failed to start captive portal DNS: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

static esp_err_t stop_captive_portal(struct webserver_tool_context *ctx)
{
    if (!ctx->dns_running) {
        return ESP_OK;
    }
    
    esp_err_t ret = dns_server_stop();
    ctx->dns_running = false;
    
    ESP_LOGI(TAG, "Captive portal DNS stopped");
    return ret;
}

// =============================================================================
// Tool Registry Implementation (MCP Pattern)
// =============================================================================

const webserver_tool_registry_t* webserver_tool_get_registry_entry(void)
{
    static const webserver_tool_registry_t registry_entry = {
        .tool_id = WEBSERVER_TOOL_ID,
        .version = WEBSERVER_TOOL_VERSION,
        .description = WEBSERVER_TOOL_DESCRIPTION,
        .capabilities = WEBSERVER_CAP_HTTP_SERVER | 
                       WEBSERVER_CAP_REST_API |
                       WEBSERVER_CAP_STATIC_FILES |
                       WEBSERVER_CAP_WIFI_CONFIG |
                       WEBSERVER_CAP_CAPTIVE_PORTAL |
                       WEBSERVER_CAP_EVENT_PUBLISH,
        .init_func = webserver_tool_init,
        .deinit_func = webserver_tool_deinit
    };
    
    return &registry_entry;
}