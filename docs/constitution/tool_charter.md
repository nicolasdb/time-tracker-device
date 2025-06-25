# Time Tracker Device: Tool Charter & Responsibility Matrix

## Overview

This document serves as the **constitutional authority** for tool boundaries and responsibilities in the Time Tracker Device ecosystem. Each tool has clearly defined scope, responsibilities, and interaction patterns.

## Architectural Principles

1. **Single Responsibility**: Each tool owns exactly one domain
2. **Async Event-Driven**: Tools communicate through ESP event system, no direct calls
3. **Task-Based Design**: Each tool runs as independent FreeRTOS task
4. **Message Passing**: All coordination through event publish/subscribe
5. **Process Map Authority**: All interactions must follow async sequence diagrams

---

## Tool Renaming Proposal

### Current Names → Proposed Names

| Current Name | Proposed Name | Reason |
|--------------|---------------|---------|
| `debug_tool` | `system_monitor_tool` | Actually monitors system health, generates dashboards, tracks tool states |
| `feedback_tool` | `led_control_tool` | Pure LED hardware control and visual animations |
| `payload_tool` | `event_formatter_tool` | Formats events into JSON payloads with timestamps |

### Names That Are Correct
- `rfid_tool` ✅ (Hardware RFID interface)
- `network_tool` ✅ (WiFi/network management)  
- `http_tool` ✅ (HTTP communication)
- `fs_tool` ✅ (File system operations)
- `ntp_tool` ✅ (Network time protocol)
- `webserver_tool` ✅ (Web server for configuration)

---

## Tool Responsibility Matrix

### Hardware Interface Layer

#### `rfid_tool` - RFID Hardware Interface
**Domain**: RC522 Hardware Communication
**Responsibilities**:
- ✅ RC522 SPI communication
- ✅ Tag UID reading and validation
- ✅ Hardware debouncing (immediate)
- ✅ Grace period (5 seconds after boot - ignore tag presence)
- ✅ Event creation (TAG_PLACED, TAG_REMOVED)
- ✅ Event queue management (10-event buffer)

**NOT Responsible For**:
- ❌ Session timing logic
- ❌ Flow awareness
- ❌ User experience timing
- ❌ Visual feedback
- ❌ Network communication

**Interface**:
```c
rfid_tool_handle_t rfid_tool_init(const rfid_tool_config_t* config);
esp_err_t rfid_tool_get_next_event(rfid_tool_handle_t handle, rfid_event_t* event);
```

**Grace Period Logic**:
```
Boot → 5 second grace period → Normal operation
During grace: Read tags but emit NO events (prevent false positives)
After grace: Emit events on tag state changes
```

---

#### `led_control_tool` (formerly `feedback_tool`) - Visual Output
**Domain**: RGB LED Hardware Control
**Responsibilities**:
- ✅ RGB LED hardware control (WS2812/similar)
- ✅ Color patterns and animations
- ✅ Priority-based visual state queue
- ✅ Animation timing and transitions
- ✅ Visual feedback execution ONLY

**NOT Responsible For**:
- ❌ Deciding WHEN to show feedback
- ❌ Flow state logic
- ❌ Session timing
- ❌ System state monitoring

**Interface**:
```c
led_control_tool_handle_t led_control_tool_init(const led_control_config_t* config);
esp_err_t led_control_tool_show_state(led_control_tool_handle_t handle, visual_state_t state);
```

---

### Data Processing Layer

#### `event_formatter_tool` (formerly `payload_tool`) - Event Formatting
**Domain**: Event Data Transformation
**Responsibilities**:
- ✅ Convert internal events to JSON format
- ✅ Add timestamps (NTP-synchronized)
- ✅ Add device metadata (device_id, boot_counter)
- ✅ Calculate precise timing offsets
- ✅ Prepare data for HTTP transmission

**NOT Responsible For**:
- ❌ Event detection
- ❌ Network transmission
- ❌ File storage
- ❌ Flow timing

**Interface**:
```c
event_formatter_tool_handle_t event_formatter_tool_init(const event_formatter_config_t* config);
esp_err_t event_formatter_tool_format_event(event_formatter_tool_handle_t handle, 
                                           const rfid_event_t* event, 
                                           formatted_payload_t* result);
```

---

#### `system_monitor_tool` (formerly `debug_tool`) - System Health & Flow Awareness
**Domain**: System Monitoring & Cognitive Flow Timing
**Responsibilities**:
- ✅ ASCII dashboard generation
- ✅ System health aggregation from all tools
- ✅ Tool status registration and monitoring
- ✅ Robot expression states (◕‿◕) 
- ✅ **Flow awareness timing (60+ min → orange breathing)**
- ✅ **Flow urgency timing (90+ min → orange pulsing)**
- ✅ Real-time system status updates

**Flow Timing Logic**:
```
Session Detection:
1. Monitor RFID events from rfid_tool
2. Track session start/end times
3. Calculate session duration
4. At 60 minutes → Request FLOW_AWARENESS visual state
5. At 90 minutes → Request FLOW_URGENCY visual state
6. On session end → Reset timers
```

**NOT Responsible For**:
- ❌ LED hardware control (delegates to led_control_tool)
- ❌ Event detection
- ❌ Network communication
- ❌ File operations

**Interface**:
```c
system_monitor_tool_handle_t system_monitor_tool_init(const system_monitor_config_t* config);
esp_err_t system_monitor_tool_register_tool(system_monitor_tool_handle_t handle, const tool_registration_t* reg);
esp_err_t system_monitor_tool_notify_session_event(system_monitor_tool_handle_t handle, session_event_type_t event);
```

---

### Communication Layer

#### `network_tool` - Network Management
**Domain**: WiFi and Network Connectivity
**Responsibilities**:
- ✅ WiFi connection management
- ✅ Multi-network rotation
- ✅ Access Point mode for configuration
- ✅ Network status monitoring
- ✅ Connection retry logic

**Interface**:
```c
network_tool_handle_t network_tool_init(const network_tool_config_t* config);
esp_err_t network_tool_get_status(network_tool_handle_t handle, network_status_t* status);
```

---

#### `http_tool` - HTTP Communication
**Domain**: HTTP Client Operations
**Responsibilities**:
- ✅ HTTP POST requests to webhook server
- ✅ Exponential backoff retry logic
- ✅ Request queuing and management
- ✅ Error handling and status reporting

**Interface**:
```c
http_tool_handle_t http_tool_init(const http_tool_config_t* config);
esp_err_t http_tool_send_payload(http_tool_handle_t handle, const char* json_payload);
```

---

#### `ntp_tool` - Network Time Protocol
**Domain**: Time Synchronization
**Responsibilities**:
- ✅ NTP server synchronization
- ✅ Time offset calculation
- ✅ Clock drift compensation
- ✅ Timezone handling

**Interface**:
```c
ntp_tool_handle_t ntp_tool_init(const ntp_tool_config_t* config);
esp_err_t ntp_tool_get_timestamp(ntp_tool_handle_t handle, uint64_t* timestamp_ms);
```

---

### Storage Layer

#### `fs_tool` - File System Operations
**Domain**: Local File Storage
**Responsibilities**:
- ✅ LittleFS operations
- ✅ JSON event logging (append-only)
- ✅ Configuration file management
- ✅ Storage space monitoring
- ✅ File cleanup and rotation

**Interface**:
```c
fs_tool_handle_t fs_tool_init(const fs_tool_config_t* config);
esp_err_t fs_tool_append_json_log(fs_tool_handle_t handle, const char* filename, cJSON* json_data);
```

---

### Configuration Layer

#### `webserver_tool` - Configuration Web Server
**Domain**: Device Configuration Interface
**Responsibilities**:
- ✅ HTTP server for device configuration
- ✅ WiFi credential management
- ✅ Device setting updates
- ✅ Configuration persistence

**Interface**:
```c
webserver_tool_handle_t webserver_tool_init(const webserver_tool_config_t* config);
esp_err_t webserver_tool_start(webserver_tool_handle_t handle);
```

---

## Orchestration Layer

### `main.c` - System Orchestrator
**Domain**: Tool Coordination ONLY
**Responsibilities**:
- ✅ Tool initialization in correct order (per process maps)
- ✅ Event routing between tools
- ✅ State machine transitions
- ✅ Grace period coordination
- ✅ Tool dependency injection

**NOT Responsible For**:
- ❌ Business logic of any kind
- ❌ Direct hardware control
- ❌ Data processing
- ❌ Decision making (delegates to appropriate tools)

**Orchestration Flow**:
```
1. Boot Sequence (per 01_boot_sequence.mmd)
2. Grace Period (5 seconds - rfid_tool coordination)
3. Event Loop:
   - Get RFID events → route to event_formatter_tool
   - Format events → route to fs_tool + http_tool
   - Monitor system → delegate to system_monitor_tool
   - Visual feedback → delegate to led_control_tool
```

---

## Tool Communication Patterns

### Event Flow Architecture
```
rfid_tool → main.c → event_formatter_tool → main.c → fs_tool + http_tool
                  ↓
            system_monitor_tool → main.c → led_control_tool
```

### Information Flow
- **Upstream**: Hardware events flow from rfid_tool to processing tools
- **Downstream**: Processed data flows to storage and communication tools
- **Sideways**: System monitoring observes all tools and provides feedback

### Dependency Injection
All tools receive their dependencies through initialization:
```c
// Example: system_monitor_tool needs led_control_tool for visual feedback
system_monitor_config_t config = {
    .led_control_handle = led_control_handle,
    .update_interval_ms = 1000
};
```

---

## Flow Awareness Implementation

### Responsibility: `system_monitor_tool`
The system_monitor_tool is responsible for tracking session durations and triggering flow awareness states:

```c
// In system_monitor_tool implementation
typedef struct {
    uint64_t session_start_time_ms;
    bool session_active;
    bool flow_awareness_triggered;
    bool flow_urgency_triggered;
    led_control_tool_handle_t led_handle;  // For visual feedback delegation
} session_timing_context_t;

esp_err_t system_monitor_tool_notify_session_event(system_monitor_tool_handle_t handle, 
                                                  session_event_type_t event) {
    // SESSION_STARTED: Record start time
    // SESSION_ENDED: Reset all timers
    // Called by main.c when processing RFID events
}

// Internal timer check (called every second)
static void check_flow_states(system_monitor_tool_handle_t handle) {
    // At 60 minutes: led_control_tool_show_state(handle->led_handle, FLOW_AWARENESS)
    // At 90 minutes: led_control_tool_show_state(handle->led_handle, FLOW_URGENCY)
}
```

---

## Process Map Compliance

This charter ensures all tool responsibilities align with the constitutional process maps:

- **01_boot_sequence.mmd**: Tool initialization order preserved
- **02_tag_placement_happy_path.mmd**: Event flow exactly as diagrammed
- **03_error_handling_http_retry.mmd**: Error handling responsibilities clear
- **04_circular_buffer_stress_test.mmd**: Buffer management boundaries defined
- **05_device_state_machine.mmd**: State management orchestration clarified

---

## Implementation Action Plan

### Phase 1: Immediate Clarifications (No Code Changes)
1. ✅ Document current tool responsibilities (this charter)
2. ✅ Identify boundary violations in current code
3. ✅ Plan renaming strategy

### Phase 2: Renaming Implementation
1. Rename directories and files:
   - `debug_tool` → `system_monitor_tool`
   - `feedback_tool` → `led_control_tool`  
   - `payload_tool` → `event_formatter_tool`
2. Update all header includes and references
3. Update process maps to reflect new names

### Phase 3: Boundary Enforcement
1. Move flow timing logic from main.c to system_monitor_tool
2. Ensure rfid_tool only handles hardware and grace period
3. Verify led_control_tool has no decision logic
4. Clean up any remaining boundary violations

### Phase 4: Process Map Updates
1. Update all .mmd files with correct tool names
2. Add flow timing sequences to process maps
3. Validate all tool interactions match charter

---

## Success Criteria

- ✅ Every tool has exactly one clear domain
- ✅ No tool violates another tool's boundaries
- ✅ Flow awareness logic has a clear owner (system_monitor_tool)
- ✅ Grace period logic clearly belongs to rfid_tool
- ✅ All process maps align with charter
- ✅ Tool names accurately reflect their responsibilities

---

## Visual Feedback Specifications (Integrated from Feedback_colorMap.md)

### Core Visual States & Patterns

#### Primary States
- **🔵 BLUE:** System readiness (breathing 3s cycle)
- **🟢 GREEN:** Active sessions (flash 300ms → solid → flash 300ms)
- **🟠 ORANGE:** Flow awareness (60min breathing, 90min pulsing)
- **🔴 RED:** Critical errors (override all states)
- **🟡 YELLOW:** Warnings (temporary issues)
- **🟣 PURPLE:** AP mode/configuration (breathing 2s cycle)
- **🩵 CYAN:** Initialization (flashing 200ms intervals)

#### Flow Awareness Implementation
**Responsibility: `system_monitor_tool`**
```c
// 60 minutes: Orange breathing (gentle awareness)
// 90 minutes: Orange pulsing (transition invitation)
// Pattern: 5-second cycle, 4:7:8 breathing ratio
```

#### Grace Period Visual Logic
**Responsibility: `rfid_tool` + `led_control_tool`**
```c
// Boot → CYAN flashing → CYAN breathing (5s grace) → BLUE breathing
// During grace: Read tags but emit NO events
// Prevents false positives from pre-existing tags
```

#### State Priority Hierarchy
```
1. RED (Critical errors) - Override everything
2. CYAN (Initialization) - Override operations  
3. PURPLE (AP mode) - Override operational states
4. ORANGE (Flow awareness) - During sessions only
5. GREEN (Session states) - Normal operation
6. BLUE (Idle ready) - Baseline state
7. YELLOW (Warnings) - Background only
```

---

*This charter serves as the constitutional authority for tool architecture. Any code changes must respect these boundaries. Process maps must align with this charter.*