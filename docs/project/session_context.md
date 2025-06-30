# Session Context - Quick Recovery

> **Rapid context loading for Claude sessions with memory/context limitations**

## Current Mission State

### **Primary Objective**
Rebuild `/main/main.c` with constitutional compliance following container architecture pattern:
- **HOST**: main.c as Docker-like container orchestrator
- **CONTAINERS**: Tools as isolated IPO processes  
- **CONTRACTS**: Process maps as smart contracts binding input/output

### **Current Phase**
**PHASE**: MAIN_ORCHESTRATOR_REBUILD_REQUIRED  
**STATUS**: Tools validated and working, main.c deleted, orchestrator missing
**NEXT**: Rebuild main.c as HOST with constitutional compliance

---

## Architecture Decisions Made

### **Container Architecture Pattern** ✅ Confirmed
```
┌─────────────┐    ESP_EVENT    ┌─────────────┐    ESP_EVENT    ┌─────────────┐
│ RFID_TOOL   │◄──────────────►│ PAYLOAD_TOOL│◄──────────────►│ HTTP_TOOL   │
│ (Container) │                │ (Container) │                │ (Container) │
└─────────────┘                └─────────────┘                └─────────────┘
```

### **Constitutional Authority** ✅ Confirmed
- **Process Maps**: Single source of truth for container behavior
- **ESP_EVENT**: Universal communication bus (no direct calls)
- **Container Isolation**: Zero coupling between tool containers
- **Interface Contracts**: Defined input/output specifications

### **Constitutional Enhancement** ✅ Complete
- **Container Implementation Guidelines**: `/docs/validation/container_contracts.md`
- **Constitutional Validation Commands**: `/validate_constitutional`, `/container_status`
- **Enhanced Ecosystem Workflow**: Constitutional validation integrated with existing commands
- **Process Map Authority**: Maintained as supreme authority

---

## Key Problem Being Solved

### **Previous Codebase Issues**
- **State synchronization hell**: Fix RFID → break feedback → break state → break detection
- **Coupling nightmare**: Changes cascade through multiple components
- **Debugging chaos**: Unclear component boundaries and responsibilities

### **Solution: Container Isolation**
- **Fix RFID**: Only touch RFID container (Process Map 07)
- **Fix payload**: Only touch payload container (Process Map 13)
- **Fix HTTP**: Only touch HTTP container (Process Map 14)
- **Zero cascade failures**: ESP_EVENT firewall prevents coupling

---

## Development Workflow

### **Before ANY Code Changes**
1. **Load Context**: Read this file + constitutional docs
2. **Run Validation**: Check compliance checklist
3. **Verify Contracts**: Ensure changes respect container boundaries
4. **Check Process Maps**: Validate against constitutional authority

### **After ANY Code Changes**  
1. **Update Progress**: Modify `current_state_spr.md`
2. **Validate Compliance**: Run architecture tests
3. **Update Session**: Record validation results
4. **Save Context**: Update this file with new state

---

## Critical Files for Context Loading

### **Constitutional Authority (MUST READ)**
```
docs/constitution/process_maps/01_device_master_fsm.mmd  # Main architecture
docs/constitution/process_maps/07_tag_detection_fsm.mmd  # RFID container
docs/constitution/process_maps/13_payload_fsm.mmd        # Payload container  
docs/constitution/process_maps/14_http_fsm.mmd          # HTTP container
```

### **Constitutional Enhancement (REFERENCE)**
```
docs/validation/container_contracts.md     # Implementation guidelines (subordinate to process maps)
.claude/commands/validate_constitutional.md # Constitutional validation command
.claude/commands/container_status.md       # Container health check command
```

### **Project Status (MUST READ)**
```
docs/project/current_state_spr.md         # Current implementation status
docs/project/validation_status.md         # Validation results (when created)
```

---

## Memory/Context Recovery Protocol

### **Session Start Checklist**
- [ ] **Load Constitutional Authority**: Read Process Map 01
- [ ] **Load Container Contracts**: Review tool interfaces  
- [ ] **Load Current State**: Check project status
- [ ] **Load Session Context**: Read this file
- [ ] **Run Compliance Check**: Validate current architecture
- [ ] **Identify Next Steps**: Based on validation results

### **Context Window Refresh**
When Claude context gets full or memory is limited:
1. **Save Progress**: Update all project docs
2. **Record Validation State**: Document what was tested
3. **Update Session Context**: Modify this file
4. **Create Recovery Point**: Known good state snapshot

---

## Current Development Status

### **Completed** ✅
- Architecture pattern defined (container/HOST model)
- Constitutional authority established (process maps remain supreme)
- Validation framework created (implementation guidance)
- Constitutional validation commands integrated
- Event flow repair completed (but needs validation)

### **Current Reality** ✅⚠️
- **Tools individually validated** and working (feedback, wifi, rfid, fs, webhook, system_monitor)
- **Hardware completely validated** (ESP32-C3, WiFi, LittleFS, WS2812B, RC522)
- **main.c deleted** - central orchestrator missing
- **Tool registry missing** - no ESP_EVENT coordination
- **Constitutional compliance** - need to implement Process Map 01 orchestrator

### **Next Steps** 📋
1. **Rebuild main.c** as HOST orchestrator following Process Map 01
2. **Implement tool registry** with ESP_EVENT coordination
3. **Restore operational system** with constitutional compliance
4. **Validate end-to-end flow** (RFID → payload → HTTP)
5. **Test complete system** with hardware validation

---

## Emergency Protocols

### **Constitutional Violation Detected** 🚨
```
STOP DEVELOPMENT IMMEDIATELY
1. Identify the violation type
2. Review constitutional authority (process maps)
3. Refactor to achieve compliance
4. Re-validate architecture integrity
5. Update documentation
```

### **Memory/Context Limitation** 💭
```
SAVE STATE IMMEDIATELY
1. Update current_state_spr.md
2. Update session_context.md (this file)
3. Record validation status
4. Create recovery checkpoint
5. Resume with context loading protocol
```

---

## Key Insights to Remember

### **Why Constitutional Compliance Matters**
- **Prevents architectural debt** that destroys maintainability
- **Enables safe iteration** without cascading breakages
- **Provides clear debugging** through component isolation
- **Future-proofs development** against coupling hell

### **Container Isolation Benefits**
- **Change RFID logic** → Zero impact on payload/HTTP
- **Modify payload format** → Zero impact on RFID/HTTP
- **Fix HTTP retries** → Zero impact on detection/formatting
- **Debug state issues** → Clear component boundaries

---

*This file MUST be updated after every significant development session to maintain context recovery capability.*