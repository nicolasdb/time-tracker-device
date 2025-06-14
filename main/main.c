/*
 * MCP-Inspired Time Tracker Device - Phase 1
 * Pure Orchestrator Main using feedback_tool
 * 
 * Demonstrates MCP tool composition patterns with feedback_tool integration.
 * Target: ~150 lines pure orchestrator (Phase 5 goal).
 */

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "feedback_tool.h"

static const char *TAG = "MCP_ORCHESTRATOR";

// Global tool handles (will be moved to tool registry in Phase 2)
static feedback_tool_handle_t feedback_tool = NULL;

/*
 * Phase 1: MCP-Inspired Orchestrator with feedback_tool Integration
 * 
 * This demonstrates the transformation from business logic container
 * to pure tool orchestrator using MCP patterns.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== MCP-Inspired Time Tracker Device ===");
    ESP_LOGI(TAG, "Phase 1: feedback_tool Integration");
    ESP_LOGI(TAG, "Branch: esp-idf_mcpStyle_refactor");
    
    // =========================================================================
    // Phase 1: Tool Initialization (MCP Pattern)
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
    
    feedback_tool_capabilities_t caps = feedback_tool_get_capabilities(feedback_tool);
    ESP_LOGI(TAG, "Capabilities: 0x%02X", caps);
    
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
    
    // TODO Phase 2: Initialize other tools
    feedback_tool_validate_init_step(feedback_tool, "wifi_tool", false); // Not implemented yet
    vTaskDelay(pdMS_TO_TICKS(500));
    
    feedback_tool_validate_init_step(feedback_tool, "rfid_tool", false); // Not implemented yet
    vTaskDelay(pdMS_TO_TICKS(500));
    
    feedback_tool_validate_init_step(feedback_tool, "webhook_tool", false); // Not implemented yet
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
        feedback_tool_status_t status;
        if (feedback_tool_get_status(feedback_tool, &status) == ESP_OK) {
            ESP_LOGI(TAG, "Tool Status: initialized=%d, active=%d, queue=%d, uptime=%lums",
                     status.is_initialized, status.is_active, 
                     status.queue_count, (unsigned long)status.uptime_ms);
        }
        
        // Demonstrate different tool operations each cycle
        switch (demo_cycle % 8) {
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
                const feedback_tool_registry_t* registry = feedback_tool_get_registry_entry();
                ESP_LOGI(TAG, "Registry: %s - %s (caps: 0x%02X)", 
                         registry->tool_id, registry->description, registry->capabilities);
                break;
                
            case 7:
                ESP_LOGI(TAG, "Demo: Breathing IDLE state (4-second cycle)");
                feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
                vTaskDelay(pdMS_TO_TICKS(8000)); // Show 2 full breathing cycles
                break;
        }
        
        demo_cycle++;
        
        // Between demos, return to IDLE briefly
        if ((demo_cycle % 8) != 7) {
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
            ESP_LOGI(TAG, "Returning to IDLE state...");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        
        // Stop after full demo cycle for Phase 1 testing
        if (demo_cycle >= 8) {
            ESP_LOGI(TAG, "Phase 1 demonstration complete");
            ESP_LOGI(TAG, "Ready for Phase 2: Additional tool transformations");
            break;
        }
    }
    
    // =========================================================================
    // Phase 1: Clean Tool Shutdown (MCP Pattern)
    // =========================================================================
    
    ESP_LOGI(TAG, "=== Shutting Down Tools ===");
    
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
    
    ESP_LOGI(TAG, "Phase 1 complete - MCP tool pattern validated!");
    ESP_LOGI(TAG, "Next: Phase 2 - Implement additional tools and tool registry");
}