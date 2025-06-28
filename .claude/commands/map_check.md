# Map Check Command

## Purpose
Validate proposed changes against process maps to prevent architectural drift and ensure ecosystem compatibility.

## Usage
```bash
/map_check <component> <proposed_change>
```

## Examples
```bash
/map_check "rfid_tool" "add_session_metadata"
/map_check "fs_tool" "implement_event_logging" 
/map_check "http_tool" "modify_payload_format"
/map_check "main.c" "change_boot_sequence"
```

## Validation Process

### **Step 1: Process Map References**
Check against relevant process maps in `docs/constitution/process_maps/`:

#### **Device Master FSM Changes**
- **Reference:** `01_device_master_fsm.mmd`
- **Validates:** Tool initialization order, esp_event hub communication, system monitor health checking
- **Critical:** 5-second health check timeout, esp_event as central communication hub, system_monitor role

#### **Tag Detection Changes**
- **Reference:** `07_tag_detection_fsm.mmd`
- **Validates:** RFID scanning, debounce logic, grace period handling
- **Critical:** 5-second boot grace period, 200ms debounce confirmation, polling intervals

#### **Tag Event Processing Changes**
- **Reference:** `08_tag_event_fsm.mmd`
- **Validates:** Event formatting, payload creation, esp_event posting
- **Critical:** Event payload format, uptime timestamp calculation, esp_event_post usage

#### **Visual Feedback Changes**
- **Reference:** `11_feedback_fsm.mmd`
- **Validates:** LED state management, feedback recipes, system state mapping
- **Critical:** State-to-LED mapping, recipe execution, no decision logic in feedback

#### **Payload Creation Changes**
- **Reference:** `13_payload_fsm.mmd`
- **Validates:** NTP dependency checking, timestamp calculation, data formatting
- **Critical:** NTP sync validation, real timestamp calculation, device metadata inclusion

#### **HTTP Communication Changes**
- **Reference:** `14_http_fsm.mmd`
- **Validates:** Webhook transmission, retry logic, error handling
- **Critical:** Exponential backoff, local storage fallback, retry count limits

### **Step 2: Architectural Boundary Check**

#### **Tool Responsibility Validation**
**Reference:** `docs/constitution/tool_charter.md` for complete tool boundaries and responsibilities.

**Key Boundaries:**
- `rfid_tool`: Hardware interface + 5s grace period ONLY
- `system_monitor_tool`: Flow awareness timing (60/90min) + system health
- `led_control_tool`: Visual patterns ONLY (no decision logic)
- `event_formatter_tool`: JSON formatting + timestamps ONLY
- All other tools: Single domain responsibility per charter

#### **Communication Protocol Validation**
- **Events only** - no direct tool-to-tool calls
- **State reporting** - tools report status to main.c
- **No shared globals** - all data through handles
- **Clear interfaces** - defined input/output contracts

### **Step 3: Ecosystem Integration Check**

#### **Data Format Compatibility**
```json
// Standard event format MUST be maintained
{
  "event_type": "tag_placed|tag_removed",
  "tag_uid": "string",
  "device_id": "string", 
  "timestamp": "ISO8601_UTC",
  "metadata": {
    "internal_millis": "number",
    "ntp_offset_ms": "number",
    "ntp_synced": "boolean",
    "boot_counter": "number",
    "firmware_version": "string"
  }
}
```

#### **Webhook Server Expectations**
- **HTTP POST** to `/api/v1/time-events`
- **JSON Content-Type** with standard headers
- **Retry logic** with exponential backoff
- **Error code** handling for 4xx and 5xx responses

#### **Agent System Requirements**
- **Precise timestamps** for Math Agent analysis
- **Event metadata** for context understanding
- **Session boundaries** for Zuri/Ulyss planning
- **Device health** for Athena system evolution

### **Step 4: Process Map Compliance**

#### **Sequence Compliance**
- Does the change follow the exact sequence shown in relevant diagrams?
- Are new steps inserted in logical positions?
- Do timing requirements remain intact?

#### **State Machine Compliance**
- Does the change respect state boundaries?
- Are new states properly nested or top-level?
- Do transitions follow defined trigger conditions?

#### **Error Handling Compliance**
- Does the change maintain error propagation?
- Are retry mechanisms preserved?
- Is graceful degradation maintained?

## Validation Outcomes

### **✅ APPROVED Changes**
- Follow process map sequences exactly
- Respect tool boundaries and responsibilities  
- Maintain ecosystem integration requirements
- Preserve error handling and recovery logic
- Support future expansion without breaking existing flows

### **⚠️ CONDITIONAL Changes**
- Minor deviations from process maps with clear justification
- Tool boundary adjustments with ecosystem impact assessment
- Performance optimizations that don't affect external interfaces
- Error handling improvements that enhance existing logic

### **❌ REJECTED Changes**
- Break tool responsibility boundaries
- Modify ecosystem integration interfaces without coordination
- Change process map sequences without architectural review
- Remove error handling or recovery mechanisms
- Create tight coupling between tools

## Integration with Other Commands

### **Before Making Changes**
```bash
/ecosystem_context     # Understand system role
/map_check "component" "change"  # Validate against process maps
```

### **After Validation**
```bash
# If approved, proceed with implementation
# Follow process maps exactly during development
/save_progress "phase" # Track with ecosystem impact notes
```

### **If Rejected**
```bash
# Reconsider approach or request process map update
# Process map updates require architectural review
# Consider alternative approaches that maintain compliance
```

## Process Map Update Protocol

If legitimate changes require process map updates:

1. **Architectural Review Required** - Not during implementation phases
2. **Ecosystem Impact Assessment** - Consider webhook server, agents
3. **Documentation Update** - All related diagrams and specs
4. **Team Consensus** - If working with others
5. **Version Control** - Clear change tracking

## Reference Files
- `docs/constitution/tool_charter.md` - Master tool boundaries and responsibilities
- `docs/constitution/process_maps/*.mmd` - All constitutional process diagrams
- `docs/constitution/process_maps/README.md` - How to read process maps
- `docs/implementation/architecture/` - Implementation guidance
- `docs/ecosystem/integration_points.md` - Interface specifications

---
*Process maps are constitutional documents - they define the system's fundamental behavior and should not be modified during implementation phases*
