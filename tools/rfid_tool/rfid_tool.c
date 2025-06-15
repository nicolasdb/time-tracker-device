/**
 * @file rfid_tool.c
 * @brief MCP-Inspired RFID Tool Implementation
 * 
 * Transformed from rfid_manager to follow MCP tool composition patterns.
 * Uses handle-based architecture with event-driven communication.
 */

#include "rfid_tool.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "RFID_TOOL";

// =============================================================================
// MCP Tool Event System
// =============================================================================

ESP_EVENT_DEFINE_BASE(RFID_TOOL_EVENTS);

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
};

// =============================================================================
// Forward Declarations
// =============================================================================

#if CONFIG_RFID_MODULE_RC522
static void rfid_picc_state_changed_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data);
static spi_host_device_t get_spi_host(int config_host);
static void update_tag_type(rfid_tag_info_t *tag_info, uint8_t sak);
#endif
static esp_err_t publish_rfid_event(struct rfid_tool_context *ctx, rfid_tool_event_type_t type, void* data);

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
    publish_rfid_event(ctx, RFID_TOOL_EVENT_READY, NULL);
    
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
        publish_rfid_event(ctx, RFID_TOOL_EVENT_SCAN_STARTED, NULL);
        
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
        publish_rfid_event(ctx, RFID_TOOL_EVENT_SCAN_STOPPED, NULL);
        
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

esp_err_t rfid_tool_register_event_handler(
    rfid_tool_handle_t handle,
    rfid_tool_event_type_t event_type,
    esp_event_handler_t event_handler,
    void* event_handler_arg)
{
    if (!handle || !event_handler) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    
    return esp_event_handler_register_with(ctx->event_loop, 
                                          RFID_TOOL_EVENTS, 
                                          event_type, 
                                          event_handler, 
                                          event_handler_arg);
}

esp_err_t rfid_tool_unregister_event_handler(
    rfid_tool_handle_t handle,
    rfid_tool_event_type_t event_type,
    esp_event_handler_t event_handler)
{
    if (!handle || !event_handler) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct rfid_tool_context *ctx = (struct rfid_tool_context*)handle;
    
    return esp_event_handler_unregister_with(ctx->event_loop, 
                                           RFID_TOOL_EVENTS, 
                                           event_type, 
                                           event_handler);
}

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
 * @brief RC522 PICC state changed event handler (Enhanced from legacy)
 */
static void rfid_picc_state_changed_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    struct rfid_tool_context *ctx = (struct rfid_tool_context *)arg;
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;
    
    ESP_LOGI(TAG, "RC522 event received: picc state changed from %d to %d", 
             event->old_state, picc->state);
    
    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        // Tag detected
        ESP_LOGI(TAG, "Tag detected");
        rc522_picc_print(picc);
        
        if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Create tag data
            memset(&ctx->current_tag, 0, sizeof(ctx->current_tag));
            
            // Copy the UID from rc522_picc_uid_t structure
            ctx->current_tag.uid_length = picc->uid.length <= RFID_TOOL_MAX_UID_LEN ? 
                                         picc->uid.length : RFID_TOOL_MAX_UID_LEN;
            memcpy(ctx->current_tag.uid, picc->uid.value, ctx->current_tag.uid_length);
            
            // Copy SAK and update tag type
            ctx->current_tag.sak = picc->sak;
            update_tag_type(&ctx->current_tag, picc->sak);
            
            // Set detection time
            ctx->current_tag.detection_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            ctx->tag_present = true;
            ctx->tag_detection_count++;
            
            xSemaphoreGive(ctx->state_mutex);
            
            // Create event data for publishing
            rfid_tool_event_t rfid_event = {
                .type = RFID_TOOL_EVENT_TAG_DETECTED,
                .data.tag_info.tag = ctx->current_tag
            };
            
            // Generate UID string
            rfid_tool_tag_uid_to_string(&ctx->current_tag, 
                                       rfid_event.data.tag_info.uid_string, 
                                       sizeof(rfid_event.data.tag_info.uid_string));
            
            // Publish event for other tools
            publish_rfid_event(ctx, RFID_TOOL_EVENT_TAG_DETECTED, &rfid_event);
            
            // Also post to internal event loop for compatibility
            esp_err_t ret = esp_event_post_to(ctx->event_loop, 
                             RFID_TOOL_EVENTS, 
                             RFID_TOOL_EVENT_TAG_DETECTED, 
                             &rfid_event, 
                             sizeof(rfid_event), 
                             pdMS_TO_TICKS(100));
            
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to post tag detected event: %s", esp_err_to_name(ret));
            }
        }
    }
    else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        // Tag removed
        ESP_LOGI(TAG, "Tag removed");
        
        if (xSemaphoreTake(ctx->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            ctx->tag_present = false;
            memset(&ctx->current_tag, 0, sizeof(ctx->current_tag));
            
            xSemaphoreGive(ctx->state_mutex);
            
            // Create event data
            rfid_tool_event_t rfid_event = {
                .type = RFID_TOOL_EVENT_TAG_REMOVED
            };
            
            // Publish event for other tools
            publish_rfid_event(ctx, RFID_TOOL_EVENT_TAG_REMOVED, &rfid_event);
            
            // Also post to internal event loop for compatibility
            esp_err_t ret = esp_event_post_to(ctx->event_loop, 
                             RFID_TOOL_EVENTS, 
                             RFID_TOOL_EVENT_TAG_REMOVED, 
                             &rfid_event, 
                             sizeof(rfid_event), 
                             pdMS_TO_TICKS(100));
            
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to post tag removed event: %s", esp_err_to_name(ret));
            }
        }
    }
}
#endif

/**
 * @brief Publish RFID event to other tools (MCP pattern)
 */
static esp_err_t publish_rfid_event(struct rfid_tool_context *ctx, rfid_tool_event_type_t type, void* data)
{
    if (!ctx->publish_events) {
        return ESP_OK;
    }
    
    rfid_tool_event_t event = {.type = type};
    if (data) {
        memcpy(&event, data, sizeof(rfid_tool_event_t));
    }
    
    ESP_LOGD(TAG, "Publishing RFID event: %s", rfid_tool_event_to_string(type));
    
    return esp_event_post(RFID_TOOL_EVENTS, type, &event, sizeof(event), 0);
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