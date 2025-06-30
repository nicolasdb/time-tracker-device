# Container Status Command

> _Quick container architecture health check and isolation verification_

## Usage

```bash
/container_status
```

## 🏗️ **Container Architecture Overview**

Quick status check for the containerized architecture pattern:
- **HOST**: `main.c` as Docker-like container orchestrator
- **CONTAINERS**: Tools as isolated IPO processes  
- **CONTRACTS**: Process maps as smart contract bindings
- **FIREWALL**: ESP_EVENT as universal communication bus

---

## 📊 **Container Health Dashboard**

### **Container Isolation Status**

**RFID_TOOL Container**:
```
Status: [ISOLATED/COUPLED/NOT_TESTED]
Input: Hardware SPI only ✅
Output: RFID_EVENTS only ✅
State: Self-contained ✅
Communication: ESP_EVENT only ✅
Process Map: 07_tag_detection_fsm.mmd ✅
```

**PAYLOAD_TOOL Container**:
```
Status: [ISOLATED/COUPLED/NOT_TESTED]
Input: RFID_EVENTS, NTP, SYSTEM_MONITOR ✅
Output: PAYLOAD_EVENTS, FS_EVENTS ✅  
State: Self-contained ✅
Communication: ESP_EVENT only ✅
Process Map: 13_payload_fsm.mmd ✅
```

**HTTP_TOOL Container**:
```
Status: [ISOLATED/COUPLED/NOT_TESTED]
Input: PAYLOAD_EVENTS, Network ✅
Output: HTTP_EVENTS, FS_EVENTS ✅
State: Self-contained ✅
Communication: ESP_EVENT only ✅
Process Map: 14_http_fsm.mmd ✅
```

**FEEDBACK_TOOL Container**:
```
Status: [ISOLATED/COUPLED/NOT_TESTED]
Input: Multiple event types ✅
Output: Hardware LED control ✅
State: Self-contained ✅
Communication: ESP_EVENT only ✅
Process Map: 11_feedback_fsm.mmd ✅
```

### **Communication Bus Status**

**ESP_EVENT Universal Bus**:
```
Registration: [COMPLETE/INCOMPLETE/FAILED]
Event Bases: [REGISTERED/MISSING/INVALID]
Handlers: [ACTIVE/INACTIVE/ERROR]
Data Integrity: [VALID/CORRUPTED/UNKNOWN]
Performance: [OPTIMAL/DEGRADED/CRITICAL]
```

**Container Communication Matrix**:
```
RFID → PAYLOAD: ESP_EVENT ✅
PAYLOAD → HTTP: ESP_EVENT ✅
HTTP → FS: ESP_EVENT ✅
NETWORK → FEEDBACK: ESP_EVENT ✅
All Others: ESP_EVENT Only ✅
```

---

## 🔍 **Quick Container Validation**

### **Isolation Check Commands**

**Direct Coupling Detection**:
```bash
# Quick check for forbidden direct calls
grep -r "payload_tool_" tools/rfid_tool/ 
grep -r "http_tool_" tools/payload_tool/
grep -r "rfid_tool_" tools/http_tool/
# Expected: No results (zero coupling)
```

**Shared State Detection**:
```bash
# Quick check for shared globals
grep -r "extern.*_state" tools/
grep -r "static.*_shared" tools/
# Expected: No results (self-contained state)
```

**Event Communication Verification**:
```bash
# Verify ESP_EVENT usage
grep -c "esp_event_post" tools/*/
grep -c "esp_event_handler_register" tools/*/
# Expected: Non-zero results (event-based communication)
```

### **Container Boundary Validation**

**Physical Boundaries**:
- ✅ Each container in separate `/tools/` directory
- ✅ No cross-container header includes
- ✅ Independent CMakeLists.txt files
- ✅ Self-contained dependencies

**Logical Boundaries**:
- ✅ No shared memory between containers
- ✅ ESP_EVENT only communication
- ✅ Independent error handling
- ✅ Self-managed lifecycle

**Interface Boundaries**:
- ✅ Defined input contracts
- ✅ Defined output contracts  
- ✅ Stable event structures
- ✅ Backward compatibility

---

## 🎯 **Container Status Indicators**

### **✅ HEALTHY CONTAINER**
```
Isolation: PERFECT
Boundaries: ENFORCED
Communication: ESP_EVENT_ONLY
Contracts: RESPECTED
Process Map: COMPLIANT
```

### **⚠️ CONTAINER CONCERNS**
```
Isolation: MINOR_VIOLATIONS
Boundaries: EDGE_CASES
Communication: MOSTLY_ESP_EVENT
Contracts: COMPATIBILITY_ISSUES
Process Map: MINOR_DEVIATIONS
```

### **🚨 CONTAINER CRISIS**
```
Isolation: MAJOR_VIOLATIONS
Boundaries: COMPROMISED
Communication: DIRECT_COUPLING
Contracts: BROKEN
Process Map: NON_COMPLIANT
```

---

## 📈 **Container Metrics**

### **Isolation Metrics**
- **Coupling Score**: 0/10 (perfect isolation)
- **State Isolation**: 100% self-contained
- **Communication Purity**: 100% ESP_EVENT
- **Boundary Integrity**: 100% enforced

### **Performance Metrics**
- **Event Latency**: <10ms typical
- **Memory Isolation**: No leaks between containers
- **CPU Isolation**: Independent processing
- **Resource Usage**: Per-container tracking

### **Quality Metrics**
- **Contract Compliance**: 100% adherence
- **Process Map Adherence**: 100% compliance
- **Test Coverage**: Per-container validation
- **Documentation**: Complete interface specs

---

## 🔧 **Container Troubleshooting**

### **Common Issues**

**Direct Coupling Detected**:
```
Problem: Container calling other container directly
Solution: Replace with ESP_EVENT communication
Command: /validate_constitutional to identify violations
```

**Shared State Found**:
```
Problem: Containers sharing global variables
Solution: Move state inside container handles
Command: Refactor to self-contained state management
```

**Interface Contract Violation**:
```
Problem: Container input/output doesn't match contract
Solution: Review and update container implementation
Command: Check docs/validation/container_contracts.md
```

**Process Map Non-Compliance**:
```
Problem: Container behavior doesn't follow process map
Solution: Align implementation with constitutional authority
Command: /map_check "container" "current_implementation"
```

---

## 🔄 **Integration with Ecosystem Commands**

### **Container-Aware Workflow**
```bash
# 1. Full context with container awareness
/ecosystem_context       # System understanding
/container_status        # Container health check

# 2. Validate before changes
/map_check "container" "proposed_change"
/validate_constitutional # Full validation

# 3. Monitor during development  
/container_status        # Quick health checks

# 4. Track container impact
/save_progress "phase" STATUS "container isolation maintained"
```

### **Container Status in Other Commands**

**Enhanced `/phase_status`**:
- Now includes container isolation status
- Reports container health metrics
- Tracks container-related issues

**Enhanced `/save_progress`**:
- Documents container compliance status
- Tracks container architecture evolution
- Records container validation results

---

## 📚 **Container Documentation References**

### **Core Container Documents**
- `docs/validation/container_contracts.md` - Interface specifications
- `docs/validation/compliance_checklist.md` - Validation requirements
- `docs/constitution/process_maps/` - Container behavior authority

### **Container Status Files**
- `docs/project/validation_status.md` - Container compliance tracking
- `docs/project/session_context.md` - Container architecture context

---

## 🎯 **Container Architecture Benefits**

### **Development Benefits**
- **Safe Iteration**: Change one container without affecting others
- **Clear Debugging**: Container boundaries isolate issues
- **Parallel Development**: Containers can be developed independently
- **Predictable Integration**: Interface contracts prevent surprises

### **Maintenance Benefits**
- **Isolated Fixes**: Bug fixes don't cascade to other containers
- **Independent Testing**: Each container can be tested in isolation
- **Clear Ownership**: Each container has defined responsibilities
- **Evolution Safety**: Containers can evolve independently

### **Architectural Benefits**
- **Scalability**: Add new containers without affecting existing ones
- **Flexibility**: Replace containers without system-wide changes
- **Reliability**: Container failures don't cascade
- **Modularity**: Clear separation of concerns

---

## 🚀 **Quick Actions**

### **Immediate Container Health Check**
```bash
/container_status        # This command - quick overview
/validate_constitutional # Full constitutional validation
```

### **Container Issue Investigation**
```bash
# Check specific container isolation
grep -r "container_name" tools/other_container/

# Verify event communication
grep -r "esp_event" tools/container_name/

# Review interface contracts
cat docs/validation/container_contracts.md
```

### **Container Recovery Actions**
```bash
# If coupling detected
/validate_constitutional  # Identify all violations
# Refactor to ESP_EVENT communication
# Re-run validation

# If process map violation
/map_check "container" "current_state"
# Align with constitutional authority
# Test compliance
```

---

*This command provides rapid container architecture health assessment integrated with the ecosystem-aware command structure.*