# Container Contracts - Implementation Guidelines

> **Container isolation implementation guidelines derived from constitutional process maps**

## **⚖️ Constitutional Authority Hierarchy**

1. **SUPREME AUTHORITY**: Process Maps (`docs/constitution/process_maps/`)
2. **IMPLEMENTATION GUIDANCE**: Container Contracts (this document)
3. **VALIDATION PROCEDURES**: Compliance checklists and tests

**Note**: This document provides implementation guidance for achieving process map compliance. In case of conflicts, process maps take absolute precedence.

## Architecture Overview

```txt
┌─────────────┐    ESP_EVENT   ┌─────────────┐    ESP_EVENT   ┌─────────────┐
│ RFID_TOOL   │◄──────────────►│ PAYLOAD_TOOL│◄──────────────►│ HTTP_TOOL   │
│ (Container) │                │ (Container) │                │ (Container) │
└─────────────┘                └─────────────┘                └─────────────┘
```

## Container Contract Format

Each tool container MUST conform to:
- **Input Interface**: Defined ESP_EVENT subscriptions
- **Output Interface**: Defined ESP_EVENT publications  
- **Internal State**: Completely private to the container
- **Process Map**: Constitutional authority for behavior

---

## RFID_TOOL Container Contract

**Process Map Authority**: `docs/constitution/process_maps/07_tag_detection_fsm.mmd`

### Input Interface
```yaml
Hardware:
  - SPI_Bus: GPIO pins for RC522 communication
  - Power: 3.3V supply for RC522 module

ESP_Events: []  # No event inputs - hardware driven
```

### Output Interface
```yaml
ESP_Events:
  - Event_Base: RFID_EVENTS
  - Events:
    - RFID_EVENT_TAG_DETECTED:
        Data: rfid_event_data_t
        Fields:
          - tag_uid: string (hex format)
          - detection_timestamp_us: uint64_t
          - signal_strength: uint8_t
    - RFID_EVENT_TAG_REMOVED:
        Data: rfid_event_data_t  
        Fields:
          - tag_uid: string (hex format)
          - removal_timestamp_us: uint64_t
```

### Container Guarantees
- ✅ 100ms scan loop maximum
- ✅ Debouncing to prevent spurious events
- ✅ UID validation and error checking
- ✅ Self-contained state management

---

## PAYLOAD_TOOL Container Contract

**Process Map Authority**: `docs/constitution/process_maps/13_payload_fsm.mmd`

### Input Interface
```yaml
ESP_Events:
  - Event_Base: RFID_EVENTS
  - Required_Events:
    - RFID_EVENT_TAG_DETECTED
    - RFID_EVENT_TAG_REMOVED
  - Data: rfid_event_data_t

Dependencies:
  - NTP_SYNC: Required for timestamp calculation
  - SYSTEM_MONITOR: Required for health report data
```

### Output Interface
```yaml
ESP_Events:
  - Event_Base: PAYLOAD_EVENTS
  - Events:
    - PAYLOAD_EVENT_READY:
        Data: payload_event_data_t
        Fields:
          - payload_json: string (formatted JSON)
          - payload_size: size_t
          - event_type: string
    - PAYLOAD_EVENT_BACKLOG:
        Data: payload_backlog_data_t
        Fields:
          - pending_count: uint32_t
          - reason: string

FS_Events:
  - Event_Base: FS_EVENTS  
  - Events:
    - FS_EVENT_LOG_REQUEST:
        Data: fs_log_request_t
```

### Container Guarantees
- ✅ NTP dependency check before processing
- ✅ ISO8601 timestamp formatting
- ✅ Device metadata injection
- ✅ Health report inclusion
- ✅ JSON schema compliance

---

## HTTP_TOOL Container Contract

**Process Map Authority**: `docs/constitution/process_maps/14_http_fsm.mmd`

### Input Interface
```yaml
ESP_Events:
  - Event_Base: PAYLOAD_EVENTS
  - Required_Events:
    - PAYLOAD_EVENT_READY
  - Data: payload_event_data_t

Dependencies:
  - Network_Connection: WiFi connectivity required
  - FS_TOOL: For offline storage capability
```

### Output Interface
```yaml
ESP_Events:
  - Event_Base: HTTP_EVENTS
  - Events:
    - HTTP_EVENT_SUCCESS:
        Data: http_response_data_t
        Fields:
          - status_code: uint16_t
          - response_body: string
          - transmission_time_ms: uint32_t
    - HTTP_EVENT_RETRY:
        Data: http_retry_data_t
        Fields:
          - retry_count: uint8_t
          - next_retry_ms: uint32_t
    - HTTP_EVENT_FAILED:
        Data: http_error_data_t
        Fields:
          - error_code: esp_err_t
          - max_retries_reached: bool

FS_Events:
  - Event_Base: FS_EVENTS
  - Events:
    - FS_EVENT_STORE_REQUEST:
        Data: fs_store_request_t  # For offline queue
```

### Container Guarantees
- ✅ Exponential backoff retry logic
- ✅ Offline queue management
- ✅ HTTP connection management
- ✅ Error classification and reporting

---

## FEEDBACK_TOOL Container Contract

**Process Map Authority**: `docs/constitution/process_maps/11_feedback_fsm.mmd`

### Input Interface
```yaml
ESP_Events:
  - Event_Base: RFID_EVENTS
  - Events: [RFID_EVENT_TAG_DETECTED, RFID_EVENT_TAG_REMOVED]
  
  - Event_Base: NETWORK_TOOL_EVENTS  
  - Events: [NETWORK_TOOL_EVENT_STA_CONNECTED, NETWORK_TOOL_EVENT_STA_DISCONNECTED]
  
  - Event_Base: HTTP_EVENTS
  - Events: [HTTP_EVENT_SUCCESS, HTTP_EVENT_FAILED]
```

### Output Interface
```yaml
Hardware:
  - WS2812B_LED: GPIO7 for visual feedback
  - LED_Patterns:
    - IDLE: Green breathing
    - TAG_DETECTED: Solid green
    - TRANSMITTING: Blue pulse
    - ERROR: Red flash
    - WIFI_CONNECTING: Blue blink
```

### Container Guarantees
- ✅ Real-time visual feedback
- ✅ State-based LED patterns
- ✅ Non-blocking operation
- ✅ Hardware abstraction

---

## Constitutional Compliance Rules

### 1. Container Isolation
- ❌ **FORBIDDEN**: Direct function calls between containers
- ✅ **REQUIRED**: All communication via ESP_EVENT only
- ✅ **REQUIRED**: Self-contained state management

### 2. Interface Contracts
- ❌ **FORBIDDEN**: Changing input/output interfaces without constitutional amendment
- ✅ **REQUIRED**: Backward compatibility for interface changes
- ✅ **REQUIRED**: Event data structure versioning

### 3. Process Map Authority
- ❌ **FORBIDDEN**: Container behavior not defined in process maps
- ✅ **REQUIRED**: All state transitions must follow process map diagrams
- ✅ **REQUIRED**: Process map compliance validation

### 4. Development Workflow
- ✅ **REQUIRED**: Container contract validation before any changes
- ✅ **REQUIRED**: Interface compliance testing
- ✅ **REQUIRED**: Process map adherence verification

---

## Validation Commands

```bash
# Validate container interfaces
/validate_contracts

# Check process map compliance  
/validate_architecture

# Verify event flow integrity
/validate_events
```

---

*Constitutional Authority: Process Maps are the single source of truth for container behavior and interface definitions.*