---
name: Constitutional Tool Integration
about: Integrate a tool container with constitutional compliance validation
title: "[TOOL] Integrate [TOOL_NAME] with constitutional validation"
labels: ["constitutional", "tool-integration", "container-architecture"]
assignees: ''

---

## Constitutional Tool Integration Request

### **Constitutional Authority**
- **Process Map**: `/docs/constitution/process_maps/[XX]_[tool_name]_fsm.mmd`
- **Container Pattern**: HOST ↔ ESP_EVENT ↔ CONTAINER
- **Constitutional Requirement**: Zero coupling, event-driven only

### **Tool Information**
- **Tool Name**: [e.g., fs_tool, feedback_tool, rfid_tool]
- **Process Map Authority**: [XX_tool_name_fsm.mmd]
- **Container Interface**: [Brief description of IPO process]
- **ESP_EVENT Integration**: [Event types published/subscribed]

### **Constitutional Compliance Checklist**

#### **🏗️ Container Architecture**
- [ ] Tool implements handle-based interface pattern
- [ ] Zero static globals, all context in handle
- [ ] Self-contained with no external dependencies
- [ ] Implements IPO (Input-Process-Output) pattern

#### **📡 ESP_EVENT Communication**
- [ ] All communication via ESP_EVENT only
- [ ] No direct function calls to other tools
- [ ] Event subscription/publishing properly implemented
- [ ] Event data structures defined and documented

#### **📋 Process Map Compliance**
- [ ] Implementation matches process map FSM exactly
- [ ] State transitions follow constitutional authority
- [ ] Error handling matches process map specifications
- [ ] Timing requirements met per process map

#### **🔒 Container Isolation**
- [ ] Tool runs independently without other containers
- [ ] Graceful handling of missing dependencies
- [ ] No coupling violations detected
- [ ] Tool registry integration functional

#### **🧪 Integration Testing**
- [ ] Tool compiles with clean HOST
- [ ] ESP_EVENT coordination functional
- [ ] Health check reports proper status
- [ ] Dashboard integration working
- [ ] No memory leaks detected

### **Constitutional Validation Commands**
Before submitting, run these validation commands:

```bash
# Constitutional compliance check
/validate_constitutional

# Container isolation test
/container_status

# Process map validation
/map_check "[tool_name]" "[integration_change]"
```

### **Implementation Plan**

#### **Phase 1: Minimal Container**
- [ ] Create basic tool structure with handle pattern
- [ ] Implement ESP_EVENT subscription/publishing
- [ ] Add to tool registry with constitutional interface
- [ ] Validate container isolation

#### **Phase 2: Process Map Implementation**
- [ ] Implement FSM according to process map authority
- [ ] Add all required state transitions
- [ ] Implement error handling per constitutional requirements
- [ ] Validate timing and sequencing

#### **Phase 3: Integration Validation**
- [ ] Test with existing containers
- [ ] Validate zero coupling principle
- [ ] Confirm dashboard integration
- [ ] Run full constitutional compliance validation

### **Constitutional References**
- **Master Architecture**: `/docs/constitution/process_maps/01_device_master_fsm.mmd`
- **Container Contracts**: `/docs/validation/container_contracts.md`
- **Development Authority**: `/CLAUDE.md`
- **Current State**: `/docs/project/current_state_spr.md`

### **Success Criteria**
- ✅ Process map compliance validated
- ✅ ESP_EVENT isolation confirmed
- ✅ Container pattern implemented
- ✅ Zero coupling violations
- ✅ Integration tests passing
- ✅ Constitutional validation commands pass

### **Constitutional Notes**
> **Process Maps are Constitutional Authority**: Implementation must follow process maps exactly. Any deviations require process map updates first.
> 
> **Container Isolation is Sacred**: Tools must communicate only via ESP_EVENT. Direct coupling is a constitutional violation.
> 
> **Smart Contracts Pattern**: Each tool is a container with clear input/output contracts defined by process maps.

---

**Related Constitutional Documents:**
- Process Map Authority: `/docs/constitution/process_maps/`
- Container Architecture: `/docs/architecture/mcp_patterns_spr.md`
- Constitutional Commands: `/.claude/commands/`