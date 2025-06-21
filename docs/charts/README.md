# ESP32 Time Tracker - Sequence Diagrams Collection

This directory contains the complete set of process maps for the ESP32 Time Tracker device using the clean MCP-inspired architecture.

## 📊 Diagram Overview

### **01_boot_sequence.mmd**
**Purpose:** Shows system initialization and tool startup sequence with grace period
**Key Insights:**
- CYAN initialization feedback during startup (user confidence)
- 5-second grace period prevents false positives from existing tags
- Tool initialization order matters (fs_tool first, http_tool last)
- State reporting throughout boot process
- Graceful fallback to AP mode (purple breathing) if WiFi fails
- Smart handling of pre-existing tags during reboot

### **02_tag_placement_happy_path.mmd** 
**Purpose:** Normal operation when everything works perfectly
**Key Insights:**
- RFID debounce logic prevents duplicate events
- Clean data flow: RFID → Buffer → Payload → FS → HTTP
- Internal clock + NTP offset for accurate timestamps (no more 1970 disasters)
- LED feedback: GREEN flash → GREEN solid → session confirmation
- Dashboard updates with system status
- Offline queuing shown as BLUE breathing with GREEN tints

### **03_error_handling_http_retry.mmd**
**Purpose:** What happens when HTTP requests fail
**Key Insights:**
- Exponential backoff prevents network spam
- WiFi reconnection resets retry counter
- Progressive error states: YELLOW warnings → RED errors
- BLUE breathing with GREEN tints shows offline mode with queued data
- System gracefully degrades but continues functioning
- Dashboard robot face changes based on error severity

### **04_circular_buffer_stress_test.mmd**
**Purpose:** How system handles rapid events and buffer overflow
**Key Insights:**
- Debounce logic filters out invalid rapid scans
- Circular buffer prevents system freeze under load
- Overflow handling: recent events prioritized over old
- RED fast blinks for critical buffer overflow (not orange)
- Dashboard shows buffer status and stress indicators
- System continues functioning even under extreme load

### **05_device_state_machine.mmd**
**Purpose:** Overall device behavior and state transitions
**Key Insights:**
- Clear states for all operating modes including grace period
- Flow awareness states (60min breathing, 90min pulsing) 
- Nested states show internal tool behavior
- Recovery paths from error conditions
- Manual reset and configuration flows
- Boot sequence with initialization and grace period handling

## 🎯 How to Use These Diagrams

### **For Development with Claude Code:**
1. Reference specific diagram when asking for code implementation
2. Point out the exact sequence of operations needed
3. Use state names and transitions for error handling logic
4. Mention tool responsibilities from sequence flows

### **For Architecture Validation:**
1. Walk through scenarios using the diagrams
2. Look for missing error cases or edge conditions
3. Verify state transitions make sense
4. Check that tool boundaries are respected

### **For Team Communication:**
1. Use diagrams to explain system behavior to non-technical stakeholders
2. Reference specific sequences when discussing bugs
3. Show expected vs actual behavior using the flows
4. Use state machine for planning test scenarios

## 🔄 Diagram to Story Translation

Each sequence diagram can be read as a story:

**Example from 02_tag_placement_happy_path.mmd:**
> "Nicolas places his Deep Work tag on the device. The RFID tool notices it immediately and thinks: 'Is this the same tag I just saw?' After checking, it realizes this is a new session and gets excited. It tells the LED to flash green while the payload tool calculates the real timestamp using the internal clock plus NTP offset. When the HTTP tool successfully sends to Supabase, the LED switches to solid green, confirming the session is being tracked. The dashboard shows the session as active with accurate timing."

## 🛠️ Tool Integration Points

Each diagram shows how tools communicate through main.c:
- **No direct tool-to-tool calls** - all coordination via main.c
- **State reporting** - tools report status using A2A-style updates  
- **Event-driven** - tools signal when work is ready
- **Error isolation** - failed tools don't crash others

## 🧪 Testing Strategy

Use these diagrams to design tests:
1. **Unit Tests** - Test each tool's internal logic shown in sequences
2. **Integration Tests** - Verify tool communication flows
3. **System Tests** - Run complete scenarios from diagrams
4. **Stress Tests** - Use buffer overflow scenarios for load testing

## 📈 Performance Considerations

The diagrams reveal performance bottlenecks and UX considerations:
- **HTTP sending** is the slowest operation (network dependent)
- **FS operations** should be batched when possible  
- **Circular buffer** prevents blocking under load
- **Debounce logic** reduces unnecessary processing
- **Grace period** prevents false positives during reboot
- **Flow awareness timing** supports ultradian rhythms (60/90min cycles)

## 🔍 Debugging Guide

When debugging, map your issue to the relevant diagram:
- **Boot problems** → 01_boot_sequence.mmd
- **Event detection issues** → 02_tag_placement_happy_path.mmd
- **Network/sync problems** → 03_error_handling_http_retry.mmd
- **Performance/overload** → 04_circular_buffer_stress_test.mmd
- **State confusion** → 05_device_state_machine.mmd

---

*Generated by BMO for the Time Tracker Quest - These diagrams serve as the Rosetta Stone between human understanding and AI implementation!* ✨
