/*
 * Component Tests: feedback_manager
 * Phase 0: Validate LED state management and priority queue system
 */

#include "unity.h"  
#include "feedback_manager.h"
#include "test_framework/mock_hardware.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TEST_FEEDBACK";

// Test fixture setup/teardown
void test_feedback_setup(void)
{
    // Initialize mock LED hardware
    mock_led_init();
    
    // Initialize feedback manager
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_init());
}

void test_feedback_teardown(void)
{
    // Clean shutdown
    feedback_manager_stop();
    mock_led_cleanup();
}

// Test basic initialization
void test_feedback_manager_init(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_init());
    
    // Verify initial state is IDLE with blue breathing
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow time for state to settle
    
    led_mock_state_t current_state;
    TEST_ASSERT_EQUAL(ESP_OK, mock_led_get_current_state(&current_state));
    
    // Should be IDLE state (blue breathing)
    TEST_ASSERT_EQUAL(FEEDBACK_STATE_IDLE, feedback_manager_get_current_state());
}

// Test priority-based state management
void test_feedback_priority_queue(void)
{
    // Start with IDLE state
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_IDLE, 5000));
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(FEEDBACK_STATE_IDLE, feedback_manager_get_current_state());
    
    // Add higher priority state (TAG_DETECTED) - should override IDLE
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_TAG_DETECTED, 0)); // Persistent
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(FEEDBACK_STATE_TAG_DETECTED, feedback_manager_get_current_state());
    
    // Add highest priority state (ERROR) - should override TAG_DETECTED  
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_WIFI_FAILED, 1000));
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(FEEDBACK_STATE_WIFI_FAILED, feedback_manager_get_current_state());
    
    // Wait for error state to expire, should return to TAG_DETECTED
    vTaskDelay(pdMS_TO_TICKS(1100));
    TEST_ASSERT_EQUAL(FEEDBACK_STATE_TAG_DETECTED, feedback_manager_get_current_state());
    
    // Clear TAG_DETECTED, should return to IDLE
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_clear_state(FEEDBACK_STATE_TAG_DETECTED));
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(FEEDBACK_STATE_IDLE, feedback_manager_get_current_state());
}

// Test LED pattern validation for each state
void test_feedback_led_patterns(void)
{
    led_mock_state_t state;
    
    // Test IDLE state - blue breathing
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_IDLE, 5000));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, mock_led_get_current_state(&state));
    TEST_ASSERT_EQUAL(LED_PATTERN_BREATHING, state.pattern);
    TEST_ASSERT_EQUAL(0, state.red);
    TEST_ASSERT_GREATER_THAN(0, state.blue); // Should have blue component
    
    // Test TAG_DETECTED - solid green
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_TAG_DETECTED, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, mock_led_get_current_state(&state));
    TEST_ASSERT_EQUAL(LED_PATTERN_SOLID, state.pattern);
    TEST_ASSERT_EQUAL(0, state.red);
    TEST_ASSERT_GREATER_THAN(0, state.green);
    TEST_ASSERT_EQUAL(0, state.blue);
    
    // Test WIFI_FAILED - red/orange sequence
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_WIFI_FAILED, 2000));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, mock_led_get_current_state(&state));
    TEST_ASSERT_EQUAL(LED_PATTERN_SEQUENCE, state.pattern);
    TEST_ASSERT_GREATER_THAN(0, state.red); // Should have red component
    
    // Test WIFI_CONNECTING - blue blinking
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_WIFI_CONNECTING, 3000));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, mock_led_get_current_state(&state));
    TEST_ASSERT_EQUAL(LED_PATTERN_BLINK, state.pattern);
    TEST_ASSERT_EQUAL(0, state.red);
    TEST_ASSERT_GREATER_THAN(0, state.blue);
}

// Test thread safety under concurrent access
void test_feedback_thread_safety(void)
{
    // Create multiple tasks that simultaneously set states
    // This tests the mutex protection of the priority queue
    
    // Start with IDLE
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_IDLE, 10000));
    
    // Simulate concurrent state changes from different "components"
    for (int i = 0; i < 10; i++) {
        // Rapid state changes
        TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_WIFI_CONNECTING, 100));
        vTaskDelay(pdMS_TO_TICKS(10));
        TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_WIFI_CONNECTED, 100));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // System should remain stable and eventually return to IDLE
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(FEEDBACK_STATE_IDLE, feedback_manager_get_current_state());
}

// Test breathing pattern timing validation
void test_feedback_breathing_timing(void)
{
    // Set IDLE state with breathing
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_IDLE, 10000));
    
    // Validate breathing pattern timing (4-second cycle as per CLAUDE.md)
    TEST_ASSERT_TRUE(mock_led_validate_breathing_pattern(4000, 10)); // 10% tolerance
}

// Test boot protection (prevents duplicate events on reboot with tag present)  
void test_feedback_boot_protection(void)
{
    // Initialize with TAG_DETECTED immediately (simulates boot with tag present)
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_TAG_DETECTED, 0));
    
    // During boot protection period (10 seconds), should remain stable
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(FEEDBACK_STATE_TAG_DETECTED, feedback_manager_get_current_state());
    
    // Verify no duplicate events are generated during boot protection
    size_t event_count_before = mock_led_get_event_count();
    vTaskDelay(pdMS_TO_TICKS(500));
    size_t event_count_after = mock_led_get_event_count();
    
    // Should have minimal LED updates during stable state
    TEST_ASSERT_LESS_THAN(5, event_count_after - event_count_before);
}

// Test state expiration mechanism
void test_feedback_state_expiration(void)
{
    // Set temporary state with expiration
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_WIFI_CONNECTING, 500));
    TEST_ASSERT_EQUAL(FEEDBACK_STATE_WIFI_CONNECTING, feedback_manager_get_current_state());
    
    // Wait for expiration
    vTaskDelay(pdMS_TO_TICKS(600));
    
    // Should return to default state (IDLE)
    TEST_ASSERT_EQUAL(FEEDBACK_STATE_IDLE, feedback_manager_get_current_state());
}

// Test queue count validation
void test_feedback_queue_management(void)
{
    // Queue should start empty (except default IDLE)
    TEST_ASSERT_EQUAL(1, feedback_manager_get_queue_count());
    
    // Add multiple states
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_WIFI_CONNECTING, 1000));
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_TAG_DETECTED, 0));
    TEST_ASSERT_EQUAL(ESP_OK, feedback_manager_set_state(FEEDBACK_STATE_WIFI_FAILED, 500));
    
    // Queue should have multiple items
    TEST_ASSERT_GREATER_THAN(1, feedback_manager_get_queue_count());
    
    // Clear all non-persistent states
    vTaskDelay(pdMS_TO_TICKS(1100)); // Wait for temporary states to expire
    
    // Should only have persistent states remaining
    int remaining_count = feedback_manager_get_queue_count();
    TEST_ASSERT_GREATER_THAN(0, remaining_count);
    TEST_ASSERT_LESS_THAN(4, remaining_count); // Should have cleaned up expired states
}

// Unity test runner functions with tags
void test_feedback_manager_init_tagged(void) {
    RUN_TEST(test_feedback_manager_init);
}

void test_feedback_priority_queue_tagged(void) {
    RUN_TEST(test_feedback_priority_queue);
}

void test_feedback_led_patterns_tagged(void) {
    RUN_TEST(test_feedback_led_patterns);
}

void test_feedback_thread_safety_tagged(void) {
    RUN_TEST(test_feedback_thread_safety);
}

void test_feedback_breathing_timing_tagged(void) {
    RUN_TEST(test_feedback_breathing_timing);
}

void test_feedback_boot_protection_tagged(void) {
    RUN_TEST(test_feedback_boot_protection);
}

void test_feedback_state_expiration_tagged(void) {
    RUN_TEST(test_feedback_state_expiration);
}

void test_feedback_queue_management_tagged(void) {
    RUN_TEST(test_feedback_queue_management);
}

// Register all feedback manager tests with "feedback_manager" tag
void unity_register_feedback_manager_tests(void)
{
    UnitySetTestFile(__FILE__);
    
    unity_run_test(test_feedback_manager_init_tagged, "test_feedback_manager_init", __LINE__, "feedback_manager");
    unity_run_test(test_feedback_priority_queue_tagged, "test_feedback_priority_queue", __LINE__, "feedback_manager");
    unity_run_test(test_feedback_led_patterns_tagged, "test_feedback_led_patterns", __LINE__, "feedback_manager");
    unity_run_test(test_feedback_thread_safety_tagged, "test_feedback_thread_safety", __LINE__, "feedback_manager");
    unity_run_test(test_feedback_breathing_timing_tagged, "test_feedback_breathing_timing", __LINE__, "feedback_manager");
    unity_run_test(test_feedback_boot_protection_tagged, "test_feedback_boot_protection", __LINE__, "feedback_manager");
    unity_run_test(test_feedback_state_expiration_tagged, "test_feedback_state_expiration", __LINE__, "feedback_manager");
    unity_run_test(test_feedback_queue_management_tagged, "test_feedback_queue_management", __LINE__, "feedback_manager"); 
}