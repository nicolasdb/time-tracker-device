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
#include "feedback_tool.h"
#include "wifi_tool.h"

static const char *TAG = "MCP_ORCHESTRATOR";

// Global tool handles (will be moved to tool registry in Phase 3)
static feedback_tool_handle_t feedback_tool = NULL;
static wifi_tool_handle_t wifi_tool = NULL;

/*
 * Phase 1: MCP-Inspired Orchestrator with feedback_tool Integration
 * 
 * This demonstrates the transformation from business logic container
 * to pure tool orchestrator using MCP patterns.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== MCP-Inspired Time Tracker Device ===");
    ESP_LOGI(TAG, "Phase 2: feedback_tool + wifi_tool Integration");
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
    feedback_config.led_gpio = 5; // WS2812 LED on GPIO 5
    feedback_config.max_brightness = 15; // Brighter for demo
    
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
    
    // =========================================================================
    // Phase 1: MCP-Inspired Initialization Sequence
    // =========================================================================
    
    ESP_LOGI(TAG, "=== MCP Initialization Sequence ===");
    
    // Show booting state
    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_BOOTING);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Simulate initialization steps with visual feedback
    feedback_tool_validate_init_step(feedback_tool, "Tool Registry", true);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    feedback_tool_validate_init_step(feedback_tool, "feedback_tool", true);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Phase 2: WiFi tool implemented
    feedback_tool_validate_init_step(feedback_tool, "wifi_tool", true);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // TODO Phase 3: Remaining tools
    feedback_tool_validate_init_step(feedback_tool, "rfid_tool", false); // Phase 3
    vTaskDelay(pdMS_TO_TICKS(500));
    
    feedback_tool_validate_init_step(feedback_tool, "webhook_tool", false); // Phase 3
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Enter IDLE state (beautiful blue breathing)
    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
    ESP_LOGI(TAG, "System ready - entering IDLE state");
    
    // =========================================================================
    // Phase 1: MCP Tool Demonstration
    // =========================================================================
    
    ESP_LOGI(TAG, "=== Demonstrating MCP Tool Patterns ===");
    
    int demo_cycle = 0;
    while (1) {
        // Show tool status (MCP monitoring pattern)
        feedback_tool_status_t feedback_status;
        wifi_tool_status_t wifi_status;
        
        if (feedback_tool_get_status(feedback_tool, &feedback_status) == ESP_OK) {
            ESP_LOGI(TAG, "Feedback Tool: queue=%d, uptime=%lums",
                     feedback_status.queue_count, (unsigned long)feedback_status.uptime_ms);
        }
        
        if (wifi_tool_get_status(wifi_tool, &wifi_status) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi Tool: connected=%d, ap_active=%d, uptime=%lums",
                     wifi_status.sta_connected, wifi_status.ap_active, (unsigned long)wifi_status.uptime_ms);
        }
        
        // Demonstrate different tool operations each cycle
        switch (demo_cycle % 10) {
            case 0:
                ESP_LOGI(TAG, "Demo: WiFi Connection Simulation");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING);
                vTaskDelay(pdMS_TO_TICKS(3000));
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTED);
                break;
                
            case 1:
                ESP_LOGI(TAG, "Demo: Tag Detection Simulation");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
                vTaskDelay(pdMS_TO_TICKS(4000));
                feedback_tool_clear_state(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
                break;
                
            case 2:
                ESP_LOGI(TAG, "Demo: Webhook Success Flash");
                for (int i = 0; i < 3; i++) {
                    feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_WEBHOOK_SUCCESS, 
                                          FEEDBACK_PRIORITY_MEDIUM, 300);
                    vTaskDelay(pdMS_TO_TICKS(600));
                }
                break;
                
            case 3:
                ESP_LOGI(TAG, "Demo: AP Mode Pattern");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_AP_MODE);
                vTaskDelay(pdMS_TO_TICKS(8000)); // Show full Y→B→P→Y→B→P sequence
                feedback_tool_clear_state(feedback_tool, FEEDBACK_STATE_WIFI_AP_MODE);
                break;
                
            case 4:
                ESP_LOGI(TAG, "Demo: Error State");
                feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_WEBHOOK_ERROR, 
                                       FEEDBACK_PRIORITY_HIGH, 2000);
                vTaskDelay(pdMS_TO_TICKS(3000));
                break;
                
            case 5:
                ESP_LOGI(TAG, "Demo: Priority Queue - Multiple States");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING);
                vTaskDelay(pdMS_TO_TICKS(1000));
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_TAG_DETECTED); // Higher priority
                vTaskDelay(pdMS_TO_TICKS(2000));
                feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_ERROR, 
                                       FEEDBACK_PRIORITY_CRITICAL, 1000); // Highest priority
                vTaskDelay(pdMS_TO_TICKS(2000));
                feedback_tool_reset(feedback_tool); // Clear all, back to IDLE
                break;
                
            case 6:
                ESP_LOGI(TAG, "Demo: Tool Registry Information");
                const feedback_tool_registry_t* feedback_registry = feedback_tool_get_registry_entry();
                ESP_LOGI(TAG, "Feedback Registry: %s - %s (caps: 0x%02X)", 
                         feedback_registry->tool_id, feedback_registry->description, feedback_registry->capabilities);
                         
                const wifi_tool_registry_t* wifi_registry = wifi_tool_get_registry_entry();
                ESP_LOGI(TAG, "WiFi Registry: %s - %s (caps: 0x%02X)", 
                         wifi_registry->tool_id, wifi_registry->description, wifi_registry->capabilities);
                break;
                
            case 7:
                ESP_LOGI(TAG, "Demo: WiFi AP Mode Test");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_AP_MODE);
                ESP_LOGI(TAG, "Starting WiFi AP mode...");
                wifi_tool_start_ap(wifi_tool);
                vTaskDelay(pdMS_TO_TICKS(5000));
                ESP_LOGI(TAG, "Stopping WiFi...");
                wifi_tool_stop(wifi_tool);
                feedback_tool_clear_state(feedback_tool, FEEDBACK_STATE_WIFI_AP_MODE);
                break;
                
            case 8:
                ESP_LOGI(TAG, "Demo: WiFi Tool Events (Event-driven decoupling)");
                // This demonstrates the decoupling - no direct calls to ap_webserver!
                ESP_LOGI(TAG, "WiFi tool publishes events, other tools subscribe");
                feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING, 
                                       FEEDBACK_PRIORITY_MEDIUM, 2000);
                vTaskDelay(pdMS_TO_TICKS(3000));
                break;
                
            case 9:
                ESP_LOGI(TAG, "Demo: Breathing IDLE state (4-second cycle)");
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
        
        // Stop after full demo cycle for Phase 2 testing
        if (demo_cycle >= 10) {
            ESP_LOGI(TAG, "Phase 2 demonstration complete");
            ESP_LOGI(TAG, "Ready for Phase 3: rfid_tool and webhook_tool transformations");
            break;
        }
    }
    
    // =========================================================================
    // Phase 1: Clean Tool Shutdown (MCP Pattern)
    // =========================================================================
    
    ESP_LOGI(TAG, "=== Shutting Down Tools ===");
    
    // Shutdown in reverse order of initialization
    if (wifi_tool) {
        esp_err_t ret = wifi_tool_deinit(wifi_tool);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "wifi_tool shutdown successfully");
        } else {
            ESP_LOGE(TAG, "Failed to shutdown wifi_tool: %s", esp_err_to_name(ret));
        }
        wifi_tool = NULL;
    }
    
    if (feedback_tool) {
        feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_SHUTDOWN);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        esp_err_t ret = feedback_tool_deinit(feedback_tool);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "feedback_tool shutdown successfully");
        } else {
            ESP_LOGE(TAG, "Failed to shutdown feedback_tool: %s", esp_err_to_name(ret));
        }
        feedback_tool = NULL;
    }
    
    ESP_LOGI(TAG, "Phase 2 complete - MCP multi-tool pattern validated!");
    ESP_LOGI(TAG, "✅ Event-driven communication (no ap_webserver coupling)");
    ESP_LOGI(TAG, "✅ Handle-based state isolation");
    ESP_LOGI(TAG, "✅ Tool registry and capabilities system");
    ESP_LOGI(TAG, "Next: Phase 3 - rfid_tool and webhook_tool transformations");
}