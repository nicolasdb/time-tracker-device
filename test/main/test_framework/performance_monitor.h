/*
 * Performance Monitoring Framework for MCP-Inspired Refactoring
 * Phase 0: Baseline capture and regression detection
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Performance baseline metrics for regression detection
 */
typedef struct {
    size_t heap_free_baseline;          // Free heap at baseline
    size_t heap_largest_block_baseline; // Largest free block
    size_t stack_hwm_baseline;          // Stack high water mark
    uint32_t task_count_baseline;       // Number of tasks
    uint32_t event_loop_latency_us;     // Event processing latency
    uint32_t led_update_frequency_hz;   // LED update rate
    uint32_t wifi_connect_time_ms;      // WiFi connection time
    uint32_t webhook_response_time_ms;  // HTTP response time
} performance_baseline_t;

/**
 * @brief Initialize performance monitoring framework
 * @return ESP_OK on success
 */
esp_err_t performance_monitor_init(void);

/**
 * @brief Capture baseline performance metrics
 * @return ESP_OK on success
 */
esp_err_t performance_monitor_capture_baseline(void);

/**
 * @brief Get current performance baseline
 * @param baseline Pointer to store baseline data
 * @return ESP_OK on success
 */
esp_err_t performance_monitor_get_baseline(performance_baseline_t* baseline);

/**
 * @brief Validate performance against baseline (detect regressions)
 * @param tolerance_percent Acceptable performance degradation percentage
 * @return ESP_OK if within tolerance, ESP_FAIL if regression detected
 */
esp_err_t performance_monitor_validate_regression(float tolerance_percent);

/**
 * @brief Test setup - called before each test
 */
void performance_monitor_test_setup(void);

/**
 * @brief Test teardown - called after each test, validates no leaks
 */
void performance_monitor_test_teardown(void);

/**
 * @brief Log current system performance metrics
 */
void performance_monitor_log_current(void);

/**
 * @brief Start continuous performance monitoring (for long-running tests)
 * @param sample_interval_ms Monitoring sample interval
 * @return ESP_OK on success
 */
esp_err_t performance_monitor_start_continuous(uint32_t sample_interval_ms);

/**
 * @brief Stop continuous performance monitoring
 * @return ESP_OK on success
 */
esp_err_t performance_monitor_stop_continuous(void);

#ifdef __cplusplus
}
#endif