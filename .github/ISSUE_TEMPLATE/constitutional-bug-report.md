---
name: Constitutional Bug Report
about: Report constitutional violations, coupling issues, or container architecture problems
title: "[BUG] Constitutional violation - [BRIEF_DESCRIPTION]"
labels: ["bug", "constitutional-violation", "coupling-issue"]
assignees: ''

---

## Constitutional Violation Report

### **Constitutional Authority Violated**
- **Process Map**: [Which process map authority was violated]
- **Container Principle**: [Which container architecture principle was broken]
- **ESP_EVENT Rule**: [Communication violation details]
- **Coupling Violation**: [Direct coupling detected between components]

### **Bug Description**
[Clear description of the constitutional violation or architectural problem]

### **Constitutional Impact**

#### **🚨 Violation Type**
- [ ] **Direct Coupling**: Tool-to-tool direct function calls detected
- [ ] **Process Map Deviation**: Implementation doesn't match process map authority
- [ ] **ESP_EVENT Bypass**: Communication outside event system
- [ ] **Container Boundary Violation**: Tool accessing another tool's internals
- [ ] **Static Global Usage**: Global state instead of handle-based pattern
- [ ] **HOST Pattern Violation**: Main.c not acting as proper orchestrator

#### **🏗️ Architecture Impact**
- [ ] **Container Isolation Broken**: Tools are coupled
- [ ] **Event Flow Disrupted**: ESP_EVENT coordination affected
- [ ] **Tool Registry Issues**: Registration/discovery problems
- [ ] **Health Check Problems**: Monitoring system affected
- [ ] **Memory Safety Issues**: Buffer overflows, unsafe string handling

### **Constitutional Diagnosis**

#### **Expected Constitutional Behavior**
[What should happen according to process maps and container architecture]

#### **Actual Violation Behavior**
[What is actually happening that violates constitutional principles]

#### **Constitutional Compliance Check**
Run constitutional validation commands to identify specific violations:

```bash
# Full constitutional compliance analysis
/validate_constitutional

# Container isolation verification
/container_status

# Process map compliance check
/map_check "[affected_component]" "[violation_details]"
```

### **Reproduction Steps**
1. [First step]
2. [Second step]
3. [Third step]
4. [Constitutional violation occurs]

### **Constitutional Context**

#### **Environment Information**
- **ESP32 Platform**: [ESP32-C3, etc.]
- **Constitutional Phase**: [Current development phase]
- **HOST Status**: [Main.c orchestrator status]
- **Tool Registry State**: [Number of registered containers]

#### **Affected Components**
- **HOST**: [main.c orchestrator impact]
- **Container(s)**: [Which tool containers are affected]
- **ESP_EVENT**: [Event system impact]
- **Process Map**: [Which process maps are affected]

### **Constitutional Fix Requirements**

#### **🏛️ Constitutional Compliance**
- [ ] Must restore process map authority compliance
- [ ] Must eliminate direct coupling violations
- [ ] Must restore ESP_EVENT-only communication
- [ ] Must maintain container isolation

#### **🏗️ Container Architecture**
- [ ] Restore handle-based pattern if violated
- [ ] Fix tool registry integration
- [ ] Restore proper HOST orchestration
- [ ] Validate container boundary integrity

#### **📡 ESP_EVENT Integration**
- [ ] Replace direct calls with event communication
- [ ] Restore proper event subscription/publishing
- [ ] Fix event data structure issues
- [ ] Validate event flow coordination

### **Constitutional Validation**

#### **Pre-Fix Validation**
- [ ] Constitutional violation confirmed via validation commands
- [ ] Impact assessment completed
- [ ] Process map authority consultation completed

#### **Post-Fix Validation**
- [ ] Constitutional compliance restored
- [ ] Container isolation verified
- [ ] ESP_EVENT communication functional
- [ ] Process map authority followed

### **Constitutional References**
- **Violated Process Map**: `/docs/constitution/process_maps/[XX]_[component]_fsm.mmd`
- **Container Contracts**: `/docs/validation/container_contracts.md`
- **Constitutional Authority**: `/CLAUDE.md`
- **Architecture Patterns**: `/docs/architecture/mcp_patterns_spr.md`

### **Additional Constitutional Context**
[Any additional information that helps understand the constitutional violation]

### **Constitutional Recovery Plan**

#### **Emergency Protocol**
- [ ] **STOP DEVELOPMENT**: Halt all work until constitutional compliance restored
- [ ] **ASSESS VIOLATION**: Understand scope of constitutional damage
- [ ] **PLAN RECOVERY**: Design constitutional compliance restoration
- [ ] **IMPLEMENT FIX**: Restore constitutional architecture
- [ ] **VALIDATE COMPLIANCE**: Confirm constitutional restoration

#### **Prevention Strategy**
- [ ] Enhanced constitutional validation in CI/CD
- [ ] Regular container isolation testing
- [ ] Process map compliance automation
- [ ] Developer constitutional training

---

**Constitutional Emergency Protocol:**
> 🚨 **Constitutional violations must be fixed immediately**
> 
> Process maps are supreme authority. Container isolation is sacred. ESP_EVENT-only communication is mandatory. Direct coupling is forbidden.
> 
> **Recovery steps**: Stop → Assess → Plan → Fix → Validate