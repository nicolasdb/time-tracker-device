# webserver_tool Integration Example

## AP Mode Fallback Sequence Implementation

This example shows how to integrate webserver_tool with wifi_tool for automatic AP mode fallback.

### 1. Event Handler Setup (main.c)

```c
#include "wifi_tool.h"
#include "webserver_tool.h"
#include "fs_tool.h"

// Global tool handles
static wifi_tool_handle_t wifi_tool = NULL;
static webserver_tool_handle_t webserver_tool = NULL;
static fs_tool_handle_t fs_tool = NULL;

// WiFi event handler for AP mode integration
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != WIFI_TOOL_EVENTS) return;
    
    wifi_tool_event_t* wifi_event = (wifi_tool_event_t*)event_data;
    
    switch (event_id) {
        case WIFI_TOOL_EVENT_AP_STARTED:
            ESP_LOGI(TAG, "AP mode started - launching webserver");
            // Start webserver when AP mode starts
            webserver_tool_start(webserver_tool);
            break;
            
        case WIFI_TOOL_EVENT_AP_STOPPED:
            ESP_LOGI(TAG, "AP mode stopped - stopping webserver");
            // Stop webserver when AP mode stops
            webserver_tool_stop(webserver_tool);
            break;
            
        case WIFI_TOOL_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected - stopping AP mode");
            // Successfully connected to WiFi, stop AP mode
            webserver_tool_stop(webserver_tool);
            break;
    }
}

// Webserver event handler for restart requests
static void webserver_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != WEBSERVER_TOOL_EVENTS) return;
    
    switch (event_id) {
        case WEBSERVER_TOOL_EVENT_RESTART_REQUESTED:
            ESP_LOGI(TAG, "Restart requested via web interface");
            // Give time for HTTP response to be sent
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
            break;
    }
}
```

### 2. Tool Initialization (main.c)

```c
void app_main(void) {
    // Initialize NVS
    nvs_flash_init();
    
    // Initialize event loop
    esp_event_loop_create_default();
    
    // Initialize filesystem tool
    fs_tool_config_t fs_config = fs_tool_create_default_config();
    fs_tool = fs_tool_init(&fs_config);
    
    // Initialize WiFi tool
    wifi_tool_config_t wifi_config = wifi_tool_create_default_config();
    wifi_config.enable_ap_fallback = true;  // Enable automatic AP fallback
    wifi_tool = wifi_tool_init(&wifi_config);
    
    // Set WiFi-fs dependency
    wifi_tool_set_fs_dependency(wifi_tool, fs_tool);
    
    // Initialize webserver tool
    webserver_tool_config_t webserver_config = webserver_tool_create_default_config();
    webserver_config.enable_cors = true;  // Enable captive portal
    webserver_tool = webserver_tool_init(&webserver_config);
    
    // Set webserver-fs dependency
    webserver_tool_set_fs_dependency(webserver_tool, fs_tool);
    
    // Register event handlers
    esp_event_handler_register(WIFI_TOOL_EVENTS, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(WEBSERVER_TOOL_EVENTS, ESP_EVENT_ANY_ID, webserver_event_handler, NULL);
    
    // Load WiFi configuration and start connection
    cJSON *wifi_config_json = NULL;
    esp_err_t ret = fs_tool_load_json_config(fs_tool, "wifi.json", &wifi_config_json);
    
    if (ret == ESP_OK && wifi_config_json) {
        wifi_tool_load_networks_from_json(wifi_tool, wifi_config_json);
        cJSON_Delete(wifi_config_json);
        
        // Start automatic WiFi connection (will fallback to AP if needed)
        wifi_tool_start_auto_connection(wifi_tool);
    } else {
        ESP_LOGW(TAG, "No WiFi config found, starting AP mode");
        wifi_tool_start_ap(wifi_tool);
    }
}
```

### 3. Complete Flow Sequence

```
System Boot
     │
     ▼
Load wifi.json via fs_tool
     │
     ▼
Try WiFi Connection (5 attempts, 30s timeout each)
     │
     ├─ SUCCESS ──► Normal Operation (Blue breathing LED)
     │
     ▼ FAILURE
Start AP Mode ("TimeTracker-Setup", password: "configure")
     │
     ▼ WIFI_TOOL_EVENT_AP_STARTED
Start webserver_tool (HTTP + DNS)
     │
     ▼
LED Pattern: Yellow flash → Blue → Purple → Purple (loop)
     │
     ▼
User connects to "TimeTracker-Setup" WiFi
     │
     ▼
Captive portal DNS redirects all requests to 192.168.4.1
     │
     ▼
User browser opens http://192.168.4.1/wifi_setup.html
     │
     ▼
User configures WiFi via web interface
     │
     ▼
POST /api/networks → webserver_tool → fs_tool_save_json_config()
     │
     ▼
User clicks "Apply & Restart" 
     │
     ▼ WEBSERVER_TOOL_EVENT_RESTART_REQUESTED
Device restarts with new WiFi configuration
```

### 4. File Integration

**webserver_tool requires fs_tool for:**
- Loading existing WiFi networks: `fs_tool_load_json_config(fs_tool, "wifi.json", &config)`
- Saving new WiFi networks: `fs_tool_save_json_config(fs_tool, "wifi.json", updated_config)`
- Serving HTML template: `fopen("/littlefs/wifi_setup.html", "r")`

**Key MCP Principles Demonstrated:**
- ✅ **Dependency Injection**: `webserver_tool_set_fs_dependency(webserver, fs_tool)`
- ✅ **Event-Driven Communication**: WiFi events trigger webserver start/stop
- ✅ **No Direct Coupling**: Tools communicate via events, not function calls
- ✅ **Self-Contained**: webserver_tool contains all HTTP/DNS functionality

### 5. REST API Usage

**GET /api/networks** - Load current WiFi configuration
```json
{
  "networks": [
    {"ssid": "MyNetwork", "password": "MyPassword"},
    {"ssid": "BackupNetwork", "password": "BackupPassword"}
  ]
}
```

**POST /api/networks** - Add new WiFi network
```json
Request: {"ssid": "NewNetwork", "password": "NewPassword"}
Response: {"success": true, "message": "Network added successfully"}
```

**DELETE /api/networks/0** - Delete network at index 0
```json
Response: {"success": true, "message": "Network deleted successfully"}
```

**POST /api/apply** - Apply settings and restart
```json
Response: {"success": true, "message": "Settings will be applied, device restarting..."}
```

This implementation provides a complete AP mode fallback system with captive portal for WiFi configuration.