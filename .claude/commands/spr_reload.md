# SPR Context Reload Command

> _Quick context loading from compressed SPR documentation for next development phase_

## Usage

```bash
/spr_reload
```

## What This Command Does

### 🔄 Rapid Context Building
1. **Current State**: Load project status from `docs/project/current_state_spr.md`
2. **Architecture**: Load MCP patterns from `docs/architecture/mcp_patterns_spr.md`  
3. **ESP32 Knowledge**: Load critical fixes from `docs/architecture/esp32_solutions_spr.md`
4. **Next Phase**: Identify immediate next steps and dependencies

### 📋 Quick Status Check
- Which phase is complete, which is next
- Tool ecosystem status and capabilities
- Hardware validation results  
- Critical architecture principles to remember

### 🎯 Ready State Confirmation
- All SPR patterns loaded and expanded
- Next phase dependencies identified
- Development environment context established
- Ready to continue with tool development/debugging

## Current SPR Context Summary

### Project Status [Auto-loaded from SPR]
```
COMPLETE: Phase-5.1|7-tool-architecture|multi-network-wifi|production-ready
TOOLS: feedback(0x1F)|wifi(0x7F)|rfid(0x6F)|fs(0xFF)|webhook(0x9F)|webserver(0x8F)
HARDWARE: ESP32-C3|WS2812B-GPIO7|RC522-SPI|LittleFS-1536K|real-networks
NEXT: Phase-5.2|ntp-tool|time-synchronization|accurate-timestamps
```

### Architecture Patterns [Auto-loaded from SPR]
```
HANDLE: tool_init→context→tool_deinit|no-static-globals|opaque-pointers
EVENTS: publish-subscribe|ESP_EVENT_POST|no-coupling|immediate-return
REGISTRY: capabilities-bitmask|metadata-discovery|tool-version|health-status
VIOLATIONS: static-globals|direct-calls|header-includes|blocking-handlers
```

### ESP32 Critical Knowledge [Auto-loaded from SPR]
```
STRING: snprintf-not-strncpy|buffer-safety|avoid-stringop-truncation
FORMAT: PRIu32-macros|inttypes-include|proper-specifiers
STACK: 8192-main-task|explicit-sizes|prevent-overflow
EVENTS: no-vTaskDelay|immediate-return|event-loop-safe
```

### Build System [Auto-loaded from SPR]
```
BUILD: private-includes|managed-deps|platformio-flags|self-contained
DEPENDENCIES: embedded-within-tool|idf-component-yml|managed-components
DEPLOYMENT: tool-archive|portable|cross-project-ready
```

## Development Environment Check

### ✅ Ready When:
- SPR patterns mentally expanded and understood
- Current phase status clear (Phase 5.1 complete → 5.2 next)
- Critical ESP32 fixes remembered (no blocking handlers, proper string handling)
- MCP architecture principles loaded (handle-based, event-driven)
- Build system patterns understood (self-contained tools, private includes)

### 🎯 Next Phase Ready
- **Target**: Phase 5.2 - NTP Tool Implementation
- **Dependencies**: WiFi tool connection events for sync triggering
- **Pattern**: MCP tool (handle-based, event-driven, registry integration)
- **Validation**: Hardware testing with time accuracy measurement

This command instantly loads all compressed knowledge needed for effective development work.
