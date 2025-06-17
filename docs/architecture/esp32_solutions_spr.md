# ESP32 Critical Solutions [SPR]
*Compressed knowledge from 200+ lines of critical fixes*

## String & Memory Safety
```
STRING: snprintf-not-strncpy|buffer-safety|avoid-stringop-truncation
FORMAT: PRIu32-macros|inttypes-include|no-bare-integers|proper-specifiers
MEMORY: 8192-stack|explicit-sizes|no-default-3584|prevent-overflow
ALLOCATION: handle-context|no-static-globals|free-resources|no-leaks
```

## Event System Rules  
```
EVENTS: no-vTaskDelay|no-blocking|immediate-return|event-loop-safety
HANDLERS: fast-execution|no-mutex-timeout|no-long-operations|publish-only
TIMING: event-immediate|task-based-delays|non-blocking-only
CRITICAL: ESP-event-loop|blocking-kills-system|handler-immediate-return
```

## Build System Integration
```
BUILD: private-includes|managed-deps|platformio-flags|component-register
DEPENDENCIES: embedded-within-tool|idf-component-yml|self-contained-archive
RESOLUTION: PRIV_INCLUDE_DIRS|lib-extra-dirs|no-components-dependency
PLATFORMIO: build_flags=-I-tools/tool/internal|lib_extra_dirs=tools
```

## State Management Patterns
```
PRIORITY: clear-conflicts|IDLE-needs-clearing|state-transitions|queue-mgmt
FEEDBACK: priority-queue|automatic-expiration|thread-safe|breathing-patterns
TRANSITIONS: explicit-clearing|BOOTING→IDLE|higher-overrides-lower
```

## Memory & Performance
```
PARTITION: 2MB-app|1536K-LittleFS|expanded-from-1MB|adequate-space
PERFORMANCE: 9.6%-RAM|53.8%-Flash|stable-operation|no-regression
VALIDATION: 60s-continuous|hardware-tested|real-network|multi-location
```

## Common Implementation Fixes
```c
// String handling
snprintf(dest, sizeof(dest), "%s", src);  // NOT strncpy

// Format specifiers  
ESP_LOGI(TAG, "Value: %" PRIu32, uint32_value);  // NOT %d

// Stack configuration
#define MCP_TASK_STACK_SIZE 8192  // NOT default 3584

// Event handlers
static void handler(...) {
    // NO vTaskDelay() - immediate return only
    feedback_tool_set_state_simple(tool, STATE);
}
```

---
*References: Original docs/architecture_insights.md Phase 4.3, 4.4 critical fixes*
