# Ecosystem Integration Points

## 🔗 **Interface Specifications**

This document defines the exact integration requirements between the Time Tracker Device and the broader Cognitive Wealth ecosystem.

## 📡 **Device → Webhook Server Interface**

### **HTTP Endpoint Requirements**
```
POST /api/v1/time-events
Content-Type: application/json
Authorization: Bearer {device_token}
```

### **Standard Event Payload**
```json
{
  "event_type": "tag_placed|tag_removed",
  "tag_uid": "04B78FB0790000",
  "device_id": "ESP32_F0F5BD",
  "timestamp": "2025-01-15T10:30:45.123Z",
  "metadata": {
    "internal_millis": 123456789,
    "ntp_offset_ms": 1642234245000,
    "ntp_synced": true,
    "boot_counter": 42,
    "wifi_network": "WorkNetwork_5G",
    "signal_strength": -45,
    "firmware_version": "v3.1.2"
  }
}
```

### **Response Format**
```json
{
  "status": "success|error",
  "event_id": "uuid_v4",
  "timestamp_received": "2025-01-15T10:30:45.456Z",
  "validation": {
    "timestamp_valid": true,
    "device_authorized": true,
    "event_format_valid": true
  },
  "config_updates": {
    "flow_timing": {
      "awareness_60min": 3600000,
      "urgency_90min": 5400000
    },
    "visual_feedback": {
      "idle_breathing_cycle": 3000,
      "session_flash_duration": 300
    }
  }
}
```

### **Error Responses**
```json
{
  "status": "error",
  "error_code": "INVALID_TIMESTAMP|DEVICE_NOT_FOUND|MALFORMED_PAYLOAD",
  "message": "Human readable error description",
  "retry_after": 30,
  "support_reference": "error_ref_uuid"
}
```

## 🌐 **Webhook Server Architecture**

### **Server Role (fs_tool Pattern)**
```
Device Events → Validation → Normalization → Database Storage
     ↓              ↓            ↓               ↓
Rate Limiting   Auth Check   Data Cleanup   Persistence
```

### **Key Responsibilities**
- **Event Validation:** Timestamp, format, device authorization
- **Data Normalization:** Consistent timezone, format standardization
- **Rate Limiting:** Prevent spam, handle burst events
- **Authentication:** Device registration and token management
- **Database Gatekeeper:** Only component with direct DB write access
- **Configuration Management:** Dynamic device settings updates

### **Expected Server Capabilities**
```
REQUIRED ENDPOINTS:
├─ POST /api/v1/time-events        → Primary event ingestion
├─ GET  /api/v1/device/config     → Configuration retrieval
├─ POST /api/v1/device/register   → New device registration  
├─ GET  /api/v1/device/status     → Health check endpoint
└─ POST /api/v1/device/heartbeat  → Keep-alive mechanism
```

## 🗄️ **Database Schema Requirements**

### **Time Events Table**
```sql
CREATE TABLE time_events (
  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
  device_id VARCHAR(32) NOT NULL,
  event_type VARCHAR(16) NOT NULL, -- 'tag_placed', 'tag_removed'
  tag_uid VARCHAR(32) NOT NULL,
  timestamp_utc TIMESTAMPTZ NOT NULL,
  internal_millis BIGINT,
  ntp_offset_ms BIGINT,
  ntp_synced BOOLEAN,
  boot_counter INTEGER,
  wifi_network VARCHAR(64),
  signal_strength INTEGER,
  firmware_version VARCHAR(16),
  received_at TIMESTAMPTZ DEFAULT NOW(),
  processed BOOLEAN DEFAULT FALSE,
  
  -- Indexes for performance
  INDEX idx_device_timestamp (device_id, timestamp_utc),
  INDEX idx_tag_events (tag_uid, event_type, timestamp_utc),
  INDEX idx_processing (processed, received_at)
);
```

### **Device Registry Table**
```sql
CREATE TABLE devices (
  device_id VARCHAR(32) PRIMARY KEY,
  device_name VARCHAR(64),
  mac_address VARCHAR(18) UNIQUE,
  first_seen TIMESTAMPTZ DEFAULT NOW(),
  last_heartbeat TIMESTAMPTZ,
  firmware_version VARCHAR(16),
  hardware_revision VARCHAR(16),
  location VARCHAR(64),
  user_id UUID REFERENCES users(id),
  active BOOLEAN DEFAULT TRUE,
  configuration JSONB
);
```

### **Processed Sessions Table** (Created by agents)
```sql
CREATE TABLE work_sessions (
  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
  device_id VARCHAR(32) REFERENCES devices(device_id),
  tag_uid VARCHAR(32),
  start_event_id UUID REFERENCES time_events(id),
  end_event_id UUID REFERENCES time_events(id),
  start_time TIMESTAMPTZ,
  end_time TIMESTAMPTZ,
  duration_seconds INTEGER,
  project_name VARCHAR(128),
  session_type VARCHAR(32),
  created_at TIMESTAMPTZ DEFAULT NOW(),
  
  INDEX idx_user_sessions (device_id, start_time),
  INDEX idx_project_analysis (project_name, start_time)
);
```

## 🤖 **Database → AI Agents Interface**

### **Agent Data Access Pattern**
```
Database (Read-Only) → A2A Task Queue → Agent Processing → Memory Blocks
     ↓                      ↓               ↓              ↓
Raw Events          Task Coordination   Analysis      Knowledge Storage
```

### **Math Agent Data Requirements**
```sql
-- Time analysis queries
SELECT 
  device_id,
  DATE(timestamp_utc) as date,
  tag_uid,
  COUNT(*) as event_count,
  SUM(duration_seconds) as total_time
FROM work_sessions 
WHERE start_time >= '2025-01-01'
GROUP BY device_id, date, tag_uid;
```

### **Zuri Agent Data Requirements**
```sql
-- Daily reflection data  
SELECT 
  device_id,
  DATE(start_time) as work_date,
  ARRAY_AGG(project_name) as projects,
  SUM(duration_seconds) as total_focus_time,
  COUNT(DISTINCT tag_uid) as context_switches,
  MIN(start_time) as first_session,
  MAX(end_time) as last_session
FROM work_sessions
WHERE start_time >= CURRENT_DATE - INTERVAL '1 day'
GROUP BY device_id, work_date;
```

### **Agent Memory Integration**
```
Time Events → Session Processing → Agent Analysis → Memory Blocks → Templates
     ↓              ↓                  ↓              ↓            ↓
Device Data    Structured Data    Insights      Knowledge    Evolution
```

## 🔄 **Configuration Management**

### **Device Configuration Schema**
```json
{
  "device_id": "ESP32_F0F5BD",
  "config_version": "1.2.3",
  "last_updated": "2025-01-15T10:30:45Z",
  "settings": {
    "flow_awareness": {
      "enabled": true,
      "awareness_60min_ms": 3600000,
      "urgency_90min_ms": 5400000,
      "breathing_cycle_ms": 5000,
      "pulsing_cycle_ms": 1500
    },
    "visual_feedback": {
      "idle_breathing_cycle_ms": 3000,
      "session_flash_duration_ms": 300,
      "brightness_level": 128,
      "color_theme": "default"
    },
    "connectivity": {
      "heartbeat_interval_s": 300,
      "retry_backoff_base_ms": 1000,
      "max_retry_attempts": 8,
      "offline_queue_size": 100
    },
    "debugging": {
      "verbose_logging": false,
      "serial_output": true,
      "led_debug_mode": false
    }
  }
}
```

### **Dynamic Configuration Updates**
```
Agent Analysis → Template Evolution → Configuration Updates → Device Sync
      ↓                ↓                     ↓                ↓
User Patterns    Optimized Settings    Server Storage    Runtime Apply
```

## 🔍 **Monitoring & Health**

### **Device Health Metrics**
```json
{
  "device_id": "ESP32_F0F5BD",
  "timestamp": "2025-01-15T10:30:45Z",
  "system": {
    "uptime_ms": 86400000,
    "free_heap": 45632,
    "wifi_signal": -45,
    "ntp_synced": true,
    "boot_counter": 42
  },
  "performance": {
    "events_per_hour": 12,
    "avg_response_time_ms": 150,
    "sync_success_rate": 98.5,
    "buffer_utilization": 15
  },
  "errors": {
    "rfid_read_errors": 0,
    "network_timeouts": 2,
    "ntp_sync_failures": 0,
    "storage_errors": 0
  }
}
```

### **Ecosystem Health Dashboard**
```
Device Status → Server Metrics → Database Performance → Agent Activity
     ↓              ↓                  ↓                  ↓
Connectivity   Processing Rate    Query Performance   Insight Generation
```

## 🚀 **Integration Testing Strategy**

### **Device-Server Integration Tests**
- **Event delivery** under normal conditions
- **Offline resilience** with network failures
- **Retry logic** with server unavailability
- **Configuration updates** with dynamic settings
- **Error handling** with malformed data

### **Server-Database Integration Tests**
- **Data validation** and normalization
- **Performance** under load conditions
- **Transaction integrity** during failures
- **Query optimization** for agent access

### **End-to-End Ecosystem Tests**
- **Device event** → **Agent insight** complete flow
- **Configuration change** → **Device behavior** validation
- **Multi-device** coordination and data aggregation
- **Long-term** pattern recognition and evolution

## 🔮 **Future Integration Points**

### **Enhanced Device Capabilities**
- **Biometric sensors** integration
- **Environmental data** collection
- **Context awareness** (calendar, location)
- **Voice interaction** for manual logging

### **Advanced Analytics Integration**
- **Real-time** pattern recognition
- **Predictive modeling** for optimization
- **Cross-user insights** (anonymized)
- **Recommendation engines** for flow improvement

### **External System Integration**
- **Calendar systems** for automatic context
- **Project management** tools for goal tracking
- **Health platforms** for holistic optimization
- **Productivity suites** for workflow integration

---

**Integration Philosophy:** Loose coupling with strong contracts - each layer can evolve independently while maintaining reliable communication through well-defined interfaces.
