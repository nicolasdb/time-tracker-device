/**
 * @file rfid_tool.c
 * @brief MCP-Inspired RFID Tool Implementation
 * 
 * Transformed from rfid_manager to follow MCP tool composition patterns.
 * Uses handle-based architecture with event-driven communication.
 */

#include "rfid_tool.h"
#include "event_system.h"   // Universal event system for async communication
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

static const char *TAG = "RFID_TOOL";

// =============================================================================
// MCP Tool Event System
// =============================================================================

// ESP_EVENT_DEFINE_BASE(RFID_TOOL_EVENTS); // REMOVED: Using universal RFID_EVENTS from event_system.h

// External tool events for Phase 5.4 integration
ESP_EVENT_DECLARE_BASE(FS_TOOL_EVENTS);

// =============================================================================
// MCP Tool Configuration & Constants
// =============================================================================

#define RFID_TOOL_TASK_STACK_SIZE    4096
#define RFID_TOOL_TASK_PRIORITY      5
#define RFID_TOOL_EVENT_QUEUE_SIZE   8

// RC522 Library Integration (same as legacy)
#if CONFIG_RFID_MODULE_RC522
#include <rc522.h>
#include <driver/rc522_spi.h>
#include <rc522_picc.h>
#endif

// =============================================================================
// Internal Tool Structure (Enhanced from rfid_manager)
// =============================================================================

/**
 * @brief MCP-Inspired RFID Tool Context
 */
struct rfid_tool_context {
    // Tool Metadata (MCP Pattern)
    rfid_tool_config_t config;
    rfid_tool_capabilities_t capabilities;
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    
    // RFID State Management (Handle-based, no static globals)
    bool is_scanning;
    bool tag_present;
    rfid_tag_info_t current_tag;
    uint32_t scan_count;
    uint32_t tag_detection_count;
    uint32_t error_count;
    
    // RC522 Resources (Properly encapsulated)
#if CONFIG_RFID_MODULE_RC522
    rc522_driver_handle_t driver;
    rc522_handle_t scanner;
#endif
    esp_event_loop_handle_t event_loop;
    
    // Thread Safety
    SemaphoreHandle_t state_mutex;
    bool publish_events;
    
    // Phase 5.4: FS Tool Integration for Event Logging
    void* fs_tool_handle;                   ///< FS tool handle for event logging (opaque pointer)
    bool enable_event_logging;              ///< Enable event logging to filesystem
    
    // State Change Detection (Clean approach)
    char previous_tag_id[21];               ///< Last recorded tag state (empty = no tag)
    
    // 5-Second Debounce Logic (Process Map Authority)
    char last_debounced_tag_id[21];         ///< Last tag that passed debounce check
    uint64_t last_detection_time_us;        ///< Last detection timestamp (microseconds)
    uint32_t debounce_period_ms;            ///< Debounce period (5000ms per process maps)
    
    // Circular Buffer for Stress Test Compliance (Process Map Authority)
    rfid_tool_event_t event_buffer[50];     ///< Circular buffer for events (50 capacity per process maps)
    uint8_t buffer_write_index;             ///< Write index for circular buffer
    uint8_t buffer_read_index;              ///< Read index for circular buffer  
    uint8_t buffer_count;                   ///< Current number of events in buffer
    bool buffer_overflow_flag;              ///< Set when buffer overflows (oldest events lost)
};

// =============================================================================
// Forward Declarations
// =============================================================================

#if CONFIG_RFID_MODULE_RC522
static void rfid_picc_state_changed_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data);
static spi_host_device_t get_spi_host(int config_host);
static void update_tag_type(rfid_tag_info_t *tag_info, uint8_t sak);
#endif
static esp_err_t rfid_tool_publish_universal_event(struct rfid_tool_context *ctx, rfid_tool_event_type_t type, const char* tag_uid);
static esp_err_t log_hardware_event(struct rfid_tool_context *ctx, const char* tag_id, bool tag_present, uint64_t boot_timestamp_us);

// Circular Buffer Management (Process Map Authority)
static esp_err_t circular_buffer_push(struct rfid_tool_context *ctx, const rfid_tool_event_t *event);
static esp_err_t circular_buffer_pop(struct rfid_tool_context *ctx, rfid_tool_event_t *event);
static bool circular_buffer_is_full(struct rfid_tool_context *ctx);
static bool circular_buffer_is_empty(struct rfid_tool_context *ctx);

// =============================================================================
// MCP Tool Interface Implementation
// =============================================================================

const char* rfid_tool_get_id(void)
{
    return RFID_TOOL_ID;
}

const char* rfid_tool_get_version(void)
{
    return RFID_TOOL_VERSION;
}

rfid_tool_config_t rfid_tool_create_default_config(void)
{
    rfid_tool_config_t config = {0};
    
    // Default RC522 configuration (using Kconfig defaults)
    config.rc522_config.spi_host = 1;  // SPI2_HOST
    config.rc522_config.miso_gpio = CONFIG_RFID_RC522_SPI_MISO;
    config.rc522_config.mosi_gpio = CONFIG_RFID_RC522_SPI_MOSI;
    config.rc522_config.sclk_gpio = CONFIG_RFID_RC522_SPI_SCLK;
    config.rc522_config.cs_gpio = CONFIG_RFID_RC522_SPI_CS;
    config.rc522_config.rst_gpio = CONFIG_RFID_RST_GPIO;
    config.rc522_config.clock_speed_hz = 1000000;  // 1MHz
    
    // Default behavior settings
    config.auto_start_scanning = true;
    config.scan_interval_ms = 100;
    config.enable_tag_cache = true;
    
    // Default event publishing
    config.publish_events = true;
    config.event_queue_size = RFID_TOOL_EVENT_QUEUE_SIZE;
    config.event_task_stack_size = RFID_TOOL_TASK_STACK_SIZE;
    
    return config;
}

rfid_tool_handle_t rfid_tool_init(const rfid_tool_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Configuration cannot be NULL");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Initializing MCP-inspired RFID tool v%s", RFID_TOOL_VERSION);
    
    // Allocate tool context
    struct rfid_tool_context *ctx = calloc(1, sizeof(struct rfid_tool_context));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate tool context");
        return NULL;
    }
    
    // Copy configuration
    ctx->config = *config;
    ctx->uptime_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ctx->publish_events = config->publish_events;
    
    // Initialize 5-second debounce logic per process map authority
    ctx->debounce_period_ms = 5000;  // 5 seconds per process maps
    ctx->last_detection_time_us = 0;
    ctx->last_debounced_tag_id[0] = '\0';  // Empty string
    ctx->previous_tag_id[0] = '\0';        // Empty string
    
    // Initialize circular buffer per process map authority (stress test compliance)
    ctx->buffer_write_index = 0;
    ctx->buffer_read_index = 0;
    ctx->buffer_count = 0;
    ctx->buffer_overflow_flag = false;
    
    // Set capabilities
    ctx->capabilities = RFID_CAP_TAG_DETECTION | 
                       RFID_CAP_AUTO_SCAN |
                       RFID_CAP_EVENT_PUBLISH |
                       RFID_CAP_UID_EXTRACTION |
                       RFID_CAP_HEALTH_MONITOR |
                       RFID_CAP_TYPE_DETECTION;
    
    // Initialize synchronization
    ctx->state_mutex = xSemaphoreCreateMutex();
    if (!ctx->state_mutex) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        free(ctx);
        return NULL;
    }
    
    // Initialize event loop (same pattern as legacy rfid_manager)
    esp_event_loop_args_t event_loop_args = {
        .queue_size = config->event_queue_size,
        .task_name = "rfid_tool_event",
        .task_priority = RFID_TOOL_TASK_PRIORITY,
        .task_stack_size = config->event_task_stack_size,
        .task_core_id = tskNO_AFFINITY
    };
    
    esp_err_t ret = esp_event_loop_create(&event_loop_args, &ctx->event_loop);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        vSemaphoreDelete(ctx->state_mutex);
        free(ctx);
        return NULL;
    }
    
#if CONFIG_RFID_MODULE_RC522
    ESP_LOGI(TAG, "Initializing RC522 module");
    
    // Configure RC522 driver (enhanced from legacy)
    rc522_spi_config_t driver_config = {
        .host_id = get_spi_host(config->rc522_config.spi_host),
        .bus_config = &(spi_bus_config_t){
            .miso_io_num = config->rc522_config.miso_gpio,
            .mosi_io_num = config->rc522_config.mosi_gpio,
            .sclk_io_num = config->rc522_config.sclk_gpio,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 0,
        },
        .dev_config = {
            .spics_io_num = config->rc522_config.cs_gpio,
            .clock_speed_hz = config->rc522_config.clock_speed_hz,
            .mode = 0,
            .queue_size = 1,
        },
        .rst_io_num = config->rc522_config.rst_gpio,
    };
    
    // Create RC522 driver
    ESP_LOGI(TAG, "Creating RC522 SPI driver with MISO=%d, MOSI=%d, SCK=%d, SS=%d, RST=%d", 
             config->rc522_config.miso_gpio, config->rc522_config.mosi_gpio, 
             config->rc522_config.sclk_gpio, config->rc522_config.cs_gpio, config->rc522_config.rst_gpio);
    ret = rc522_spi_create(&driver_config, &ctx->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RC522 driver: %s", esp_err_to_name(ret));
        esp_event_loop_delete(ctx->event_loop);
        vSemaphoreDelete(ctx->state_mutex);
        free(ctx);
        return NULL;
    }
    ESP_LOGI(TAG, "RC522 SPI driver created successfully");
    
    // Install RC522 driver
    ESP_LOGI(TAG, "Installing RC522 driver");
    ret = rc522_driver_install(ctx->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install RC522 driver: %s", esp_err_to_name(ret));
        esp_event_loop_delete(ctx->event_loop);
        vSemaphoreDelete(ctx->state_mutex);
        free(ctx);
        return NULL;
    }
    ESP_LOGI(TAG, "RC522 driver installed successfully");
    
    // Configure RC522 scanner
    rc522_config_t scanner_config = {
        .driver = ctx->driver,
    };
    
    // Create RC522 scanner
    ESP_LOGI(TAG, "Creating RC522 scanner");
    ret = rc522_create(&scanner_config, &ctx->scanner);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RC522 scanner: %s", esp_err_to_name(ret));
        esp_event_loop_delete(ctx->event_loop);
        vSemaphoreDelete(ctx->state_mutex);
        free(ctx);
        return NULL;
    }
    ESP_LOGI(TAG, "RC522 scanner created successfully");
    
    // Register RC522 event handler
    ESP_LOGI(TAG, "Registering RC522 event handler");
    ret = rc522_register_events(ctx->scanner, RC522_EVENT_PICC_STATE_CHANGED, 
                               rfid_picc_state_changed_handler, ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register RC522 event handler: %s", esp_err_to_name(ret));
        rc522_destroy(ctx->scanner);
        esp_event_loop_delete(ctx->event_loop);
        vSemaphoreDelete(ctx->state_mutex);
        free(ctx);
        return NULL;
    }
    ESP_LOGI(TAG, "RC522 event handler registered successfully");
    
    ESP_LOGI(TAG, "RC522 initialization complete");
#endif
    
    ctx->is_initialized = true;
    ctx->is_active = true;
    
    // Auto-start scanning if configured
    if (config->auto_start_scanning) {
        rfid_tool_start_scanning(ctx);
    }
    
    ESP_LOGI(TAG, "RFID tool initialized successfully");
    ESP_LOGI(TAG, "  Auto-start: %s, Event publishing: %s", 
             config->auto_start_scanning ? "enabled" : "disabled",
             ctx->publish_events ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Capabilities: 0x%02X", ctx->capabilities);
    
    // Publish tool ready event
    rfid_tool_publish_universal_event(ctx, RFID_TOOL_EVENT_READY, NULL);
    
    return ctx;
}

esp_err_t rfid_tool_deinit(rfid_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    
    ESP_LOGI(TAG, "Deinitializing RFID tool");
    
    // Stop scanning if active
    if (ctx->is_scanning) {
        rfid_tool_stop_scanning(handle);
    }
    
    ctx->is_active = false;
    
#if CONFIG_RFID_MODULE_RC522
    // Destroy RC522 scanner
    if (ctx->scanner) {
        rc522_destroy(ctx->scanner);
    }
    
    // Note: RC522 driver can't be deinitialized with current API
#endif
    
    // Delete event loop
    if (ctx->event_loop) {
        esp_err_t ret = esp_event_loop_delete(ctx->event_loop);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete event loop: %s", esp_err_to_name(ret));
        }
    }
    
    // Cleanup synchronization
    if (ctx->state_mutex) {
        vSemaphoreDelete(ctx->state_mutex);
    }
    
    // Free context
    free(ctx);
    
    ESP_LOGI(TAG, "RFID tool deinitialized");
    return ESP_OK;
}

rfid_tool_capabilities_t rfid_tool_get_capabilities(rfid_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    return ctx->capabilities;
}

esp_err_t rfid_tool_get_status(rfid_tool_handle_t handle, rfid_tool_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    
    if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        status->is_initialized = ctx->is_initialized;
        status->is_active = ctx->is_active;
        status->is_scanning = ctx->is_scanning;
        status->tag_present = ctx->tag_present;
        status->current_tag = ctx->current_tag;
        status->scan_count = ctx->scan_count;
        status->tag_detection_count = ctx->tag_detection_count;
        status->error_count = ctx->error_count;
        status->uptime_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - ctx->uptime_start;
        status->capabilities = ctx->capabilities;
        
        xSemaphoreGive(ctx->state_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Set FS tool handle for event logging (Phase 5.4)
 */
esp_err_t rfid_tool_set_fs_tool_handle(rfid_tool_handle_t handle, void* fs_tool_handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context *)handle;
    
    if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ctx->fs_tool_handle = fs_tool_handle;
        ctx->enable_event_logging = (fs_tool_handle != NULL);
        xSemaphoreGive(ctx->state_mutex);
        
        ESP_LOGI(TAG, "FS tool handle %s for event logging", 
                 fs_tool_handle ? "enabled" : "disabled");
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

// =============================================================================
// RFID Operations Implementation
// =============================================================================

esp_err_t rfid_tool_start_scanning(rfid_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    
    if (!ctx->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Check if already scanning
        if (ctx->is_scanning) {
            ESP_LOGW(TAG, "Already scanning for tags");
            xSemaphoreGive(ctx->state_mutex);
            return ESP_OK;
        }
        
#if CONFIG_RFID_MODULE_RC522
        // Start RC522 scanner
        ESP_LOGI(TAG, "Starting RC522 scanner");
        esp_err_t ret = rc522_start(ctx->scanner);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start RC522 scanner: %s", esp_err_to_name(ret));
            ctx->error_count++;
            xSemaphoreGive(ctx->state_mutex);
            return ret;
        }
        ESP_LOGI(TAG, "RC522 scanner started successfully");
#endif
        
        ctx->is_scanning = true;
        ctx->scan_count++;
        
        xSemaphoreGive(ctx->state_mutex);
        
        ESP_LOGI(TAG, "Started scanning for tags");
        
        // Publish scanning started event
        rfid_tool_publish_universal_event(ctx, RFID_TOOL_EVENT_SCAN_STARTED, NULL);
        
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

esp_err_t rfid_tool_stop_scanning(rfid_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    
    if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Check if scanning
        if (!ctx->is_scanning) {
            ESP_LOGW(TAG, "Not scanning for tags");
            xSemaphoreGive(ctx->state_mutex);
            return ESP_OK;
        }
        
#if CONFIG_RFID_MODULE_RC522
        // Use rc522_pause instead of rc522_stop (same as legacy)
        esp_err_t ret = rc522_pause(ctx->scanner);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to pause RC522 scanner: %s", esp_err_to_name(ret));
            ctx->error_count++;
            xSemaphoreGive(ctx->state_mutex);
            return ret;
        }
#endif
        
        ctx->is_scanning = false;
        ctx->tag_present = false;
        memset(&ctx->current_tag, 0, sizeof(ctx->current_tag));
        
        xSemaphoreGive(ctx->state_mutex);
        
        ESP_LOGI(TAG, "Stopped scanning for tags");
        
        // Publish scanning stopped event
        rfid_tool_publish_universal_event(ctx, RFID_TOOL_EVENT_SCAN_STOPPED, NULL);
        
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

bool rfid_tool_is_tag_present(rfid_tool_handle_t handle)
{
    if (!handle) {
        return false;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    return ctx->tag_present;
}

esp_err_t rfid_tool_get_current_tag(rfid_tool_handle_t handle, rfid_tag_info_t *tag_info)
{
    if (!handle || !tag_info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    
    if (!ctx->tag_present) {
        return ESP_ERR_NOT_FOUND;
    }
    
    if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *tag_info = ctx->current_tag;
        xSemaphoreGive(ctx->state_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

esp_err_t rfid_tool_get_tag_uid_string(rfid_tool_handle_t handle, char* uid_string, size_t buffer_size)
{
    if (!handle || !uid_string || buffer_size < 21) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    
    if (!ctx->tag_present) {
        return ESP_ERR_NOT_FOUND;
    }
    
    return rfid_tool_tag_uid_to_string(&ctx->current_tag, uid_string, buffer_size);
}

esp_err_t rfid_tool_get_device_uid(char* device_uid, size_t buffer_size)
{
    if (!device_uid || buffer_size < 13) {  // 12 hex chars + null terminator
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t chipid[6];
    esp_efuse_mac_get_default(chipid);
    
    // Use all 6 bytes of MAC address for device ID (same as legacy)
    sprintf(device_uid, "%02X%02X%02X%02X%02X%02X", 
           chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5]);
    
    return ESP_OK;
}

// =============================================================================
// Event Handler Interface (Compatible with legacy)
// =============================================================================

// Legacy handler registration removed - Use universal event system via event_system.h:
// subscribe_to_rfid_events(handler, context) for RFID_EVENTS registration

// =============================================================================
// Internal Helper Functions
// =============================================================================

#if CONFIG_RFID_MODULE_RC522
/**
 * @brief Map Kconfig SPI host value to ESP-IDF SPI host device enum
 */
static spi_host_device_t get_spi_host(int config_host) {
#ifdef CONFIG_IDF_TARGET_ESP32C3
    // ESP32-C3 only has SPI2_HOST available
    return SPI2_HOST;
#else
    // For other targets like ESP32
    switch(config_host) {
        case 1: return SPI2_HOST;  // HSPI
        case 2: return SPI3_HOST;  // VSPI
        default: return SPI2_HOST; // Default to SPI2_HOST
    }
#endif
}

/**
 * @brief RC522 PICC state changed event handler (Phase 5.4: Clean state-change detection)
 */
static void rfid_picc_state_changed_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    struct rfid_tool_context *ctx = (struct rfid_tool_context *)arg;
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;
    
    if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for state change");
        return;
    }
    
    // Extract current tag ID (empty string if no tag)
    char current_tag_id[21] = {0};
    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        // Tag present - extract UID
        rfid_tag_info_t temp_tag = {0};
        temp_tag.uid_length = picc->uid.length <= RFID_TOOL_MAX_UID_LEN ? 
                              picc->uid.length : RFID_TOOL_MAX_UID_LEN;
        memcpy(temp_tag.uid, picc->uid.value, temp_tag.uid_length);
        rfid_tool_tag_uid_to_string(&temp_tag, current_tag_id, sizeof(current_tag_id));
        
        // Update current_tag for compatibility (legacy code expects this)
        ctx->current_tag = temp_tag;
        ctx->current_tag.detection_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        ctx->current_tag.boot_timestamp_us = esp_timer_get_time();
        ctx->current_tag.sak = picc->sak;
        update_tag_type(&ctx->current_tag, picc->sak);
        ctx->tag_present = true;
    } else {
        // No tag present - current_tag_id remains empty
        ctx->tag_present = false;
    }
    
    // State change detection: only log when tag_id changes
    if (strcmp(ctx->previous_tag_id, current_tag_id) != 0) {
        uint64_t timestamp_us = esp_timer_get_time();
        
        if (strlen(current_tag_id) > 0) {
            // Tag insertion: current_tag_id has value
            
            // PROCESS MAP AUTHORITY: 5-Second Debounce Logic
            bool should_process_event = false;
            
            // Check if this is a different tag OR if enough time has passed
            if (strcmp(ctx->last_debounced_tag_id, current_tag_id) != 0) {
                // Different tag - always process
                should_process_event = true;
                ESP_LOGI(TAG, "🏷️ Different tag detected: %s", current_tag_id);
            } else {
                // Same tag - check timing
                uint64_t time_since_last_us = timestamp_us - ctx->last_detection_time_us;
                uint64_t debounce_threshold_us = (uint64_t)ctx->debounce_period_ms * 1000;
                
                if (time_since_last_us >= debounce_threshold_us) {
                    should_process_event = true;
                    ESP_LOGI(TAG, "🏷️ Same tag after debounce period: %s (%.1fs elapsed)", 
                             current_tag_id, time_since_last_us / 1000000.0);
                } else {
                    ESP_LOGD(TAG, "🏷️ Duplicate tag within debounce period: %s (%.1fs < 5.0s)", 
                             current_tag_id, time_since_last_us / 1000000.0);
                    
                    // PROCESS MAP AUTHORITY: Emit ignored event for visual feedback
                    ESP_LOGI(TAG, "🏷️ Publishing TAG_IGNORED event for debounce suppression");
                    rfid_tool_publish_universal_event(ctx, RFID_TOOL_EVENT_TAG_IGNORED, current_tag_id);
                }
            }
            
            if (should_process_event) {
                ctx->tag_detection_count++;
                
                // Update debounce state
                snprintf(ctx->last_debounced_tag_id, sizeof(ctx->last_debounced_tag_id), "%s", current_tag_id);
                ctx->last_debounced_tag_id[sizeof(ctx->last_debounced_tag_id) - 1] = '\0';
                ctx->last_detection_time_us = timestamp_us;
                
                // Create event for process map integration
                rfid_tool_event_t rfid_event = {
                    .type = RFID_TOOL_EVENT_TAG_DETECTED,
                    .data.tag_info.tag = ctx->current_tag
                };
                snprintf(rfid_event.data.tag_info.uid_string, sizeof(rfid_event.data.tag_info.uid_string), "%s", current_tag_id);
                
                // PROCESS MAP AUTHORITY: Circular buffer for stress test compliance
                esp_err_t buffer_ret = circular_buffer_push(ctx, &rfid_event);
                if (buffer_ret == ESP_OK) {
                    ESP_LOGI(TAG, "🏷️ Event buffered (count: %d/50)", ctx->buffer_count);
                } else {
                    ESP_LOGW(TAG, "🏷️ Failed to buffer event: %s", esp_err_to_name(buffer_ret));
                }
                
                // Immediate publish for real-time feedback (normal operation)
                ESP_LOGI(TAG, "🏷️ Tag passed debounce check, publishing TAG_DETECTED event");
                rfid_tool_publish_universal_event(ctx, RFID_TOOL_EVENT_TAG_DETECTED, current_tag_id);
                
                // Log hardware state change
                log_hardware_event(ctx, current_tag_id, true, timestamp_us);
            } else {
                ESP_LOGD(TAG, "🏷️ Tag event suppressed by debounce logic");
            }
            
        } else {
            // Tag removal: use previous_tag_id (current is empty)
            
            // Publish tag removal event with previous tag UID for session matching
            ESP_LOGI(TAG, "🏷️ Tag removed, publishing TAG_REMOVED event");
            rfid_tool_publish_universal_event(ctx, RFID_TOOL_EVENT_TAG_REMOVED, ctx->previous_tag_id);
            
            // Log hardware state change (with previous tag_id)
            log_hardware_event(ctx, ctx->previous_tag_id, false, timestamp_us);
        }
        
        ESP_LOGI(TAG, "State change detected: '%s' → '%s'", 
                 ctx->previous_tag_id, current_tag_id);
        
        // Update previous state for next comparison
        snprintf(ctx->previous_tag_id, sizeof(ctx->previous_tag_id), "%s", current_tag_id);
    }
    
    xSemaphoreGive(ctx->state_mutex);
}
#endif

/**
 * @brief Publish RFID event directly to universal event system (CLEAN APPROACH)
 */
static esp_err_t rfid_tool_publish_universal_event(struct rfid_tool_context *ctx, rfid_tool_event_type_t type, const char* tag_uid)
{
    if (!ctx->publish_events) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "📡 Publishing RFID event: %s", rfid_tool_event_to_string(type));
    
    // ✅ CLEAN: Create universal event data directly on stack (no conversion!)
    rfid_event_data_t rfid_data = {
        .detection_time_us = esp_timer_get_time(),
        .internal_millis = esp_timer_get_time() / 1000,
        .is_new_session = true,
        .during_grace_period = false
    };
    
    // Copy tag UID directly (no complex conversion)
    if (tag_uid) {
        snprintf(rfid_data.tag_uid, sizeof(rfid_data.tag_uid), "%s", tag_uid);
        rfid_data.tag_uid[sizeof(rfid_data.tag_uid) - 1] = '\0';
    } else {
        strcpy(rfid_data.tag_uid, "");
    }
    
    // Map event types to universal IDs
    rfid_event_id_t event_id;
    switch (type) {
        case RFID_TOOL_EVENT_TAG_DETECTED:
            event_id = RFID_EVENT_TAG_DETECTED;
            break;
        case RFID_TOOL_EVENT_TAG_REMOVED:
            event_id = RFID_EVENT_TAG_REMOVED;
            break;
        case RFID_TOOL_EVENT_TAG_IGNORED:
            event_id = RFID_EVENT_TAG_IGNORED;
            break;
        case RFID_TOOL_EVENT_SCAN_STARTED:
        case RFID_TOOL_EVENT_READY:
            event_id = RFID_EVENT_READY;
            break;
        case RFID_TOOL_EVENT_ERROR:
            event_id = RFID_EVENT_ERROR;
            break;
        default:
            ESP_LOGW(TAG, "❌ Unknown RFID event type: %d", type);
            return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "✅ Direct post: tag_uid='%s', event_id=%d", rfid_data.tag_uid, event_id);
    
    // Enhanced debugging for event delivery
    ESP_LOGI(TAG, "🔍 DEBUG: About to post RFID_EVENTS event_id=%d, data_size=%d", event_id, sizeof(rfid_data));
    
    // Post directly to universal event system (clean, simple)
    esp_err_t ret = esp_event_post(RFID_EVENTS, event_id, &rfid_data, sizeof(rfid_data), portMAX_DELAY);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Event posted successfully: RFID_EVENTS, event_id=%d", event_id);
    } else {
        ESP_LOGW(TAG, "❌ Event post failed: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Log RFID hardware state change to filesystem via FS tool (Phase 5.4)
 */
static esp_err_t log_hardware_event(struct rfid_tool_context *ctx, const char* tag_id, bool tag_present, uint64_t boot_timestamp_us)
{
    if (!ctx->enable_event_logging || !ctx->fs_tool_handle) {
        return ESP_OK; // Logging disabled or no FS tool available
    }

    // Create JSON log entry with clean state model
    char log_entry[256];
    int ret = snprintf(log_entry, sizeof(log_entry), 
                      "{\"tag_id\":\"%s\",\"tag_present\":%s,\"boot_timestamp_us\":%" PRIu64 "}",
                      tag_id, tag_present ? "true" : "false", boot_timestamp_us);
    
    if (ret >= sizeof(log_entry)) {
        ESP_LOGW(TAG, "Log entry truncated");
        return ESP_ERR_NO_MEM;
    }
    
    // Publish event to FS tool for logging via event system (MCP pattern)
    esp_err_t event_ret = esp_event_post(FS_TOOL_EVENTS, 0, log_entry, strlen(log_entry) + 1, 0);
    
    if (event_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post FS logging event: %s", esp_err_to_name(event_ret));
    } else {
        ESP_LOGI(TAG, "Hardware state change: tag_id='%s' present=%s", tag_id, tag_present ? "true" : "false");
    }
    
    return event_ret;
}

#if CONFIG_RFID_MODULE_RC522
/**
 * @brief Update tag type based on SAK value (Enhanced from legacy)
 */
static void update_tag_type(rfid_tag_info_t *tag_info, uint8_t sak)
{
    if (sak == 0x08) {
        tag_info->type = RFID_TAG_TYPE_MIFARE_1K;
    } else if (sak == 0x18) {
        tag_info->type = RFID_TAG_TYPE_MIFARE_4K;
    } else if (sak == 0x00) {
        tag_info->type = RFID_TAG_TYPE_MIFARE_UL;
    } else {
        tag_info->type = RFID_TAG_TYPE_UNKNOWN;
    }
}
#endif

// =============================================================================
// Utility Functions Implementation
// =============================================================================

const char* rfid_tool_event_to_string(rfid_tool_event_type_t event_type)
{
    switch (event_type) {
        case RFID_TOOL_EVENT_TAG_DETECTED: return "TAG_DETECTED";
        case RFID_TOOL_EVENT_TAG_REMOVED: return "TAG_REMOVED";
        case RFID_TOOL_EVENT_TAG_IGNORED: return "TAG_IGNORED";
        case RFID_TOOL_EVENT_SCAN_STARTED: return "SCAN_STARTED";
        case RFID_TOOL_EVENT_SCAN_STOPPED: return "SCAN_STOPPED";
        case RFID_TOOL_EVENT_ERROR: return "ERROR";
        case RFID_TOOL_EVENT_READY: return "READY";
        default: return "UNKNOWN";
    }
}

const char* rfid_tool_tag_type_to_string(rfid_tag_type_t tag_type)
{
    switch (tag_type) {
        case RFID_TAG_TYPE_MIFARE_1K: return "MIFARE_1K";
        case RFID_TAG_TYPE_MIFARE_4K: return "MIFARE_4K";
        case RFID_TAG_TYPE_MIFARE_UL: return "MIFARE_UL";
        case RFID_TAG_TYPE_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

esp_err_t rfid_tool_tag_uid_to_string(const rfid_tag_info_t* tag_info, char* uid_string, size_t buffer_size)
{
    if (!tag_info || !uid_string || buffer_size < (tag_info->uid_length * 2 + 1)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < tag_info->uid_length; i++) {
        snprintf(uid_string + (i * 2), 3, "%02X", tag_info->uid[i]);
    }
    
    uid_string[tag_info->uid_length * 2] = '\0';
    
    return ESP_OK;
}

// =============================================================================
// Circular Buffer Public Interface (Process Map Authority)
// =============================================================================

esp_err_t rfid_tool_get_buffered_event(rfid_tool_handle_t handle, rfid_tool_event_t *event)
{
    if (!handle || !event) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    
    if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        esp_err_t result = circular_buffer_pop(ctx, event);
        xSemaphoreGive(ctx->state_mutex);
        
        if (result == ESP_OK) {
            ESP_LOGD(TAG, "📊 Event retrieved from buffer: count=%d/50", ctx->buffer_count);
        }
        
        return result;
    }
    
    return ESP_ERR_TIMEOUT;
}

bool rfid_tool_has_buffered_events(rfid_tool_handle_t handle)
{
    if (!handle) {
        return false;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    return !circular_buffer_is_empty(ctx);
}

esp_err_t rfid_tool_get_buffer_status(rfid_tool_handle_t handle, uint8_t *buffer_count, bool *buffer_overflow)
{
    if (!handle || !buffer_count || !buffer_overflow) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    
    if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *buffer_count = ctx->buffer_count;
        *buffer_overflow = ctx->buffer_overflow_flag;
        xSemaphoreGive(ctx->state_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

// =============================================================================
// Tool Registry Implementation (MCP Pattern)
// =============================================================================

const rfid_tool_registry_t* rfid_tool_get_registry_entry(void)
{
    static const rfid_tool_registry_t registry_entry = {
        .tool_id = RFID_TOOL_ID,
        .version = RFID_TOOL_VERSION,
        .description = RFID_TOOL_DESCRIPTION,
        .capabilities = RFID_CAP_TAG_DETECTION | 
                       RFID_CAP_AUTO_SCAN |
                       RFID_CAP_EVENT_PUBLISH |
                       RFID_CAP_UID_EXTRACTION |
                       RFID_CAP_HEALTH_MONITOR |
                       RFID_CAP_TYPE_DETECTION,
        .init_func = rfid_tool_init,
        .deinit_func = rfid_tool_deinit
    };
    
    return &registry_entry;
}

// =============================================================================
// Circular Buffer Implementation (Process Map Authority)
// =============================================================================

/**
 * @brief Push event to circular buffer (stress test compliance)
 * @param ctx Tool context
 * @param event Event to push
 * @return ESP_OK on success, ESP_ERR_NO_MEM if buffer full
 */
static esp_err_t circular_buffer_push(struct rfid_tool_context *ctx, const rfid_tool_event_t *event)
{
    if (!ctx || !event) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (circular_buffer_is_full(ctx)) {
        // Buffer overflow - remove oldest event (per process map stress test)
        ctx->buffer_read_index = (ctx->buffer_read_index + 1) % 50;
        ctx->buffer_count--;
        ctx->buffer_overflow_flag = true;
        ESP_LOGW(TAG, "📊 Circular buffer overflow - oldest event evicted");
    }
    
    // Add new event at write position
    ctx->event_buffer[ctx->buffer_write_index] = *event;
    ctx->buffer_write_index = (ctx->buffer_write_index + 1) % 50;
    ctx->buffer_count++;
    
    ESP_LOGD(TAG, "📊 Event buffered: count=%d/50", ctx->buffer_count);
    return ESP_OK;
}

/**
 * @brief Pop event from circular buffer
 * @param ctx Tool context  
 * @param event Output event
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if buffer empty
 */
static esp_err_t circular_buffer_pop(struct rfid_tool_context *ctx, rfid_tool_event_t *event)
{
    if (!ctx || !event) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (circular_buffer_is_empty(ctx)) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get event from read position
    *event = ctx->event_buffer[ctx->buffer_read_index];
    ctx->buffer_read_index = (ctx->buffer_read_index + 1) % 50;
    ctx->buffer_count--;
    
    ESP_LOGD(TAG, "📊 Event dequeued: count=%d/50", ctx->buffer_count);
    return ESP_OK;
}

/**
 * @brief Check if circular buffer is full
 */
static bool circular_buffer_is_full(struct rfid_tool_context *ctx)
{
    return ctx ? (ctx->buffer_count >= 50) : false;
}

/**
 * @brief Check if circular buffer is empty
 */
static bool circular_buffer_is_empty(struct rfid_tool_context *ctx)
{
    return ctx ? (ctx->buffer_count == 0) : true;
}