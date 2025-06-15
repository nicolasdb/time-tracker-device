# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-C3 based RFID time tracking system using ESP-IDF framework. The device reads RFID/NFC tags and sends placement/removal events to a webhook server. It features WiFi connectivity, web-based configuration, LED feedback, and persistent storage via LittleFS.

## Current State

- **Phase 3A ✅ COMPLETE**: 3-tool MCP architecture validated on hardware
  - feedback_tool (LED states with priority queue)
  - wifi_tool (WiFi connectivity with event publishing)  
  - rfid_tool (RC522 RFID with embedded component)
- **Next**: Phase 3B (webhook_tool) + 3C (fs_tool + partition expansion)
- **Flash Usage**: 82.2% (need partition expansion for full ecosystem)

## Build System & Commands

**IMPORTANT**: Claude should NOT execute build commands directly. The user handles building, flashing, and monitoring through VSCode + PlatformIO GUI.

### User's Role (VSCode + PlatformIO)
- **Building**: User builds via PlatformIO GUI in VSCode
- **Flashing**: User flashes via PlatformIO GUI in VSCode  
- **Monitoring**: User monitors hardware via PlatformIO serial monitor
- **Configuration**: User runs `idf.py menuconfig` when needed

### Claude's Role
- **Code analysis and modifications only**
- **Request user to build/test after changes**
- **Provide guidance on what to test**
- **Never execute build commands directly**

## Architecture Overview

The project follows **MCP-inspired tool-based architecture** with modular design:

### Core Tools Location
- **main/**: Application entry point and pure orchestration logic (no business logic)
- **tools/**: Self-contained MCP-style tools
  - **feedback_tool/**: LED status indicators with priority queue management
  - **wifi_tool/**: WiFi connectivity with event-driven AP/STA modes
  - **rfid_tool/**: RFID/NFC reading with embedded RC522 component
  - *(Future: webhook_tool, fs_tool, ntp_tool)*

### Key Architectural Patterns

1. **Self-Contained Tools**: Each tool contains all dependencies (embedded components)
2. **Handle-Based Design**: No static globals, proper context encapsulation
3. **Event-Driven Communication**: ESP event system for inter-tool coordination
4. **Tool Registry**: Capabilities discovery and metadata management
5. **Priority Queue Feedback**: Visual LED patterns with automatic state management

### Critical Architecture Lessons Learned

**✅ MCP PATTERNS WORK FOR EMBEDDED**
- **Handle-based lifecycle**: `tool_init()` → `tool_operations()` → `tool_deinit()`
- **Event publishing**: Tools publish events, others subscribe (no direct coupling)
- **Capabilities discovery**: Bitmask enumeration for feature detection
- **Tool registry**: Metadata, version, description patterns

**✅ SELF-CONTAINED TOOL DEPLOYMENT**
```bash
# Tools are truly portable
/tools/rfid_tool/
├── include/rfid_tool.h           # MCP interface
├── rfid_tool.c                   # Implementation  
├── CMakeLists.txt                # Self-contained build
├── rc522/                        # **EMBEDDED COMPONENT**
└── Kconfig                       # Configuration

# Single archive deployment
tar -czf rfid_tool_v1.0.0.tar.gz tools/rfid_tool/
```

**✅ PLATFORMIO BUILD MASTERY**
```ini
# platformio.ini - Critical pattern for private includes
[env:esp32c3_mcp]
build_flags = 
    -I tools/rfid_tool/rc522/internal  # Resolves PRIV_INCLUDE_DIRS

lib_extra_dirs = 
    tools                              # Self-contained tools only
    managed_components
```

## Configuration Management

### Compile-Time Configuration (Kconfig)
Access via `idf.py menuconfig`:
- Navigate to "Time Tracker Configuration" for project-specific settings
- Tool-specific configs under individual tool directories
- Critical settings: GPIO pins, capabilities, timeouts

### Runtime Configuration (LittleFS JSON)
Configuration files stored in `/littlefs/`:
- `wifi.json`: Network credentials with multi-SSID support
- `webhook_config.json`: Runtime webhook settings
- `log.json`: Event log for retry mechanism

### Hardware Pin Configuration
Default GPIO assignments (configurable via Kconfig):
- RC522 RFID: SPI interface (MISO=5, MOSI=6, SCK=4, CS=10, RST=9)
- WS2812B LED: Single data pin
- Status indicators: Configurable GPIO

## Important Implementation Details

### Tool Interface Patterns
```c
// Universal MCP tool interface
typedef struct tool_context {
    tool_config_t config;
    tool_capabilities_t capabilities;  // Bitmask enumeration
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    // Tool-specific state...
} tool_context_t;

// Standard lifecycle
tool_handle_t tool_init(const tool_config_t *config);
esp_err_t tool_deinit(tool_handle_t handle);
tool_capabilities_t tool_get_capabilities(tool_handle_t handle);
esp_err_t tool_get_status(tool_handle_t handle, tool_status_t *status);
```

### Event-Driven Communication
```c
// Tools publish events for others to subscribe
ESP_EVENT_DEFINE_BASE(TOOL_EVENTS);

// Example: RFID tool publishes tag detection
rfid_tool_event_t event = {.type = RFID_TOOL_EVENT_TAG_DETECTED};
esp_event_post(RFID_TOOL_EVENTS, RFID_TOOL_EVENT_TAG_DETECTED, &event, sizeof(event), 0);
```

### LED Feedback System

**Visual State Patterns:**
- **Idle**: Blue breathing (4-second cycle) - system ready
- **WiFi Connecting**: Blue blinking (fast) - attempting connection
- **WiFi Connected**: Cyan flash (1 second) - connection established  
- **AP Mode**: Yellow→Blue→Purple (0.3s,0.3s,2.0s) - configuration mode
- **Tag Detected**: Solid green (persistent) - time tracking session active
- **Errors**: Various red patterns for different error types

**Key Implementation Notes:**
- **Priority-based**: Higher priority states override lower ones
- **Session-aware**: Tag detected state is persistent until tag removed
- **Automatic expiration**: Connection states are temporary, return to idle
- **Thread-safe**: Mutex-protected queue for concurrent access

## Development Best Practices

### MCP Tool Creation Checklist
- [ ] Handle-based interface (no static globals)
- [ ] Event publishing for state changes
- [ ] Tool registry entry with metadata
- [ ] Capabilities bitmask enumeration
- [ ] Self-contained dependencies (embedded components)
- [ ] Clean lifecycle management (init/deinit)
- [ ] Status and health reporting

### Build System Patterns
- **Private Includes**: Use `build_flags = -I tools/tool_name/component/internal`
- **Component Registration**: `idf_component_register()` with explicit source lists
- **Dependencies**: All tool dependencies embedded within tool directory
- **No External Coupling**: Tools work without /components directory

### Common Issues & Solutions
- **Private headers not found**: Add to `build_flags` in platformio.ini
- **Tool coupling**: Use events, never direct function calls between tools
- **Memory issues**: Always use handle-based design, no static globals
- **Build conflicts**: Ensure tool dependencies are self-contained

## Memory Management

### Current Status (Phase 3A)
- **RAM**: 8.9% usage (29016/327680 bytes) - Excellent
- **Flash**: 82.2% usage (861584/1048576 bytes) - Need expansion for full ecosystem

### Partition Expansion Plan (Phase 3C)
```csv
# Target partition table for full tool ecosystem
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 2M,        # EXPAND: 1M → 2M
littlefs, data, spiffs,  0x210000, 1.5M,     # EXPAND: 1M → 1.5M
```

## Testing

### Hardware Testing Requirements
- ESP32-C3-DevKitM-1 board
- RC522 RFID module (SPI connection)
- WS2812B LED strip/module
- RFID tags for testing

### Validation Checklist
- [ ] All tools initialize successfully
- [ ] LED patterns work correctly for each state
- [ ] WiFi AP mode activates and provides web interface
- [ ] RFID scanning active and ready for tag detection
- [ ] Tool registry discovers all capabilities
- [ ] Clean shutdown sequence completes without errors
- [ ] Memory usage remains stable during operation

## Memory Management Protocol

### After Each Phase Completion
**Claude: Update these documents in order:**
1. **CLAUDE.md**: Update "Current State" section with new phase status
2. **docs/architecture_insights.md**: Add new technical insights and patterns
3. **docs/phase_reports.md**: Add hardware validation logs and metrics
4. **refactor_mission_brief.md**: Mark phase complete with success criteria

### MCP Architecture Standards (Apply to all projects)
```c
// Universal tool interface pattern for embedded systems
typedef struct {
    tool_config_t config;
    tool_capabilities_t capabilities;  // Bitmask enumeration
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    // Tool-specific state...
} tool_context_t;

// Standard lifecycle: init → operations → deinit
tool_handle_t tool_init(const tool_config_t *config);
esp_err_t tool_deinit(tool_handle_t handle);

// Event-driven communication (no direct coupling)
ESP_EVENT_DEFINE_BASE(TOOL_EVENTS);
esp_event_post(TOOL_EVENTS, event_type, &event, sizeof(event), 0);
```

### Self-Contained Tool Deployment Standard
- **Embed all dependencies** within tool directory
- **Use build_flags** for private includes: `-I tools/tool_name/internal`
- **Handle-based design** - no static globals
- **Event publishing** for all state changes
- **Tool registry** with capabilities discovery

## Next Steps

### Phase 3B: webhook_tool
- Transform webhook_manager using proven MCP patterns
- Subscribe to RFID_TOOL_EVENTS for automatic webhook transmission
- Break wifi_manager coupling via event subscription
- Implement retry queue with persistent storage

### Phase 3C: fs_tool + Partition Expansion
- Create fs_tool for persistent storage management
- Embed esp_littlefs component within fs_tool
- Expand partition table to 2MB app + 1.5MB LittleFS
- Complete tool ecosystem ready for production deployment