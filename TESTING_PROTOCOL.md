# Constitutional Testing Protocol

## Overview

This document defines the testing strategy for constitutional tools without requiring full hardware components.

## Testing Architecture

```
Test Sequencer Tool
    ↓ (publishes test events)
ESP_EVENT System
    ↓ (distributes to tools)
Constitutional Tools (system_monitor, smart_contracts, fs_tool, feedback_tool)
    ↓ (respond via ESP_EVENT)
Validation Framework
    ↓ (generates reports)
Console Output & Logs
```

## Test Sequences

### 1. Boot Validation Test
**Purpose**: Verify constitutional tool initialization and ESP_EVENT system

**Test Steps**:
1. ESP_EVENT system operational check
2. Tool availability verification
3. Constitutional compliance validation
4. Health check timeout validation

**Expected Results**:
- All tools initialize with handle-based patterns
- ESP_EVENT communication established
- Constitutional compliance: 100%

### 2. State Transition Test
**Purpose**: Validate feedback_tool state management and visual patterns

**Test Steps**:
1. Simulate state: IDLE → blue breathing
2. Simulate state: TAG_DETECTED → green solid
3. Simulate state: WIFI_CONNECTING → blue blinking
4. Simulate state: FLOW_60 → orange breathing
5. Simulate state: FLOW_90 → orange pulsing
6. Simulate state: ERROR → red blinking

**Expected Results**:
- Each state change publishes ESP_EVENT
- feedback_tool responds to state changes
- Visual patterns updated (logged, not displayed)

### 3. Filesystem Operations Test
**Purpose**: Validate fs_tool JSON operations and health monitoring

**Test Steps**:
1. JSON config save simulation
2. JSON config load simulation
3. JSON log append simulation
4. Filesystem health check
5. Space monitoring validation

**Expected Results**:
- All operations complete without errors
- JSON operations simulated successfully
- Health checks pass
- Space monitoring reports simulated values

### 4. Tool Integration Test
**Purpose**: Validate ESP_EVENT communication between tools

**Test Steps**:
1. system_monitor → fs_tool: Dashboard save
2. feedback_tool state response to events
3. smart_contracts validation chain
4. End-to-end event flow validation

**Expected Results**:
- Events propagate correctly between tools
- Each tool responds appropriately
- Container isolation maintained
- No direct function calls between tools

### 5. Constitutional Compliance Test
**Purpose**: Verify constitutional architecture adherence

**Test Steps**:
1. Handle-based design validation
2. ESP_EVENT communication validation
3. Memory safety validation (snprintf, PRIu32)
4. Process map authority compliance

**Expected Results**:
- No constitutional violations detected
- All tools follow constitutional patterns
- Smart contracts validation passes
- Process maps respected

## Running Tests

### Manual Test Execution
```bash
# Build with test sequencer
idf.py build

# Flash and monitor
idf.py flash monitor

# Look for test sequence output in logs
```

### Test Commands
In the ESP32 console or via events:
1. Boot validation runs automatically
2. State transitions can be triggered via events
3. FS operations simulated in background
4. Tool integration tested during normal operation

## Hardware Simulation

### LED Feedback (feedback_tool)
- **Real Hardware**: WS2812B RGB LED control
- **Simulation**: GPIO toggle + logged color values
- **Test Method**: State changes logged with expected colors

### Filesystem (fs_tool)
- **Real Hardware**: LittleFS on ESP32 partition
- **Simulation**: In-memory JSON operations
- **Test Method**: Save/load operations return success

### Validation Criteria

#### ✅ Constitutional Compliance
- Handle-based design (no static globals)
- ESP_EVENT-only communication (no direct calls)
- Constitutional memory safety (snprintf, PRIu32)
- Container isolation (zero coupling)

#### ✅ Tool Integration
- Events published and received correctly
- State changes propagate through system
- Dashboard generation works
- Health checks operational

#### ✅ Performance
- Tool initialization < 5 seconds
- Event response time < 100ms
- Dashboard generation < 1 second
- Memory usage within limits

## Test Report Format

```
🧪 CONSTITUTIONAL TEST SEQUENCER REPORT
=======================================
📊 Test Summary:
   Tests Executed: 5
   Tests Passed: 5
   Tests Failed: 0
   Success Rate: 100.0%

⏱️ Execution Time:
   Total Uptime: 12,345 ms
   Test Duration: 5,678 ms

🏛️ Constitutional Compliance:
   Handle-based Design: ✅
   ESP_EVENT Communication: ✅
   Container Isolation: ✅
   Memory Safety: ✅
   Process Map Authority: ✅

🔧 Tool Integration Status:
   system_monitor_tool: TESTED
   smart_contracts_tool: TESTED
   fs_tool: TESTED
   feedback_tool: TESTED

Status: CONSTITUTIONAL COMPLIANCE ACHIEVED
```

## Hardware Component Status

### Missing Components
- `espressif/led_strip`: Required for WS2812B LED control
- `joltwallet/littlefs`: Required for filesystem operations

### Current Workaround
- Constitutional tools compile without hardware components
- Functionality simulated via logging and events
- Full constitutional architecture validated
- Ready for hardware component restoration

### Restoration Steps
1. **Restore led_strip component**:
   ```bash
   idf.py add-dependency "espressif/led_strip^2.0.0"
   ```

2. **Restore littlefs component**:
   ```bash
   idf.py add-dependency "joltwallet/littlefs^1.14.8"
   ```

3. **Update tool implementations**:
   - Enable actual LED control in feedback_tool
   - Enable actual filesystem operations in fs_tool

## Testing Recommendations

1. **Start with constitutional testing** (this protocol)
2. **Validate tool integration** via test sequencer
3. **Restore hardware components** when ready
4. **Repeat tests** with real hardware
5. **Validate end-to-end functionality** with complete system

This protocol ensures constitutional architecture validation independent of hardware component availability.