/**
 * @file smart_contracts_tool.h
 * @brief Constitutional Smart Contracts Framework - Process Map Compliance Validation
 * 
 * This tool implements constitutional validation contracts that enforce architectural integrity.
 * NOT blockchain smart contracts - constitutional process map compliance validators.
 * 
 * Constitutional Authority: docs/constitution/process_maps/
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Constitutional Smart Contracts Event Base
ESP_EVENT_DECLARE_BASE(SMART_CONTRACTS_EVENTS);

// Constitutional Smart Contracts Events
typedef enum {
    SMART_CONTRACTS_EVENT_VALIDATE_REQUEST,     // Constitutional validation requested
    SMART_CONTRACTS_EVENT_CONTRACT_VIOLATION,   // Constitutional contract violation detected
    SMART_CONTRACTS_EVENT_COMPLIANCE_ACHIEVED,  // Constitutional compliance achieved
    SMART_CONTRACTS_EVENT_FRAMEWORK_STATUS,     // Constitutional framework status update
} smart_contracts_event_t;

// Constitutional Contract Types (NOT blockchain)
typedef enum {
    CONTRACT_TYPE_CONTAINER,      // Container isolation contracts
    CONTRACT_TYPE_PROCESS,        // Process map FSM contracts
    CONTRACT_TYPE_COMMUNICATION,  // ESP_EVENT communication contracts
    CONTRACT_TYPE_MEMORY,         // Memory safety contracts
    CONTRACT_TYPE_HARDWARE,       // Hardware configuration contracts
} constitutional_contract_type_t;

// Constitutional Validation Levels
typedef enum {
    VALIDATION_PERMISSIVE,        // Warnings only
    VALIDATION_STRICT,            // Enforce constitutional compliance
    VALIDATION_EMERGENCY,         // Emergency constitutional protocol
} constitutional_validation_level_t;

// Constitutional Violation Severity
typedef enum {
    VIOLATION_SEVERITY_LOW,       // Minor constitutional deviation
    VIOLATION_SEVERITY_MEDIUM,    // Significant constitutional violation
    VIOLATION_SEVERITY_HIGH,      // Critical constitutional violation
    VIOLATION_SEVERITY_EMERGENCY, // Emergency constitutional violation
} constitutional_violation_severity_t;

// Constitutional Validation Statistics
typedef struct {
    size_t total_validations;         // Total constitutional validations performed
    size_t container_validations;     // Container contract validations
    size_t process_validations;       // Process map contract validations
    size_t communication_validations; // Communication contract validations 
    size_t memory_validations;        // Memory safety contract validations
    size_t hardware_validations;      // Hardware configuration contract validations
    size_t violations_detected;       // Total constitutional violations detected
} constitutional_validation_stats_t;

// Constitutional Validation Request
typedef struct {
    char tool_name[64];              // Tool name for validation
    char process_map_id[64];         // Process map identifier
    constitutional_validation_level_t level; // Validation strictness level
    bool validation_passed;          // Constitutional validation result
    int violations_found;            // Number of violations detected
    char report[512];                // Constitutional validation report
} constitutional_validation_request_t;

// Constitutional Contract Status
typedef struct {
    size_t total_contracts;          // Total constitutional contracts
    size_t active_contracts;         // Active constitutional contracts
    float compliance_rate;           // Constitutional compliance percentage
    constitutional_validation_stats_t stats; // Constitutional validation statistics
} constitutional_contract_status_t;

// Constitutional Violation Event Data
typedef struct {
    const char* contract_id;         // Constitutional contract identifier
    const char* tool_name;           // Tool name that violated contract
    constitutional_contract_type_t violation_type; // Type of constitutional violation
    constitutional_violation_severity_t severity;  // Violation severity level
} constitutional_violation_event_t;

// Constitutional Smart Contracts Tool Handle (Handle-based pattern)
typedef struct smart_contracts_tool* smart_contracts_tool_handle_t;

/**
 * @brief Initialize Constitutional Smart Contracts Framework
 * 
 * Initializes the constitutional validation framework with process map compliance contracts.
 * NOT blockchain smart contracts - constitutional process map validators.
 * 
 * @param out_handle Output handle for constitutional tool instance
 * @return ESP_OK on success, error code on failure
 */
esp_err_t smart_contracts_tool_init(smart_contracts_tool_handle_t* out_handle);

/**
 * @brief Validate Constitutional Compliance
 * 
 * Executes constitutional contracts to validate architectural compliance:
 * - Container isolation (zero coupling)
 * - ESP_EVENT-only communication
 * - Process map FSM compliance
 * - Memory safety patterns
 * 
 * @param handle Constitutional tool handle
 * @param tool_name Tool name to validate
 * @param request Constitutional validation request structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t smart_contracts_tool_validate_constitutional_compliance(
    smart_contracts_tool_handle_t handle,
    const char* tool_name,
    constitutional_validation_request_t* request
);

/**
 * @brief Get Constitutional Contract Status
 * 
 * Retrieves current status of all constitutional contracts and validation statistics.
 * 
 * @param handle Constitutional tool handle
 * @param status Output constitutional contract status
 * @return ESP_OK on success, error code on failure
 */
esp_err_t smart_contracts_tool_get_contract_status(
    smart_contracts_tool_handle_t handle,
    constitutional_contract_status_t* status
);

/**
 * @brief Generate Constitutional Dashboard
 * 
 * Generates comprehensive constitutional compliance dashboard.
 * Constitutional requirement: 1KB+ buffer for dashboard generation.
 * 
 * @param handle Constitutional tool handle
 * @param dashboard_buffer Output buffer for dashboard (1KB+ recommended)
 * @param buffer_size Size of dashboard buffer
 * @return ESP_OK on success, error code on failure
 */
esp_err_t smart_contracts_tool_generate_dashboard(
    smart_contracts_tool_handle_t handle,
    char* dashboard_buffer,
    size_t buffer_size
);

/**
 * @brief Deinitialize Constitutional Smart Contracts Framework
 * 
 * Properly cleanup constitutional validation framework.
 * 
 * @param handle Constitutional tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t smart_contracts_tool_deinit(smart_contracts_tool_handle_t handle);

// Constitutional Smart Contracts Macros

/**
 * @brief Constitutional Contract Validation Macro
 * 
 * Validates constitutional compliance for a tool with error handling.
 */
#define CONSTITUTIONAL_VALIDATE(handle, tool_name) do { \
    constitutional_validation_request_t req = {0}; \
    snprintf(req.tool_name, sizeof(req.tool_name), "%s", tool_name); \
    req.level = VALIDATION_STRICT; \
    esp_err_t _ret = smart_contracts_tool_validate_constitutional_compliance(handle, tool_name, &req); \
    if (_ret != ESP_OK || !req.validation_passed) { \
        ESP_LOGE("CONSTITUTIONAL", "Constitutional violation in %s: %s", tool_name, req.report); \
    } \
} while(0)

/**
 * @brief Constitutional Emergency Protocol Macro
 * 
 * Initiates emergency constitutional protocol on critical violations.
 */
#define CONSTITUTIONAL_EMERGENCY(violation_msg) do { \
    ESP_LOGE("CONSTITUTIONAL_EMERGENCY", "🚨 CONSTITUTIONAL EMERGENCY: %s", violation_msg); \
    ESP_LOGE("CONSTITUTIONAL_EMERGENCY", "Process maps are supreme authority"); \
    ESP_LOGE("CONSTITUTIONAL_EMERGENCY", "Container isolation is sacred"); \
    ESP_LOGE("CONSTITUTIONAL_EMERGENCY", "ESP_EVENT-only communication is mandatory"); \
} while(0)

/**
 * @brief Constitutional Success Declaration Macro
 * 
 * Declares constitutional compliance achievement.
 */
#define CONSTITUTIONAL_SUCCESS(tool_name) do { \
    ESP_LOGI("CONSTITUTIONAL", "🏛️ Constitutional compliance achieved: %s", tool_name); \
    ESP_LOGI("CONSTITUTIONAL", "✅ Container isolation verified"); \
    ESP_LOGI("CONSTITUTIONAL", "✅ ESP_EVENT communication confirmed"); \
    ESP_LOGI("CONSTITUTIONAL", "✅ Process map compliance validated"); \
    ESP_LOGI("CONSTITUTIONAL", "✅ Memory safety enforced"); \
} while(0)

#ifdef __cplusplus
}
#endif