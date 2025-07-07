/**
 * @file test_sequencer_tool.h
 * @brief Constitutional Testing Sequencer - Validate Tool Integration
 * 
 * Tests constitutional tool interactions by simulating system states and
 * validating tool responses through ESP_EVENT communication.
 * 
 * Constitutional Authority: Validates process map compliance and tool isolation
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constitutional Test Sequencer
// =============================================================================

/**
 * @brief Constitutional test sequencer handle
 */
typedef struct test_sequencer_tool* test_sequencer_tool_handle_t;

/**
 * @brief Test sequence types
 */
typedef enum {
    TEST_SEQUENCE_BOOT_VALIDATION,      // Test boot sequence
    TEST_SEQUENCE_STATE_TRANSITIONS,    // Test feedback state changes  
    TEST_SEQUENCE_FS_OPERATIONS,        // Test filesystem read/write
    TEST_SEQUENCE_TOOL_INTEGRATION,     // Test tool-to-tool communication
    TEST_SEQUENCE_ERROR_HANDLING,       // Test error conditions
    TEST_SEQUENCE_PERFORMANCE,          // Test performance characteristics
    TEST_SEQUENCE_CONSTITUTIONAL        // Test constitutional compliance
} test_sequence_type_t;

/**
 * @brief Test configuration
 */
typedef struct {
    uint32_t sequence_interval_ms;       // Time between test steps
    uint32_t validation_timeout_ms;      // Timeout for validations
    bool enable_logging;                 // Enable detailed logging
    bool publish_events;                 // Publish test events
    char report_file[64];               // Report output file
} test_sequencer_config_t;

/**
 * @brief Test result structure
 */
typedef struct {
    test_sequence_type_t sequence_type;
    bool passed;
    uint32_t steps_executed;
    uint32_t steps_passed;
    uint32_t steps_failed;
    uint32_t execution_time_ms;
    char details[256];
} test_result_t;

// =============================================================================
// Constitutional Test Events
// =============================================================================

ESP_EVENT_DECLARE_BASE(TEST_SEQUENCER_EVENTS);

typedef enum {
    TEST_SEQUENCER_EVENT_STARTED = 0,
    TEST_SEQUENCER_EVENT_STEP_COMPLETE,
    TEST_SEQUENCER_EVENT_SEQUENCE_COMPLETE,
    TEST_SEQUENCER_EVENT_VALIDATION_FAILED,
    TEST_SEQUENCER_EVENT_REPORT_GENERATED
} test_sequencer_event_id_t;

// =============================================================================
// Constitutional Test Interface
// =============================================================================

/**
 * @brief Get constitutional test sequencer identification
 */
const char* test_sequencer_tool_get_id(void);

/**
 * @brief Get constitutional test sequencer version
 */
const char* test_sequencer_tool_get_version(void);

/**
 * @brief Create default test configuration
 */
test_sequencer_config_t test_sequencer_tool_create_default_config(void);

/**
 * @brief Initialize constitutional test sequencer
 */
test_sequencer_tool_handle_t test_sequencer_tool_init(const test_sequencer_config_t *config);

/**
 * @brief Deinitialize constitutional test sequencer
 */
esp_err_t test_sequencer_tool_deinit(test_sequencer_tool_handle_t handle);

// =============================================================================
// Constitutional Test Sequences
// =============================================================================

/**
 * @brief Execute constitutional boot validation test
 * Tests: Tool initialization order, constitutional compliance, health checks
 */
esp_err_t test_sequencer_execute_boot_validation(test_sequencer_tool_handle_t handle, 
                                                test_result_t *result);

/**
 * @brief Execute constitutional state transition test
 * Tests: feedback_tool state changes, visual patterns, flow awareness
 */
esp_err_t test_sequencer_execute_state_transitions(test_sequencer_tool_handle_t handle, 
                                                  test_result_t *result);

/**
 * @brief Execute constitutional filesystem operations test
 * Tests: fs_tool JSON save/load, space monitoring, health checks
 */
esp_err_t test_sequencer_execute_fs_operations(test_sequencer_tool_handle_t handle, 
                                              test_result_t *result);

/**
 * @brief Execute constitutional tool integration test
 * Tests: system_monitor → fs_tool → feedback_tool communication chain
 */
esp_err_t test_sequencer_execute_tool_integration(test_sequencer_tool_handle_t handle, 
                                                 test_result_t *result);

/**
 * @brief Execute constitutional error handling test
 * Tests: Error propagation, graceful degradation, recovery
 */
esp_err_t test_sequencer_execute_error_handling(test_sequencer_tool_handle_t handle, 
                                               test_result_t *result);

/**
 * @brief Execute constitutional compliance test
 * Tests: Smart contracts validation, process map adherence
 */
esp_err_t test_sequencer_execute_constitutional_compliance(test_sequencer_tool_handle_t handle, 
                                                          test_result_t *result);

/**
 * @brief Execute real hardware validation test
 * Tests: Actual LED control, filesystem operations, hardware capabilities
 */
esp_err_t test_sequencer_execute_real_hardware_validation(test_sequencer_tool_handle_t handle,
                                                        test_result_t *result);

// =============================================================================
// Constitutional Test Utilities
// =============================================================================

/**
 * @brief Generate comprehensive test report
 */
esp_err_t test_sequencer_generate_report(test_sequencer_tool_handle_t handle, 
                                        char* report_buffer, 
                                        size_t buffer_size);

/**
 * @brief Simulate system state change for testing
 */
esp_err_t test_sequencer_simulate_state(test_sequencer_tool_handle_t handle, 
                                       const char* state_name,
                                       uint32_t duration_ms);

/**
 * @brief Validate tool response to test stimulus
 */
esp_err_t test_sequencer_validate_response(test_sequencer_tool_handle_t handle, 
                                          const char* expected_response,
                                          uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

/**
 * @brief Constitutional Testing Example
 * 
 * // Initialize test sequencer
 * test_sequencer_config_t config = test_sequencer_tool_create_default_config();
 * test_sequencer_tool_handle_t sequencer = test_sequencer_tool_init(&config);
 * 
 * // Execute state transition test
 * test_result_t result;
 * test_sequencer_execute_state_transitions(sequencer, &result);
 * 
 * // Execute filesystem test
 * test_sequencer_execute_fs_operations(sequencer, &result);
 * 
 * // Execute tool integration test
 * test_sequencer_execute_tool_integration(sequencer, &result);
 * 
 * // Generate report
 * char report[2048];
 * test_sequencer_generate_report(sequencer, report, sizeof(report));
 * 
 * // Cleanup
 * test_sequencer_tool_deinit(sequencer);
 */