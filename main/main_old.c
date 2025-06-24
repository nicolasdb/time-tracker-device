/*
 * MCP-Inspired Time Tracker Device - Process Map Compliance
 * Boot Sequence Authority: docs/charts/01_boot_sequence.mmd
 * 
 * Implements exact process map boot sequence with tool registry.
 * Phase 5.5: Complete architectural compliance with new payload_tool and debug_tool.
 */

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <inttypes.h>

// New tools per process map authority
#include "fs_tool.h"
#include "debug_tool.h"
#include "feedback_tool.h"
#include "wifi_tool.h"         // Will be network_tool per process maps
#include "ntp_tool.h"
#include "payload_tool.h"
#include "rfid_tool.h"
#include "webhook_tool.h"       // Will be http_tool per process maps
#include "webserver_tool.h"

// Tool registry system
#include "tool_registry.h"

static const char *TAG = "MCP_ORCHESTRATOR";

// Process map configuration
#define BOOT_GRACE_PERIOD_MS 5000   // 5-second grace period per process maps
#define DASHBOARD_UPDATE_INTERVAL_MS 30000  // Dashboard update every 30 seconds

// MCP Task configuration
#define MCP_TASK_STACK_SIZE 8192   // Adequate stack for 8-component init + dashboard
#define MCP_TASK_PRIORITY   5      // Normal priority

// =============================================================================
// Tool Interface Definitions (MCP Pattern)
// =============================================================================

// Universal tool interface for fs_tool
static const tool_interface_t fs_tool_interface = {
    .get_id = fs_tool_get_id,
    .get_version = fs_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))fs_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))fs_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))fs_tool_cleanup
};

// Universal tool interface for debug_tool
static const tool_interface_t debug_tool_interface = {
    .get_id = debug_tool_get_id,
    .get_version = debug_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))debug_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))debug_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))debug_tool_cleanup
};

// Universal tool interface for feedback_tool
static const tool_interface_t feedback_tool_interface = {
    .get_id = feedback_tool_get_id,
    .get_version = feedback_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))feedback_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))feedback_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))feedback_tool_cleanup
};

// Universal tool interface for wifi_tool (network_tool)
static const tool_interface_t wifi_tool_interface = {
    .get_id = wifi_tool_get_id,
    .get_version = wifi_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))wifi_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))wifi_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))wifi_tool_cleanup
};

// Universal tool interface for ntp_tool
static const tool_interface_t ntp_tool_interface = {
    .get_id = ntp_tool_get_id,
    .get_version = ntp_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))ntp_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))ntp_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))ntp_tool_cleanup
};

// Universal tool interface for payload_tool
static const tool_interface_t payload_tool_interface = {
    .get_id = payload_tool_get_id,
    .get_version = payload_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))payload_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))payload_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))payload_tool_cleanup
};

// Universal tool interface for rfid_tool
static const tool_interface_t rfid_tool_interface = {
    .get_id = rfid_tool_get_id,
    .get_version = rfid_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))rfid_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))rfid_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))rfid_tool_cleanup
};

// Universal tool interface for webhook_tool (http_tool)
static const tool_interface_t webhook_tool_interface = {
    .get_id = webhook_tool_get_id,
    .get_version = webhook_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))webhook_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))webhook_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))webhook_tool_cleanup
};

// =============================================================================
// Event Coordination - WiFi to Feedback Tool (Preserved from Phase 4.2)
// =============================================================================

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base != WIFI_TOOL_EVENTS) {
        return;
    }
    
    // Get feedback_tool handle from registry
    feedback_tool_handle_t feedback_tool = NULL;
    tool_registry_get_handle("feedback", (void**)&feedback_tool);
    
    // Get webserver_tool handle from registry
    webserver_tool_handle_t webserver_tool = NULL;
    tool_registry_get_handle("webserver", (void**)&webserver_tool);
    
    // Get ntp_tool handle from registry
    ntp_tool_handle_t ntp_tool = NULL;
    tool_registry_get_handle("ntp", (void**)&ntp_tool);
    
    if (!feedback_tool) {
        ESP_LOGW(TAG, "Feedback tool not available for WiFi event");
        return;
    }
    
    wifi_tool_event_t* wifi_event = (wifi_tool_event_t*)event_data;
    
    switch (event_id) {
        case WIFI_TOOL_EVENT_STA_CONNECTING:
            ESP_LOGI(TAG, "📶 WiFi connecting - blue blinking");
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING);
            break;
            
        case WIFI_TOOL_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "📶 WiFi connected to '%s' - solid blue", wifi_event->data.sta_info.ssid);
            feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTED, FEEDBACK_PRIORITY_MEDIUM, 2000);
            break;
            
        case WIFI_TOOL_EVENT_IP_ACQUIRED:
            ESP_LOGI(TAG, "🌐 IP acquired: %s - returning to idle", wifi_event->data.ip_info.ip_address);
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
            // Stop webserver since we're now connected to WiFi
            if (webserver_tool) {
                ESP_LOGI(TAG, "🌐 Stopping webserver - WiFi connected");
                webserver_tool_stop(webserver_tool);
            }
            // Trigger NTP sync on WiFi connection (Phase 5.2)
            if (ntp_tool) {
                ESP_LOGI(TAG, "🕐 WiFi connected - triggering NTP time sync");
                ntp_tool_sync_now(ntp_tool);
            }
            break;
            
        case WIFI_TOOL_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "📶 WiFi disconnected - returning to connecting state");
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING);
            break;
            
        case WIFI_TOOL_EVENT_STA_FAILED:
            ESP_LOGW(TAG, "📶 WiFi connection failed - error state");
            feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_ERROR, FEEDBACK_PRIORITY_HIGH, 3000);
            break;
            
        case WIFI_TOOL_EVENT_AP_STARTED:
            ESP_LOGI(TAG, "📶 WiFi AP mode started - AP mode sequence");
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_AP_MODE);
            // Start webserver for configuration interface
            if (webserver_tool) {
                ESP_LOGI(TAG, "🌐 Starting webserver for AP mode configuration");
                webserver_tool_start(webserver_tool);
            }
            break;
            
        default:
            break;
    }
}

// =========================================================================
// RFID Event Handler for Visual Feedback
// =========================================================================

static void rfid_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base != RFID_TOOL_EVENTS) {
        ESP_LOGD(TAG, "📡 RFID event handler: base mismatch");
        return;
    }
    
    // Get feedback_tool handle from registry
    feedback_tool_handle_t feedback_tool = NULL;
    esp_err_t ret = tool_registry_get_handle("feedback", (void**)&feedback_tool);
    if (ret != ESP_OK || !feedback_tool) {
        ESP_LOGD(TAG, "📡 RFID event handler: feedback_tool not available");
        return;
    }
    
    ESP_LOGI(TAG, "📡 RFID event received: event_id=%ld", event_id);
    
    switch (event_id) {
        case RFID_TOOL_EVENT_TAG_DETECTED:
            ESP_LOGI(TAG, "🏷️  RFID tag detected - green solid (active session)");
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
            break;
            
        case RFID_TOOL_EVENT_TAG_REMOVED:
            ESP_LOGI(TAG, "🏷️  RFID tag removed - returning to idle");
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
            break;
            
        default:
            ESP_LOGW(TAG, "📡 Unknown RFID event: %ld", event_id);
            break;
    }
}

/*
 * Process Map Boot Sequence Task
 * 
 * Implements exact boot sequence per docs/charts/01_boot_sequence.mmd
 * Tool initialization order: fs_tool → debug_tool → feedback_tool → network_tool → ntp_tool → payload_tool → rfid_tool → http_tool
 */
static void process_map_boot_task(void *arg)
{
    ESP_LOGI(TAG, "=== Process Map Boot Sequence Starting ===");
    ESP_LOGI(TAG, "Constitutional Authority: docs/charts/01_boot_sequence.mmd");
    ESP_LOGI(TAG, "Phase 5.5: Complete MCP Tool Compliance with payload_tool + debug_tool");
    
    // =============================================================================
    // Step 1: Initialize Tool Registry (Foundation)
    // =============================================================================
    
    ESP_LOGI(TAG, "🔧 Step 1: Initializing Tool Registry System");
    esp_err_t registry_ret = tool_registry_init();
    if (registry_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize tool registry: %s", esp_err_to_name(registry_ret));
        return;
    }
    ESP_LOGI(TAG, "✅ Tool registry initialized");
    
    // Initialize feedback tool with MCP configuration pattern
    feedback_tool_config_t feedback_config = feedback_tool_create_default_config();
    // GPIO and brightness now configured via Kconfig
    
    feedback_tool = feedback_tool_init(&feedback_config);
    if (!feedback_tool) {
        ESP_LOGE(TAG, "Failed to initialize feedback tool");
        return;
    }
    
    // Display tool information (MCP discovery pattern)
    ESP_LOGI(TAG, "Feedback Tool: %s v%s", 
             feedback_tool_get_id(), 
             feedback_tool_get_version());
    
    feedback_tool_capabilities_t feedback_caps = feedback_tool_get_capabilities(feedback_tool);
    ESP_LOGI(TAG, "Feedback Capabilities: 0x%02X", feedback_caps);
    
    // Initialize WiFi tool with MCP configuration pattern
    wifi_tool_config_t wifi_config = wifi_tool_create_default_config();
    
    wifi_tool = wifi_tool_init(&wifi_config);
    if (!wifi_tool) {
        ESP_LOGE(TAG, "Failed to initialize wifi tool");
        return;
    }
    
    // Display WiFi tool information (MCP discovery pattern)
    ESP_LOGI(TAG, "WiFi Tool: %s v%s", 
             wifi_tool_get_id(), 
             wifi_tool_get_version());
    
    wifi_tool_capabilities_t wifi_caps = wifi_tool_get_capabilities(wifi_tool);
    ESP_LOGI(TAG, "WiFi Capabilities: 0x%02X", wifi_caps);
    
    // Initialize RFID tool with MCP configuration pattern (Phase 3A)
    rfid_tool_config_t rfid_config = rfid_tool_create_default_config();
    
    rfid_tool = rfid_tool_init(&rfid_config);
    if (!rfid_tool) {
        ESP_LOGE(TAG, "Failed to initialize rfid tool");
        return;
    }
    
    // Display RFID tool information (MCP discovery pattern)
    ESP_LOGI(TAG, "RFID Tool: %s v%s", 
             rfid_tool_get_id(), 
             rfid_tool_get_version());
    
    rfid_tool_capabilities_t rfid_caps = rfid_tool_get_capabilities(rfid_tool);
    ESP_LOGI(TAG, "RFID Capabilities: 0x%02X", rfid_caps);
    
    // Initialize fs_tool with MCP configuration pattern (Phase 3C)
    fs_tool_config_t fs_config = fs_tool_create_default_config();
    
    fs_tool = fs_tool_init(&fs_config);
    if (!fs_tool) {
        ESP_LOGE(TAG, "Failed to initialize fs tool");
        return;
    }
    
    // Display fs_tool information (MCP discovery pattern)
    ESP_LOGI(TAG, "FS Tool: %s v%s", 
             fs_tool_get_id(), 
             fs_tool_get_version());
    
    fs_tool_capabilities_t fs_caps = fs_tool_get_capabilities(fs_tool);
    ESP_LOGI(TAG, "FS Capabilities: 0x%02X", fs_caps);
    
    // Initialize webhook_tool with MCP configuration pattern (Phase 3B)
    webhook_tool_config_t webhook_config = webhook_tool_create_default_config();
    
    webhook_tool = webhook_tool_init(&webhook_config);
    if (!webhook_tool) {
        ESP_LOGE(TAG, "Failed to initialize webhook tool");
        return;
    }
    
    // Display webhook_tool information (MCP discovery pattern)
    ESP_LOGI(TAG, "Webhook Tool: %s v%s", 
             webhook_tool_get_id(), 
             webhook_tool_get_version());
    
    webhook_tool_capabilities_t webhook_caps = webhook_tool_get_capabilities(webhook_tool);
    ESP_LOGI(TAG, "Webhook Capabilities: 0x%02X", webhook_caps);
    
    // Initialize webserver_tool with MCP configuration pattern (Phase 5.1)
    webserver_tool_config_t webserver_config = webserver_tool_create_default_config();
    
    webserver_tool = webserver_tool_init(&webserver_config);
    if (!webserver_tool) {
        ESP_LOGE(TAG, "Failed to initialize webserver tool");
        return;
    }
    
    // Display webserver_tool information (MCP discovery pattern)
    ESP_LOGI(TAG, "Webserver Tool: %s v%s", 
             webserver_tool_get_id(), 
             webserver_tool_get_version());
    
    webserver_tool_capabilities_t webserver_caps = webserver_tool_get_capabilities(webserver_tool);
    ESP_LOGI(TAG, "Webserver Capabilities: 0x%02X", webserver_caps);
    
    // Initialize ntp_tool with MCP configuration pattern (Phase 5.2)
    ntp_tool_config_t ntp_config = ntp_tool_create_default_config();
    
    ntp_tool = ntp_tool_init(&ntp_config);
    if (!ntp_tool) {
        ESP_LOGE(TAG, "Failed to initialize ntp tool");
        return;
    }
    
    // Display ntp_tool information (MCP discovery pattern)
    ESP_LOGI(TAG, "NTP Tool: %s v%s", 
             ntp_tool_get_id(), 
             ntp_tool_get_version());
    
    ntp_tool_capabilities_t ntp_caps = ntp_tool_get_capabilities(ntp_tool);
    ESP_LOGI(TAG, "NTP Capabilities: 0x%02X", ntp_caps);
    
    // =========================================================================
    // Phase 3C+3B: Enhanced MCP Self-Test Sequence with Visual Feedback
    // =========================================================================
    
    ESP_LOGI(TAG, "=== MCP Tools Self-Test Sequence ===");
    
    // Show booting state
    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_BOOTING);
    ESP_LOGI(TAG, "🔄 Starting 8-component system initialization...");
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    // Test 1: Tool Registry System
    ESP_LOGI(TAG, "📋 TEST 1/8: Tool Registry System");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_START, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    feedback_tool_validate_init_step(feedback_tool, "Tool Registry", true);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 2: Feedback Tool (Self-Test)
    ESP_LOGI(TAG, "💡 TEST 2/8: feedback_tool (LED Visual Feedback)");
    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING); // Show blinking pattern
    vTaskDelay(pdMS_TO_TICKS(1000));
    feedback_tool_validate_init_step(feedback_tool, "feedback_tool", feedback_tool != NULL);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 3: WiFi Tool
    ESP_LOGI(TAG, "📶 TEST 3/8: wifi_tool (Network Connectivity)");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_WIFI_PREP, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    wifi_tool_status_t wifi_status;
    bool wifi_test_ok = (wifi_tool_get_status(wifi_tool, &wifi_status) == ESP_OK);
    feedback_tool_validate_init_step(feedback_tool, "wifi_tool", wifi_test_ok);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 4: RFID Tool
    ESP_LOGI(TAG, "🏷️  TEST 4/8: rfid_tool (RC522 RFID Scanner)");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_RFID, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    rfid_tool_status_t rfid_status;
    bool rfid_test_ok = (rfid_tool_get_status(rfid_tool, &rfid_status) == ESP_OK && rfid_status.is_scanning);
    feedback_tool_validate_init_step(feedback_tool, "rfid_tool", rfid_test_ok);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 5: Filesystem Tool
    ESP_LOGI(TAG, "💾 TEST 5/8: fs_tool (LittleFS Persistent Storage)");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_FS, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    fs_tool_status_t fs_status;
    bool fs_test_ok = (fs_tool_get_status(fs_tool, &fs_status) == ESP_OK && fs_status.is_mounted);
    feedback_tool_validate_init_step(feedback_tool, "fs_tool", fs_test_ok);
    if (fs_test_ok) {
        ESP_LOGI(TAG, "   ✅ LittleFS mounted: %d%% usage, %lu operations", 
                 fs_status.usage_percent, fs_status.file_operations_count);
    }
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 6: Webhook Tool
    ESP_LOGI(TAG, "🌐 TEST 6/8: webhook_tool (HTTP Event Transmission)");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_WEBHOOK, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    webhook_tool_status_t webhook_status;
    bool webhook_test_ok = (webhook_tool_get_status(webhook_tool, &webhook_status) == ESP_OK);
    feedback_tool_validate_init_step(feedback_tool, "webhook_tool", webhook_test_ok);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 7: Webserver Tool
    ESP_LOGI(TAG, "🌐 TEST 7/8: webserver_tool (AP Mode Configuration Interface)");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_WEBSERVER_STARTING, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    webserver_tool_status_t webserver_status;
    bool webserver_test_ok = (webserver_tool_get_status(webserver_tool, &webserver_status) == ESP_OK);
    feedback_tool_validate_init_step(feedback_tool, "webserver_tool", webserver_test_ok);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 8: NTP Tool (Phase 5.2)
    ESP_LOGI(TAG, "🕐 TEST 8/8: ntp_tool (NTP Time Synchronization)");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_START, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    ntp_tool_status_t ntp_status;
    bool ntp_test_ok = (ntp_tool_get_status(ntp_tool, &ntp_status) == ESP_OK && ntp_status.is_initialized);
    feedback_tool_validate_init_step(feedback_tool, "ntp_tool", ntp_test_ok);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // All tests complete - celebrate with success pattern
    ESP_LOGI(TAG, "🎉 All 8 components initialized successfully!");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_COMPLETE, FEEDBACK_PRIORITY_HIGH, 2000);
    vTaskDelay(pdMS_TO_TICKS(2500));
    
    // =========================================================================
    // Phase 4.2: MCP Dependency Injection - WiFi Tool Configuration Loading
    // =========================================================================
    
    ESP_LOGI(TAG, "🔌 Setting up MCP tool dependencies");
    
    // Register WiFi event handler for visual feedback coordination
    ESP_LOGI(TAG, "🎯 Registering event coordination handlers...");
    esp_err_t event_ret = esp_event_handler_register(WIFI_TOOL_EVENTS, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    if (event_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ WiFi→Feedback event coordination registered");
    } else {
        ESP_LOGW(TAG, "⚠️  Event handler registration failed: %s", esp_err_to_name(event_ret));
    }
    
    // Register RFID event handler for visual feedback coordination
    esp_err_t rfid_event_ret = esp_event_handler_register(RFID_TOOL_EVENTS, ESP_EVENT_ANY_ID, rfid_event_handler, NULL);
    if (rfid_event_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ RFID→Feedback event coordination registered");
    } else {
        ESP_LOGW(TAG, "⚠️  RFID event handler registration failed: %s", esp_err_to_name(rfid_event_ret));
    }
    
    // Inject fs_tool dependency into wifi_tool  
    ESP_LOGI(TAG, "📁 Setting up WiFi tool dependencies...");
    esp_err_t dep_ret = wifi_tool_set_fs_dependency(wifi_tool, fs_tool);
    if (dep_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ WiFi tool dependency injection successful");
        
        // Load WiFi configuration from LittleFS via main.c orchestration
        ESP_LOGI(TAG, "📁 Loading WiFi configuration from LittleFS...");
        cJSON *wifi_config = NULL;
        esp_err_t load_ret = fs_tool_load_json_config(fs_tool, "wifi.json", &wifi_config);
        
        if (load_ret == ESP_OK && wifi_config) {
            ESP_LOGI(TAG, "✅ WiFi configuration loaded from LittleFS");
            
            // Pass configuration to wifi_tool (proper MCP separation)
            esp_err_t json_ret = wifi_tool_load_networks_from_json(wifi_tool, wifi_config);
            if (json_ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ WiFi networks loaded into wifi_tool");
                
                // Start automatic WiFi connection
                ESP_LOGI(TAG, "🌐 Starting automatic WiFi connection...");
                esp_err_t conn_ret = wifi_tool_start_auto_connection(wifi_tool);
                if (conn_ret == ESP_OK) {
                    ESP_LOGI(TAG, "✅ WiFi auto-connection started");
                } else {
                    ESP_LOGW(TAG, "⚠️  WiFi auto-connection failed: %s", esp_err_to_name(conn_ret));
                }
            } else {
                ESP_LOGW(TAG, "⚠️  Failed to load networks into wifi_tool: %s", esp_err_to_name(json_ret));
            }
            
            cJSON_Delete(wifi_config);
        } else {
            ESP_LOGW(TAG, "⚠️  Failed to load wifi.json: %s", esp_err_to_name(load_ret));
        }
    } else {
        ESP_LOGW(TAG, "⚠️  WiFi tool dependency injection failed: %s", esp_err_to_name(dep_ret));
    }
    
    // Inject fs_tool dependency into webserver_tool
    ESP_LOGI(TAG, "📁 Setting up Webserver tool dependencies...");
    esp_err_t webserver_dep_ret = webserver_tool_set_fs_dependency(webserver_tool, fs_tool);
    if (webserver_dep_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Webserver tool dependency injection successful");
    } else {
        ESP_LOGW(TAG, "⚠️  Webserver tool dependency injection failed: %s", esp_err_to_name(webserver_dep_ret));
    }
    
    // Clear BOOTING state to allow WiFi feedback during connection attempts
    feedback_tool_clear_state(feedback_tool, FEEDBACK_STATE_BOOTING);
    ESP_LOGI(TAG, "✅ Component initialization complete - enabling WiFi feedback");
    
    // Don't enter IDLE yet - wait for WiFi connection to complete
    // IDLE state will be triggered by IP_ACQUIRED event in wifi_event_handler
    ESP_LOGI(TAG, "🔄 System initialization complete - WiFi connection will trigger IDLE state");
    
    // Allow feedback task to process state change before dashboard generation
    ESP_LOGI(TAG, "⏳ Waiting for state transition to process...");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // =========================================================================
    // Phase 4.2: Production RFID Time Tracker Operation
    // =========================================================================
    
    ESP_LOGI(TAG, "🚀 RFID Time Tracker ready for production use!");
    ESP_LOGI(TAG, "📶 WiFi: Connect attempts will show blue blinking → solid blue when connected");
    ESP_LOGI(TAG, "🏷️  RFID: Place/remove tags to see green solid (active) → blue breathing (idle)");
    ESP_LOGI(TAG, "🌐 Webhooks: Events will be transmitted when WiFi connects");
    
    // Production monitoring loop - show ASCII dashboard every 30 seconds
    while (1) {
        // Generate and display ASCII dashboard (Phase 4.3 enhancement)
        feedback_dashboard_t dashboard;
        esp_err_t dash_ret = feedback_tool_generate_dashboard(feedback_tool, &dashboard);
        
        if (dash_ret == ESP_OK) {
            // Display beautiful ASCII dashboard (proper MCP pattern)
            printf("\n--- RFID TIME TRACKER STATUS ---\n"); // Simple header instead of ANSI
            printf("%s", dashboard.ascii_dashboard);
            
            // Also log JSON status for debugging
            ESP_LOGD(TAG, "System Status JSON: %s", dashboard.json_status);
        } else {
            // Fallback to simple status if dashboard fails
            ESP_LOGW(TAG, "Dashboard generation failed, using fallback status");
            wifi_tool_status_t wifi_status;
            rfid_tool_status_t rfid_status;
            
            if (wifi_tool_get_status(wifi_tool, &wifi_status) == ESP_OK &&
                rfid_tool_get_status(rfid_tool, &rfid_status) == ESP_OK) {
                ESP_LOGI(TAG, "📶 WiFi: %s | 🏷️ RFID: %s | ⏱️ Uptime: %.1f min", 
                         wifi_status.sta_connected ? "Connected" : "Disconnected",
                         rfid_status.is_scanning ? "Ready" : "Error",
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) / 60000.0f);
            }
        }
        
        // Wait 30 seconds before next dashboard update
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
    
    // Task should never reach here, but clean exit if it does
    vTaskDelete(NULL);
}

/*
 * Main Application Entry Point
 * 
 * Minimal work here - just create MCP initialization task with proper stack
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== MCP Time Tracker Starting ===");
    ESP_LOGI(TAG, "Creating MCP initialization task with %d bytes stack", MCP_TASK_STACK_SIZE);
    
    // Initialize NVS (required for WiFi) - minimal work in main task
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create MCP initialization task with adequate stack
    BaseType_t task_ret = xTaskCreate(
        mcp_init_task,           // Task function
        "mcp_init",              // Task name
        MCP_TASK_STACK_SIZE,     // Stack size (8192 bytes)
        NULL,                    // Parameters
        MCP_TASK_PRIORITY,       // Priority
        NULL                     // Task handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MCP initialization task");
        return;
    }
    
    ESP_LOGI(TAG, "MCP initialization task created successfully");
    // Main task ends here - MCP task takes over
}
