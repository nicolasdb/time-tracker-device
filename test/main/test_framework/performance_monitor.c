/*
 * Performance Monitoring Framework Implementation
 * Phase 0: Baseline capture and regression detection
 */

#include "performance_monitor.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/time.h>

static const char *TAG = "PERF_MONITOR";
static performance_baseline_t baseline = {0};
static bool baseline_captured = false;
static size_t test_start_heap = 0;

esp_err_t performance_monitor_init(void)
{
    ESP_LOGI(TAG, "Initializing Performance Monitor");
    
    // Clear baseline
    memset(&baseline, 0, sizeof(baseline));
    baseline_captured = false;
    
    return ESP_OK;
}

esp_err_t performance_monitor_capture_baseline(void)
{
    ESP_LOGI(TAG, "Capturing Performance Baseline");
    
    // Capture heap metrics
    baseline.heap_free_baseline = esp_get_free_heap_size();
    baseline.heap_largest_block_baseline = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    
    // Capture task metrics
    baseline.task_count_baseline = uxTaskGetNumberOfTasks();
    
    // Capture stack high water mark for current task
    baseline.stack_hwm_baseline = uxTaskGetStackHighWaterMark(NULL);
    
    // Initialize timing baselines (will be measured during actual operations)
    baseline.event_loop_latency_us = 0;
    baseline.led_update_frequency_hz = 0;
    baseline.wifi_connect_time_ms = 0;
    baseline.webhook_response_time_ms = 0;
    
    baseline_captured = true;
    
    ESP_LOGI(TAG, "Baseline captured:");
    ESP_LOGI(TAG, "  Free heap: %d bytes", baseline.heap_free_baseline);
    ESP_LOGI(TAG, "  Largest block: %d bytes", baseline.heap_largest_block_baseline);
    ESP_LOGI(TAG, "  Task count: %d", baseline.task_count_baseline);
    ESP_LOGI(TAG, "  Stack HWM: %d bytes", baseline.stack_hwm_baseline);
    
    return ESP_OK;
}

esp_err_t performance_monitor_get_baseline(performance_baseline_t* out_baseline)
{
    if (!baseline_captured || !out_baseline) {
        return ESP_ERR_INVALID_STATE;
    }
    
    memcpy(out_baseline, &baseline, sizeof(performance_baseline_t));
    return ESP_OK;
}

esp_err_t performance_monitor_validate_regression(float tolerance_percent)
{
    if (!baseline_captured) {
        ESP_LOGE(TAG, "Baseline not captured, cannot validate regression");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Current metrics
    size_t current_heap = esp_get_free_heap_size();
    size_t current_largest = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    uint32_t current_tasks = uxTaskGetNumberOfTasks();
    size_t current_stack_hwm = uxTaskGetStackHighWaterMark(NULL);
    
    // Calculate regression thresholds
    size_t heap_threshold = baseline.heap_free_baseline * (1.0 - tolerance_percent / 100.0);
    size_t block_threshold = baseline.heap_largest_block_baseline * (1.0 - tolerance_percent / 100.0);
    
    bool regression_detected = false;
    
    // Check heap regression
    if (current_heap < heap_threshold) {
        ESP_LOGE(TAG, "HEAP REGRESSION: Current %d < Threshold %d (%.1f%% loss)", 
                 current_heap, heap_threshold, 
                 ((float)(baseline.heap_free_baseline - current_heap) / baseline.heap_free_baseline) * 100);
        regression_detected = true;
    }
    
    // Check largest block regression
    if (current_largest < block_threshold) {
        ESP_LOGE(TAG, "BLOCK REGRESSION: Current %d < Threshold %d", 
                 current_largest, block_threshold);
        regression_detected = true;
    }
    
    // Check task count increase (should be stable)
    if (current_tasks > baseline.task_count_baseline + 2) {
        ESP_LOGE(TAG, "TASK REGRESSION: Current %d > Baseline %d + 2", 
                 current_tasks, baseline.task_count_baseline);
        regression_detected = true;
    }
    
    // Check stack usage (should not increase significantly)
    if (current_stack_hwm > baseline.stack_hwm_baseline * 1.5) {
        ESP_LOGE(TAG, "STACK REGRESSION: Current %d > Baseline %d * 1.5", 
                 current_stack_hwm, baseline.stack_hwm_baseline);
        regression_detected = true;
    }
    
    if (regression_detected) {
        ESP_LOGE(TAG, "PERFORMANCE REGRESSION DETECTED");
        performance_monitor_log_current();
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Performance validation passed (tolerance: %.1f%%)", tolerance_percent);
    return ESP_OK;
}

void performance_monitor_test_setup(void)
{
    // Capture heap at start of test
    test_start_heap = esp_get_free_heap_size();
}

void performance_monitor_test_teardown(void)
{
    // Check for memory leaks
    size_t test_end_heap = esp_get_free_heap_size();
    
    if (test_end_heap < test_start_heap) {
        size_t leak_size = test_start_heap - test_end_heap;
        ESP_LOGE(TAG, "MEMORY LEAK DETECTED: %d bytes", leak_size);
        
        // Force garbage collection attempt
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Re-check after GC
        test_end_heap = esp_get_free_heap_size();
        if (test_end_heap < test_start_heap) {
            ESP_LOGE(TAG, "CONFIRMED MEMORY LEAK: %d bytes", test_start_heap - test_end_heap);
        } else {
            ESP_LOGI(TAG, "Memory leak resolved after GC");
        }
    }
}

void performance_monitor_log_current(void)
{
    size_t current_heap = esp_get_free_heap_size();
    size_t current_largest = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    uint32_t current_tasks = uxTaskGetNumberOfTasks();
    size_t current_stack_hwm = uxTaskGetStackHighWaterMark(NULL);
    
    ESP_LOGI(TAG, "Current Performance Metrics:");
    ESP_LOGI(TAG, "  Free heap: %d bytes", current_heap);
    ESP_LOGI(TAG, "  Largest block: %d bytes", current_largest);
    ESP_LOGI(TAG, "  Task count: %d", current_tasks);
    ESP_LOGI(TAG, "  Stack HWM: %d bytes", current_stack_hwm);
    
    if (baseline_captured) {
        ESP_LOGI(TAG, "Baseline Comparison:");
        ESP_LOGI(TAG, "  Heap delta: %d bytes", (int)current_heap - (int)baseline.heap_free_baseline);
        ESP_LOGI(TAG, "  Block delta: %d bytes", (int)current_largest - (int)baseline.heap_largest_block_baseline);
        ESP_LOGI(TAG, "  Task delta: %d", (int)current_tasks - (int)baseline.task_count_baseline);
    }
}

esp_err_t performance_monitor_start_continuous(uint32_t sample_interval_ms)
{
    // TODO: Implement continuous monitoring task
    ESP_LOGW(TAG, "Continuous monitoring not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t performance_monitor_stop_continuous(void)
{
    // TODO: Implement continuous monitoring task stop
    ESP_LOGW(TAG, "Continuous monitoring not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}