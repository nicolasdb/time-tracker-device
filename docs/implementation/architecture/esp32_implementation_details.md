# ESP32 Implementation Details & Dashboard

## 1. 🎛️ Debug Dashboard Tool

### **debug_tool Architecture**
```c
typedef struct {
    char ascii_art[256];     // Current art based on system state
    char status_line[64];    // Dynamic status message
    tool_status_t tools[8];  // All tool states
    system_metrics_t metrics; // Performance data
    uint32_t last_update;
} dashboard_state_t;

// API for main.c
void debug_update_dashboard(void);
void debug_print_dashboard(void);
char* debug_get_json_state(void);  // For web API or logging
```

### **Themed Dashboard Example**
```c
// Friendly Robot Theme with dynamic expressions
void debug_render_robot_theme(dashboard_state_t* dash) {
    // Dynamic robot face based on error count
    char* robot_face = (dash->error_count == 0) ? "(◕‿◕)っ" : 
                      (dash->error_count < 3) ? "(•ᴗ•)っ" : "(ಠ_ಠ)っ";
    
    snprintf(dash->ascii_art, sizeof(dash->ascii_art),
        "╭───────────────────────╮\n"
        "│  ╔════════════════╗   │\n"
        "│  ║   %s▄︻▇      ║   │\n"  // Dynamic face
        "│  ╚════════════════╝   │\n"
        "│  RFID: %s %s         │\n"  // Status emoji + last tag
        "│  WiFi: %s %s (%dms)   │\n"  // Signal + IP + latency
        "│  Temp: %.1f°C %s      │\n"  // Temp + emoji
        "│  Uptime: %s           │\n"
        "╰───────────────────────╯",
        robot_face,
        get_rfid_emoji(), dash->last_tag,
        get_wifi_emoji(), dash->ip_addr, dash->ping_ms,
        dash->temperature, get_temp_emoji(dash->temperature),
        format_uptime(dash->uptime_ms)
    );
}
```

### **JSON State API**
```json
{
  "timestamp": "2025-01-15T10:30:00Z",
  "system": {
    "state": "operational",
    "uptime_ms": 3600000,
    "free_heap": 45632,
    "temperature": 23.5
  },
  "tools": {
    "rfid": { "state": "scanning", "last_tag": "3A:FD:90:2E", "scan_count": 147 },
    "network": { "state": "connected", "ip": "192.168.1.105", "rssi": -45 },
    "fs": { "state": "idle", "free_space": 87654, "write_count": 23 },
    "http": { "state": "idle", "success_rate": 98.5, "retry_count": 2 }
  },
  "metrics": {
    "events_today": 15,
    "last_sync": "2025-01-15T10:28:45Z",
    "error_count": 0
  }
}
```

---

## 2. 🔄 Error Recovery Best Practices

### **Tool Recovery Strategy**
```c
typedef enum {
    RECOVERY_NONE,        // Tool handles own errors
    RECOVERY_RESTART,     // Restart tool process
    RECOVERY_RESET,       // Full tool reset + reinit
    RECOVERY_SYSTEM       // System reboot required
} recovery_action_t;

typedef struct {
    uint16_t error_code;
    uint8_t retry_count;
    recovery_action_t action;
    uint32_t last_error_time;
} error_context_t;

// Progressive error handling
recovery_action_t determine_recovery(error_context_t* ctx) {
    if (ctx->retry_count < 3) return RECOVERY_RESTART;
    if (ctx->retry_count < 6) return RECOVERY_RESET;
    return RECOVERY_SYSTEM;  // Last resort
}
```

### **Graceful Degradation**
```c
// System continues with reduced functionality
if (ntp_tool_failed) {
    // Use internal clock with warning
    use_internal_timestamps_only = true;
    feedback_show_warning(WARNING_NO_TIME_SYNC);
}

if (http_tool_failed) {
    // Store events locally until recovery
    store_events_offline_only = true;
    feedback_show_warning(WARNING_OFFLINE_MODE);
}
```

---

## 3. 📊 Circular Buffer Explanation

### **The Problem**
When events come faster than we can process (or during network outages):
- **Linear buffer:** Once full, what happens?
  - Stop accepting new events? ❌ (lose current data)
  - Grow buffer? ❌ (run out of RAM)
  - Block/wait? ❌ (system freezes)

### **Circular Buffer Solution**
```c
#define EVENT_BUFFER_SIZE 50

typedef struct {
    rfid_event_t events[EVENT_BUFFER_SIZE];
    uint8_t write_index;    // Where to write next event
    uint8_t read_index;     // Where to read next event
    uint8_t count;          // How many events currently stored
    bool overflow;          // Flag if we've wrapped around
} circular_buffer_t;

// Add new event (always succeeds)
void buffer_add_event(circular_buffer_t* buf, rfid_event_t* event) {
    buf->events[buf->write_index] = *event;
    buf->write_index = (buf->write_index + 1) % EVENT_BUFFER_SIZE;
    
    if (buf->count < EVENT_BUFFER_SIZE) {
        buf->count++;
    } else {
        // Buffer full - oldest event gets overwritten
        buf->read_index = (buf->read_index + 1) % EVENT_BUFFER_SIZE;
        buf->overflow = true;  // Flag data loss
    }
}

// Get next event (returns false if buffer empty)
bool buffer_get_event(circular_buffer_t* buf, rfid_event_t* event) {
    if (buf->count == 0) return false;
    
    *event = buf->events[buf->read_index];
    buf->read_index = (buf->read_index + 1) % EVENT_BUFFER_SIZE;
    buf->count--;
    
    return true;
}
```

### **Benefits**
- ✅ **Never blocks** - always accepts new events
- ✅ **Bounded memory** - uses exactly 50 event slots
- ✅ **Recent data priority** - keeps newest events, discards oldest
- ✅ **Overflow detection** - know when data was lost

### **Real-world Example**
```
Normal operation: [E1][E2][E3][  ][  ]...
                   ↑read     ↑write

Buffer full: [E46][E47][E48][E49][E50]
             ↑read              ↑write

New event comes: [E51][E47][E48][E49][E50]  ← E46 overwritten
                       ↑read ↑write

System processes: [E51][  ][E48][E49][E50]  ← E47 sent to HTTP
                            ↑read ↑write
```

---

## 4. 📡 HTTP Exponential Backoff

### **Retry Strategy**
```c
typedef struct {
    uint8_t attempt;           // Current attempt (0-based)
    uint8_t max_attempts;      // Give up after this many
    uint32_t base_delay_ms;    // Starting delay
    uint32_t max_delay_ms;     // Cap on delay
    uint32_t next_retry_time;  // When to try again
    bool connection_restored;  // Reset backoff when WiFi returns
} retry_context_t;

uint32_t calculate_backoff_delay(retry_context_t* ctx) {
    // Exponential: 1s, 2s, 4s, 8s, 16s, cap at 30s
    uint32_t delay = ctx->base_delay_ms * (1 << ctx->attempt);
    return (delay > ctx->max_delay_ms) ? ctx->max_delay_ms : delay;
}

void on_wifi_reconnected(retry_context_t* ctx) {
    // Reset backoff when connection restored
    ctx->attempt = 0;
    ctx->next_retry_time = 0;  // Retry immediately
    ctx->connection_restored = true;
}
```

### **Implementation Example**
```c
// Initialize retry context
retry_context_t http_retry = {
    .attempt = 0,
    .max_attempts = 8,           // Give up after 8 tries
    .base_delay_ms = 1000,       // Start with 1 second
    .max_delay_ms = 30000,       // Cap at 30 seconds
    .next_retry_time = 0
};

// In http_tool task
esp_err_t http_send_with_retry(webhook_payload_t* payload) {
    esp_err_t result = http_post(payload);
    
    if (result == ESP_OK) {
        // Success - reset retry context
        http_retry.attempt = 0;
        return ESP_OK;
    }
    
    // Failed - schedule retry with backoff
    if (http_retry.attempt < http_retry.max_attempts) {
        uint32_t delay = calculate_backoff_delay(&http_retry);
        http_retry.next_retry_time = xTaskGetTickCount() + pdMS_TO_TICKS(delay);
        http_retry.attempt++;
        
        // Save payload for retry
        fs_save_retry_payload(payload);
        
        return ESP_ERR_HTTP_EAGAIN;  // Will retry later
    }
    
    // Gave up - log error and discard
    ESP_LOGE(TAG, "HTTP send failed after %d attempts", http_retry.max_attempts);
    return ESP_FAIL;
}
```

---

## 🎯 Integration Benefits

This design gives you:
- ✅ **Visual debugging** - ASCII dashboard shows system state at a glance
- ✅ **Graceful degradation** - system works even when components fail
- ✅ **Data preservation** - circular buffer prevents event loss
- ✅ **Smart retry logic** - exponential backoff prevents network spam
- ✅ **JSON API** - structured data for web interfaces or logging

*beep boop* 

Ready to create sequence diagrams with this solid foundation? The flows will be much cleaner now! ✨
