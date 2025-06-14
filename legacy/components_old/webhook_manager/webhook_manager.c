#include "webhook_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_littlefs.h"
#include "cJSON.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "wifi_manager.h"
#include "sdkconfig.h"

static const char *TAG = "webhook_manager";

// Configurations from Kconfig
// #define MAX_LOG_ENTRIES 50
// #define DEFAULT_MAX_RETRIES 3
// #define DEFAULT_RETRY_DELAY_MS 5000

struct webhook_manager {
    char config_path[128];
    char log_path[128];
    char webhook_url[256];
    char device_id[33];        // Store device ID for all events
    int max_retries;
    int retry_delay_ms;
    bool is_configured;
    bool is_connected;
    webhook_event_t events[CONFIG_WEBHOOK_MANAGER_MAX_LOG_ENTRIES];
    int event_count;
    uint32_t last_retry_time;  // Changed from int64_t to uint32_t
    TaskHandle_t task_handle;  // For storing the task handle
};

// Forward declarations
static esp_err_t webhook_manager_load_log(webhook_manager_handle_t handle);
static esp_err_t webhook_manager_save_log(webhook_manager_handle_t handle);
static esp_err_t webhook_manager_send_http_request(webhook_manager_handle_t handle, const webhook_event_t *event);
static char* webhook_manager_create_json_payload(const webhook_event_t *event);
static void webhook_manager_format_iso_time(char* buf, size_t buf_size, time_t time_value);
static esp_err_t webhook_manager_create_default_log(webhook_manager_handle_t handle);

// HTTP event handler
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP Client Error");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP Client Connected");
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP Client Finished");
            break;
        default:
            break;
    }
    return ESP_OK;
}

webhook_manager_handle_t webhook_manager_init(const char *config_path, const char *log_path) {
    ESP_LOGI(TAG, "Initializing webhook manager");
    
    webhook_manager_handle_t handle = calloc(1, sizeof(struct webhook_manager));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for webhook manager");
        return NULL;
    }
    
    // Initialize with Kconfig values
    strncpy(handle->config_path, config_path, sizeof(handle->config_path) - 1);
    strncpy(handle->log_path, log_path, sizeof(handle->log_path) - 1);
    handle->device_id[0] = '\0';  // Start with empty device ID
    handle->max_retries = CONFIG_WEBHOOK_MANAGER_MAX_RETRIES;
    handle->retry_delay_ms = CONFIG_WEBHOOK_MANAGER_RETRY_DELAY_MS;
    strncpy(handle->webhook_url, CONFIG_WEBHOOK_MANAGER_URL, sizeof(handle->webhook_url) - 1);
    handle->is_configured = true;
    handle->is_connected = false;
    handle->event_count = 0;
    handle->last_retry_time = 0;
    handle->task_handle = NULL;
    
    ESP_LOGI(TAG, "Webhook manager initialized with URL: %s", handle->webhook_url);
    ESP_LOGI(TAG, "Max retries: %d, Retry delay: %d ms", handle->max_retries, handle->retry_delay_ms);
    
    return handle;
}

esp_err_t webhook_manager_deinit(webhook_manager_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Save log before exiting
    webhook_manager_save_log(handle);
    
    // Clean up task if it was created through this handle
    if (handle->task_handle != NULL) {
        vTaskDelete(handle->task_handle);
        handle->task_handle = NULL;
    }
    
    free(handle);
    return ESP_OK;
}

static esp_err_t webhook_manager_create_default_log(webhook_manager_handle_t handle) {
    ESP_LOGI(TAG, "Creating default log file at %s", handle->log_path);
    
    // Create empty log file with events array
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }
    
    cJSON *events = cJSON_CreateArray();
    if (events == NULL) {
        ESP_LOGE(TAG, "Failed to create events array");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    cJSON_AddItemToObject(root, "events", events);
    
    // Convert to string
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON to string");
        return ESP_FAIL;
    }
    
    // Write to file
    FILE *f = fopen(handle->log_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open log file for writing: %s", handle->log_path);
        free(json_str);
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", json_str);
    fclose(f);
    free(json_str);
    
    ESP_LOGI(TAG, "Default log file created successfully");
    return ESP_OK;
}

static esp_err_t webhook_manager_load_log(webhook_manager_handle_t handle) {
    FILE *f = fopen(handle->log_path, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "Event log file not found: %s", handle->log_path);
        return webhook_manager_create_default_log(handle);
    }
    
    // Read file into buffer
    char buffer[4096] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (bytes_read == 0) {
        ESP_LOGW(TAG, "Empty log file");
        return ESP_OK;
    }
    
    // Parse JSON
    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse event log JSON");
        return ESP_FAIL;
    }
    
    // Extract events array
    cJSON *events = cJSON_GetObjectItem(root, "events");
    if (!cJSON_IsArray(events)) {
        ESP_LOGE(TAG, "Events is not an array in log file");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // Process each event
    int num_events = cJSON_GetArraySize(events);
    handle->event_count = 0;
    
    for (int i = 0; i < num_events && handle->event_count < CONFIG_WEBHOOK_MANAGER_MAX_LOG_ENTRIES; i++) {
        cJSON *event = cJSON_GetArrayItem(events, i);
        
        cJSON *event_type = cJSON_GetObjectItem(event, "event_type");
        cJSON *tag_uid = cJSON_GetObjectItem(event, "tag_uid");
        cJSON *device_id = cJSON_GetObjectItem(event, "device_id");
        cJSON *timestamp = cJSON_GetObjectItem(event, "timestamp");
        cJSON *sent = cJSON_GetObjectItem(event, "sent");
        cJSON *attempts = cJSON_GetObjectItem(event, "attempts");
        cJSON *tag_type = cJSON_GetObjectItem(event, "tag_type");
        
        if (cJSON_IsString(event_type) && cJSON_IsString(tag_uid) && 
            cJSON_IsString(device_id) && cJSON_IsNumber(timestamp)) {
            
            webhook_event_t *evt = &handle->events[handle->event_count];
            
            if (strcmp(event_type->valuestring, "tag_placed") == 0 || 
                strcmp(event_type->valuestring, "tag_insert") == 0) {
                evt->event_type = WEBHOOK_EVENT_TAG_PLACED;
            } else if (strcmp(event_type->valuestring, "tag_removed") == 0) {
                evt->event_type = WEBHOOK_EVENT_TAG_REMOVED;
            } else {
                continue; // Skip unknown event type
            }
            
            strncpy(evt->tag_uid, tag_uid->valuestring, sizeof(evt->tag_uid) - 1);
            strncpy(evt->device_id, device_id->valuestring, sizeof(evt->device_id) - 1);
            evt->timestamp = (uint32_t)timestamp->valueint;
            
            evt->sent = cJSON_IsTrue(sent);
            evt->attempts = cJSON_IsNumber(attempts) ? attempts->valueint : 0;
            
            if (cJSON_IsString(tag_type) && tag_type->valuestring != NULL) {
                strncpy(evt->tag_type, tag_type->valuestring, sizeof(evt->tag_type) - 1);
            } else {
                evt->tag_type[0] = '\0';
            }
            
            handle->event_count++;
        }
    }
    
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Loaded %d events from log file", handle->event_count);
    return ESP_OK;
}

static esp_err_t webhook_manager_save_log(webhook_manager_handle_t handle) {
    // Create JSON object
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }
    
    cJSON *events = cJSON_CreateArray();
    if (events == NULL) {
        ESP_LOGE(TAG, "Failed to create events array");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    cJSON_AddItemToObject(root, "events", events);
    
    // Add events to the array
    for (int i = 0; i < handle->event_count; i++) {
        webhook_event_t *evt = &handle->events[i];
        
        cJSON *event = cJSON_CreateObject();
        if (event == NULL) {
            ESP_LOGE(TAG, "Failed to create event object");
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        
        cJSON_AddStringToObject(event, "event_type", 
                              evt->event_type == WEBHOOK_EVENT_TAG_PLACED ? "tag_insert" : "tag_removed");
        cJSON_AddStringToObject(event, "tag_uid", evt->tag_uid);
        cJSON_AddStringToObject(event, "device_id", evt->device_id);
        cJSON_AddNumberToObject(event, "timestamp", evt->timestamp);
        cJSON_AddBoolToObject(event, "sent", evt->sent);
        cJSON_AddNumberToObject(event, "attempts", evt->attempts);
        
        if (strlen(evt->tag_type) > 0) {
            cJSON_AddStringToObject(event, "tag_type", evt->tag_type);
        }
        
        cJSON_AddItemToArray(events, event);
    }
    
    // Convert to string
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON to string");
        return ESP_FAIL;
    }
    
    // Write to file
    FILE *f = fopen(handle->log_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open log file for writing");
        free(json_str);
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", json_str);
    fclose(f);
    free(json_str);
    
    ESP_LOGI(TAG, "Saved %d events to log file", handle->event_count);
    return ESP_OK;
}

esp_err_t webhook_manager_send_event(webhook_manager_handle_t handle, 
                                   webhook_event_type_t event_type,
                                   const char *tag_uid,
                                   const char *tag_type) {
    if (handle == NULL || tag_uid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // If log is full, shift everything to remove oldest
    if (handle->event_count >= CONFIG_WEBHOOK_MANAGER_MAX_LOG_ENTRIES) {
        ESP_LOGW(TAG, "Event log full, dropping oldest event");
        for (int i = 0; i < CONFIG_WEBHOOK_MANAGER_MAX_LOG_ENTRIES - 1; i++) {
            handle->events[i] = handle->events[i + 1];
        }
        handle->event_count--;
    }
    
    // Create new event
    webhook_event_t *evt = &handle->events[handle->event_count];
    evt->event_type = event_type;
    strncpy(evt->tag_uid, tag_uid, sizeof(evt->tag_uid) - 1);
    
    // Use the stored device ID if available, otherwise use a placeholder
    if (strlen(handle->device_id) > 0) {
        strncpy(evt->device_id, handle->device_id, sizeof(evt->device_id) - 1);
    } else {
        strncpy(evt->device_id, "UNSET_DEVICE_ID", sizeof(evt->device_id) - 1);
        ESP_LOGW(TAG, "Device ID not set! Using placeholder. Call webhook_manager_set_device_id()");
    }
    
    // Set timestamp
    time_t now;
    time(&now);
    evt->timestamp = (uint32_t)now;
    
    // Set tag type if provided
    if (tag_type != NULL) {
        strncpy(evt->tag_type, tag_type, sizeof(evt->tag_type) - 1);
    } else {
        evt->tag_type[0] = '\0';
    }
    
    evt->sent = false;
    evt->attempts = 0;
    
    handle->event_count++;
    
    // Try to send immediately if WiFi is connected
    if (wifi_manager_is_connected()) {
        ESP_LOGI(TAG, "WiFi connected, sending event immediately");
        if (webhook_manager_send_http_request(handle, evt) == ESP_OK) {
            evt->sent = true;
            ESP_LOGI(TAG, "Event sent successfully");
        } else {
            ESP_LOGW(TAG, "Failed to send event, will retry later");
            evt->attempts++;
        }
    } else {
        ESP_LOGW(TAG, "WiFi not connected, event queued for later");
    }
    
    // Save log after adding new event
    webhook_manager_save_log(handle);
    
    return ESP_OK;
}

esp_err_t webhook_manager_process_pending(webhook_manager_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if WiFi is connected
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected, skipping processing of pending events");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    // Check retry delay
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - handle->last_retry_time < handle->retry_delay_ms) {
        return ESP_OK; // Too soon to retry
    }
    
    handle->last_retry_time = now;
    
    int sent_count = 0;
    int pending_count = 0;
    
    for (int i = 0; i < handle->event_count; i++) {
        webhook_event_t *evt = &handle->events[i];
        
        if (!evt->sent && evt->attempts < handle->max_retries) {
            pending_count++;
            
            if (webhook_manager_send_http_request(handle, evt) == ESP_OK) {
                evt->sent = true;
                sent_count++;
                ESP_LOGI(TAG, "Successfully sent pending event (%s)", 
                        evt->event_type == WEBHOOK_EVENT_TAG_PLACED ? "tag_insert" : "tag_removed");
            } else {
                evt->attempts++;
                ESP_LOGW(TAG, "Failed to send pending event, attempts: %d/%d", 
                        evt->attempts, handle->max_retries);
            }
        }
    }
    
    if (pending_count > 0) {
        ESP_LOGI(TAG, "Processed pending events: %d/%d sent successfully", sent_count, pending_count);
        
        // Save log after processing
        webhook_manager_save_log(handle);
    }
    
    return ESP_OK;
}

esp_err_t webhook_manager_get_pending_count(webhook_manager_handle_t handle, int *pending_count) {
    if (handle == NULL || pending_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *pending_count = 0;
    
    for (int i = 0; i < handle->event_count; i++) {
        webhook_event_t *evt = &handle->events[i];
        if (!evt->sent && evt->attempts < handle->max_retries) {
            (*pending_count)++;
        }
    }
    
    return ESP_OK;
}

esp_err_t webhook_manager_get_status(webhook_manager_handle_t handle, 
                                   bool *is_configured, 
                                   bool *is_connected) {
    if (handle == NULL || is_configured == NULL || is_connected == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *is_configured = handle->is_configured;
    *is_connected = handle->is_connected;
    
    return ESP_OK;
}

static esp_err_t webhook_manager_send_http_request(webhook_manager_handle_t handle, const webhook_event_t *event) {
    if (!handle->is_configured) {
        ESP_LOGE(TAG, "Webhook not configured");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = ESP_FAIL;
    esp_http_client_handle_t client = NULL;
    char *json_payload = NULL;
    
    // Create JSON payload
    json_payload = webhook_manager_create_json_payload(event);
    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON payload");
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "Sending webhook request to %s", handle->webhook_url);
    ESP_LOGI(TAG, "Payload: %s", json_payload);
    
    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = handle->webhook_url,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };
    
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        goto cleanup;
    }
    
    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    // Set post field
    err = esp_http_client_set_post_field(client, json_payload, strlen(json_payload));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set post field: %s", esp_err_to_name(err));
        goto cleanup;
    }
    
    // Perform request
    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    
    // Check status code
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);
    
    if (status_code >= 200 && status_code < 300) {
        handle->is_connected = true;
        err = ESP_OK;
    } else if (status_code == 429) {
        // Too Many Requests - implement exponential backoff
        ESP_LOGW(TAG, "Rate limited by server (429), implementing backoff");
        // Double the retry delay temporarily for this session
        handle->retry_delay_ms *= 2;
        err = ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "HTTP request failed with status code %d", status_code);
        err = ESP_FAIL;
    }
    
cleanup:
    if (json_payload != NULL) {
        free(json_payload);
    }
    
    if (client != NULL) {
        esp_http_client_cleanup(client);
    }
    
    return err;
}

static char* webhook_manager_create_json_payload(const webhook_event_t *event) {
    // Check for null event
    if (event == NULL) {
        ESP_LOGE(TAG, "Null event passed to create_json_payload");
        return NULL;
    }
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create root JSON object");
        return NULL;
    }
    
    // Format event type and determine tag_present value
    const char *event_type_str = "unknown";
    bool tag_present = false;

    switch (event->event_type) {
        case WEBHOOK_EVENT_TAG_PLACED:
            event_type_str = "tag_insert";  // Changed from "tag_placed" to "tag_insert" to match server expectations
            tag_present = true; // Tag is present on tag_placed event
            break;
        case WEBHOOK_EVENT_TAG_REMOVED:
            event_type_str = "tag_removed";  // This one already matches server expectations
            tag_present = false; // Tag is not present on tag_removed event
            break;
        default:
            ESP_LOGW(TAG, "Unknown event type: %d", event->event_type);
            break;
    }
    
    // Format timestamp
    char timestamp_str[32] = {0};
    webhook_manager_format_iso_time(timestamp_str, sizeof(timestamp_str), (time_t)event->timestamp);
    
    // Limit string lengths to prevent buffer overflows
    char tag_uid[33] = {0};
    char device_id[33] = {0};
    char tag_type[17] = {0};
    
    // Copy with length limits
    strncpy(tag_uid, event->tag_uid, sizeof(tag_uid) - 1);
    strncpy(device_id, event->device_id, sizeof(device_id) - 1);
    
    if (strlen(event->tag_type) > 0) {
        strncpy(tag_type, event->tag_type, sizeof(tag_type) - 1);
    }
    
    // Create the RFID poll result object
    cJSON *rfid_poll_result = cJSON_CreateObject();
    if (rfid_poll_result == NULL) {
        ESP_LOGE(TAG, "Failed to create RFID poll result JSON object");
        cJSON_Delete(root);
        return NULL;
    }
    
    // Add fields to the rfid_poll_result object
    bool success = true;
    
    // Required fields
    if (cJSON_AddStringToObject(rfid_poll_result, "timestamp", timestamp_str) == NULL) {
        success = false;
    }
    
    if (success && cJSON_AddBoolToObject(rfid_poll_result, "tag_present", tag_present) == NULL) {
        success = false;
    }
    
    if (success && cJSON_AddStringToObject(rfid_poll_result, "tag_id", tag_uid) == NULL) {
        success = false;
    }
    
    if (success && cJSON_AddStringToObject(rfid_poll_result, "device_id", device_id) == NULL) {
        success = false;
    }
    
    if (success && cJSON_AddStringToObject(rfid_poll_result, "event_type", event_type_str) == NULL) {
        success = false;
    }
    
    // Optional fields
    if (success && strlen(tag_type) > 0) {
        if (cJSON_AddStringToObject(rfid_poll_result, "tag_type", tag_type) == NULL) {
            success = false;
        }
    }
    
    // Add WiFi status (if available in future)
    // if (success) {
    //     cJSON_AddStringToObject(rfid_poll_result, "wifi_status", "connected");
    // }
    
    // Add time status (if available in future)
    // if (success) {
    //     cJSON_AddStringToObject(rfid_poll_result, "time_status", "synchronized");
    // }
    
    // Add the rfid_poll_result object to the root
    if (success) {
        cJSON_AddItemToObject(root, "rfid_poll_result", rfid_poll_result);
    } else {
        cJSON_Delete(rfid_poll_result);
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to build JSON payload");
        return NULL;
    }
    
    // Add firmware version and hardware info as extra fields in the root
    if (success && cJSON_AddStringToObject(root, "firmware_version", "v1.0.0") == NULL) {
        success = false;
    }
    
    if (success && cJSON_AddStringToObject(root, "hardware", "ESP32-C3") == NULL) {
        success = false;
    }
    
    // Convert to string
    char *json_str = NULL;
    
    if (success) {
        json_str = cJSON_Print(root);
        if (json_str == NULL) {
            ESP_LOGE(TAG, "Failed to print JSON to string");
        }
    }
    
    // Clean up
    cJSON_Delete(root); // This will also delete rfid_poll_result
    
    // Check if the JSON string is too large
    if (json_str && strlen(json_str) > 1024) {
        ESP_LOGW(TAG, "JSON payload is very large (%d bytes)", (int)strlen(json_str));
    }
    
    return json_str;
}

esp_err_t webhook_manager_set_task_handle(webhook_manager_handle_t handle, TaskHandle_t task_handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handle->task_handle = task_handle;
    return ESP_OK;
}

static void webhook_manager_format_iso_time(char* buf, size_t buf_size, time_t time_value) {
    struct tm timeinfo;
    localtime_r(&time_value, &timeinfo);
    
    // Format: YYYY-MM-DDTHH:MM:SS
    strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%S", &timeinfo);
}

esp_err_t webhook_manager_load_configuration(webhook_manager_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Configuration is handled by Kconfig, so this is a no-op
    ESP_LOGI(TAG, "Using hardcoded webhook settings - URL: %s", CONFIG_WEBHOOK_MANAGER_URL);
    return ESP_OK;
}

esp_err_t webhook_manager_load_log_file(webhook_manager_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Loading webhook event log");
    return webhook_manager_load_log(handle);
}

esp_err_t webhook_manager_check_connectivity(webhook_manager_handle_t handle) {
    if (handle == NULL || !handle->is_configured) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Configure HTTP client for GET request instead of HEAD
    esp_http_client_config_t config = {
        .url = handle->webhook_url,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_GET,  // Using GET instead of HEAD since server doesn't support HEAD
        .timeout_ms = 3000,         // Short timeout for fast checking
    };
    
    esp_err_t err = ESP_FAIL;
    esp_http_client_handle_t client = NULL;
    
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for connectivity check");
        return ESP_FAIL;
    }
    
    // Perform request
    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Webhook server connectivity check failed: %s", esp_err_to_name(err));
        handle->is_connected = false;
    } else {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code >= 200 && status_code < 400) {  // Any 2xx or 3xx is considered success
            ESP_LOGI(TAG, "Webhook server is reachable, status code: %d", status_code);
            handle->is_connected = true;
        } else {
            ESP_LOGW(TAG, "Webhook server returned error status: %d", status_code);
            handle->is_connected = false;
        }
    }
    
    // Clean up
    esp_http_client_cleanup(client);
    
    return handle->is_connected ? ESP_OK : ESP_FAIL;
}
esp_err_t webhook_manager_set_device_id(webhook_manager_handle_t handle, const char *device_id) {
    if (handle == NULL || device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Setting device ID to: %s", device_id);
    strncpy(handle->device_id, device_id, sizeof(handle->device_id) - 1);
    
    return ESP_OK;
}
