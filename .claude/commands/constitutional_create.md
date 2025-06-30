# Constitutional Create Command

**Usage**: `/constitutional_create`

**Purpose**: Implement constitutional architecture with built-in compliance validation

## Command Description

This command ensures constitutional compliance during the implementation phase by:
1. Validating constitutional requirements before implementation
2. Implementing with constitutional patterns (handle-based, ESP_EVENT)
3. Running constitutional compliance checks during development
4. Ensuring container isolation and process map compliance

## Constitutional Implementation Workflow

### Phase 1: Pre-Implementation Constitutional Check
```bash
# Validate constitutional readiness
- Process map authority consultation
- Container architecture compliance check
- ESP_EVENT communication pattern verification
- Constitutional gate requirements review
```

### Phase 2: Constitutional Implementation
```bash
# Implement with constitutional patterns
- Handle-based design pattern
- ESP_EVENT-only communication
- Zero coupling between containers
- Process map FSM compliance
```

### Phase 3: Constitutional Validation During Development
```bash
# Continuous constitutional compliance
- Container isolation verification
- ESP_EVENT communication validation
- Memory safety checks (snprintf, no globals)
- Process map compliance validation
```

## Constitutional Implementation Checklist

### 🏛️ Constitutional Authority
- [ ] **Process Map Compliance**: Implementation follows FSM exactly
- [ ] **State Transitions**: Match constitutional process map authority
- [ ] **Error Handling**: Follows constitutional specifications
- [ ] **Timing Requirements**: Met per process map authority

### 🏗️ Container Architecture
- [ ] **Handle-Based Pattern**: No static globals, context in handle
- [ ] **Self-Contained**: No external dependencies outside ESP_EVENT
- [ ] **IPO Process**: Clear Input-Process-Output pattern
- [ ] **Container Isolation**: Zero coupling with other tools

### 📡 ESP_EVENT Integration
- [ ] **Event-Only Communication**: No direct function calls
- [ ] **Event Subscription**: Proper event handling implementation
- [ ] **Event Publishing**: Constitutional event broadcasting
- [ ] **Event Data Structures**: Properly defined and documented

### 🔒 Memory Safety (Constitutional Requirement)
- [ ] **snprintf Usage**: Safe string handling (not strncpy)
- [ ] **Buffer Management**: Adequate buffer sizes (1KB+ for dashboards)
- [ ] **Handle Context**: All state in handle, no static globals
- [ ] **Dynamic Allocation**: Proper memory management

## Constitutional Code Patterns

### Handle-Based Pattern (Constitutional Requirement)
```c
// Constitutional tool structure
typedef struct {
    bool is_initialized;
    // Tool-specific context
    constitutional_capabilities_t capabilities;
} constitutional_tool_context_t;

// Constitutional tool handle
typedef struct constitutional_tool* constitutional_tool_handle_t;
```

### ESP_EVENT Communication (Constitutional Requirement)
```c
// Constitutional event publishing
esp_err_t constitutional_tool_publish_event(
    constitutional_tool_handle_t handle,
    const char* event_id,
    void* event_data,
    size_t data_size
) {
    return esp_event_post(CONSTITUTIONAL_EVENTS, 
                         EVENT_ID, 
                         event_data, 
                         data_size, 
                         portMAX_DELAY);
}
```

### Constitutional Validation Integration
```c
// Constitutional compliance validation
esp_err_t constitutional_tool_validate_compliance(
    constitutional_tool_handle_t handle
) {
    // 1. Process map compliance check
    // 2. Container isolation verification
    // 3. ESP_EVENT communication validation
    // 4. Memory safety verification
}
```

## Constitutional Development Process

### Step 1: Constitutional Foundation
```c
// Create minimal constitutional tool structure
// Implement handle-based pattern
// Set up ESP_EVENT integration
// Validate container isolation
```

### Step 2: Process Map Implementation
```c
// Implement FSM according to process map authority
// Add state transitions per constitutional requirements
// Implement error handling per process map specifications
// Validate timing and sequencing requirements
```

### Step 3: Constitutional Integration
```c
// Integrate with HOST orchestrator
// Register with tool registry using constitutional interface
// Validate ESP_EVENT communication
// Run constitutional compliance validation
```

## Constitutional Validation Commands Integration

During implementation, run these commands regularly:

```bash
# Constitutional compliance validation
/validate_constitutional

# Container isolation verification
/container_status

# Process map compliance check
/map_check "[tool_name]" "[implementation_details]"

# Constitutional gate execution
/smart_contract_validate "[tool]" "[implementation]"
```

## Constitutional Implementation Success Criteria

### Technical Achievement
- ✅ Compiles without warnings or errors
- ✅ Implements handle-based pattern correctly
- ✅ ESP_EVENT communication functional
- ✅ Memory safety maintained (snprintf, proper buffers)

### Constitutional Achievement
- ✅ Process map compliance validated
- ✅ Container isolation verified
- ✅ Zero coupling confirmed
- ✅ Constitutional gates pass

### Integration Achievement
- ✅ HOST orchestrator coordination functional
- ✅ Tool registry integration operational
- ✅ ESP_EVENT coordination working
- ✅ Constitutional validation commands pass

## Error Recovery Protocol

If constitutional violations are detected during implementation:

### 🚨 Constitutional Emergency Protocol
```
STOP IMPLEMENTATION IMMEDIATELY
1. Identify constitutional violation type
2. Consult process map authority
3. Plan constitutional compliance restoration
4. Implement constitutional fix
5. Re-validate constitutional compliance
```

---

**Constitutional Implementation Philosophy**: 
> Every line of code must respect constitutional authority. Process maps are supreme. Container isolation is sacred. ESP_EVENT-only communication is mandatory.

**Constitutional Quality Gate**:
> No implementation is complete until it passes all constitutional validation commands and maintains constitutional compliance.