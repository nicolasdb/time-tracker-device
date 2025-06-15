# MCP-Inspired Embedded Architecture Template

This template codifies the patterns proven successful in the ESP32-C3 time tracker project for reuse in other embedded projects.

## Project Structure Template

```
project_root/
├── CLAUDE.md                     # Claude Code context (copy from template)
├── main/                         # Pure orchestrator (no business logic)
│   ├── main.c                    # Tool initialization and event routing
│   └── CMakeLists.txt            # Main component registration
├── tools/                        # Self-contained MCP tools
│   ├── feedback_tool/            # Example: LED status management
│   │   ├── include/feedback_tool.h
│   │   ├── feedback_tool.c
│   │   ├── CMakeLists.txt        # Self-contained build
│   │   ├── embedded_component/   # All dependencies embedded
│   │   └── Kconfig              # Tool configuration
│   └── [other_tools]/           # Additional project-specific tools
├── docs/                        # Architecture documentation
│   ├── architecture_insights.md # Technical patterns and lessons
│   ├── phase_reports.md         # Hardware validation logs
│   └── mcp_embedded_template.md # This template
├── platformio.ini               # Build configuration
├── partitions.csv              # Flash partition table
└── CMakeLists.txt              # ESP-IDF project root
```

## Universal Tool Interface (Copy to all projects)

```c
// tool_interface.h - Universal MCP tool interface for embedded
#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include <stdint.h>
#include <stdbool.h>

// Universal tool handle (opaque pointer)
typedef void* tool_handle_t;

// Universal tool configuration structure
typedef struct {
    const char* tool_id;
    const char* version;
    const char* description;
    uint32_t capabilities;           // Bitmask enumeration
    bool auto_start;
    uint32_t event_queue_size;
    uint32_t task_stack_size;
    // Tool-specific config follows...
} tool_config_t;

// Universal tool status structure
typedef struct {
    bool is_initialized;
    bool is_active;
    uint32_t uptime_ms;
    uint32_t capabilities;
    uint32_t error_count;
    // Tool-specific status follows...
} tool_status_t;

// Universal tool registry entry
typedef struct {
    const char* tool_id;
    const char* version;
    const char* description;
    uint32_t capabilities;
    tool_handle_t (*init_func)(const tool_config_t *config);
    esp_err_t (*deinit_func)(tool_handle_t handle);
} tool_registry_t;

// Universal tool lifecycle (implement in every tool)
tool_handle_t tool_init(const tool_config_t *config);
esp_err_t tool_deinit(tool_handle_t handle);
uint32_t tool_get_capabilities(tool_handle_t handle);
esp_err_t tool_get_status(tool_handle_t handle, tool_status_t *status);
const tool_registry_t* tool_get_registry_entry(void);

// Universal event-driven communication pattern
// ESP_EVENT_DEFINE_BASE(TOOL_EVENTS);  // In .c file
// ESP_EVENT_DECLARE_BASE(TOOL_EVENTS); // In .h file
```

## Tool Creation Checklist

### 1. Tool Directory Structure
```
tools/my_tool/
├── include/my_tool.h             # Public interface
├── my_tool.c                     # Implementation
├── CMakeLists.txt                # Self-contained build
├── embedded_dependency/          # All dependencies here
│   ├── src/                      # Dependency source
│   ├── include/                  # Public headers
│   └── internal/                 # Private headers
└── Kconfig                       # Tool configuration
```

### 2. Build System Integration
```cmake
# CMakeLists.txt - Self-contained tool
set(dependency_srcs
    "embedded_dependency/src/file1.c"
    "embedded_dependency/src/file2.c"
)

idf_component_register(
    SRCS "my_tool.c" ${dependency_srcs}
    INCLUDE_DIRS "include" "embedded_dependency/include"
    PRIV_INCLUDE_DIRS "embedded_dependency/internal"
    REQUIRES 
        freertos 
        log 
        esp_event
    PRIV_REQUIRES
        esp_common
)
```

```ini
# platformio.ini - Private include resolution
[env:target]
build_flags = 
    -I tools/my_tool/embedded_dependency/internal

lib_extra_dirs = 
    tools
    managed_components
```

### 3. Implementation Pattern
```c
// my_tool.c - Universal implementation pattern
#include "my_tool.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "MY_TOOL";

// Tool event base
ESP_EVENT_DEFINE_BASE(MY_TOOL_EVENTS);

// Tool context (handle-based, no static globals)
struct my_tool_context {
    my_tool_config_t config;
    my_tool_capabilities_t capabilities;
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    
    // Tool-specific state
    // ... your tool state here ...
    
    // Thread safety
    SemaphoreHandle_t state_mutex;
};

// Universal interface implementation
my_tool_handle_t my_tool_init(const my_tool_config_t *config) {
    // Allocate context, initialize, return handle
}

esp_err_t my_tool_deinit(my_tool_handle_t handle) {
    // Clean shutdown, free resources
}

my_tool_capabilities_t my_tool_get_capabilities(my_tool_handle_t handle) {
    // Return capabilities bitmask
}

esp_err_t my_tool_get_status(my_tool_handle_t handle, my_tool_status_t *status) {
    // Fill status structure
}

const my_tool_registry_t* my_tool_get_registry_entry(void) {
    // Return tool metadata
}

// Event publishing helper
static esp_err_t publish_my_tool_event(struct my_tool_context *ctx, my_tool_event_type_t type, void* data) {
    my_tool_event_t event = {.type = type};
    if (data) {
        memcpy(&event, data, sizeof(my_tool_event_t));
    }
    return esp_event_post(MY_TOOL_EVENTS, type, &event, sizeof(event), 0);
}
```

## Main.c Orchestrator Pattern

```c
// main.c - Pure orchestrator (no business logic)
#include "my_tool.h"
#include "other_tool.h"

static my_tool_handle_t my_tool = NULL;
static other_tool_handle_t other_tool = NULL;

void app_main(void) {
    ESP_LOGI(TAG, "Starting MCP-inspired tool orchestrator");
    
    // 1. Initialize tools with MCP patterns
    my_tool_config_t my_config = my_tool_create_default_config();
    my_tool = my_tool_init(&my_config);
    
    other_tool_config_t other_config = other_tool_create_default_config();
    other_tool = other_tool_init(&other_config);
    
    // 2. Tool discovery and capabilities
    ESP_LOGI(TAG, "My Tool Registry: %s (caps: 0x%02X)", 
             my_tool_get_registry_entry()->tool_id,
             my_tool_get_capabilities(my_tool));
    
    // 3. Demonstrate tool patterns (replace with your logic)
    demonstrate_tool_patterns();
    
    // 4. Clean shutdown
    my_tool_deinit(my_tool);
    other_tool_deinit(other_tool);
    
    ESP_LOGI(TAG, "MCP tool architecture demonstration complete");
}
```

## Memory Management Protocol

### Document Updates After Each Phase
1. **CLAUDE.md**: Update current state and next steps
2. **docs/architecture_insights.md**: Add technical insights
3. **docs/phase_reports.md**: Add validation logs
4. **project_specific_mission.md**: Mark phase complete

### Global Standards Checklist
- [ ] Handle-based design (no static globals)
- [ ] Event-driven communication (ESP event system)
- [ ] Self-contained tools (embedded dependencies)
- [ ] Build system integration (PlatformIO + ESP-IDF)
- [ ] Tool registry with capabilities discovery
- [ ] Clean lifecycle management (init/deinit)
- [ ] Hardware validation on target platform

## Deployment Strategy

### Tool Portability
```bash
# Each tool is a complete package
tar -czf my_tool_v1.0.0.tar.gz tools/my_tool/

# Can be extracted into any ESP32 project
tar -xzf my_tool_v1.0.0.tar.gz -C other_project/tools/
```

### Integration Steps
1. Copy tool directory to new project
2. Add tool to platformio.ini lib_extra_dirs
3. Add private includes to build_flags if needed
4. Initialize tool in main.c
5. Subscribe to tool events as needed

## Proven Patterns Summary

✅ **Self-Contained Tools**: All dependencies embedded  
✅ **Handle-Based Design**: No static globals, proper encapsulation  
✅ **Event-Driven Communication**: No direct coupling between tools  
✅ **Tool Registry**: Metadata and capabilities discovery  
✅ **Build System Mastery**: PlatformIO + ESP-IDF integration  
✅ **Hardware Validation**: Real hardware testing required  
✅ **Memory Management**: Explicit document update protocol  

This template enables rapid development of clean, maintainable embedded systems using MCP-inspired patterns proven on ESP32 hardware.