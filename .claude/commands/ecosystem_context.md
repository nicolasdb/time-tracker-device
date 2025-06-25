# Ecosystem Context Command

## Purpose
Load complete cognitive wealth ecosystem understanding before making architectural decisions or major changes.

## When to Use
- Starting development on ecosystem integration features
- Before making changes that affect device interfaces
- When working on webhook server development
- Before implementing new data formats or protocols

## Context Loaded

### **System Architecture**
```
Layer 1: Edge Devices (this device) → Data capture and user interaction
Layer 2: Webhook Server            → Data validation and gateway  
Layer 3: Database (Supabase)       → Persistent storage
Layer 4: AI Agents                 → Pattern analysis and insights
Layer 5: Human Interface           → Dashboard and recommendations
```

### **Device Role & Responsibilities**
- **Data Producer:** Transforms human activity into structured events
- **User Interface:** Visual feedback and flow state support
- **Edge Intelligence:** Local processing and offline resilience
- **Ecosystem Integration:** Standard protocols for system compatibility

### **Integration Requirements**

#### **Webhook Server Interface**
```json
POST /api/v1/time-events
{
  "event_type": "tag_placed|tag_removed",
  "tag_uid": "04B78FB0790000", 
  "device_id": "ESP32_F0F5BD",
  "timestamp": "2025-01-15T10:30:45.123Z",
  "metadata": {
    "internal_millis": 123456789,
    "ntp_offset_ms": 1642234245000,
    "ntp_synced": true,
    "boot_counter": 42
  }
}
```

#### **Database Schema Expectations**
- **time_events** table with device_id, event_type, tag_uid, timestamp_utc
- **devices** table with device registry and configuration
- **work_sessions** table created by agent processing

#### **Agent System Requirements**
- **Math Agent:** Needs precise timing data for statistical analysis
- **Zuri Agent:** Needs daily summaries for reflection generation
- **Ulyss Agent:** Needs weekly patterns for planning
- **Athena Agent:** Needs long-term trends for strategic guidance

### **Architectural Patterns**

#### **fs_tool Pattern Consistency**
```
Device fs_tool = Local file system CRUD operations
Webhook Server = Database CRUD operations (same pattern, different scale)
```

#### **Data Flow Similarity**
```
RFID Events → Local Buffer → fs_tool → File Storage
HTTP Events → Server Buffer → webhook_tool → Database Storage
```

### **Design Constraints**

#### **Must Maintain**
- **Offline-first operation** - device works without server
- **Standard JSON protocols** - webhook server compatibility
- **Precise timestamps** - agent analysis requirements
- **Error propagation** - ecosystem debugging capability

#### **Must Consider**
- **Multi-device coordination** - eventual fleet management
- **Configuration management** - dynamic settings from server
- **Monitoring integration** - health metrics for ecosystem
- **Privacy preservation** - user data ownership

### **Future Integration Points**
- **Biometric sensors** for enhanced context
- **Environmental data** for holistic analysis  
- **Calendar integration** for automatic context
- **Cross-device coordination** for seamless experience

## Integration with Development Commands

Use with other commands for complete context:
```bash
/ecosystem_context        # Load this context
/map_check "component"    # Validate against process maps
/save_progress "phase"    # Track with ecosystem impact
```

## Key Files for Ecosystem Understanding
- `docs/constitution/ecosystem_contract.md` - Complete system architecture & integration specs
- `docs/constitution/tool_charter.md` - Device responsibilities and tool boundaries  
- `docs/constitution/process_maps/` - Constitutional process flows and device behavior

---
*Use this context to ensure all development decisions consider the device's role in the broader cognitive wealth ecosystem*
