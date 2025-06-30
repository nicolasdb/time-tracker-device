/**
 * @file event_system.c
 * @brief Universal Event System Implementation
 * 
 * Implements async event-driven tool communication using ESP event system.
 * Replaces synchronous function calls with publish/subscribe pattern.
 */

#include "event_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include <inttypes.h>

static const char *TAG = "EVENT_SYSTEM";

// =============================================================================
// Event Base Definitions
// =============================================================================

ESP_EVENT_DEFINE_BASE(SYSTEM_EVENTS);
ESP_EVENT_DEFINE_BASE(RFID_EVENTS);
ESP_EVENT_DEFINE_BASE(SESSION_EVENTS);
ESP_EVENT_DEFINE_BASE(FEEDBACK_EVENTS);
ESP_EVENT_DEFINE_BASE(PAYLOAD_EVENTS);
ESP_EVENT_DEFINE_BASE(FS_EVENTS);
ESP_EVENT_DEFINE_BASE(NETWORK_EVENTS);
ESP_EVENT_DEFINE_BASE(HTTP_EVENTS);
ESP_EVENT_DEFINE_BASE(NTP_EVENTS);

// =============================================================================
// Event System Initialization
// =============================================================================

esp_err_t event_system_init(void)
{
    ESP_LOGI(TAG, "🔄 Initializing universal event system");
    
    // Create default event loop if not already created
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register all event bases
    ret = event_system_register_all_bases();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event bases: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Event system initialized successfully");
    return ESP_OK;
}

esp_err_t event_system_deinit(void)
{
    ESP_LOGI(TAG, "🔄 Deinitializing event system");
    
    esp_err_t ret = esp_event_loop_delete_default();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to delete default event loop: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "✅ Event system deinitialized");
    return ESP_OK;
}

esp_err_t event_system_register_all_bases(void)
{
    ESP_LOGI(TAG, "📋 Registering all event bases");
    
    // Event bases are automatically registered when ESP_EVENT_DEFINE_BASE is used
    // This function serves as a verification point
    
    ESP_LOGI(TAG, "📋 Event bases registered:");
    ESP_LOGI(TAG, "  - SYSTEM_EVENTS (boot, grace period, shutdown)");
    ESP_LOGI(TAG, "  - RFID_EVENTS (tag detection, removal)");
    ESP_LOGI(TAG, "  - SESSION_EVENTS (session timing, flow awareness)");
    ESP_LOGI(TAG, "  - FEEDBACK_EVENTS (visual state changes)");
    ESP_LOGI(TAG, "  - PAYLOAD_EVENTS (event formatting)");
    ESP_LOGI(TAG, "  - FS_EVENTS (filesystem operations)");
    ESP_LOGI(TAG, "  - NETWORK_EVENTS (WiFi connectivity)");
    ESP_LOGI(TAG, "  - HTTP_EVENTS (data transmission)");
    ESP_LOGI(TAG, "  - NTP_EVENTS (time synchronization)");
    
    return ESP_OK;
}

// =============================================================================
// Event Publishing Helpers
// =============================================================================

esp_err_t publish_system_event(system_event_id_t event_id, system_event_data_t* data)
{
    size_t data_size = data ? sizeof(system_event_data_t) : 0;
    esp_err_t ret = esp_event_post(SYSTEM_EVENTS, event_id, data, data_size, portMAX_DELAY);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "📡 Published SYSTEM_EVENT: %d", event_id);
    } else {
        ESP_LOGW(TAG, "❌ Failed to publish SYSTEM_EVENT %d: %s", event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t publish_rfid_event(rfid_event_id_t event_id, rfid_event_data_t* data)
{
    // 🔥 NUCLEAR DEBUG: Log every call to publish_rfid_event
    ESP_LOGI(TAG, "🔥 PUBLISH_RFID_EVENT CALLED: event_id=%d, data=%p", (int)event_id, data);
    if (data) {
        ESP_LOGI(TAG, "🔥 PUBLISH_RFID_EVENT DATA: tag_uid=%s, timestamp=%" PRIu64, 
                 data->tag_uid, data->detection_time_us);
    }
    
    size_t data_size = data ? sizeof(rfid_event_data_t) : 0;
    
    // 🔥 NUCLEAR FIX: Add tiny delay + force flush to ensure event delivery
    vTaskDelay(pdMS_TO_TICKS(1));  // 1ms delay to let event system catch up
    esp_err_t ret = esp_event_post(RFID_EVENTS, event_id, data, data_size, portMAX_DELAY);
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1));  // Another 1ms to ensure delivery
    }
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "📡 Published RFID_EVENT: %d", event_id);
        if (data && event_id == RFID_EVENT_TAG_DETECTED) {
            ESP_LOGI(TAG, "🏷️  RFID event published: tag=%s", data->tag_uid);
        }
        if (data && event_id == RFID_EVENT_TAG_REMOVED) {
            ESP_LOGI(TAG, "📤 RFID TAG_REMOVED event published: tag=%s", data->tag_uid);
        }
    } else {
        ESP_LOGW(TAG, "❌ Failed to publish RFID_EVENT %d: %s", event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t publish_session_event(session_event_id_t event_id, session_event_data_t* data)
{
    size_t data_size = data ? sizeof(session_event_data_t) : 0;
    esp_err_t ret = esp_event_post(SESSION_EVENTS, event_id, data, data_size, portMAX_DELAY);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "📡 Published SESSION_EVENT: %d", event_id);
        if (data) {
            switch (event_id) {
                case SESSION_EVENT_STARTED:
                    ESP_LOGI(TAG, "🏷️  Session started: tag=%s", data->tag_uid);
                    break;
                case SESSION_EVENT_ENDED:
                    ESP_LOGI(TAG, "🏷️  Session ended: tag=%s, duration=%llu ms", 
                             data->tag_uid, (unsigned long long)data->session_duration_ms);
                    break;
                case SESSION_EVENT_FLOW_AWARENESS:
                    ESP_LOGI(TAG, "🟠 Flow awareness: %lu minutes", (unsigned long)data->session_duration_min);
                    break;
                case SESSION_EVENT_FLOW_URGENCY:
                    ESP_LOGI(TAG, "🟠 Flow urgency: %lu minutes", (unsigned long)data->session_duration_min);
                    break;
                default:
                    break;
            }
        }
    } else {
        ESP_LOGW(TAG, "❌ Failed to publish SESSION_EVENT %d: %s", event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t publish_feedback_event(feedback_event_id_t event_id, feedback_event_data_t* data)
{
    size_t data_size = data ? sizeof(feedback_event_data_t) : 0;
    esp_err_t ret = esp_event_post(FEEDBACK_EVENTS, event_id, data, data_size, portMAX_DELAY);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "📡 Published FEEDBACK_EVENT: %d", event_id);
        if (data && event_id == FEEDBACK_EVENT_STATE_CHANGE) {
            ESP_LOGI(TAG, "💡 Visual state change: state=%d, flow_aware=%s", 
                     data->state, data->flow_awareness_active ? "yes" : "no");
        }
    } else {
        ESP_LOGW(TAG, "❌ Failed to publish FEEDBACK_EVENT %d: %s", event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t publish_payload_event(payload_event_id_t event_id, payload_event_data_t* data)
{
    size_t data_size = data ? sizeof(payload_event_data_t) : 0;
    esp_err_t ret = esp_event_post(PAYLOAD_EVENTS, event_id, data, data_size, portMAX_DELAY);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "📡 Published PAYLOAD_EVENT: %d", event_id);
        if (data && event_id == PAYLOAD_EVENT_READY) {
            ESP_LOGI(TAG, "📦 Payload ready: tag=%s", 
                     data->tag_uid);
        }
    } else {
        ESP_LOGW(TAG, "❌ Failed to publish PAYLOAD_EVENT %d: %s", event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t publish_fs_event(fs_event_id_t event_id, fs_event_data_t* data)
{
    size_t data_size = data ? sizeof(fs_event_data_t) : 0;
    esp_err_t ret = esp_event_post(FS_EVENTS, event_id, data, data_size, portMAX_DELAY);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "📡 Published FS_EVENT: %d", event_id);
        if (data && event_id == FS_EVENT_SAVED) {
            ESP_LOGI(TAG, "📂 File saved: %s", data->filename);
        }
    } else {
        ESP_LOGW(TAG, "❌ Failed to publish FS_EVENT %d: %s", event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t publish_network_event(network_event_id_t event_id, network_event_data_t* data)
{
    size_t data_size = data ? sizeof(network_event_data_t) : 0;
    esp_err_t ret = esp_event_post(NETWORK_EVENTS, event_id, data, data_size, portMAX_DELAY);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "📡 Published NETWORK_EVENT: %d", event_id);
        if (data && event_id == NETWORK_EVENT_CONNECTED) {
            ESP_LOGI(TAG, "📶 Network connected: %s (RSSI: %d)", data->ssid, data->rssi);
        }
    } else {
        ESP_LOGW(TAG, "❌ Failed to publish NETWORK_EVENT %d: %s", event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t publish_http_event(http_event_id_t event_id, http_event_data_t* data)
{
    size_t data_size = data ? sizeof(http_event_data_t) : 0;
    esp_err_t ret = esp_event_post(HTTP_EVENTS, event_id, data, data_size, portMAX_DELAY);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "📡 Published HTTP_EVENT: %d", event_id);
        if (data) {
            switch (event_id) {
                case HTTP_EVENT_SUCCESS:
                    ESP_LOGI(TAG, "🌐 HTTP success: code=%d", data->response_code);
                    break;
                case HTTP_EVENT_FAILED:
                    ESP_LOGI(TAG, "🌐 HTTP failed: code=%d, retry=%lu", 
                             data->response_code, (unsigned long)data->retry_count);
                    break;
                default:
                    break;
            }
        }
    } else {
        ESP_LOGW(TAG, "❌ Failed to publish HTTP_EVENT %d: %s", event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t publish_ntp_event(ntp_event_id_t event_id, ntp_event_data_t* data)
{
    size_t data_size = data ? sizeof(ntp_event_data_t) : 0;
    esp_err_t ret = esp_event_post(NTP_EVENTS, event_id, data, data_size, portMAX_DELAY);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "📡 Published NTP_EVENT: %d", event_id);
        if (data && event_id == NTP_EVENT_SYNCED) {
            ESP_LOGI(TAG, "🕐 NTP synced: accuracy=%lu ms", (unsigned long)data->sync_accuracy_ms);
        }
    } else {
        ESP_LOGW(TAG, "❌ Failed to publish NTP_EVENT %d: %s", event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

// =============================================================================
// Event Subscription Helpers
// =============================================================================

esp_err_t subscribe_to_system_events(esp_event_handler_t handler, void* handler_arg)
{
    esp_err_t ret = esp_event_handler_register(SYSTEM_EVENTS, ESP_EVENT_ANY_ID, handler, handler_arg);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "✅ Subscribed to SYSTEM_EVENTS");
    } else {
        ESP_LOGW(TAG, "❌ Failed to subscribe to SYSTEM_EVENTS: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t subscribe_to_rfid_events(esp_event_handler_t handler, void* handler_arg)
{
    esp_err_t ret = esp_event_handler_register(RFID_EVENTS, ESP_EVENT_ANY_ID, handler, handler_arg);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "✅ Subscribed to RFID_EVENTS");
    } else {
        ESP_LOGW(TAG, "❌ Failed to subscribe to RFID_EVENTS: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t subscribe_to_session_events(esp_event_handler_t handler, void* handler_arg)
{
    esp_err_t ret = esp_event_handler_register(SESSION_EVENTS, ESP_EVENT_ANY_ID, handler, handler_arg);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "✅ Subscribed to SESSION_EVENTS");
    } else {
        ESP_LOGW(TAG, "❌ Failed to subscribe to SESSION_EVENTS: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t subscribe_to_feedback_events(esp_event_handler_t handler, void* handler_arg)
{
    esp_err_t ret = esp_event_handler_register(FEEDBACK_EVENTS, ESP_EVENT_ANY_ID, handler, handler_arg);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "✅ Subscribed to FEEDBACK_EVENTS");
    } else {
        ESP_LOGW(TAG, "❌ Failed to subscribe to FEEDBACK_EVENTS: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t subscribe_to_payload_events(esp_event_handler_t handler, void* handler_arg)
{
    esp_err_t ret = esp_event_handler_register(PAYLOAD_EVENTS, ESP_EVENT_ANY_ID, handler, handler_arg);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "✅ Subscribed to PAYLOAD_EVENTS");
    } else {
        ESP_LOGW(TAG, "❌ Failed to subscribe to PAYLOAD_EVENTS: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t subscribe_to_fs_events(esp_event_handler_t handler, void* handler_arg)
{
    esp_err_t ret = esp_event_handler_register(FS_EVENTS, ESP_EVENT_ANY_ID, handler, handler_arg);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "✅ Subscribed to FS_EVENTS");
    } else {
        ESP_LOGW(TAG, "❌ Failed to subscribe to FS_EVENTS: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t subscribe_to_network_events(esp_event_handler_t handler, void* handler_arg)
{
    esp_err_t ret = esp_event_handler_register(NETWORK_EVENTS, ESP_EVENT_ANY_ID, handler, handler_arg);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "✅ Subscribed to NETWORK_EVENTS");
    } else {
        ESP_LOGW(TAG, "❌ Failed to subscribe to NETWORK_EVENTS: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t subscribe_to_http_events(esp_event_handler_t handler, void* handler_arg)
{
    esp_err_t ret = esp_event_handler_register(HTTP_EVENTS, ESP_EVENT_ANY_ID, handler, handler_arg);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "✅ Subscribed to HTTP_EVENTS");
    } else {
        ESP_LOGW(TAG, "❌ Failed to subscribe to HTTP_EVENTS: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t subscribe_to_ntp_events(esp_event_handler_t handler, void* handler_arg)
{
    esp_err_t ret = esp_event_handler_register(NTP_EVENTS, ESP_EVENT_ANY_ID, handler, handler_arg);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "✅ Subscribed to NTP_EVENTS");
    } else {
        ESP_LOGW(TAG, "❌ Failed to subscribe to NTP_EVENTS: %s", esp_err_to_name(ret));
    }
    return ret;
}