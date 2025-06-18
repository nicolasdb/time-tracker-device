# Optimized CLAUDE.md [SPR-Optimized]

> *This file provides guidance to Claude Code when working with this repository*

## Project Overview

ESP32-C3 RFID time tracking system with 8-component MCP architecture. Fully operational production device with multi-network WiFi, visual feedback, persistent storage, and time synchronization foundation.

## Current State [SPR]

```txt
PHASE: 5.4-complete|RFID-timestamps|work-session-tracking|boot-counter-precision|state-change-detection
TOOLS: feedback(0x1F)|wifi(0x7F)|rfid(0x6F)|fs(0xFF)|webhook(0x9F)|webserver(0x8F)|ntp(0x7F)
HARDWARE: ESP32-C3|WS2812B-GPIO7|RC522-SPI|LittleFS-1536K|RFID-state-coordination
NEXT: Phase-5.5|FS-event-logging|monitor-display|persistence-foundation
STATUS: time-tracking-ready|RFID-session-detection|visual-feedback-coordinated|event-spam-prevented
```

→ **Detail**: docs/project/current_state_spr.md

## Build System & Commands

**IMPORTANT**: Claude should NOT execute build commands directly. User handles building, flashing, and monitoring through VSCode + PlatformIO GUI.

### User's Role (VSCode + PlatformIO)

- Building: User builds via PlatformIO GUI in VSCode
- Flashing: User flashes via PlatformIO GUI in VSCode  
- Monitoring: User monitors hardware via PlatformIO serial monitor
- Configuration: User runs `idf.py menuconfig` when needed

### Claude's Role

- Code analysis and modifications only
- Request user to build/test after changes
- Provide guidance on what to test
- Never execute build commands directly

## Architecture Overview [SPR]

```txt
PATTERNS: handle-based|event-driven|self-contained|tool-registry
COMMUNICATION: publish-subscribe|ESP_EVENT_POST|no-coupling
DEPLOYMENT: embedded-deps|portable-tools|cross-project-reusable
MEMORY: 8192-stack|handle-context|no-static-globals|stable-operation
```

→ **Complete Patterns**: docs/architecture/mcp_patterns_spr.md
→ **ESP32 Solutions**: docs/architecture/esp32_solutions_spr.md
→ **Build System**: docs/architecture/build_system_spr.md

## Critical Development Principles

```txt
❌ NEVER: static-globals|direct-coupling|blocking-handlers|header-dependencies
✅ ALWAYS: handle-based|event-driven|self-contained|hardware-validation
🔥 ESP32: snprintf-not-strncpy|PRIu32-formats|8192-stack|no-vTaskDelay-in-handlers
🏗️ BUILD: private-includes|managed-deps|embedded-components|tool-archives
```

## Tool Development Checklist

```txt
REQUIRED: handle-based|event-driven|self-contained|registry-entry
TESTING: isolation-first|integration-second|hardware-validation-final
LIFECYCLE: init→register→subscribe→operate→publish→deinit
CAPABILITIES: bitmask-enum|metadata-discovery|status-reporting
```

## Hardware Validation Status

```txt
TESTED: ESP32-C3-DevKitM-1|WS2812B|RC522|LittleFS|WiFi-networks
OPERATION: 60s-continuous|stable-memory|state-coordination|multi-location
PERFORMANCE: 9.6%-RAM|53.8%-Flash|production-ready|flawless-visual
```

→ **Complete Results**: docs/project/hardware_validation_spr.md

## Configuration Management

### Compile-Time (Kconfig)

- `idf.py menuconfig` → "Time Tracker Configuration"  
- Tool-specific configs in `/tools/*/Kconfig`
- GPIO pins, stack sizes, hardware parameters

### Runtime (LittleFS JSON)

- `wifi.json`: Network credentials with multi-SSID support
- `webhook_config.json`: Runtime webhook settings
- `log.json`: Event log for retry mechanism

## Memory Management Protocol

### After Each Phase Completion

1. **docs/project/current_state_spr.md**: Update phase status [SPR format]
2. **docs/project/hardware_validation_spr.md**: Add essential test results [SPR format]  
3. **CLAUDE.md**: Update current state section only
4. **Archive**: Move detailed logs to docs/archive/ if needed

## Next Steps: Multi-Phase Roadmap

```txt
Phase-5.5: FS-event-logging|monitor-display|persistence-foundation|JSON-storage
Phase-5.6: webhook-intelligence|real-timestamp-calculation|session-duration-logic|enhanced-payloads
Phase-6: priority-system-redesign|boot-optimization|edge-case-hardening|production-polish

IMMEDIATE: FS-tool-event-handler|RFID+NTP-log-storage|serial-monitor-commands
DEPENDENCIES: event-persistence→timestamp-calculation→session-intelligence→optimization
```

## SPR Documentation System
**This project uses SPR (Sparse Priming Representation)** for 82% documentation size reduction while preserving all information.

### 📖 Essential Reading:
- **docs/SPR_GUIDE.md** → What is SPR? How to read/write SPR format
- **docs/MAINTENANCE_STRATEGY.md** → How to keep SPR docs current

### 📁 File Structure Reference:
```txt
CLAUDE.md                          → Current context (this file)
docs/SPR_GUIDE.md                  → SPR explanation & vocabulary
docs/architecture/*.md             → Technical knowledge [SPR compressed]  
docs/project/*.md                  → Project status [SPR compressed]
docs/archive/*.md                  → Historical preservation (detailed logs)
tools/*/CLAUDE.md                  → Tool-specific integration guides
```

### 🔄 Update Protocol:
When adding new phase information, compress repeated patterns into SPR format following existing vocabulary. Archive detailed logs to docs/archive/. See save_progress command for workflow.

---
*Total Optimization: 362 → 105 lines (71% reduction) using SPR compression*
*All critical information preserved - expand SPR patterns for full context*
