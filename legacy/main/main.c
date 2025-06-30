/*
 * MCP-Inspired Time Tracker Device - Process Map Compliance
 * Boot Sequence Authority: docs/charts/01_boot_sequence.mmd
 * 
 * Implements exact process map boot sequence with tool registry.
 * Phase 5.5: Complete architectural compliance with new payload_tool and system_monitor_tool.
 */

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include <string.h>  // For memcpy

// New tools per process map authority
#include "fs_tool.h"
#include "system_monitor_tool.h"
#include "event_system.h"
#include "feedback_tool.h"
#include "network_tool.h"       // Network connectivity per process maps
#include "ntp_tool.h"
#include "payload_tool.h"
#include "rfid_tool.h"
#include "http_tool.h"          // HTTP transmission per process maps
#include "webserver_tool.h"

// Tool registry system
#include "tool_registry.h"
// cJSON for fs_tool event logging integration
#include "cJSON.h"
// Event system for async tool communication
#include "event_system.h"

static const char *TAG = "MCP_ORCHESTRATOR";

// Process map configuration
#define BOOT_GRACE_PERIOD_MS 5000   // 5-second grace period per process maps
#define DASHBOARD_UPDATE_INTERVAL_MS 30000  // Dashboard update every 30 seconds

// MCP Task configuration
#define MCP_TASK_STACK_SIZE 8192   // Adequate stack for 8-component init + dashboard
#define MCP_TASK_PRIORITY   5      // Normal priority

// MCP Task configuration optimized for event-driven architecture

// Event Processing (Simple Orchestration Only)
// Session tracking and batching logic moved to proper tools per process maps

// =============================================================================
// Tool Interface Definitions (MCP Pattern)
// =============================================================================

// Universal tool interface for fs_tool
static const tool_interface_t fs_tool_interface = {
    .get_id = fs_tool_get_id,
    .get_version = fs_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))fs_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))fs_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))fs_tool_deinit
};

// Universal tool interface for system_monitor_tool
static const tool_interface_t system_monitor_tool_interface = {
    .get_id = system_monitor_tool_get_id,
    .get_version = system_monitor_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))system_monitor_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))system_monitor_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))system_monitor_tool_cleanup
};

// Universal tool interface for feedback_tool
static const tool_interface_t feedback_tool_interface = {
    .get_id = feedback_tool_get_id,
    .get_version = feedback_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))feedback_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))feedback_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))feedback_tool_deinit
};

// Universal tool interface for wifi_tool (network_tool)
static const tool_interface_t network_tool_interface = {
    .get_id = network_tool_get_id,
    .get_version = network_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))network_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))network_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))network_tool_deinit
};

// Universal tool interface for ntp_tool
static const tool_interface_t ntp_tool_interface = {
    .get_id = ntp_tool_get_id,
    .get_version = ntp_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))ntp_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))ntp_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))ntp_tool_deinit
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
    .cleanup = (esp_err_t (*)(void*))rfid_tool_deinit
};

// Universal tool interface for webhook_tool (http_tool)
static const tool_interface_t http_tool_interface = {
    .get_id = http_tool_get_id,
    .get_version = http_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))http_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))http_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))http_tool_deinit
};

// Universal tool interface for webserver_tool
static const tool_interface_t webserver_tool_interface = {
    .get_id = webserver_tool_get_id,
    .get_version = webserver_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))webserver_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))webserver_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))webserver_tool_deinit
};

// =============================================================================
// Simple Orchestration Functions (Process Map Compliant)
// =============================================================================

// =============================================================================
// REMOVED: Legacy RFID Event Processing (Replaced by Event-Driven Architecture)
// =============================================================================

// REMOVED: Old manual RFID processing task per Process Map Authority
// Process Maps 13 & 14: payload_tool now handles RFID events directly via ESP event system
// Constitutional event flow: RFID → payload_tool → http_tool (no main.c orchestration)

// =============================================================================
// Event Coordination - Constitutional Authority Process Map 01 
// =============================================================================

/**
 * @brief Constitutional RFID Event Handler per Process Map 01
 * Flow: RFID → ESP_EVENT → orchestrator → payload_tool (direct call)
 */
static void orchestrator_rfid_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base != RFID_EVENTS) {
        return;
    }
    
    ESP_LOGI(TAG, "🏷️ Orchestrator handling RFID event per Process Map 01: event_id=%ld", event_id);
    
    // Get payload_tool handle from registry (Constitutional Authority)
    payload_tool_handle_t payload_tool = NULL;
    tool_registry_get_handle("payload", (void**)&payload_tool);
    
    if (!payload_tool) {
        ESP_LOGW(TAG, "⚠️ payload_tool not found in registry");
        return;
    }
    
    // Constitutional Authority: Call payload_tool directly (no event recursion)
    esp_err_t payload_ret = payload_tool_build_and_transmit_payload(payload_tool, event_id, event_data);
    if (payload_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Constitutional flow: RFID → orchestrator → payload_tool → PAYLOAD_EVENTS");
    } else {
        ESP_LOGW(TAG, "⚠️ Payload processing failed: %s", esp_err_to_name(payload_ret));
    }
}

// Event Coordination - WiFi to Feedback Tool (Preserved from Phase 4.2)
// =============================================================================

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base != NETWORK_TOOL_EVENTS) {
        return;
    }
    
    // Get feedback_tool handle from registry
    feedback_tool_handle_t feedback_tool = NULL;
    tool_registry_get_handle("feedback", (void**)&feedback_tool);
    
    // Get webserver_tool handle from registry
    webserver_tool_handle_t webserver_tool = NULL;
    tool_registry_get_handle(webserver_tool_get_id(), (void**)&webserver_tool);
    
    // Get ntp_tool handle from registry
    ntp_tool_handle_t ntp_tool = NULL;
    tool_registry_get_handle("ntp", (void**)&ntp_tool);
    
    if (!feedback_tool) {
        ESP_LOGW(TAG, "Feedback tool not available for WiFi event");
        return;
    }
    
    network_tool_event_t* wifi_event = (network_tool_event_t*)event_data;
    
    switch (event_id) {
        case NETWORK_TOOL_EVENT_STA_CONNECTING:
            ESP_LOGI(TAG, "📶 WiFi connecting - blue blinking");
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING);
            break;
            
        case NETWORK_TOOL_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "📶 WiFi connected to '%s' - solid blue", wifi_event->data.sta_info.ssid);
            feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTED, FEEDBACK_PRIORITY_MEDIUM, 2000);
            break;
            
        case NETWORK_TOOL_EVENT_IP_ACQUIRED:
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
            
        case NETWORK_TOOL_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "📶 WiFi disconnected - returning to connecting state");
            feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_WIFI_CONNECTING);
            break;
            
        case NETWORK_TOOL_EVENT_STA_FAILED:
            ESP_LOGW(TAG, "📶 WiFi connection failed - error state");
            feedback_tool_set_state(feedback_tool, FEEDBACK_STATE_ERROR, FEEDBACK_PRIORITY_HIGH, 3000);
            break;
            
        case NETWORK_TOOL_EVENT_AP_STARTED:
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
// REMOVED: Legacy RFID Event Handler (Replaced by payload_tool event subscription)
// =========================================================================

// REMOVED: Manual RFID event queueing per Process Map Authority  
// Process Map 13: payload_tool handles RFID events directly via ESP event subscription

/*
 * Process Map Boot Sequence Task
 * 
 * Implements exact boot sequence per docs/charts/01_boot_sequence.mmd
 * Tool initialization order: fs_tool → system_monitor_tool → feedback_tool → network_tool → ntp_tool → payload_tool → rfid_tool → http_tool
 */
static void process_map_boot_task(void *arg)
{
    ESP_LOGI(TAG, "=== Process Map Boot Sequence Starting ===");
    ESP_LOGI(TAG, "Constitutional Authority: docs/charts/01_boot_sequence.mmd");
    ESP_LOGI(TAG, "Phase 6.0: EVENT-DRIVEN ARCHITECTURE REFACTOR");
    
    // =============================================================================
    // Step 0: Initialize Event System (Foundation for Async Communication)
    // =============================================================================
    
    ESP_LOGI(TAG, "📡 Step 0: Initializing Universal Event System");
    esp_err_t event_ret = event_system_init();
    if (event_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize event system: %s", esp_err_to_name(event_ret));
        return;
    }
    ESP_LOGI(TAG, "✅ Event system initialized - async tool communication ready");
    
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
    
    // =============================================================================
    // Step 2: fs_tool - First per process map (file system foundation)
    // =============================================================================
    
    ESP_LOGI(TAG, "💾 Step 2: fs_tool - LittleFS initialization");
    fs_tool_config_t fs_config = fs_tool_create_default_config();
    fs_tool_handle_t fs_tool = fs_tool_init(&fs_config);
    if (!fs_tool) {
        ESP_LOGE(TAG, "Failed to initialize fs_tool");
        return;
    }
    
    // Register with tool registry
    esp_err_t fs_reg_ret = tool_registry_register("fs", fs_tool, &fs_tool_interface, true);
    if (fs_reg_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register fs_tool: %s", esp_err_to_name(fs_reg_ret));
    }
    
    ESP_LOGI(TAG, "✅ fs_tool: %s v%s initialized", fs_tool_get_id(), fs_tool_get_version());
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // =============================================================================
    // Step 3: system_monitor_tool - Second per process map (ASCII dashboard)
    // =============================================================================
    
    ESP_LOGI(TAG, "🐛 Step 3: system_monitor_tool - ASCII dashboard system");
    system_monitor_tool_config_t debug_config = system_monitor_tool_create_default_config();
    system_monitor_tool_handle_t debug_tool = system_monitor_tool_init(&debug_config);
    if (!debug_tool) {
        ESP_LOGE(TAG, "Failed to initialize system_monitor_tool");
        return;
    }
    
    // Register with tool registry
    esp_err_t debug_reg_ret = tool_registry_register("system_monitor", debug_tool, &system_monitor_tool_interface, false);
    if (debug_reg_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register system_monitor_tool: %s", esp_err_to_name(debug_reg_ret));
    }
    
    ESP_LOGI(TAG, "✅ system_monitor_tool: %s v%s initialized", system_monitor_tool_get_id(), system_monitor_tool_get_version());
    
    // Register fs_tool with system_monitor_tool for dashboard visibility
    system_monitor_tool_registration_t fs_registration = {
        .tool_handle = fs_tool,
        .tool_id = fs_tool_get_id(),
        .tool_version = fs_tool_get_version(),
        .get_status_func = (esp_err_t (*)(void*, void*))fs_tool_get_status,
        .status_struct_size = sizeof(fs_tool_status_t)
    };
    esp_err_t fs_monitor_ret = system_monitor_tool_register_tool(debug_tool, &fs_registration);
    if (fs_monitor_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ fs_tool registered with system monitor");
    } else {
        ESP_LOGW(TAG, "Failed to register fs_tool with system monitor: %s", esp_err_to_name(fs_monitor_ret));
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // =============================================================================
    // Step 4: feedback_tool - Third per process map (visual feedback)
    // =============================================================================
    
    ESP_LOGI(TAG, "💡 Step 4: feedback_tool - LED visual feedback");
    feedback_tool_config_t feedback_config = feedback_tool_create_default_config();
    feedback_tool_handle_t feedback_tool = feedback_tool_init(&feedback_config);
    if (!feedback_tool) {
        ESP_LOGE(TAG, "Failed to initialize feedback_tool");
        return;
    }
    
    // Register with tool registry
    esp_err_t feedback_reg_ret = tool_registry_register("feedback", feedback_tool, &feedback_tool_interface, true);
    if (feedback_reg_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register feedback_tool: %s", esp_err_to_name(feedback_reg_ret));
    }
    
    // EVENT-DRIVEN ARCHITECTURE: Start system monitor event subscription
    esp_err_t event_sub_ret = system_monitor_tool_start_event_subscription(debug_tool);
    if (event_sub_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ System monitor event subscription started - async session timing active");
    } else {
        ESP_LOGW(TAG, "❌ Failed to start system monitor event subscription: %s", esp_err_to_name(event_sub_ret));
    }
    
    // EVENT-DRIVEN ARCHITECTURE: Start feedback tool event subscription
    esp_err_t feedback_sub_ret = feedback_tool_start_event_subscription(feedback_tool);
    if (feedback_sub_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Feedback tool event subscription started - async visual feedback active");
    } else {
        ESP_LOGW(TAG, "❌ Failed to start feedback tool event subscription: %s", esp_err_to_name(feedback_sub_ret));
    }
    
    // Show initialization pattern per process map
    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_BOOTING);
    ESP_LOGI(TAG, "✅ feedback_tool: %s v%s initialized", feedback_tool_get_id(), feedback_tool_get_version());
    
    // Register feedback_tool with system_monitor_tool for dashboard visibility
    system_monitor_tool_registration_t feedback_registration = {
        .tool_handle = feedback_tool,
        .tool_id = feedback_tool_get_id(),
        .tool_version = feedback_tool_get_version(),
        .get_status_func = (esp_err_t (*)(void*, void*))feedback_tool_get_status,
        .status_struct_size = sizeof(feedback_tool_status_t)
    };
    esp_err_t feedback_monitor_ret = system_monitor_tool_register_tool(debug_tool, &feedback_registration);
    if (feedback_monitor_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ feedback_tool registered with system monitor");
    } else {
        ESP_LOGW(TAG, "Failed to register feedback_tool with system monitor: %s", esp_err_to_name(feedback_monitor_ret));
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // =============================================================================
    // Step 5: network_tool (wifi_tool) - Fourth per process map
    // =============================================================================
    
    ESP_LOGI(TAG, "📶 Step 5: network_tool (wifi_tool) - Network connectivity");
    network_tool_config_t wifi_config = network_tool_create_default_config();
    network_tool_handle_t wifi_tool = network_tool_init(&wifi_config);
    if (!wifi_tool) {
        ESP_LOGE(TAG, "Failed to initialize wifi_tool");
        return;
    }
    
    // Register with tool registry
    esp_err_t wifi_reg_ret = tool_registry_register("network", wifi_tool, &network_tool_interface, true);
    if (wifi_reg_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register network_tool: %s", esp_err_to_name(wifi_reg_ret));
    }
    
    // Set up dependency injection (fs_tool -> wifi_tool)
    esp_err_t dep_ret = network_tool_set_fs_dependency(wifi_tool, fs_tool);
    if (dep_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ WiFi-FS dependency injection successful");
    }
    
    ESP_LOGI(TAG, "✅ wifi_tool: %s v%s initialized", network_tool_get_id(), network_tool_get_version());
    
    // Register network_tool with system_monitor_tool for dashboard visibility
    system_monitor_tool_registration_t network_registration = {
        .tool_handle = wifi_tool,
        .tool_id = network_tool_get_id(),
        .tool_version = network_tool_get_version(),
        .get_status_func = (esp_err_t (*)(void*, void*))network_tool_get_status,
        .status_struct_size = sizeof(network_tool_status_t)
    };
    esp_err_t network_monitor_ret = system_monitor_tool_register_tool(debug_tool, &network_registration);
    if (network_monitor_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ network_tool registered with system monitor");
    } else {
        ESP_LOGW(TAG, "Failed to register network_tool with system monitor: %s", esp_err_to_name(network_monitor_ret));
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // =============================================================================
    // Step 6: ntp_tool - Fifth per process map (time synchronization)
    // =============================================================================
    
    ESP_LOGI(TAG, "🕐 Step 6: ntp_tool - Time synchronization");
    ntp_tool_config_t ntp_config = ntp_tool_create_default_config();
    ntp_tool_handle_t ntp_tool = ntp_tool_init(&ntp_config);
    if (!ntp_tool) {
        ESP_LOGE(TAG, "Failed to initialize ntp_tool");
        return;
    }
    
    // Register with tool registry
    esp_err_t ntp_reg_ret = tool_registry_register("ntp", ntp_tool, &ntp_tool_interface, false);
    if (ntp_reg_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register ntp_tool: %s", esp_err_to_name(ntp_reg_ret));
    }
    
    ESP_LOGI(TAG, "✅ ntp_tool: %s v%s initialized", ntp_tool_get_id(), ntp_tool_get_version());
    
    // Register ntp_tool with system_monitor_tool for dashboard visibility
    system_monitor_tool_registration_t ntp_registration = {
        .tool_handle = ntp_tool,
        .tool_id = ntp_tool_get_id(),
        .tool_version = ntp_tool_get_version(),
        .get_status_func = (esp_err_t (*)(void*, void*))ntp_tool_get_status,
        .status_struct_size = sizeof(ntp_tool_status_t)
    };
    esp_err_t ntp_monitor_ret = system_monitor_tool_register_tool(debug_tool, &ntp_registration);
    if (ntp_monitor_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ ntp_tool registered with system monitor");
    } else {
        ESP_LOGW(TAG, "Failed to register ntp_tool with system monitor: %s", esp_err_to_name(ntp_monitor_ret));
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // =============================================================================
    // Step 7: payload_tool - Sixth per process map (event formatting) 
    // =============================================================================
    
    ESP_LOGI(TAG, "📦 Step 7: payload_tool - Event payload formatting");
    payload_tool_config_t payload_config = payload_tool_create_default_config();
    payload_tool_handle_t payload_tool = payload_tool_init(&payload_config);
    if (!payload_tool) {
        ESP_LOGE(TAG, "Failed to initialize payload_tool");
        return;
    }
    
    // Register with tool registry
    esp_err_t payload_reg_ret = tool_registry_register("payload", payload_tool, &payload_tool_interface, true);
    if (payload_reg_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register payload_tool: %s", esp_err_to_name(payload_reg_ret));
    }
    
    // Increment boot counter
    payload_tool_increment_boot_counter(payload_tool);
    
    ESP_LOGI(TAG, "✅ payload_tool: %s v%s initialized", payload_tool_get_id(), payload_tool_get_version());
    
    // Constitutional compliance: No tool dependencies - pure event-driven communication
    ESP_LOGI(TAG, "✅ payload_tool configured per Process Map 13 - pure event-driven");
    
    // Register payload_tool with system_monitor_tool for dashboard visibility
    system_monitor_tool_registration_t payload_registration = {
        .tool_handle = payload_tool,
        .tool_id = payload_tool_get_id(),
        .tool_version = payload_tool_get_version(),
        .get_status_func = (esp_err_t (*)(void*, void*))payload_tool_get_status,
        .status_struct_size = sizeof(payload_tool_status_t)
    };
    esp_err_t payload_monitor_ret = system_monitor_tool_register_tool(debug_tool, &payload_registration);
    if (payload_monitor_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ payload_tool registered with system monitor");
    } else {
        ESP_LOGW(TAG, "Failed to register payload_tool with system monitor: %s", esp_err_to_name(payload_monitor_ret));
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // =============================================================================
    // Step 8: rfid_tool - Seventh per process map (RFID scanning)
    // =============================================================================
    
    ESP_LOGI(TAG, "🏷️ Step 8: rfid_tool - RFID scanner (grace period mode)");
    rfid_tool_config_t rfid_config = rfid_tool_create_default_config();
    rfid_tool_handle_t rfid_tool = rfid_tool_init(&rfid_config);
    if (!rfid_tool) {
        ESP_LOGE(TAG, "Failed to initialize rfid_tool");
        return;
    }
    
    // Register with tool registry
    esp_err_t rfid_reg_ret = tool_registry_register("rfid", rfid_tool, &rfid_tool_interface, true);
    if (rfid_reg_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register rfid_tool: %s", esp_err_to_name(rfid_reg_ret));
    }
    
    ESP_LOGI(TAG, "✅ rfid_tool: %s v%s initialized", rfid_tool_get_id(), rfid_tool_get_version());
    
    // Register rfid_tool with system_monitor_tool for dashboard visibility
    system_monitor_tool_registration_t rfid_registration = {
        .tool_handle = rfid_tool,
        .tool_id = rfid_tool_get_id(),
        .tool_version = rfid_tool_get_version(),
        .get_status_func = (esp_err_t (*)(void*, void*))rfid_tool_get_status,
        .status_struct_size = sizeof(rfid_tool_status_t)
    };
    esp_err_t rfid_monitor_ret = system_monitor_tool_register_tool(debug_tool, &rfid_registration);
    if (rfid_monitor_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ rfid_tool registered with system monitor");
    } else {
        ESP_LOGW(TAG, "Failed to register rfid_tool with system monitor: %s", esp_err_to_name(rfid_monitor_ret));
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // =============================================================================
    // Step 9: http_tool (webhook_tool) - Eighth per process map
    // =============================================================================
    
    ESP_LOGI(TAG, "🌐 Step 9: http_tool (webhook_tool) - HTTP transmission");
    http_tool_config_t webhook_config = http_tool_create_default_config();
    http_tool_handle_t webhook_tool = http_tool_init(&webhook_config);
    if (!webhook_tool) {
        ESP_LOGE(TAG, "Failed to initialize webhook_tool");
        return;
    }
    
    // Register with tool registry
    esp_err_t webhook_reg_ret = tool_registry_register("http", webhook_tool, &http_tool_interface, true);
    if (webhook_reg_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register http_tool: %s", esp_err_to_name(webhook_reg_ret));
    }
    
    ESP_LOGI(TAG, "✅ webhook_tool: %s v%s initialized", http_tool_get_id(), http_tool_get_version());
    
    // Constitutional compliance: No tool dependencies - pure event-driven per Process Map 01
    ESP_LOGI(TAG, "✅ All tools communicate via ESP_EVENT per Process Map 01 authority");
    
    // Register http_tool with system_monitor_tool for dashboard visibility
    system_monitor_tool_registration_t http_registration = {
        .tool_handle = webhook_tool,
        .tool_id = http_tool_get_id(),
        .tool_version = http_tool_get_version(),
        .get_status_func = (esp_err_t (*)(void*, void*))http_tool_get_status,
        .status_struct_size = sizeof(http_tool_status_t)
    };
    esp_err_t http_monitor_ret = system_monitor_tool_register_tool(debug_tool, &http_registration);
    if (http_monitor_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ http_tool registered with system monitor");
    } else {
        ESP_LOGW(TAG, "Failed to register http_tool with system monitor: %s", esp_err_to_name(http_monitor_ret));
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // =============================================================================
    // Step 10: webserver_tool (for AP mode configuration)
    // =============================================================================
    
    ESP_LOGI(TAG, "🌐 Step 10: webserver_tool - AP mode configuration interface");
    webserver_tool_config_t webserver_config = webserver_tool_create_default_config();
    webserver_tool_handle_t webserver_tool = webserver_tool_init(&webserver_config);
    if (!webserver_tool) {
        ESP_LOGE(TAG, "Failed to initialize webserver_tool");
        return;
    }
    
    // Set up dependency injection (fs_tool -> webserver_tool)
    esp_err_t webserver_dep_ret = webserver_tool_set_fs_dependency(webserver_tool, fs_tool);
    if (webserver_dep_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Webserver-FS dependency injection successful");
    }
    
    // Register webserver_tool with tool registry  
    esp_err_t webserver_reg_ret = tool_registry_register(webserver_tool_get_id(), webserver_tool, &webserver_tool_interface, false);
    if (webserver_reg_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ webserver_tool registered with tool registry");
    } else {
        ESP_LOGW(TAG, "Failed to register webserver_tool with tool registry: %s", esp_err_to_name(webserver_reg_ret));
    }
    
    ESP_LOGI(TAG, "✅ webserver_tool: %s v%s initialized", webserver_tool_get_id(), webserver_tool_get_version());
    
    // Register webserver_tool with system_monitor_tool for dashboard visibility
    system_monitor_tool_registration_t webserver_registration = {
        .tool_handle = webserver_tool,
        .tool_id = webserver_tool_get_id(),
        .tool_version = webserver_tool_get_version(),
        .get_status_func = (esp_err_t (*)(void*, void*))webserver_tool_get_status,
        .status_struct_size = sizeof(webserver_tool_status_t)
    };
    esp_err_t webserver_monitor_ret = system_monitor_tool_register_tool(debug_tool, &webserver_registration);
    if (webserver_monitor_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ webserver_tool registered with system monitor");
    } else {
        ESP_LOGW(TAG, "Failed to register webserver_tool with system monitor: %s", esp_err_to_name(webserver_monitor_ret));
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // =============================================================================
    // Boot Sequence Validation per Process Maps
    // =============================================================================
    
    ESP_LOGI(TAG, "🎯 Boot Sequence Validation");
    boot_sequence_entry_t boot_sequence[] = {
        {"fs",        TOOL_INIT_ORDER_FS_TOOL,       false, ESP_OK},
        {"system_monitor", TOOL_INIT_ORDER_SYSTEM_MONITOR_TOOL, false, ESP_OK},
        {"feedback",  TOOL_INIT_ORDER_FEEDBACK_TOOL, false, ESP_OK},
        {"network",   TOOL_INIT_ORDER_NETWORK_TOOL,  false, ESP_OK},
        {"ntp",       TOOL_INIT_ORDER_NTP_TOOL,      false, ESP_OK},
        {"payload",   TOOL_INIT_ORDER_PAYLOAD_TOOL,  false, ESP_OK},
        {"rfid",      TOOL_INIT_ORDER_RFID_TOOL,     false, ESP_OK},
        {"http",      TOOL_INIT_ORDER_HTTP_TOOL,     false, ESP_OK},
        {webserver_tool_get_id(), TOOL_INIT_ORDER_WEBSERVER_TOOL, false, ESP_OK}
    };
    
    esp_err_t boot_ret = tool_registry_boot_sequence_init(boot_sequence, 9);
    if (boot_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Boot sequence validation PASSED");
    } else {
        ESP_LOGW(TAG, "⚠️ Boot sequence validation completed with warnings");
    }
    
    // =============================================================================
    // Event Handler Registration (Process Map Communication)
    // =============================================================================
    
    ESP_LOGI(TAG, "🎯 Registering event coordination handlers");
    esp_err_t wifi_event_ret = esp_event_handler_register(NETWORK_TOOL_EVENTS, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    if (wifi_event_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ WiFi→Feedback event coordination registered");
    }
    
    // Constitutional Authority: Register RFID→DATA orchestrator per Process Map 01
    esp_err_t rfid_event_ret = esp_event_handler_register(RFID_EVENTS, ESP_EVENT_ANY_ID, orchestrator_rfid_event_handler, NULL);
    if (rfid_event_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Constitutional flow: RFID→orchestrator→payload_tool registered");
    } else {
        ESP_LOGW(TAG, "⚠️ Failed to register RFID event handler: %s", esp_err_to_name(rfid_event_ret));
    }
    
    // =============================================================================
    // Process Map 13: Start Payload Tool Event Subscription
    // =============================================================================
    
    ESP_LOGI(TAG, "📦 Constitutional Authority: Orchestrator handles RFID→DATA per Process Map 01");
    ESP_LOGI(TAG, "✅ RFID events will flow: RFID → ESP_EVENT → orchestrator → payload_tool");
    ESP_LOGI(TAG, "🚫 Removed direct payload_tool RFID subscription - violates Process Map 01");
    
    ESP_LOGI(TAG, "🔧 Constitutional event flow: RFID → payload_tool → http_tool (Process Maps 13 & 14)");
    
    // =============================================================================
    // 5-Second Boot Grace Period per Process Maps
    // =============================================================================
    
    ESP_LOGI(TAG, "⏳ Starting 5-second boot grace period (RFID scanning disabled)");
    feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_BOOTING);  // CYAN breathing
    
    // Check for tag presence during grace period
    bool initial_tag_present = false;
    for (int i = 0; i < 50; i++) {  // 50 x 100ms = 5 seconds
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Check if tag is present (but don't trigger events)
        rfid_tool_status_t rfid_status;
        if (rfid_tool_get_status(rfid_tool, &rfid_status) == ESP_OK) {
            if (rfid_status.tag_present && !initial_tag_present) {
                initial_tag_present = true;
                ESP_LOGI(TAG, "🏷️ Tag detected during grace period (will be processed after grace period)");
            }
        }
    }
    
    ESP_LOGI(TAG, "✅ Grace period complete");
    
    // =============================================================================
    // WiFi Configuration and Auto-Connection
    // =============================================================================
    
    ESP_LOGI(TAG, "📶 Loading WiFi configuration and starting auto-connection");
    cJSON *wifi_config_json = NULL;
    esp_err_t load_ret = fs_tool_load_json_config(fs_tool, "wifi.json", &wifi_config_json);
    
    if (load_ret == ESP_OK && wifi_config_json) {
        ESP_LOGI(TAG, "✅ WiFi configuration loaded from LittleFS");
        esp_err_t json_ret = network_tool_load_networks_from_json(wifi_tool, wifi_config_json);
        if (json_ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ WiFi networks loaded into wifi_tool");
            esp_err_t conn_ret = network_tool_start_auto_connection(wifi_tool);
            if (conn_ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ WiFi auto-connection started");
            }
        }
        cJSON_Delete(wifi_config_json);
    } else {
        ESP_LOGW(TAG, "⚠️ No WiFi configuration found - will start AP mode");
        
        // Start AP mode for configuration
        esp_err_t ap_ret = network_tool_start_ap(wifi_tool);
        if (ap_ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ AP mode started for configuration");
            
            // Start webserver for captive portal
            esp_err_t webserver_ret = webserver_tool_start(webserver_tool);
            if (webserver_ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ Captive portal webserver started");
                ESP_LOGI(TAG, "📱 Connect to WiFi AP and configure credentials via web interface");
                
                // Update system monitor for AP mode
                system_monitor_tool_set_offline_mode(debug_tool, true);
            } else {
                ESP_LOGW(TAG, "⚠️ Failed to start captive portal webserver: %s", esp_err_to_name(webserver_ret));
            }
        } else {
            ESP_LOGE(TAG, "❌ Failed to start AP mode: %s", esp_err_to_name(ap_ret));
        }
    }
    
    // =============================================================================
    // Phase 5.7: FS Tool Event Logging Validation
    // =============================================================================
    
    ESP_LOGI(TAG, "📂 Validating FS tool event logging functionality");
    
    // Create a test RFID event to validate logging works
    cJSON *test_event = cJSON_CreateObject();
    if (test_event) {
        cJSON_AddStringToObject(test_event, "event_type", "validation_test");
        cJSON_AddStringToObject(test_event, "tag_uid", "TEST123456");
        cJSON_AddNumberToObject(test_event, "timestamp", esp_timer_get_time() / 1000);
        cJSON_AddStringToObject(test_event, "device_id", "ESP32_VALIDATION");
        
        esp_err_t log_ret = fs_tool_append_json_log(fs_tool, "rfid_events.json", test_event);
        if (log_ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ FS tool event logging validation: PASS");
            
            // Verify we can read it back
            cJSON *log_data = NULL;
            esp_err_t read_ret = fs_tool_load_json_log(fs_tool, "rfid_events.json", &log_data);
            if (read_ret == ESP_OK && log_data) {
                cJSON *events_array = cJSON_GetObjectItem(log_data, "events");
                if (cJSON_IsArray(events_array)) {
                    int event_count = cJSON_GetArraySize(events_array);
                    ESP_LOGI(TAG, "✅ FS tool read validation: %d events in log", event_count);
                } else {
                    ESP_LOGW(TAG, "⚠️ FS tool read validation: events array not found");
                }
                cJSON_Delete(log_data);
            } else {
                ESP_LOGW(TAG, "⚠️ FS tool read validation failed: %s", esp_err_to_name(read_ret));
            }
        } else {
            ESP_LOGE(TAG, "❌ FS tool event logging validation: FAIL - %s", esp_err_to_name(log_ret));
        }
        
        cJSON_Delete(test_event);
    } else {
        ESP_LOGE(TAG, "❌ FS tool validation: Failed to create test event JSON");
    }
    
    // =============================================================================
    // Enable RFID Scanning (Post Grace Period)
    // =============================================================================
    
    ESP_LOGI(TAG, "🏷️ Enabling RFID scanning (grace period complete)");
    tool_registry_set_state("rfid", TOOL_STATE_RUNNING);
    
    // Process initial tag if present during grace period
    if (initial_tag_present) {
        ESP_LOGI(TAG, "🏷️ Processing initial tag as fresh session start");
        feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
    } else {
        feedback_tool_set_state_simple(feedback_tool, FEEDBACK_STATE_IDLE);
    }
    
    // =============================================================================
    // Production Operation Loop with Debug Dashboard
    // =============================================================================
    
    ESP_LOGI(TAG, "🚀 System ready - entering production operation mode");
    ESP_LOGI(TAG, "📊 Debug dashboard will update every %d seconds", DASHBOARD_UPDATE_INTERVAL_MS / 1000);
    
    // Event system fully operational - no manual injection needed
    
    while (1) {
        // Generate and display debug dashboard
        system_monitor_dashboard_result_t dashboard;
        esp_err_t dash_ret = system_monitor_tool_generate_dashboard(debug_tool, &dashboard);
        
        if (dash_ret == ESP_OK) {
            printf("\n");
            printf("%s", dashboard.ascii_dashboard);
            system_monitor_tool_free_dashboard_result(&dashboard);
        } else {
            ESP_LOGW(TAG, "Dashboard generation failed");
        }
        
        // Update tool registry statistics
        tool_registry_stats_t stats;
        if (tool_registry_get_stats(&stats) == ESP_OK) {
            ESP_LOGI(TAG, "📊 Registry stats: %lu/%lu tools running", 
                     stats.running_tools, stats.total_tools);
        }
        
        // PROCESS MAP AUTHORITY: Update session timing for flow awareness (Constitutional Requirement)
        system_monitor_tool_update_session_timing(debug_tool);
        
        // Simple orchestration only - batching logic moved to proper tools
        
        // Wait for next dashboard update
        vTaskDelay(pdMS_TO_TICKS(DASHBOARD_UPDATE_INTERVAL_MS));
    }
    
    // Task should never reach here, but clean exit if it does
    tool_registry_cleanup();
    vTaskDelete(NULL);
}

/*
 * Main Application Entry Point
 * 
 * Minimal work here - just create MCP initialization task with proper stack
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== Process Map Compliant Time Tracker Starting ===");
    ESP_LOGI(TAG, "Creating process map boot task with %d bytes stack", MCP_TASK_STACK_SIZE);
    
    // Initialize NVS (required for WiFi) - minimal work in main task
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create process map boot task with adequate stack
    BaseType_t task_ret = xTaskCreate(
        process_map_boot_task,   // Task function
        "process_map_boot",      // Task name
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

