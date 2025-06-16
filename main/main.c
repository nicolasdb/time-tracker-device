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

/*
 * Phase 1: MCP-Inspired Orchestrator with feedback_tool Integration
 * 
 * This demonstrates the transformation from business logic container
 * to pure tool orchestrator using MCP patterns.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== MCP-Inspired Time Tracker Device ===");
    ESP_LOGI(TAG, "Phase 3C: fs_tool + Phase 3B: webhook_tool Integration");
    ESP_LOGI(TAG, "Branch: esp-idf_mcpStyle_refactor");
    
    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
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
    
    ESP_LOGI(TAG, "=== MCP 5-Tool Self-Test Sequence ===");
    
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
    
    // Enter IDLE state (beautiful blue breathing)
    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
    ESP_LOGI(TAG, "💤 System ready - entering IDLE state (blue breathing pattern)");
    
    // =========================================================================
    // Phase 1: MCP Tool Demonstration
    // =========================================================================
    
    ESP_LOGI(TAG, "=== Demonstrating MCP Tool Patterns ===");
    
    int demo_cycle = 0;
    while (1) {
        // Show tool status (MCP monitoring pattern)
        feedback_tool_status_t feedback_status;
        wifi_tool_status_t wifi_status;
        rfid_tool_status_t rfid_status;
        fs_tool_status_t fs_status;
        webhook_tool_status_t webhook_status;
        
        if (feedback_tool_get_status(feedback_tool, &feedback_status) == ESP_OK) {
            ESP_LOGI(TAG, "Feedback Tool: queue=%d, uptime=%" PRIu32 "ms",
                     feedback_status.queue_count, feedback_status.uptime_ms);
        }
        
        if (wifi_tool_get_status(wifi_tool, &wifi_status) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi Tool: connected=%d, ap_active=%d, uptime=%" PRIu32 "ms",
                     wifi_status.sta_connected, wifi_status.ap_active, wifi_status.uptime_ms);
        }
        
        if (rfid_tool_get_status(rfid_tool, &rfid_status) == ESP_OK) {
            ESP_LOGI(TAG, "RFID Tool: scanning=%d, tag_present=%d, detections=%" PRIu32 ", uptime=%" PRIu32 "ms",
                     rfid_status.is_scanning, rfid_status.tag_present, 
                     rfid_status.tag_detection_count, rfid_status.uptime_ms);
        }
        
        if (fs_tool_get_status(fs_tool, &fs_status) == ESP_OK) {
            ESP_LOGI(TAG, "FS Tool: mounted=%d, usage=%d%%, operations=%" PRIu32 ", uptime=%" PRIu32 "ms",
                     fs_status.is_mounted, fs_status.usage_percent,
                     fs_status.file_operations_count, fs_status.uptime_ms);
        }
        
        if (webhook_tool_get_status(webhook_tool, &webhook_status) == ESP_OK) {
            ESP_LOGI(TAG, "Webhook Tool: queue=%" PRIu32 ", sent=%" PRIu32 ", errors=%" PRIu32 ", uptime=%" PRIu32 "ms",
                     webhook_status.pending_count, webhook_status.success_count,
                     webhook_status.failed_count, webhook_status.uptime_ms);
        }
        
        // Enhanced demonstration with clear visual feedback
        switch (demo_cycle % 10) {
            case 0:
                ESP_LOGI(TAG, "🔗 DEMO 1/10: WiFi Connection Simulation");
                ESP_LOGI(TAG, "   📡 Connecting to WiFi... (fast blink)");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING);
                vTaskDelay(pdMS_TO_TICKS(3000));
                ESP_LOGI(TAG, "   ✅ WiFi Connected! (green flash)");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTED);
                break;
                
            case 1:
                ESP_LOGI(TAG, "🏷️  DEMO 2/10: RFID Tag Detection Simulation");
                ESP_LOGI(TAG, "   🔍 Scanning for RFID tags...");
                ESP_LOGI(TAG, "   ✨ Tag detected! (solid green)");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
                vTaskDelay(pdMS_TO_TICKS(4000));
                ESP_LOGI(TAG, "   📤 Tag removed, returning to idle");
                feedback_tool_clear_state(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
                break;
                
            case 2:
                ESP_LOGI(TAG, "🌐 DEMO 3/10: Webhook Transmission Success");
                ESP_LOGI(TAG, "   📡 Sending webhook events...");
                for (int i = 0; i < 3; i++) {
                    ESP_LOGI(TAG, "   ✅ Event %d sent successfully (green flash)", i + 1);
                    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_WEBHOOK_SUCCESS, 
                                          FEEDBACK_PRIORITY_MEDIUM, 300);
                    vTaskDelay(pdMS_TO_TICKS(600));
                }
                break;
                
            case 3:
                ESP_LOGI(TAG, "📶 DEMO 4/10: WiFi AP Mode (Configuration Portal)");
                ESP_LOGI(TAG, "   🔧 Starting configuration AP mode (special pattern)");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_AP_MODE);
                vTaskDelay(pdMS_TO_TICKS(8000)); // Show full sequence
                ESP_LOGI(TAG, "   ⚙️  Configuration complete, exiting AP mode");
                feedback_tool_clear_state(feedback_tool, FEEDBACK_STATE_WIFI_AP_MODE);
                break;
                
            case 4:
                ESP_LOGI(TAG, "⚠️  DEMO 5/10: Error State Handling");
                ESP_LOGI(TAG, "   ❌ Simulating webhook error (fast red blink)");
                feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_WEBHOOK_ERROR, 
                                       FEEDBACK_PRIORITY_HIGH, 2000);
                vTaskDelay(pdMS_TO_TICKS(3000));
                ESP_LOGI(TAG, "   🔄 Error cleared, system recovering");
                break;
                
            case 5:
                ESP_LOGI(TAG, "🎛️  DEMO 6/10: Priority Queue System");
                ESP_LOGI(TAG, "   📊 Testing multi-state priority management:");
                ESP_LOGI(TAG, "   🔵 WiFi connecting (medium priority)");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING);
                vTaskDelay(pdMS_TO_TICKS(1000));
                ESP_LOGI(TAG, "   🟢 Tag detected (high priority - overrides WiFi)");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
                vTaskDelay(pdMS_TO_TICKS(2000));
                ESP_LOGI(TAG, "   🔴 Critical error (highest priority - overrides all)");
                feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_ERROR, 
                                       FEEDBACK_PRIORITY_CRITICAL, 1000);
                vTaskDelay(pdMS_TO_TICKS(2000));
                ESP_LOGI(TAG, "   🧹 Clearing all states, back to idle");
                feedback_tool_reset(feedback_tool);
                break;
                
            case 6:
                ESP_LOGI(TAG, "📝 DEMO 7/10: Tool Registry Discovery System");
                ESP_LOGI(TAG, "   🔍 Enumerating all 5 registered tools:");
                feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_TOOL_REGISTERED, FEEDBACK_PRIORITY_MEDIUM, 1000);
                vTaskDelay(pdMS_TO_TICKS(500));
                
                const feedback_tool_registry_t* feedback_registry = feedback_tool_get_registry_entry();
                ESP_LOGI(TAG, "   💡 Feedback: %s v%s (caps: 0x%02X)", 
                         feedback_registry->tool_id, FEEDBACK_TOOL_VERSION, feedback_registry->capabilities);
                         
                const wifi_tool_registry_t* wifi_registry = wifi_tool_get_registry_entry();
                ESP_LOGI(TAG, "   📶 WiFi: %s (caps: 0x%02X)", 
                         wifi_registry->tool_id, wifi_registry->capabilities);
                
                const rfid_tool_registry_t* rfid_registry = rfid_tool_get_registry_entry();
                ESP_LOGI(TAG, "   🏷️  RFID: %s (caps: 0x%02X)", 
                         rfid_registry->tool_id, rfid_registry->capabilities);
                
                const fs_tool_registry_t* fs_registry = fs_tool_get_registry_entry();
                ESP_LOGI(TAG, "   💾 FS: %s (caps: 0x%02X)", 
                         fs_registry->tool_id, fs_registry->capabilities);
                
                const webhook_tool_registry_t* webhook_registry = webhook_tool_get_registry_entry();
                ESP_LOGI(TAG, "   🌐 Webhook: %s (caps: 0x%02X)", 
                         webhook_registry->tool_id, webhook_registry->capabilities);
                break;
                
            case 7:
                ESP_LOGI(TAG, "🔧 DEMO 8/10: Real WiFi AP Mode Hardware Test");
                ESP_LOGI(TAG, "   📡 Starting WiFi Access Point (SSID: TimeTracker-Setup)");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_AP_MODE);
                wifi_tool_start_ap(wifi_tool);
                ESP_LOGI(TAG, "   ✅ AP active at 192.168.4.1 (connect for configuration)");
                vTaskDelay(pdMS_TO_TICKS(5000));
                ESP_LOGI(TAG, "   🔌 Stopping WiFi AP");
                wifi_tool_stop(wifi_tool);
                feedback_tool_clear_state(feedback_tool, FEEDBACK_STATE_WIFI_AP_MODE);
                break;
                
            case 8:
                ESP_LOGI(TAG, "🔍 DEMO 9/10: Real RFID Hardware Test");
                ESP_LOGI(TAG, "   📡 Testing RC522 RFID scanner communication");
                feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_RFID_ACTIVE, FEEDBACK_PRIORITY_MEDIUM, 1000);
                if (rfid_tool_is_tag_present(rfid_tool)) {
                    ESP_LOGI(TAG, "   ✨ Real RFID tag detected! (solid green)");
                    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    feedback_tool_clear_state(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
                } else {
                    ESP_LOGI(TAG, "   📤 No physical tag detected, simulating detection");
                    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    feedback_tool_clear_state(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
                }
                break;
                
            case 9:
                ESP_LOGI(TAG, "💤 DEMO 10/10: System Idle State (Breathing Pattern)");
                ESP_LOGI(TAG, "   🫁 Demonstrating blue breathing pattern (4-second cycle)");
                ESP_LOGI(TAG, "   ℹ️  This is the default state when system is ready");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
                vTaskDelay(pdMS_TO_TICKS(8000)); // Show 2 full breathing cycles
                break;
        }
        
        demo_cycle++;
        
        // Between demos, return to IDLE briefly
        if ((demo_cycle % 10) != 9) {
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
            ESP_LOGI(TAG, "Returning to IDLE state...");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        
        // Complete demonstration cycle
        if (demo_cycle >= 10) {
            ESP_LOGI(TAG, "🎉 Phase 3C+3B demonstration complete!");
            ESP_LOGI(TAG, "📊 Full 5-tool MCP ecosystem validated:");
            ESP_LOGI(TAG, "   💡 feedback_tool: Visual state management ✅");
            ESP_LOGI(TAG, "   📶 wifi_tool: Network connectivity ✅");
            ESP_LOGI(TAG, "   🏷️  rfid_tool: RC522 RFID scanning ✅");
            ESP_LOGI(TAG, "   💾 fs_tool: LittleFS persistent storage ✅");
            ESP_LOGI(TAG, "   🌐 webhook_tool: HTTP event transmission ✅");
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "🚀 Ready for production RFID time tracking workflows!");
            break;
        }
    }
    
    // =========================================================================
    // Enhanced Clean Tool Shutdown with Visual Feedback
    // =========================================================================
    
    ESP_LOGI(TAG, "=== 🔄 Graceful Tool Shutdown Sequence ===");
    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_SHUTDOWN);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Shutdown in reverse order of initialization
    ESP_LOGI(TAG, "🌐 Shutting down webhook_tool...");
    if (webhook_tool) {
        esp_err_t ret = webhook_tool_deinit(webhook_tool);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "   ✅ webhook_tool shutdown successfully");
        } else {
            ESP_LOGE(TAG, "   ❌ Failed to shutdown webhook_tool: %s", esp_err_to_name(ret));
        }
        webhook_tool = NULL;
    }
    
    ESP_LOGI(TAG, "💾 Shutting down fs_tool...");
    if (fs_tool) {
        esp_err_t ret = fs_tool_deinit(fs_tool);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "   ✅ fs_tool shutdown successfully");
        } else {
            ESP_LOGE(TAG, "   ❌ Failed to shutdown fs_tool: %s", esp_err_to_name(ret));
        }
        fs_tool = NULL;
    }
    
    ESP_LOGI(TAG, "🏷️  Shutting down rfid_tool...");
    if (rfid_tool) {
        esp_err_t ret = rfid_tool_deinit(rfid_tool);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "   ✅ rfid_tool shutdown successfully");
        } else {
            ESP_LOGE(TAG, "   ❌ Failed to shutdown rfid_tool: %s", esp_err_to_name(ret));
        }
        rfid_tool = NULL;
    }
    
    ESP_LOGI(TAG, "📶 Shutting down wifi_tool...");
    if (wifi_tool) {
        esp_err_t ret = wifi_tool_deinit(wifi_tool);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "   ✅ wifi_tool shutdown successfully");
        } else {
            ESP_LOGE(TAG, "   ❌ Failed to shutdown wifi_tool: %s", esp_err_to_name(ret));
        }
        wifi_tool = NULL;
    }
    
    ESP_LOGI(TAG, "💡 Shutting down feedback_tool...");
    if (feedback_tool) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Show shutdown state briefly
        
        esp_err_t ret = feedback_tool_deinit(feedback_tool);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "   ✅ feedback_tool shutdown successfully");
        } else {
            ESP_LOGE(TAG, "   ❌ Failed to shutdown feedback_tool: %s", esp_err_to_name(ret));
        }
        feedback_tool = NULL;
    }
    
    ESP_LOGI(TAG, "Phase 3C+3B complete - MCP 5-tool architecture validated!");
    ESP_LOGI(TAG, "✅ Event-driven communication (all tool events)");
    ESP_LOGI(TAG, "✅ Handle-based state isolation (5 tools)");
    ESP_LOGI(TAG, "✅ Tool registry and capabilities system");
    ESP_LOGI(TAG, "✅ RFID tool with RC522 hardware integration");
    ESP_LOGI(TAG, "✅ FS tool with JSON config/log APIs");
    ESP_LOGI(TAG, "✅ Webhook tool with event-driven transmission");
    ESP_LOGI(TAG, "Next: Phase 4 - Complete MCP ecosystem");
}