# Constitutional Smart Contracts Framework Guide

> **IMPORTANT**: These are **NOT blockchain smart contracts**. They are constitutional validation contracts that enforce architectural integrity through process map compliance.

## Table of Contents
1. [What Are Constitutional Smart Contracts?](#what-are-constitutional-smart-contracts)
2. [Contract Types](#contract-types)
3. [Using the Framework](#using-the-framework)
4. [Editing and Creating Contracts](#editing-and-creating-contracts)
5. [Validation Commands](#validation-commands)
6. [Emergency Protocols](#emergency-protocols)
7. [Best Practices](#best-practices)

## What Are Constitutional Smart Contracts?

Constitutional Smart Contracts are **process map compliance validators** that automatically enforce architectural integrity:

```
Constitutional Smart Contracts = Process Map Authority + Automated Validation + Architectural Gates
```

### Key Principles
- **Process Maps as Contracts**: FSM diagrams become executable validation rules
- **Constitutional Gates**: Automated compliance checkpoints
- **Container Isolation Enforcement**: Zero-coupling validation
- **ESP_EVENT Communication Contracts**: Event-driven architecture validation

### vs. Blockchain Smart Contracts

| Constitutional Smart Contracts | Blockchain Smart Contracts |
|---|---|
| Process map validation | Cryptocurrency transactions |
| Architectural compliance | Decentralized execution |
| ESP32 embedded system | Ethereum/blockchain |
| Constitutional authority | Cryptographic consensus |
| Container isolation | Token transfers |
| ESP_EVENT communication | Gas fees |

## Contract Types

### 🏗️ Container Contracts
Validate tool isolation and boundaries:
- **Zero Coupling**: No direct tool-to-tool function calls
- **Handle-Based Pattern**: Context stored in handles, no static globals
- **Self-Contained**: Each tool operates independently
- **ESP_EVENT Only**: Communication via event system only

**Example Contract**:
```c
// RFID Tool Container Contract
const constitutional_contract_t rfid_container_contract = {
    .process_map_id = "07_tag_detection_fsm",
    .tool_name = "rfid_tool",
    .contract_type = CONTRACT_TYPE_CONTAINER,
    .validation_level = VALIDATION_STRICT,
    .is_active = true
};
```

### 📋 Process Contracts
Validate FSM compliance per process maps:
- **State Transitions**: Must match process map diagrams exactly
- **Timing Requirements**: Constitutional timing specifications
- **Error Handling**: Follow process map error paths
- **Sequence Validation**: Constitutional sequence compliance

**Example Contract**:
```c
// Feedback Tool Process Contract
const constitutional_contract_t feedback_process_contract = {
    .process_map_id = "11_feedback_fsm",
    .tool_name = "feedback_tool",
    .contract_type = CONTRACT_TYPE_PROCESS,
    .validation_level = VALIDATION_STRICT,
    .is_active = true
};
```

### 📡 Communication Contracts
Enforce ESP_EVENT-only communication:
- **No Direct Calls**: No function calls between tools
- **Event Publishing**: Proper event broadcasting patterns
- **Event Subscription**: Correct event handling implementation
- **Data Structures**: Valid event data integrity

**Example Contract**:
```c
// Universal Communication Contract
const constitutional_contract_t communication_contract = {
    .process_map_id = "esp_event_communication",
    .tool_name = "all_tools",
    .contract_type = CONTRACT_TYPE_COMMUNICATION,
    .validation_level = VALIDATION_STRICT,
    .is_active = true
};
```

### 🔒 Memory Contracts
Enforce memory safety patterns:
- **String Safety**: snprintf usage only (no strncpy)
- **Buffer Safety**: Adequate buffer sizes (1KB+ for dashboards)
- **Handle-Based**: All context in handles
- **Dynamic Allocation**: Proper memory management

**Example Contract**:
```c
// Memory Safety Contract
const constitutional_contract_t memory_contract = {
    .process_map_id = "snprintf_usage",
    .tool_name = "all_tools",
    .contract_type = CONTRACT_TYPE_MEMORY,
    .validation_level = VALIDATION_STRICT,
    .is_active = true
};
```

## Using the Framework

### Basic Validation
```c
// Initialize smart contracts framework
smart_contracts_tool_handle_t contracts_handle = NULL;
esp_err_t ret = smart_contracts_tool_init(&contracts_handle);

// Validate a tool
constitutional_validation_request_t request = {0};
snprintf(request.tool_name, sizeof(request.tool_name), "%s", "rfid_tool");
request.level = VALIDATION_STRICT;

ret = smart_contracts_tool_validate_constitutional_compliance(
    contracts_handle, "rfid_tool", &request);

if (request.validation_passed) {
    ESP_LOGI("CONSTITUTIONAL", "✅ Tool passes constitutional validation");
} else {
    ESP_LOGE("CONSTITUTIONAL", "❌ Constitutional violations: %s", request.report);
}
```

### Dashboard Generation
```c
// Generate constitutional compliance dashboard
char dashboard[1024]; // Constitutional requirement: 1KB+ buffer
esp_err_t ret = smart_contracts_tool_generate_dashboard(
    contracts_handle, dashboard, sizeof(dashboard));

if (ret == ESP_OK) {
    printf("%s\n", dashboard);
}
```

### Contract Status Monitoring
```c
// Get contract status
constitutional_contract_status_t status;
esp_err_t ret = smart_contracts_tool_get_contract_status(contracts_handle, &status);

ESP_LOGI("CONTRACTS", "Active: %zu/%zu, Compliance: %.1f%%", 
         status.active_contracts, status.total_contracts, status.compliance_rate);
```

## Editing and Creating Contracts

### Location of Contract Definitions
Contracts are defined in:
```
tools/smart_contracts_tool/smart_contracts_tool.c
```

Look for the `constitutional_contracts[]` array:

```c
static const constitutional_contract_t constitutional_contracts[] = {
    // Container Contracts
    {"01_device_master_fsm", "main", CONTRACT_TYPE_CONTAINER, VALIDATION_STRICT, true},
    {"07_tag_detection_fsm", "rfid_tool", CONTRACT_TYPE_CONTAINER, VALIDATION_STRICT, true},
    // ... more contracts
};
```

### Adding a New Contract

1. **Identify the Requirements**:
   - Which process map does this enforce?
   - What tool does it apply to?
   - What type of validation is needed?

2. **Add to Contract Array**:
```c
// Add new contract entry
{"15_new_tool_fsm", "new_tool", CONTRACT_TYPE_PROCESS, VALIDATION_STRICT, true},
```

3. **Update Contract Count**:
The count is automatically calculated, but verify:
```c
#define CONSTITUTIONAL_CONTRACT_COUNT (sizeof(constitutional_contracts) / sizeof(constitutional_contracts[0]))
```

### Modifying Validation Logic

#### Container Validation
Edit `validate_container_contract()` function:
```c
static esp_err_t validate_container_contract(smart_contracts_tool_handle_t handle, const char* tool_name) {
    ESP_LOGI(TAG, "Validating container contract for: %s", tool_name);
    
    // Add your validation logic here:
    // 1. Check for static globals (constitutional violation)
    // 2. Verify ESP_EVENT-only communication
    // 3. Validate handle-based pattern
    // 4. Check zero coupling
    
    handle->stats.container_validations++;
    return ESP_OK;  // Return ESP_FAIL for violations
}
```

#### Process Map Validation
Edit `validate_process_contract()` function:
```c
static esp_err_t validate_process_contract(smart_contracts_tool_handle_t handle, const char* process_map_id) {
    ESP_LOGI(TAG, "Validating process contract: %s", process_map_id);
    
    // Add your validation logic here:
    // 1. Load process map authority
    // 2. Verify state transitions
    // 3. Check timing requirements
    // 4. Validate error handling paths
    
    handle->stats.process_validations++;
    return ESP_OK;  // Return ESP_FAIL for violations
}
```

#### Communication Validation
Edit `validate_communication_contract()` function:
```c
static esp_err_t validate_communication_contract(smart_contracts_tool_handle_t handle) {
    ESP_LOGI(TAG, "Validating ESP_EVENT communication contracts");
    
    // Add your validation logic here:
    // 1. Check for direct function calls (violation)
    // 2. Verify event publishing patterns
    // 3. Validate event subscription handling
    // 4. Check event data structures
    
    handle->stats.communication_validations++;
    return ESP_OK;  // Return ESP_FAIL for violations
}
```

#### Memory Safety Validation
Edit `validate_memory_contract()` function:
```c
static esp_err_t validate_memory_contract(smart_contracts_tool_handle_t handle) {
    ESP_LOGI(TAG, "Validating memory safety contracts");
    
    // Add your validation logic here:
    // 1. Check snprintf usage (no strncpy)
    // 2. Verify adequate buffer sizes
    // 3. Validate handle-based patterns
    // 4. Check dynamic allocation patterns
    
    handle->stats.memory_validations++;
    return ESP_OK;  // Return ESP_FAIL for violations
}
```

## Validation Commands

### Claude Code Commands

#### `/smart_contract_validate`
Execute constitutional validation contracts:
```bash
/smart_contract_validate "tool_name" "validation_level"
```

#### `/constitutional_test`
Comprehensive constitutional testing:
```bash
/constitutional_test
```

#### `/constitutional_create`
Implement with constitutional patterns:
```bash
/constitutional_create
```

#### `/constitutional_deploy`
Deploy with constitutional validation:
```bash
/constitutional_deploy
```

### Programmatic Validation

#### Quick Validation Macro
```c
// Validate tool compliance with error handling
CONSTITUTIONAL_VALIDATE(contracts_handle, "tool_name");
```

#### Emergency Protocol Macro
```c
// Trigger constitutional emergency protocol
CONSTITUTIONAL_EMERGENCY("Critical violation description");
```

#### Success Declaration Macro
```c
// Declare constitutional compliance
CONSTITUTIONAL_SUCCESS("tool_name");
```

## Emergency Protocols

### 🚨 Constitutional Violation Detected

When violations are detected:

```
CONSTITUTIONAL EMERGENCY PROTOCOL ACTIVATED
1. Halt all development immediately
2. Analyze violation type and scope
3. Consult constitutional authority (process maps)
4. Plan constitutional compliance restoration
5. Implement constitutional fixes
6. Re-validate full constitutional compliance
7. Update constitutional documentation
```

### Emergency Recovery Commands
```bash
# Emergency constitutional recovery sequence
/validate_constitutional     # Identify all violations
/container_status           # Check container integrity
/map_check                  # Verify process map compliance
# Fix constitutional violations
/smart_contract_validate    # Re-validate compliance
/constitutional_test        # Full constitutional testing
```

### Violation Types and Responses

#### Container Violation
- **Symptom**: Direct function calls between tools
- **Response**: Refactor to ESP_EVENT-only communication
- **Validation**: Re-run container contract validation

#### Process Map Violation
- **Symptom**: State transitions don't match FSM
- **Response**: Update implementation to match process map
- **Validation**: Re-run process contract validation

#### Communication Violation
- **Symptom**: Non-ESP_EVENT communication detected
- **Response**: Convert to event-driven patterns
- **Validation**: Re-run communication contract validation

#### Memory Safety Violation
- **Symptom**: strncpy usage or inadequate buffers
- **Response**: Convert to snprintf, increase buffer sizes
- **Validation**: Re-run memory contract validation

## Best Practices

### Constitutional Development Workflow

1. **Pre-Implementation**: Run `/smart_contract_validate` before starting
2. **During Development**: Regular constitutional validation
3. **Pre-Deployment**: Full constitutional test suite
4. **Post-Deployment**: Continuous compliance monitoring

### Contract Design Principles

1. **Specificity**: Contracts should target specific process maps
2. **Strictness**: Use `VALIDATION_STRICT` for production contracts
3. **Granularity**: Separate concerns into different contract types
4. **Documentation**: Link contracts to process map authority

### Tool Integration Pattern

```c
// 1. Create tool with constitutional patterns
tool_handle_t tool = create_constitutional_tool(config);

// 2. Register with tool registry
tool_registry_register("tool_name", tool, &interface, true);

// 3. Validate constitutional compliance
CONSTITUTIONAL_VALIDATE(contracts_handle, "tool_name");

// 4. Monitor ongoing compliance
constitutional_contract_status_t status;
smart_contracts_tool_get_contract_status(contracts_handle, &status);
```

### Debugging Constitutional Issues

#### Enable Debug Logging
```c
// Set log level for constitutional debugging
esp_log_level_set("smart_contracts_tool", ESP_LOG_DEBUG);
esp_log_level_set("CONSTITUTIONAL", ESP_LOG_DEBUG);
```

#### Dashboard Analysis
```c
// Generate detailed constitutional dashboard
char dashboard[2048]; // Larger buffer for detailed analysis
smart_contracts_tool_generate_dashboard(contracts_handle, dashboard, sizeof(dashboard));
printf("%s\n", dashboard);
```

#### Violation Event Monitoring
```c
// Register for constitutional violation events
esp_event_handler_register(SMART_CONTRACTS_EVENTS, 
                          SMART_CONTRACTS_EVENT_CONTRACT_VIOLATION,
                          violation_handler, NULL);
```

## Constitutional Philosophy

> **Process maps are supreme authority.**  
> **Container isolation is sacred.**  
> **ESP_EVENT-only communication is mandatory.**

The Constitutional Smart Contracts Framework ensures that every line of code respects these principles through automated validation and enforcement.

---

## Quick Reference

### Key Files
- **Framework**: `tools/smart_contracts_tool/smart_contracts_tool.c`
- **Header**: `tools/smart_contracts_tool/include/smart_contracts_tool.h`
- **Integration**: `main/main.c` (HOST orchestrator)
- **Commands**: `.claude/commands/smart_contract_validate.md`

### Process Maps Authority
- **Master FSM**: `docs/constitution/process_maps/01_device_master_fsm.mmd`
- **Tool FSMs**: `docs/constitution/process_maps/[XX]_[tool]_fsm.mmd`

### Constitutional Documentation
- **Development Authority**: `CLAUDE.md`
- **Architecture Patterns**: `docs/architecture/`
- **Current State**: `docs/project/current_state_spr.md`

**Remember**: Constitutional Smart Contracts enforce architectural integrity, not cryptocurrency transactions!