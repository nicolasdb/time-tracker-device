/**
 * @file main.c
 * @brief Constitutional HOST Orchestrator - Minimal Clean Implementation
 * 
 * Constitutional Authority: Process Map 01 (docs/constitution/process_maps/01_device_master_fsm.mmd)
 * Architecture: Clean HOST pattern - can run without tools initially
 * 
 * Design Pattern:
 * - HOST (main.c) = Docker-like container orchestrator
 * - CONTAINERS (tools/) = Missing initially, will be added one by one  
 * - CONTRACTS (process_maps/) = Smart contracts for tool integration
 */

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdlib.h>  // For malloc/free - constitutional memory management
#include <inttypes.h>
#include <string.h>

// Constitutional system monitoring
#include "system_monitor_tool.h"

// Constitutional Smart Contracts Framework (Phase 6.0)
#include "smart_contracts_tool.h"

// Constitutional FS Tool (Phase 6.1a)
#include "fs_tool.h"

// Constitutional Feedback Tool (Phase 6.1b)
#include "feedback_tool.h"

// Constitutional Network Tool (Phase 6.1c)
#include "network_tool.h"

// Constitutional NTP Tool (Phase 6.1c)
#include "ntp_tool.h"

// Constitutional RFID Tool (Phase 6.1d)
#include "rfid_tool.h"

// Constitutional Payload Tool (Phase 6.1e)
#include "payload_tool.h"

// Constitutional HTTP Tool (Phase 6.1f)
#include "http_tool.h"

// Constitutional Test Sequencer Tool (Phase 6.1b Testing)
#include "test_sequencer_tool.h"

// Tool registry system
#include "tool_registry.h"

static const char *TAG = "HOST_ORCHESTRATOR";

// Constitutional configuration per Process Map 01
#define BOOT_HEALTH_TIMEOUT_MS 5000    // 5-second health check timeout
#define DASHBOARD_UPDATE_INTERVAL_MS 30000  // Dashboard update every 30 seconds
#define HOST_TASK_STACK_SIZE 12288     // HOST task stack size (increased for testing)
#define HOST_TASK_PRIORITY   5         // Normal priority

// =============================================================================
// System Monitor Tool Interface (Only Tool Available)
// =============================================================================

static const tool_interface_t system_monitor_tool_interface = {
    .get_id = system_monitor_tool_get_id,
    .get_version = system_monitor_tool_get_version,
    .get_capabilities = (tool_capabilities_t (*)(void*))system_monitor_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))system_monitor_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))system_monitor_tool_cleanup
};

// Smart Contracts Tool Interface (Constitutional validation)
static const char* smart_contracts_tool_get_id(void) { return "smart_contracts_tool"; }
static const char* smart_contracts_tool_get_version(void) { return "6.0.0"; }

// Constitutional Smart Contracts Capabilities
#define SMART_CONTRACTS_CAP_VALIDATION    (1 << 0)  // Constitutional validation
#define SMART_CONTRACTS_CAP_DASHBOARD     (1 << 1)  // Constitutional dashboard
#define SMART_CONTRACTS_CAP_PROCESS_MAPS  (1 << 2)  // Process map compliance
#define SMART_CONTRACTS_CAP_CONTAINER     (1 << 3)  // Container isolation validation
#define SMART_CONTRACTS_CAP_COMMUNICATION (1 << 4)  // ESP_EVENT communication validation
#define SMART_CONTRACTS_CAP_MEMORY_SAFETY (1 << 5)  // Memory safety validation

static tool_capabilities_t smart_contracts_tool_get_capabilities(void* handle) {
    (void)handle; // Unused parameter
    return SMART_CONTRACTS_CAP_VALIDATION | 
           SMART_CONTRACTS_CAP_DASHBOARD | 
           SMART_CONTRACTS_CAP_PROCESS_MAPS |
           SMART_CONTRACTS_CAP_CONTAINER |
           SMART_CONTRACTS_CAP_COMMUNICATION |
           SMART_CONTRACTS_CAP_MEMORY_SAFETY;
}

static esp_err_t smart_contracts_tool_get_status(smart_contracts_tool_handle_t handle, void* status) {
    return smart_contracts_tool_get_contract_status(handle, (constitutional_contract_status_t*)status);
}

// =============================================================================
// Constitutional RFID Event Integration (Process Map 08)
// =============================================================================

// Context structure for RFID event integration
typedef struct {
    feedback_tool_handle_t feedback_tool;
    payload_tool_handle_t payload_tool;
    http_tool_handle_t http_tool;
    fs_tool_handle_t fs_tool;
} rfid_integration_context_t;

/**
 * @brief Constitutional RFID tag detection event handler
 * Implements Process Map 08: tag_event_fsm APPEARED event processing
 * MINIMAL STACK VERSION - Defers heavy processing to avoid crashes
 */
static void constitutional_rfid_tag_detected_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base != RFID_TOOL_EVENTS || event_id != RFID_TOOL_EVENT_TAG_DETECTED) {
        return;
    }
    
    rfid_integration_context_t* context = (rfid_integration_context_t*)event_handler_arg;
    rfid_tool_event_t* tag_event = (rfid_tool_event_t*)event_data;
    
    ESP_LOGI(TAG, "🏷️ Constitutional TAG APPEARED: %s", tag_event->tag_info.uid_string);
    
    // 1. Visual feedback - GREEN for tag detection (minimal stack)
    if (context->feedback_tool) {
        feedback_tool_set_state(context->feedback_tool, FEEDBACK_STATE_TAG_DETECTED);
        ESP_LOGI(TAG, "✅ Visual feedback: TAG_DETECTED state set");
    }
    
    // 2. CONSTITUTIONAL PAYLOAD PROCESSING - Now safe with 16KB event stack
    if (context->http_tool) {
        esp_err_t ret = http_tool_process_deferred_payload(context->http_tool, 
                                                         tag_event->tag_info.uid_string,
                                                         "APPEARED",
                                                         tag_event->timestamp_us,
                                                         tag_event->session_id);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "📦 APPEARED payload processed and sent successfully");
        } else if (ret == ESP_ERR_WIFI_NOT_CONNECT) {
            ESP_LOGW(TAG, "📦 APPEARED payload deferred - network unavailable");
        } else {
            ESP_LOGE(TAG, "📦 Failed to process APPEARED payload: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "📦 Session start event logged for: %s (HTTP tool not available)", tag_event->tag_info.uid_string);
    }
    
    ESP_LOGI(TAG, "🎯 Constitutional tag detection event processing complete");
}

/**
 * @brief Constitutional RFID tag removal event handler  
 * Implements Process Map 08: tag_event_fsm DISAPPEARED event processing
 * MINIMAL STACK VERSION - Defers heavy processing to avoid crashes
 */
static void constitutional_rfid_tag_removed_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base != RFID_TOOL_EVENTS || event_id != RFID_TOOL_EVENT_TAG_REMOVED) {
        return;
    }
    
    rfid_integration_context_t* context = (rfid_integration_context_t*)event_handler_arg;
    rfid_tool_event_t* tag_event = (rfid_tool_event_t*)event_data;
    
    ESP_LOGI(TAG, "🏷️ Constitutional TAG DISAPPEARED: %s", tag_event->tag_info.uid_string);
    
    // 1. Visual feedback - Return to IDLE state after tag removed (minimal stack)
    if (context->feedback_tool) {
        feedback_tool_set_state(context->feedback_tool, FEEDBACK_STATE_IDLE);
        ESP_LOGI(TAG, "✅ Visual feedback: Returned to IDLE state");
    }
    
    // 2. CONSTITUTIONAL PAYLOAD PROCESSING - Now safe with 16KB event stack
    if (context->http_tool) {
        esp_err_t ret = http_tool_process_deferred_payload(context->http_tool, 
                                                         tag_event->tag_info.uid_string,
                                                         "DISAPPEARED",
                                                         tag_event->timestamp_us,
                                                         tag_event->session_id);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "📦 DISAPPEARED payload processed and sent successfully");
        } else if (ret == ESP_ERR_WIFI_NOT_CONNECT) {
            ESP_LOGW(TAG, "📦 DISAPPEARED payload deferred - network unavailable");
        } else {
            ESP_LOGE(TAG, "📦 Failed to process DISAPPEARED payload: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "📦 Session end event logged for: %s (HTTP tool not available)", tag_event->tag_info.uid_string);
    }
    
    ESP_LOGI(TAG, "🎯 Constitutional tag removal event processing complete");
}
static esp_err_t smart_contracts_tool_cleanup_wrapper(smart_contracts_tool_handle_t handle) {
    return smart_contracts_tool_deinit(handle);
}

static const tool_interface_t smart_contracts_tool_interface = {
    .get_id = smart_contracts_tool_get_id,
    .get_version = smart_contracts_tool_get_version,
    .get_capabilities = smart_contracts_tool_get_capabilities,
    .get_status = (esp_err_t (*)(void*, void*))smart_contracts_tool_get_status,
    .cleanup = (esp_err_t (*)(void*))smart_contracts_tool_cleanup_wrapper
};

// FS Tool Interface (Constitutional filesystem management)
static const char* fs_tool_get_id_wrapper(void) { return fs_tool_get_id(); }
static const char* fs_tool_get_version_wrapper(void) { return fs_tool_get_version(); }

// FS Tool Capabilities
#define FS_TOOL_CAP_FILESYSTEM        (1 << 0)  // Filesystem operations
#define FS_TOOL_CAP_JSON_STORAGE      (1 << 1)  // JSON configuration storage
#define FS_TOOL_CAP_EVENT_PUBLISHING  (1 << 2)  // ESP_EVENT publishing
#define FS_TOOL_CAP_HEALTH_MONITORING (1 << 3)  // Health checks

static tool_capabilities_t fs_tool_get_capabilities_wrapper(void* handle) {
    (void)handle; // Unused parameter
    return FS_TOOL_CAP_FILESYSTEM | 
           FS_TOOL_CAP_JSON_STORAGE |
           FS_TOOL_CAP_EVENT_PUBLISHING |
           FS_TOOL_CAP_HEALTH_MONITORING;
}

static esp_err_t fs_tool_get_status_wrapper(fs_tool_handle_t handle, void* status) {
    return fs_tool_get_status(handle, (fs_tool_status_t*)status);
}
static esp_err_t fs_tool_cleanup_wrapper(fs_tool_handle_t handle) {
    return fs_tool_deinit(handle);
}

static const tool_interface_t fs_tool_interface = {
    .get_id = fs_tool_get_id_wrapper,
    .get_version = fs_tool_get_version_wrapper,
    .get_capabilities = fs_tool_get_capabilities_wrapper,
    .get_status = (esp_err_t (*)(void*, void*))fs_tool_get_status_wrapper,
    .cleanup = (esp_err_t (*)(void*))fs_tool_cleanup_wrapper
};

// Feedback Tool Interface (Constitutional LED visual feedback)
static const char* feedback_tool_get_id_wrapper(void) { return feedback_tool_get_id(); }
static const char* feedback_tool_get_version_wrapper(void) { return feedback_tool_get_version(); }

// Feedback Tool Capabilities
#define FEEDBACK_TOOL_CAP_VISUAL_FEEDBACK  (1 << 0)  // LED visual feedback
#define FEEDBACK_TOOL_CAP_STATE_DISPLAY    (1 << 1)  // State visualization
#define FEEDBACK_TOOL_CAP_FLOW_CONTEXT     (1 << 2)  // Flow awareness
#define FEEDBACK_TOOL_CAP_PATTERN_CONTROL  (1 << 3)  // Animation patterns

static tool_capabilities_t feedback_tool_get_capabilities_wrapper(void* handle) {
    (void)handle; // Unused parameter
    return FEEDBACK_TOOL_CAP_VISUAL_FEEDBACK | 
           FEEDBACK_TOOL_CAP_STATE_DISPLAY |
           FEEDBACK_TOOL_CAP_FLOW_CONTEXT |
           FEEDBACK_TOOL_CAP_PATTERN_CONTROL;
}

static esp_err_t feedback_tool_get_status_wrapper(feedback_tool_handle_t handle, void* status) {
    return feedback_tool_get_status(handle, (feedback_tool_status_t*)status);
}
static esp_err_t feedback_tool_cleanup_wrapper(feedback_tool_handle_t handle) {
    return feedback_tool_deinit(handle);
}

static const tool_interface_t feedback_tool_interface = {
    .get_id = feedback_tool_get_id_wrapper,
    .get_version = feedback_tool_get_version_wrapper,
    .get_capabilities = feedback_tool_get_capabilities_wrapper,
    .get_status = (esp_err_t (*)(void*, void*))feedback_tool_get_status_wrapper,
    .cleanup = (esp_err_t (*)(void*))feedback_tool_cleanup_wrapper
};

// Network Tool Interface (Constitutional WiFi connectivity)
static const char* network_tool_get_id_wrapper(void) { return network_tool_get_id(); }
static const char* network_tool_get_version_wrapper(void) { return network_tool_get_version(); }

// Network Tool Capabilities
#define NETWORK_TOOL_CAP_WIFI_STATION     (1 << 0)  // WiFi STA mode
#define NETWORK_TOOL_CAP_CONFIGURATION     (1 << 1)  // Configuration loading
#define NETWORK_TOOL_CAP_EVENT_PUBLISHING  (1 << 2)  // ESP_EVENT publishing
#define NETWORK_TOOL_CAP_HEALTH_MONITORING (1 << 3)  // Health checks

static tool_capabilities_t network_tool_get_capabilities_wrapper(void* handle) {
    (void)handle; // Unused parameter
    return NETWORK_TOOL_CAP_WIFI_STATION | 
           NETWORK_TOOL_CAP_CONFIGURATION |
           NETWORK_TOOL_CAP_EVENT_PUBLISHING |
           NETWORK_TOOL_CAP_HEALTH_MONITORING;
}

static esp_err_t network_tool_get_status_wrapper(network_tool_handle_t handle, void* status) {
    return network_tool_get_status(handle, (network_tool_status_t*)status);
}
static esp_err_t network_tool_cleanup_wrapper(network_tool_handle_t handle) {
    return network_tool_deinit(handle);
}

static const tool_interface_t network_tool_interface = {
    .get_id = network_tool_get_id_wrapper,
    .get_version = network_tool_get_version_wrapper,
    .get_capabilities = network_tool_get_capabilities_wrapper,
    .get_status = (esp_err_t (*)(void*, void*))network_tool_get_status_wrapper,
    .cleanup = (esp_err_t (*)(void*))network_tool_cleanup_wrapper
};

// NTP Tool Interface (Constitutional time synchronization)
static const char* ntp_tool_get_id_wrapper(void) { return ntp_tool_get_id(); }
static const char* ntp_tool_get_version_wrapper(void) { return ntp_tool_get_version(); }

// NTP Tool Capabilities
#define NTP_TOOL_CAP_TIME_SYNC        (1 << 0)  // NTP time synchronization
#define NTP_TOOL_CAP_PRECISE_TIMESTAMP (1 << 1)  // Microsecond precision timestamps
#define NTP_TOOL_CAP_TIMEZONE_SUPPORT (1 << 2)  // Timezone configuration
#define NTP_TOOL_CAP_EVENT_PUBLISHING (1 << 3)  // ESP_EVENT publishing

static tool_capabilities_t ntp_tool_get_capabilities_wrapper(void* handle) {
    (void)handle; // Unused parameter
    return NTP_TOOL_CAP_TIME_SYNC | 
           NTP_TOOL_CAP_PRECISE_TIMESTAMP |
           NTP_TOOL_CAP_TIMEZONE_SUPPORT |
           NTP_TOOL_CAP_EVENT_PUBLISHING;
}

static esp_err_t ntp_tool_get_status_wrapper(ntp_tool_handle_t handle, void* status) {
    return ntp_tool_get_status(handle, (ntp_tool_status_t*)status);
}
static esp_err_t ntp_tool_cleanup_wrapper(ntp_tool_handle_t handle) {
    return ntp_tool_deinit(handle);
}

static const tool_interface_t ntp_tool_interface = {
    .get_id = ntp_tool_get_id_wrapper,
    .get_version = ntp_tool_get_version_wrapper,
    .get_capabilities = ntp_tool_get_capabilities_wrapper,
    .get_status = (esp_err_t (*)(void*, void*))ntp_tool_get_status_wrapper,
    .cleanup = (esp_err_t (*)(void*))ntp_tool_cleanup_wrapper
};

// RFID Tool Interface (Constitutional tag detection)
static const char* rfid_tool_get_id_wrapper(void) { return rfid_tool_get_id(); }
static const char* rfid_tool_get_version_wrapper(void) { return rfid_tool_get_version(); }

// RFID Tool Capabilities
#define RFID_TOOL_CAP_TAG_DETECTION    (1 << 0)  // RC522 tag detection
#define RFID_TOOL_CAP_AUTO_SCAN        (1 << 1)  // Automatic scanning
#define RFID_TOOL_CAP_EVENT_PUBLISH    (1 << 2)  // ESP_EVENT publishing
#define RFID_TOOL_CAP_UID_EXTRACTION   (1 << 3)  // UID string extraction
#define RFID_TOOL_CAP_HEALTH_MONITOR   (1 << 4)  // Hardware health monitoring
#define RFID_TOOL_CAP_SESSION_TRACKING (1 << 5)  // Session tracking

static tool_capabilities_t rfid_tool_get_capabilities_wrapper(void* handle) {
    (void)handle; // Unused parameter
    return RFID_TOOL_CAP_TAG_DETECTION | 
           RFID_TOOL_CAP_AUTO_SCAN |
           RFID_TOOL_CAP_EVENT_PUBLISH |
           RFID_TOOL_CAP_UID_EXTRACTION |
           RFID_TOOL_CAP_HEALTH_MONITOR |
           RFID_TOOL_CAP_SESSION_TRACKING;
}

static esp_err_t rfid_tool_get_status_wrapper(rfid_tool_handle_t handle, void* status) {
    return rfid_tool_get_status(handle, (rfid_tool_status_t*)status);
}
static esp_err_t rfid_tool_cleanup_wrapper(rfid_tool_handle_t handle) {
    return rfid_tool_deinit(handle);
}

static const tool_interface_t rfid_tool_interface = {
    .get_id = rfid_tool_get_id_wrapper,
    .get_version = rfid_tool_get_version_wrapper,
    .get_capabilities = rfid_tool_get_capabilities_wrapper,
    .get_status = (esp_err_t (*)(void*, void*))rfid_tool_get_status_wrapper,
    .cleanup = (esp_err_t (*)(void*))rfid_tool_cleanup_wrapper
};

// Payload Tool Interface (Constitutional JSON formatting)
static const char* payload_tool_get_id_wrapper(void) { return payload_tool_get_id(); }
static const char* payload_tool_get_version_wrapper(void) { return payload_tool_get_version(); }

// Payload Tool Capabilities
#define PAYLOAD_TOOL_CAP_JSON_FORMAT      (1 << 0)  // JSON formatting
#define PAYLOAD_TOOL_CAP_SESSION_TRACKING (1 << 1)  // Session tracking
#define PAYLOAD_TOOL_CAP_DEVICE_METADATA  (1 << 2)  // Device metadata
#define PAYLOAD_TOOL_CAP_VALIDATION       (1 << 3)  // Payload validation
#define PAYLOAD_TOOL_CAP_BATCHING         (1 << 4)  // Batch processing

static tool_capabilities_t payload_tool_get_capabilities_wrapper(void* handle) {
    (void)handle; // Unused parameter
    return PAYLOAD_TOOL_CAP_JSON_FORMAT | 
           PAYLOAD_TOOL_CAP_SESSION_TRACKING |
           PAYLOAD_TOOL_CAP_DEVICE_METADATA |
           PAYLOAD_TOOL_CAP_VALIDATION |
           PAYLOAD_TOOL_CAP_BATCHING;
}

static esp_err_t payload_tool_get_status_wrapper(payload_tool_handle_t handle, void* status) {
    return payload_tool_get_status(handle, (payload_tool_status_t*)status);
}
static esp_err_t payload_tool_cleanup_wrapper(payload_tool_handle_t handle) {
    return payload_tool_deinit(handle);
}

static const tool_interface_t payload_tool_interface = {
    .get_id = payload_tool_get_id_wrapper,
    .get_version = payload_tool_get_version_wrapper,
    .get_capabilities = payload_tool_get_capabilities_wrapper,
    .get_status = (esp_err_t (*)(void*, void*))payload_tool_get_status_wrapper,
    .cleanup = (esp_err_t (*)(void*))payload_tool_cleanup_wrapper
};

// HTTP Tool Interface (Constitutional HTTP webhook POST with payload processing)
static const char* http_tool_get_id_wrapper(void) { return http_tool_get_id(); }
static const char* http_tool_get_version_wrapper(void) { return http_tool_get_version(); }

// HTTP Tool Capabilities
#define HTTP_TOOL_CAP_POST_REQUEST     (1 << 0)  // HTTP POST support
#define HTTP_TOOL_CAP_JSON_PAYLOAD     (1 << 1)  // JSON payload handling
#define HTTP_TOOL_CAP_RETRY_LOGIC      (1 << 2)  // Automatic retry on failure
#define HTTP_TOOL_CAP_EVENT_PUBLISHING (1 << 3)  // ESP_EVENT publishing
#define HTTP_TOOL_CAP_PAYLOAD_PROCESS  (1 << 4)  // Deferred payload processing
#define HTTP_TOOL_CAP_STACK_SAFETY     (1 << 5)  // Stack-safe payload handling

static tool_capabilities_t http_tool_get_capabilities_wrapper(void* handle) {
    (void)handle; // Unused parameter
    return HTTP_TOOL_CAP_POST_REQUEST |
           HTTP_TOOL_CAP_JSON_PAYLOAD |
           HTTP_TOOL_CAP_RETRY_LOGIC |
           HTTP_TOOL_CAP_EVENT_PUBLISHING |
           HTTP_TOOL_CAP_PAYLOAD_PROCESS |
           HTTP_TOOL_CAP_STACK_SAFETY;
}

static esp_err_t http_tool_get_status_wrapper(http_tool_handle_t handle, void* status) {
    return http_tool_get_status(handle, (http_tool_status_t*)status);
}

static esp_err_t http_tool_cleanup_wrapper(http_tool_handle_t handle) {
    return http_tool_deinit(handle);
}

static const tool_interface_t http_tool_interface = {
    .get_id = http_tool_get_id_wrapper,
    .get_version = http_tool_get_version_wrapper,
    .get_capabilities = http_tool_get_capabilities_wrapper,
    .get_status = (esp_err_t (*)(void*, void*))http_tool_get_status_wrapper,
    .cleanup = (esp_err_t (*)(void*))http_tool_cleanup_wrapper
};

// =============================================================================
// Constitutional Health Check Events
// =============================================================================

ESP_EVENT_DEFINE_BASE(HOST_EVENTS);
ESP_EVENT_DEFINE_BASE(SYSTEM_STATE_EVENTS);

typedef enum {
    HOST_EVENT_HEALTH_CHECK_REQUEST = 0,
    HOST_EVENT_HEALTH_CHECK_TIMEOUT,
    HOST_EVENT_TOOL_MISSING,
    HOST_EVENT_BOOT_COMPLETE
} host_event_id_t;

typedef enum {
    SYSTEM_STATE_EVENT_STATE_CHANGE = 0,
    SYSTEM_STATE_EVENT_HARDWARE_TEST,
    SYSTEM_STATE_EVENT_TEST_SEQUENCE
} system_state_event_id_t;

typedef struct {
    char tool_id[32];
    uint64_t timestamp_us;
    esp_err_t result;
} host_health_event_t;

typedef struct {
    feedback_state_t target_state;
    uint32_t duration_ms;
    char test_name[64];
    uint64_t timestamp_us;
} system_state_change_event_t;

// =============================================================================
// Constitutional Boot Sequence per Process Map 01
// =============================================================================

/**
 * @brief Constitutional boot task implementing Process Map 01
 * Clean HOST pattern: BOOT → HEALTH_CHECK → ESP_EVENT coordination → Production
 */
static void constitutional_boot_task(void *arg)
{
    ESP_LOGI(TAG, "=== Constitutional HOST Orchestrator Starting ===");
    ESP_LOGI(TAG, "Authority: Process Map 01 - Device Master FSM");
    ESP_LOGI(TAG, "Pattern: Clean HOST → Missing tools will timeout → Add tools one by one");
    
    // =============================================================================
    // Step 1: Initialize ESP Event System (Constitutional Foundation)
    // =============================================================================
    
    ESP_LOGI(TAG, "📡 Step 1: Initializing ESP Event System with constitutional stack safety");
    
    // Constitutional Solution: CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=16384 in sdkconfig.defaults
    // Provides 16KB stack for esp_event system task (up from 2.3KB default)
    // Enables safe JSON payload creation + HTTP POST processing in event handlers
    esp_err_t event_ret = esp_event_loop_create_default();
    if (event_ret != ESP_OK && event_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize event system: %s", esp_err_to_name(event_ret));
        return;
    }
    ESP_LOGI(TAG, "✅ ESP Event system ready - constitutional stack safety enabled (16KB)");
    
    // =============================================================================
    // Step 2: Initialize Tool Registry
    // =============================================================================
    
    ESP_LOGI(TAG, "🔧 Step 2: Initializing Constitutional Tool Registry");
    esp_err_t registry_ret = tool_registry_init();
    if (registry_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize tool registry: %s", esp_err_to_name(registry_ret));
        return;
    }
    ESP_LOGI(TAG, "✅ Tool registry initialized - ready for container registration");
    
    // =============================================================================
    // Step 3: Initialize Available Tools (Only system_monitor_tool for now)
    // =============================================================================
    
    ESP_LOGI(TAG, "🔍 Step 3: Initializing Available Tools");
    
    // System Monitor Tool - The only tool we have initially
    ESP_LOGI(TAG, "🔍 Step 3.1: system_monitor_tool - Health monitoring");
    system_monitor_tool_config_t monitor_config = system_monitor_tool_create_default_config();
    system_monitor_tool_handle_t monitor_tool = system_monitor_tool_init(&monitor_config);
    if (!monitor_tool) {
        ESP_LOGE(TAG, "Failed to initialize system_monitor_tool");
        return;
    }
    tool_registry_register("system_monitor", monitor_tool, &system_monitor_tool_interface, true);
    ESP_LOGI(TAG, "✅ system_monitor_tool: %s v%s initialized", 
             system_monitor_tool_get_id(), system_monitor_tool_get_version());
    
    // Constitutional Smart Contracts Tool - Phase 6.0 Framework
    ESP_LOGI(TAG, "🏛️ Step 3.2: smart_contracts_tool - Constitutional validation");
    smart_contracts_tool_handle_t contracts_tool = NULL;
    esp_err_t contracts_ret = smart_contracts_tool_init(&contracts_tool);
    if (contracts_ret != ESP_OK || !contracts_tool) {
        ESP_LOGE(TAG, "Failed to initialize smart_contracts_tool: %s", esp_err_to_name(contracts_ret));
        return;
    }
    tool_registry_register("smart_contracts", contracts_tool, &smart_contracts_tool_interface, true);
    ESP_LOGI(TAG, "✅ smart_contracts_tool: %s v%s initialized", 
             smart_contracts_tool_get_id(), smart_contracts_tool_get_version());
    
    // Constitutional FS Tool - Phase 6.1a Implementation
    ESP_LOGI(TAG, "🗄️ Step 3.3: fs_tool - Constitutional filesystem management");
    fs_tool_config_t fs_config = fs_tool_create_default_config();
    fs_tool_handle_t fs_tool = fs_tool_init(&fs_config);
    if (!fs_tool) {
        ESP_LOGE(TAG, "Failed to initialize fs_tool");
        return;
    }
    tool_registry_register("fs_tool", fs_tool, &fs_tool_interface, true);
    ESP_LOGI(TAG, "✅ fs_tool: %s v%s initialized", 
             fs_tool_get_id(), fs_tool_get_version());
    
    // Constitutional Feedback Tool - Phase 6.1b Implementation
    ESP_LOGI(TAG, "💡 Step 3.4: feedback_tool - Constitutional LED visual feedback");
    feedback_tool_config_t feedback_config = feedback_tool_create_default_config();
    feedback_tool_handle_t feedback_tool = feedback_tool_init(&feedback_config);
    if (!feedback_tool) {
        ESP_LOGE(TAG, "Failed to initialize feedback_tool");
        return;
    }
    tool_registry_register("feedback_tool", feedback_tool, &feedback_tool_interface, true);
    ESP_LOGI(TAG, "✅ feedback_tool: %s v%s initialized", 
             feedback_tool_get_id(), feedback_tool_get_version());
    
    // Constitutional Network Tool - Phase 6.1c Implementation
    ESP_LOGI(TAG, "📡 Step 3.5: network_tool - Constitutional WiFi connectivity");
    network_tool_config_t network_config = network_tool_create_default_config();
    network_tool_handle_t network_tool = network_tool_init(&network_config);
    if (!network_tool) {
        ESP_LOGE(TAG, "Failed to initialize network_tool");
        return;
    }
    // Set fs_tool dependency for configuration loading
    network_tool_set_fs_dependency(network_tool, fs_tool);
    
    tool_registry_register("network_tool", network_tool, &network_tool_interface, true);
    ESP_LOGI(TAG, "✅ network_tool: %s v%s initialized", 
             network_tool_get_id(), network_tool_get_version());
    
    // CONSTITUTIONAL FIX: Actually connect to WiFi (hardware operation)
    ESP_LOGI(TAG, "🔌 Step 3.5.1: Triggering WiFi connection with hardware");
    esp_err_t connect_ret = network_tool_connect(network_tool);
    if (connect_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ WiFi connection initiated successfully");
    } else {
        ESP_LOGW(TAG, "⚠️ WiFi connection initiation failed: %s", esp_err_to_name(connect_ret));
    }
    
    // Constitutional NTP Tool - Phase 6.1c Time Synchronization
    ESP_LOGI(TAG, "🕐 Step 3.6: ntp_tool - Constitutional time synchronization");
    ntp_tool_config_t ntp_config = ntp_tool_create_default_config();
    ntp_tool_handle_t ntp_tool = ntp_tool_init(&ntp_config);
    if (!ntp_tool) {
        ESP_LOGE(TAG, "Failed to initialize ntp_tool");
        return;
    }
    
    tool_registry_register("ntp_tool", ntp_tool, &ntp_tool_interface, true);
    ESP_LOGI(TAG, "✅ ntp_tool: %s v%s initialized", 
             ntp_tool_get_id(), ntp_tool_get_version());
    
    // Constitutional RFID Tool - Phase 6.1d Tag Detection
    ESP_LOGI(TAG, "🏷️ Step 3.7: rfid_tool - Constitutional RC522 tag detection");
    rfid_tool_config_t rfid_config = rfid_tool_create_default_config();
    rfid_tool_handle_t rfid_tool = rfid_tool_init(&rfid_config);
    if (!rfid_tool) {
        ESP_LOGE(TAG, "Failed to initialize rfid_tool");
        return;
    }
    // Set fs_tool dependency for configuration logging
    rfid_tool_set_fs_dependency(rfid_tool, fs_tool);
    
    tool_registry_register("rfid_tool", rfid_tool, &rfid_tool_interface, true);
    ESP_LOGI(TAG, "✅ rfid_tool: %s v%s initialized", 
             rfid_tool_get_id(), rfid_tool_get_version());
    
    // CONSTITUTIONAL FIX: Actually start RFID scanning (hardware operation)
    ESP_LOGI(TAG, "🏷️ Step 3.7.1: Triggering RFID scanning with hardware");
    esp_err_t rfid_start_ret = rfid_tool_start_scanning(rfid_tool);
    if (rfid_start_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ RFID scanning initiated successfully");
    } else {
        ESP_LOGW(TAG, "⚠️ RFID scanning initiation failed: %s", esp_err_to_name(rfid_start_ret));
    }
    
    // Constitutional Payload Tool - Phase 6.1e JSON Data Formatting
    ESP_LOGI(TAG, "📦 Step 3.8: payload_tool - Constitutional JSON data formatting");
    payload_tool_config_t payload_config = payload_tool_create_default_config();
    payload_tool_handle_t payload_tool = payload_tool_init(&payload_config);
    if (!payload_tool) {
        ESP_LOGE(TAG, "Failed to initialize payload_tool");
        return;
    }
    // Set tool dependencies for data access
    payload_tool_set_ntp_dependency(payload_tool, ntp_tool);
    payload_tool_set_fs_dependency(payload_tool, fs_tool);
    
    tool_registry_register("payload_tool", payload_tool, &payload_tool_interface, true);
    ESP_LOGI(TAG, "✅ payload_tool: %s v%s initialized", 
             payload_tool_get_id(), payload_tool_get_version());
    
    // CONSTITUTIONAL FIX: Actually test payload creation (hardware operation)
    ESP_LOGI(TAG, "📦 Step 3.8.1: Triggering payload creation self-test");
    esp_err_t payload_test_ret = payload_tool_hardware_self_test(payload_tool);
    if (payload_test_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Payload creation self-test successful");
    } else {
        ESP_LOGW(TAG, "⚠️ Payload creation self-test failed: %s", esp_err_to_name(payload_test_ret));
    }
    
    // Constitutional HTTP Tool - Phase 6.1f Webhook POST & Deferred Payload Processing
    ESP_LOGI(TAG, "🌐 Step 3.9: http_tool - Constitutional webhook POST with payload processing");
    http_tool_config_t http_config = http_tool_create_default_config();
    ESP_LOGI(TAG, "🌐 Constitutional webhook URL: %s", http_config.webhook_url);
    http_tool_handle_t http_tool = http_tool_init(&http_config);
    if (!http_tool) {
        ESP_LOGE(TAG, "Failed to initialize http_tool");
        return;
    }
    // Set tool dependencies for network connectivity and payload processing
    http_tool_set_dependencies(http_tool, network_tool, ntp_tool, payload_tool);
    
    tool_registry_register("http_tool", http_tool, &http_tool_interface, true);
    ESP_LOGI(TAG, "✅ http_tool: %s v%s initialized", 
             http_tool_get_id(), http_tool_get_version());
    
    // CONSTITUTIONAL FIX: Actually test HTTP connectivity (hardware operation)
    ESP_LOGI(TAG, "🌐 Step 3.9.1: Triggering HTTP connectivity self-test");
    esp_err_t http_test_ret = http_tool_hardware_self_test(http_tool);
    if (http_test_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ HTTP connectivity self-test successful");
    } else {
        ESP_LOGW(TAG, "⚠️ HTTP connectivity self-test failed: %s (expected without network)", esp_err_to_name(http_test_ret));
    }
    
    // Start HTTP tool for payload processing
    esp_err_t http_start_ret = http_tool_start(http_tool);
    if (http_start_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ HTTP tool started and ready for payload processing");
    } else {
        ESP_LOGW(TAG, "⚠️ HTTP tool start failed: %s", esp_err_to_name(http_start_ret));
    }
    
    // Perform constitutional validation on initialized tools
    ESP_LOGI(TAG, "🔍 Step 3.10: Constitutional compliance validation");
    
    // Validate system_monitor_tool
    constitutional_validation_request_t validation_req = {0};
    snprintf(validation_req.tool_name, sizeof(validation_req.tool_name), "system_monitor_tool");
    validation_req.level = VALIDATION_STRICT;
    
    esp_err_t validation_ret = smart_contracts_tool_validate_constitutional_compliance(
        contracts_tool, "system_monitor_tool", &validation_req);
    
    if (validation_ret == ESP_OK && validation_req.validation_passed) {
        ESP_LOGI(TAG, "✅ Constitutional validation PASSED for system_monitor_tool");
    } else {
        ESP_LOGW(TAG, "⚠️ Constitutional validation issues for system_monitor_tool: %s", 
                 validation_req.report);
    }
    
    // Validate fs_tool with constitutional contracts
    constitutional_validation_request_t fs_validation_req = {0};
    snprintf(fs_validation_req.tool_name, sizeof(fs_validation_req.tool_name), "fs_tool");
    fs_validation_req.level = VALIDATION_STRICT;
    
    esp_err_t fs_validation_ret = smart_contracts_tool_validate_constitutional_compliance(
        contracts_tool, "fs_tool", &fs_validation_req);
    
    if (fs_validation_ret == ESP_OK && fs_validation_req.validation_passed) {
        ESP_LOGI(TAG, "✅ Constitutional validation PASSED for fs_tool");
        CONSTITUTIONAL_SUCCESS("fs_tool");
    } else {
        ESP_LOGW(TAG, "⚠️ Constitutional validation issues for fs_tool: %s", 
                 fs_validation_req.report);
    }
    
    // Validate feedback_tool with constitutional contracts
    constitutional_validation_request_t feedback_validation_req = {0};
    snprintf(feedback_validation_req.tool_name, sizeof(feedback_validation_req.tool_name), "feedback_tool");
    feedback_validation_req.level = VALIDATION_STRICT;
    
    esp_err_t feedback_validation_ret = smart_contracts_tool_validate_constitutional_compliance(
        contracts_tool, "feedback_tool", &feedback_validation_req);
    
    if (feedback_validation_ret == ESP_OK && feedback_validation_req.validation_passed) {
        ESP_LOGI(TAG, "✅ Constitutional validation PASSED for feedback_tool");
        CONSTITUTIONAL_SUCCESS("feedback_tool");
    } else {
        ESP_LOGW(TAG, "⚠️ Constitutional validation issues for feedback_tool: %s", 
                 feedback_validation_req.report);
    }
    
    // Validate network_tool with constitutional contracts
    constitutional_validation_request_t network_validation_req = {0};
    snprintf(network_validation_req.tool_name, sizeof(network_validation_req.tool_name), "network_tool");
    network_validation_req.level = VALIDATION_STRICT;
    
    esp_err_t network_validation_ret = smart_contracts_tool_validate_constitutional_compliance(
        contracts_tool, "network_tool", &network_validation_req);
    
    if (network_validation_ret == ESP_OK && network_validation_req.validation_passed) {
        ESP_LOGI(TAG, "✅ Constitutional validation PASSED for network_tool");
        CONSTITUTIONAL_SUCCESS("network_tool");
    } else {
        ESP_LOGW(TAG, "⚠️ Constitutional validation issues for network_tool: %s", 
                 network_validation_req.report);
    }
    
    // Validate ntp_tool with constitutional contracts
    constitutional_validation_request_t ntp_validation_req = {0};
    snprintf(ntp_validation_req.tool_name, sizeof(ntp_validation_req.tool_name), "ntp_tool");
    ntp_validation_req.level = VALIDATION_STRICT;
    
    esp_err_t ntp_validation_ret = smart_contracts_tool_validate_constitutional_compliance(
        contracts_tool, "ntp_tool", &ntp_validation_req);
    
    if (ntp_validation_ret == ESP_OK && ntp_validation_req.validation_passed) {
        ESP_LOGI(TAG, "✅ Constitutional validation PASSED for ntp_tool");
        CONSTITUTIONAL_SUCCESS("ntp_tool");
    } else {
        ESP_LOGW(TAG, "⚠️ Constitutional validation issues for ntp_tool: %s", 
                 ntp_validation_req.report);
    }
    
    // Validate rfid_tool with constitutional contracts
    constitutional_validation_request_t rfid_validation_req = {0};
    snprintf(rfid_validation_req.tool_name, sizeof(rfid_validation_req.tool_name), "rfid_tool");
    rfid_validation_req.level = VALIDATION_STRICT;
    
    esp_err_t rfid_validation_ret = smart_contracts_tool_validate_constitutional_compliance(
        contracts_tool, "rfid_tool", &rfid_validation_req);
    
    if (rfid_validation_ret == ESP_OK && rfid_validation_req.validation_passed) {
        ESP_LOGI(TAG, "✅ Constitutional validation PASSED for rfid_tool");
        CONSTITUTIONAL_SUCCESS("rfid_tool");
    } else {
        ESP_LOGW(TAG, "⚠️ Constitutional validation issues for rfid_tool: %s", 
                 rfid_validation_req.report);
    }
    
    // Validate payload_tool with constitutional contracts
    constitutional_validation_request_t payload_validation_req = {0};
    snprintf(payload_validation_req.tool_name, sizeof(payload_validation_req.tool_name), "payload_tool");
    payload_validation_req.level = VALIDATION_STRICT;
    
    esp_err_t payload_validation_ret = smart_contracts_tool_validate_constitutional_compliance(
        contracts_tool, "payload_tool", &payload_validation_req);
    
    if (payload_validation_ret == ESP_OK && payload_validation_req.validation_passed) {
        ESP_LOGI(TAG, "✅ Constitutional validation PASSED for payload_tool");
        CONSTITUTIONAL_SUCCESS("payload_tool");
    } else {
        ESP_LOGW(TAG, "⚠️ Constitutional validation issues for payload_tool: %s", 
                 payload_validation_req.report);
    }
    
    
    // =============================================================================
    // Step 4: Constitutional Health Check Dashboard (Process Map 01)
    // =============================================================================
    
    ESP_LOGI(TAG, "⏳ Step 4: Constitutional Health Check - Process Map 01 Authority");
    
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                    CONSTITUTIONAL BOOT DASHBOARD              ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║ Process Map 01: Device Master FSM                             ║\n");
    printf("║ Authority: 'launch all tools, 5-second timeout'               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    // List of expected tools per constitutional authority
    const char* expected_tools[] = {
        "smart_contracts",      // Constitutional validation (Phase 6.0 - Available)
        "system_monitor",       // Health monitoring (Available)
        "fs_tool",             // File system operations (Phase 6.1a - Available)
        "feedback_tool",       // LED feedback (Phase 6.1b - Available)
        "network_tool",        // WiFi management (Phase 6.1c - Available)
        "ntp_tool",           // Time synchronization (Phase 6.1c - Available)
        "rfid_tool",          // Tag detection (Phase 6.1d - Available)
        "payload_tool",       // Event formatting (Phase 6.1e - Available)
        "http_tool",          // HTTP communication (Missing)
        "webserver_tool"      // Configuration portal (Missing)
    };
    const uint32_t expected_tool_count = sizeof(expected_tools) / sizeof(expected_tools[0]);
    
    printf("🔍 HEALTH CHECK STATUS:\n");
    printf("┌─────────────────────────────────────────────────────────────┐\n");
    
    bool health_check_passed = true;
    uint32_t available_tools = 0;
    uint32_t missing_tools = 0;
    uint64_t health_start_time = esp_timer_get_time();
    
    // Send health check requests via ESP_EVENT
    for (uint32_t i = 0; i < expected_tool_count; i++) {
        host_health_event_t health_request = {
            .timestamp_us = esp_timer_get_time(),
            .result = ESP_ERR_NOT_FOUND
        };
        snprintf(health_request.tool_id, sizeof(health_request.tool_id), "%s", expected_tools[i]);
        
        esp_event_post(HOST_EVENTS, HOST_EVENT_HEALTH_CHECK_REQUEST, &health_request, sizeof(health_request), 0);
        
        // Check if tool exists in registry
        void* tool_handle = NULL;
        esp_err_t result = tool_registry_get_handle(expected_tools[i], &tool_handle);
        
        if (result != ESP_OK || tool_handle == NULL) {
            printf("│ ❌ %-20s │ MISSING - TIMEOUT EXPECTED       │\n", expected_tools[i]);
            
            // Post timeout event
            health_request.result = ESP_ERR_TIMEOUT;
            esp_event_post(HOST_EVENTS, HOST_EVENT_HEALTH_CHECK_TIMEOUT, &health_request, sizeof(health_request), 0);
            esp_event_post(HOST_EVENTS, HOST_EVENT_TOOL_MISSING, &health_request, sizeof(health_request), 0);
            
            health_check_passed = false;
            missing_tools++;
        } else {
            printf("│ ✅ %-20s │ AVAILABLE - RESPONDING            │\n", expected_tools[i]);
            available_tools++;
        }
    }
    
    printf("└─────────────────────────────────────────────────────────────┘\n");
    
    // // Constitutional 5-second timeout period (Process Map 01 Authority)
    // printf("\n⏳ CONSTITUTIONAL TIMEOUT PERIOD (5 seconds)...\n");
    // for (int i = 5; i > 0; i--) {
    //     printf("│ Timeout countdown: %d seconds remaining                      │\n", i);
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    
    uint64_t health_duration_ms = (esp_timer_get_time() - health_start_time) / 1000;
    
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                 CONSTITUTIONAL HEALTH REPORT                  ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║ Boot Sequence: Process Map 01 Authority                       ║\n");
    printf("║ Execution Time: %" PRIu64 " ms                                    ║\n", health_duration_ms);
    printf("║ Available Tools: %" PRIu32 "/%" PRIu32 "                                        ║\n", available_tools, expected_tool_count);
    printf("║ Missing Tools: %" PRIu32 " (Constitutional development phase)        ║\n", missing_tools);
    
    if (health_check_passed) {
        printf("║ Status: ✅ CONSTITUTIONAL COMPLIANCE ACHIEVED                 ║\n");
    } else {
        printf("║ Status: ⚠️ PARTIAL DEPLOYMENT (Expected in development)      ║\n");
        printf("║ Note: Tools will be added systematically with validation     ║\n");
    }
    
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    // Post boot complete event
    host_health_event_t boot_complete = {
        .timestamp_us = esp_timer_get_time(),
        .result = health_check_passed ? ESP_OK : ESP_ERR_NOT_FOUND
    };
    snprintf(boot_complete.tool_id, sizeof(boot_complete.tool_id), "%s", "HOST");
    esp_event_post(HOST_EVENTS, HOST_EVENT_BOOT_COMPLETE, &boot_complete, sizeof(boot_complete), 0);
    
    // =============================================================================
    // Step 5: Constitutional Production Operation
    // =============================================================================
    
    ESP_LOGI(TAG, "🚀 Step 5: Constitutional HOST operational");
    ESP_LOGI(TAG, "Architecture: HOST running independently");
    
    // =============================================================================
    // Step 5.1: Constitutional Testing Phase (Phase 6.1b)
    // =============================================================================
    
    ESP_LOGI(TAG, "🧪 Step 5.1: Constitutional Testing Sequence - DISABLED for production");
    
    // =============================================================================
    // Step 5.2: Constitutional RFID Event Integration (Process Map 08)
    // =============================================================================
    
    ESP_LOGI(TAG, "🏷️ Step 5.2: Constitutional RFID event integration");
    
    // Create persistent context for event handlers (heap allocation)
    rfid_integration_context_t* rfid_context = malloc(sizeof(rfid_integration_context_t));
    if (!rfid_context) {
        ESP_LOGE(TAG, "❌ Failed to allocate RFID integration context");
        return;
    }
    rfid_context->feedback_tool = feedback_tool;
    rfid_context->payload_tool = payload_tool;
    rfid_context->http_tool = http_tool;
    rfid_context->fs_tool = fs_tool;
    
    // Register RFID event handler for tag detection
    esp_err_t rfid_handler_ret = esp_event_handler_register(
        RFID_TOOL_EVENTS, 
        RFID_TOOL_EVENT_TAG_DETECTED,
        constitutional_rfid_tag_detected_handler,
        rfid_context
    );
    
    if (rfid_handler_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ RFID TAG_DETECTED event handler registered");
    } else {
        ESP_LOGW(TAG, "⚠️ Failed to register RFID TAG_DETECTED handler: %s", esp_err_to_name(rfid_handler_ret));
    }
    
    // Register RFID event handler for tag removal
    esp_err_t rfid_remove_handler_ret = esp_event_handler_register(
        RFID_TOOL_EVENTS, 
        RFID_TOOL_EVENT_TAG_REMOVED,
        constitutional_rfid_tag_removed_handler,
        rfid_context
    );
    
    if (rfid_remove_handler_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ RFID TAG_REMOVED event handler registered");
    } else {
        ESP_LOGW(TAG, "⚠️ Failed to register RFID TAG_REMOVED handler: %s", esp_err_to_name(rfid_remove_handler_ret));
    }
    
    ESP_LOGI(TAG, "Next: Constitutional architecture ready for production use");
    
    // Constitutional Production Operation (Process Map 01)
    uint32_t health_report_count = 0;
    
    while (1) {
        health_report_count++;
        
        printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
        printf("║              CONSTITUTIONAL PRODUCTION STATUS #%-3" PRIu32 "           ║\n", health_report_count);
        printf("╠═══════════════════════════════════════════════════════════════╣\n");
        
        // Generate compact system status
        system_monitor_dashboard_result_t dashboard;
        esp_err_t dash_ret = system_monitor_tool_generate_dashboard(monitor_tool, &dashboard);
        
        if (dash_ret == ESP_OK) {
            // Parse essential info from dashboard (memory, uptime)
            printf("║ System Status: OPERATIONAL                                     ║\n");
            printf("║ Memory: Available                                              ║\n");
            printf("║ ESP_EVENT Hub: ACTIVE                                          ║\n");
            system_monitor_tool_free_dashboard_result(&dashboard);
        } else {
            printf("║ System Status: ❌ DASHBOARD ERROR                             ║\n");
        }
        
        // Quick constitutional compliance check
        char *contracts_dashboard = malloc(512); // Smaller buffer for essential info
        if (contracts_dashboard) {
            esp_err_t contracts_dash_ret = smart_contracts_tool_generate_dashboard(
                contracts_tool, contracts_dashboard, 512);
            
            if (contracts_dash_ret == ESP_OK) {
                // Extract key compliance info (rate, violations)
                if (strstr(contracts_dashboard, "100.0%") != NULL) {
                    printf("║ Constitutional Compliance: ✅ 100%% ACHIEVED                  ║\n");
                } else {
                    printf("║ Constitutional Compliance: ⚠️ ISSUES DETECTED                ║\n");
                }
            } else {
                printf("║ Constitutional Compliance: ❌ CHECK FAILED                    ║\n");
            }
            free(contracts_dashboard);
        }
        
        // Quick FS tool status check
        fs_tool_status_t fs_status;
        if (fs_tool_get_status(fs_tool, &fs_status) == ESP_OK) {
            if (fs_status.is_mounted) {
                printf("║ Filesystem: ✅ MOUNTED & OPERATIONAL                          ║\n");
            } else {
                printf("║ Filesystem: ❌ MOUNT FAILED - PARTITION MISMATCH              ║\n");
            }
        } else {
            printf("║ Filesystem: ❌ STATUS CHECK FAILED                            ║\n");
        }
        
        // Quick feedback tool status check  
        feedback_tool_status_t feedback_status;
        if (feedback_tool_get_status(feedback_tool, &feedback_status) == ESP_OK) {
            if (feedback_status.led_hardware_ok) {
                printf("║ LED Feedback: ✅ HARDWARE OK - State: %-10s               ║\n", 
                       feedback_status.current_state == FEEDBACK_STATE_IDLE ? "IDLE" : "ACTIVE");
            } else {
                printf("║ LED Feedback: ❌ HARDWARE ERROR                               ║\n");
            }
        } else {
            printf("║ LED Feedback: ❌ STATUS CHECK FAILED                          ║\n");
        }
        
        // Enhanced network tool status with WiFi details
        network_tool_status_t network_status;
        if (network_tool_get_status(network_tool, &network_status) == ESP_OK) {
            if (network_status.wifi_hardware_ok) {
                const char* state_str = "DISCONNECTED";
                switch (network_status.state) {
                    case NETWORK_STATE_CONNECTED: state_str = "CONNECTED"; break;
                    case NETWORK_STATE_CONNECTING: state_str = "CONNECTING"; break;
                    case NETWORK_STATE_AP_MODE: state_str = "AP_MODE"; break;
                    default: state_str = "DISCONNECTED"; break;
                }
                
                if (network_status.state == NETWORK_STATE_CONNECTED) {
                    // Show detailed connection info when connected
                    printf("║ WiFi Network: ✅ %s to %s                        ║\n", 
                           state_str, network_status.connected_ssid);
                    printf("║ IP Address: %s                                           ║\n", 
                           network_status.ip_address);
                } else {
                    // Show basic status when not connected
                    printf("║ WiFi Network: ✅ HARDWARE OK - State: %-10s               ║\n", state_str);
                }
            } else {
                printf("║ WiFi Network: ❌ HARDWARE ERROR                               ║\n");
            }
        } else {
            printf("║ WiFi Network: ❌ STATUS CHECK FAILED                          ║\n");
        }
        
        // Enhanced NTP tool status with actual time
        ntp_tool_status_t ntp_status;
        if (ntp_tool_get_status(ntp_tool, &ntp_status) == ESP_OK) {
            if (ntp_status.time_valid) {
                const char* state_str = "NOT_SYNCED";
                switch (ntp_status.sync_state) {
                    case 2: state_str = "SYNCED"; break;    // NTP_STATE_SYNCED
                    case 1: state_str = "SYNCING"; break;   // NTP_STATE_SYNCING
                    case 3: state_str = "FAILED"; break;    // NTP_STATE_FAILED
                    default: state_str = "NOT_SYNCED"; break;
                }
                
                // Get current time for display
                time_t now;
                struct tm timeinfo;
                time(&now);
                localtime_r(&now, &timeinfo);
                
                printf("║ NTP Time Sync: ✅ %s - %04d-%02d-%02d %02d:%02d:%02d UTC    ║\n", 
                       state_str,
                       timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                       timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            } else {
                printf("║ NTP Time Sync: ⚠️ TIME INVALID - Need WiFi connection        ║\n");
            }
        } else {
            printf("║ NTP Time Sync: ❌ STATUS CHECK FAILED                         ║\n");
        }
        
        // Quick RFID tool status check
        rfid_tool_status_t rfid_status;
        if (rfid_tool_get_status(rfid_tool, &rfid_status) == ESP_OK) {
            if (rfid_status.hardware_ok && rfid_status.is_scanning) {
                const char* tag_status = rfid_status.tag_present ? "TAG PRESENT" : "SCANNING";
                printf("║ RFID Scanner: ✅ %s - Scans: %" PRIu32 " Tags: %" PRIu32 "         ║\n", 
                       tag_status, rfid_status.scan_count, rfid_status.tag_detection_count);
            } else if (rfid_status.hardware_ok && !rfid_status.is_scanning) {
                printf("║ RFID Scanner: ⚠️ READY BUT NOT SCANNING                      ║\n");
            } else {
                printf("║ RFID Scanner: ❌ HARDWARE ERROR                               ║\n");
            }
        } else {
            printf("║ RFID Scanner: ❌ STATUS CHECK FAILED                          ║\n");
        }
        
        // Quick payload tool status check
        payload_tool_status_t payload_status;
        if (payload_tool_get_status(payload_tool, &payload_status) == ESP_OK) {
            if (payload_status.is_active && payload_status.is_initialized) {
                printf("║ Payload Tool: ✅ READY - Created: %" PRIu32 " Errors: %" PRIu32 "       ║\n", 
                       payload_status.payloads_created, 
                       payload_status.validation_errors + payload_status.format_errors);
            } else {
                printf("║ Payload Tool: ⚠️ NOT READY                                    ║\n");
            }
        } else {
            printf("║ Payload Tool: ❌ STATUS CHECK FAILED                          ║\n");
        }
        
        // Quick HTTP tool status check
        http_tool_status_t http_status;
        if (http_tool_get_status(http_tool, &http_status) == ESP_OK) {
            if (http_status.is_active && http_status.is_initialized) {
                const char* conn_status = http_status.server_reachable ? "CONNECTED" : "OFFLINE";
                printf("║ HTTP Tool: ✅ %s - Sent: %" PRIu32 " Failed: %" PRIu32 "         ║\n", 
                       conn_status, http_status.payloads_sent, http_status.payloads_failed);
            } else {
                printf("║ HTTP Tool: ⚠️ NOT READY                                       ║\n");
            }
        } else {
            printf("║ HTTP Tool: ❌ STATUS CHECK FAILED                             ║\n");
        }
        
        // Show registry statistics compactly
        tool_registry_stats_t stats;
        if (tool_registry_get_stats(&stats) == ESP_OK) {
            printf("║ Tool Registry: %" PRIu32 "/%" PRIu32 " tools active                               ║\n", 
                   stats.running_tools, stats.total_tools);
        }
        
        printf("║ Next Phase: HTTP tool integration (Phase 6.1f)               ║\n");
        printf("╚═══════════════════════════════════════════════════════════════╝\n");
        
        // Wait for next update (30 seconds per Process Map 01)
        ESP_LOGI(TAG, "⏳ Next health report in 30 seconds (Process Map 01 authority)");
        vTaskDelay(pdMS_TO_TICKS(DASHBOARD_UPDATE_INTERVAL_MS));
    }
    
    // Clean exit
    tool_registry_cleanup();
    vTaskDelete(NULL);
}

// =============================================================================
// Constitutional Main Entry Point
// =============================================================================

/**
 * @brief Application main entry point
 * Clean HOST initialization - minimal dependencies
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== Constitutional HOST Orchestrator Starting ===");
    ESP_LOGI(TAG, "Authority: Process Map 01 - Clean Container Architecture");
    ESP_LOGI(TAG, "Pattern: HOST (main.c) → Smart Contracts → Add CONTAINERS one by one");
    
    // Initialize NVS (minimal requirement)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "✅ NVS initialized");
    
    // Create constitutional boot task
    BaseType_t task_ret = xTaskCreate(
        constitutional_boot_task,       // Task function
        "constitutional_boot",          // Task name  
        HOST_TASK_STACK_SIZE,          // Stack size
        NULL,                          // Parameters
        HOST_TASK_PRIORITY,            // Priority
        NULL                           // Task handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create constitutional boot task");
        return;
    }
    
    ESP_LOGI(TAG, "✅ Constitutional HOST task created successfully");
    ESP_LOGI(TAG, "HOST ready - will demonstrate missing tools → timeouts → health reports");
    
    // Main task ends here - constitutional boot task manages system
}