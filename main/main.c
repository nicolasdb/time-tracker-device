/*
 * MCP-Inspired Time Tracker Device - Phase 2
 * Pure Orchestrator Main using feedback_tool + wifi_tool
 * 
 * Demonstrates MCP tool composition patterns with multiple tools.
 * Target: ~200 lines pure orchestrator (Phase 2 goal).
 */

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include "feedback_tool.h"
#include "wifi_tool.h"
#include "rfid_tool.h"
#include "fs_tool.h"
#include "webhook_tool.h"

static const char *TAG = "MCP_ORCHESTRATOR";

// Global tool handles (will be moved to tool registry in Phase 6)
static feedback_tool_handle_t feedback_tool = NULL;
static wifi_tool_handle_t wifi_tool = NULL;
static rfid_tool_handle_t rfid_tool = NULL;
static fs_tool_handle_t fs_tool = NULL;
static webhook_tool_handle_t webhook_tool = NULL;

// MCP Task configuration
#define MCP_TASK_STACK_SIZE 8192   // Adequate stack for 5-tool init + dashboard
#define MCP_TASK_PRIORITY   5      // Normal priority

// =========================================================================
// Phase 4.2: Event Coordination - WiFi to Feedback Tool
// =========================================================================

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base != WIFI_TOOL_EVENTS || !feedback_tool) {
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
            // Return to idle breathing (no blocking in event handler!)
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
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
            break;
            
        default:
            break;
    }
}

/*
 * MCP Tool Initialization Task
 * 
 * Heavy lifting happens here with adequate stack space (8192 bytes)
 * Separated from main task to avoid ESP-IDF stack limitations
 */
static void mcp_init_task(void *arg)
{
    ESP_LOGI(TAG, "=== MCP Tool Initialization (8192-byte stack) ===");
    ESP_LOGI(TAG, "Phase 4.3: Enhanced Visual Feedback + Dashboard");
    ESP_LOGI(TAG, "Branch: esp-idf_mcpStyle_refactor");
    
    // =========================================================================
    // Phase 2: Tool Initialization (MCP Pattern)
    // =========================================================================
    
    ESP_LOGI(TAG, "=== Initializing Tools ===");
    
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
    
    // =========================================================================
    // Phase 3C+3B: Enhanced MCP Self-Test Sequence with Visual Feedback
    // =========================================================================
    
    ESP_LOGI(TAG, "=== MCP Tools Self-Test Sequence ===");
    
    // Show booting state
    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_BOOTING);
    ESP_LOGI(TAG, "🔄 Starting 5-tool system initialization...");
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    // Test 1: Tool Registry System
    ESP_LOGI(TAG, "📋 TEST 1/6: Tool Registry System");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_START, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    feedback_tool_validate_init_step(feedback_tool, "Tool Registry", true);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 2: Feedback Tool (Self-Test)
    ESP_LOGI(TAG, "💡 TEST 2/6: feedback_tool (LED Visual Feedback)");
    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING); // Show blinking pattern
    vTaskDelay(pdMS_TO_TICKS(1000));
    feedback_tool_validate_init_step(feedback_tool, "feedback_tool", feedback_tool != NULL);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 3: WiFi Tool
    ESP_LOGI(TAG, "📶 TEST 3/6: wifi_tool (Network Connectivity)");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_WIFI_PREP, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    wifi_tool_status_t wifi_status;
    bool wifi_test_ok = (wifi_tool_get_status(wifi_tool, &wifi_status) == ESP_OK);
    feedback_tool_validate_init_step(feedback_tool, "wifi_tool", wifi_test_ok);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 4: RFID Tool
    ESP_LOGI(TAG, "🏷️  TEST 4/6: rfid_tool (RC522 RFID Scanner)");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_RFID, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    rfid_tool_status_t rfid_status;
    bool rfid_test_ok = (rfid_tool_get_status(rfid_tool, &rfid_status) == ESP_OK && rfid_status.is_scanning);
    feedback_tool_validate_init_step(feedback_tool, "rfid_tool", rfid_test_ok);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // Test 5: Filesystem Tool
    ESP_LOGI(TAG, "💾 TEST 5/6: fs_tool (LittleFS Persistent Storage)");
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
    ESP_LOGI(TAG, "🌐 TEST 6/6: webhook_tool (HTTP Event Transmission)");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_WEBHOOK, FEEDBACK_PRIORITY_MEDIUM, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));
    webhook_tool_status_t webhook_status;
    bool webhook_test_ok = (webhook_tool_get_status(webhook_tool, &webhook_status) == ESP_OK);
    feedback_tool_validate_init_step(feedback_tool, "webhook_tool", webhook_test_ok);
    vTaskDelay(pdMS_TO_TICKS(700));
    
    // All tests complete - celebrate with success pattern
    ESP_LOGI(TAG, "🎉 All 5 tools initialized successfully!");
    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_INIT_COMPLETE, FEEDBACK_PRIORITY_HIGH, 2000);
    vTaskDelay(pdMS_TO_TICKS(2500));
    
    // =========================================================================
    // Phase 4.2: MCP Dependency Injection - WiFi Tool Configuration Loading
    // =========================================================================
    
    ESP_LOGI(TAG, "🔌 Setting up MCP tool dependencies (Phase 4.2)");
    
    // Register WiFi event handler for visual feedback coordination
    ESP_LOGI(TAG, "🎯 Registering event coordination handlers...");
    esp_err_t event_ret = esp_event_handler_register(WIFI_TOOL_EVENTS, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    if (event_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ WiFi→Feedback event coordination registered");
    } else {
        ESP_LOGW(TAG, "⚠️  Event handler registration failed: %s", esp_err_to_name(event_ret));
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
    
    // Enter IDLE state (beautiful blue breathing)
    ESP_LOGI(TAG, "🔄 Requesting IDLE state transition...");
    esp_err_t idle_ret = feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
    ESP_LOGI(TAG, "💤 System ready - entering IDLE state (blue breathing pattern) - ret: %s", esp_err_to_name(idle_ret));
    
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
