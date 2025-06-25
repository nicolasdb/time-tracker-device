# ESP32 Clean Architecture - Fixed Design

## 🎯 Core Principles

1. **Only fs_tool has CRUD permissions** - No FS chaos
2. **A2A-style state management** - Tools report status
3. **FreeRTOS async tasks** - No blocking operations
4. **Memory-first, FS-second** - Use RAM efficiently
5. **Clear data flow** - Tools pass structured data, not write directly

---

## 🎭 Revised Tool Responsibilities

### **main.c** - The State Coordinator

```c
typedef enum {
    TOOL_IDLE,
    TOOL_BUSY,
    TOOL_WAITING,
    TOOL_ERROR
} tool_state_t;

typedef struct {
    char name[16];
    tool_state_t state;
    char waiting_for[32];
    uint16_t error_code;
    uint32_t last_update;
} tool_status_t;
```

**Responsibilities:**

- Initialize tools in dependency order
- Monitor tool states (A2A-style)
- Coordinate shutdown/recovery
- Handle state transitions

---

### **fs_tool** - The Storage Gatekeeper

**State Reporting:** `fs_idle`, `fs_writing`, `fs_reading`, `fs_error_full`

**API:**

```c
esp_err_t fs_write_event(rfid_event_t* event);
esp_err_t fs_write_config(wifi_config_t* config);
esp_err_t fs_read_config(wifi_config_t* config);
esp_err_t fs_write_payload(webhook_payload_t* payload);
```

**Responsibilities:**

- **ONLY** component that writes to LittleFS
- Provides structured API for other tools
- Manages storage space and integrity
- Reports storage status to main.c

---

### **feedback_tool** - The Status Communicator

**State Reporting:** `feedback_idle`, `feedback_animating`

**Split Responsibility:**

```c
// Visual feedback only
void feedback_show_state(system_state_t state);
void feedback_animate_pattern(led_pattern_t pattern);

// Serial handled by main.c or separate debug_tool?
```

**BMO's Take:** Keep visual only, serial debug should be separate concern

---

### **ntp_tool** - The Time Authority

**State Reporting:** `ntp_synced`, `ntp_syncing`, `ntp_error`

**Lifecycle:** Boot + periodic sync (every 24h)

**API:**

```c
bool ntp_is_synced(void);
time_t ntp_get_real_time(uint32_t internal_millis);
```

**Responsibilities:**

- Sync time when WiFi available
- Calculate real timestamps from internal clock
- Provide time authority for all events

---

### **rfid_tool** - The Event Generator

**State Reporting:** `rfid_scanning`, `rfid_debouncing`, `rfid_error_hardware`

**Clean Data Flow:**

```c
typedef struct {
    char tag_uid[16];
    uint32_t internal_millis;
    rfid_event_type_t type;  // PLACED, REMOVED
    bool processed;
} rfid_event_t;

// Memory buffer, not FS writes!
rfid_event_t event_buffer[EVENT_BUFFER_SIZE];
```

**Responsibilities:**

- Detect tag changes only
- Create structured events in memory
- Signal main.c when new events ready
- Handle debounce logic

---

### **payload_tool** - The Data Formatter (Split from webhook)

**State Reporting:** `payload_idle`, `payload_formatting`

**Clean Responsibility:**

```c
typedef struct {
    char device_id[32];
    char tag_uid[16];
    char event_type[16];
    char timestamp_iso[32];  // Real UTC time
    uint16_t sequence_number;
} webhook_payload_t;

esp_err_t payload_format_event(rfid_event_t* event, webhook_payload_t* payload);
```

**Responsibilities:**

- Convert rfid_events to webhook_payloads
- Calculate real timestamps using ntp_tool
- Add device metadata
- Format for webhook validation

---

### **http_tool** - The Network Sender (Split from webhook)

**State Reporting:** `http_idle`, `http_sending`, `http_retrying`, `http_error_network`

**Clean API:**

```c
esp_err_t http_send_payload(webhook_payload_t* payload);
```

**Responsibilities:**

- Send HTTP POST only
- Handle retry logic
- Report success/failure to main.c
- No payload formatting

---

### **network_tool** - Merged wifi + webserver

**State Reporting:** `net_connected`, `net_connecting`, `net_ap_mode`, `net_error`

**Lifecycle Management:**

```c
// Boot sequence
net_connect_from_stored() -> net_try_ap_mode() -> net_serve_config()
```

**Responsibilities:**

- WiFi connection management
- AP mode + config portal (when needed)
- Store credentials via fs_tool API
- Report connection status to main.c

---

## 🔄 Communication Protocol

### **State Updates (A2A-inspired)**

```c
// Each tool reports state every 100ms
void tool_report_state(const char* tool_name, tool_state_t state, const char* details);

// main.c polls states
tool_status_t* main_get_tool_status(const char* tool_name);
```

### **Event Flow**

```c
1. rfid_tool → detects tag → updates event_buffer → signals main.c
2. main.c → calls payload_tool → format event
3. main.c → calls fs_tool → save payload
4. main.c → calls http_tool → send payload (if WiFi ready)
5. tools report states throughout process
```

### **No Direct Tool-to-Tool Calls**

- All coordination through main.c
- Tools expose APIs, don't call each other
- State updates allow main.c to orchestrate

---

## 🚦 Tool Lifecycle Management

### **Boot Sequence**

```c
1. fs_tool_init()      // Must be first
2. network_tool_init() // Start WiFi attempts
3. ntp_tool_init()     // When WiFi ready
4. rfid_tool_init()    // Start scanning
5. feedback_tool_init() // Show ready state
6. http_tool_init()    // Prepare for sends
```

### **Runtime States**

```c
// Always running
- main.c (coordinator)
- fs_tool (storage API)
- rfid_tool (scanning)
- feedback_tool (status display)

// Conditional
- network_tool (AP mode only on WiFi fail)
- ntp_tool (periodic sync)
- payload_tool (on-demand formatting)
- http_tool (when payloads ready)
```

### **Memory Management**

```c
// Use 10% RAM efficiently
#define EVENT_BUFFER_SIZE 50      // ~2KB for recent events
#define PAYLOAD_QUEUE_SIZE 20     // ~1KB for outgoing payloads
#define CONFIG_CACHE_SIZE 512     // WiFi configs in RAM

// FS used for:
- Long-term event storage (when buffer full)
- Configuration persistence
- Payload retry queue (network failures)
```

---

## 🎯 Benefits of This Design

### **Clear Boundaries**

- Each tool has ONE responsibility
- No FS write conflicts
- Predictable data flow

### **Debuggable**

- Tool states visible to main.c
- Clear error reporting
- No hidden dependencies

### **Testable**

- Tools can be mocked easily
- State machine testable
- Event flow verifiable

### **Maintainable**

- Add new tools without touching existing
- Change payload format without touching HTTP
- Swap storage without affecting business logic

---

## 🤔 Remaining Questions

1. **Debug Output:** Separate debug_tool or part of main.c?
2. **Error Recovery:** Auto-restart failed tools or manual intervention?
3. **Event Buffer Overflow:** Circular buffer or block new events?
4. **HTTP Retry Strategy:** Exponential backoff or fixed intervals?

*beep boop*

This design fixes the core architectural issues! Now we can create clean sequence diagrams that won't confuse Claude Code! ✨
