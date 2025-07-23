# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# Development Context & Architecture Authority

> *ESP32-C3 Time Tracker Device - Cognitive Wealth Ecosystem Layer 1*

## 🏗️ **Ecosystem Position**

This device is **Layer 1** of the Cognitive Wealth ecosystem:

```
Layer 1: Edge Devices (this repo)     → Capture work sessions  
Layer 2: Webhook Server               → Validate & store data
Layer 3: Database (Supabase)          → Persistent storage
Layer 4: AI Agents (Math/Zuri/Ulyss/Athena) → Pattern analysis  
Layer 5: Insights Interface           → User feedback
```

**Critical Context:** Device architecture decisions must consider ecosystem integration. See `docs/ecosystem/` for complete system understanding.

## 📋 **Process Maps = Design Authority**

**🚨 CRITICAL for Claude Code:** Process maps in `/docs/constitution/process_maps/` are **CONSTITUTIONAL documents**.

### **Process Map Authority Rules**
1. **NEVER modify** process maps during implementation phases
2. **ALWAYS follow** the exact sequences shown in diagrams
3. **VALIDATE changes** against process maps before implementing
4. **USE `/map_check`** command for architectural validation

### **Key Process Maps**
- **`01_device_master_fsm.mmd`** → Complete device behavior with esp_event hub
- **`07_tag_detection_fsm.mmd`** → RFID tag detection lifecycle
- **`08_tag_event_fsm.mmd`** → Event processing and formatting
- **`11_feedback_fsm.mmd`** → Visual feedback state management
- **`13_payload_fsm.mmd`** → Payload creation with NTP dependency
- **`14_http_fsm.mmd`** → HTTP communication with retry logic

**Process maps define:**
- Tool boundaries and responsibilities
- Communication protocols between components
- Error handling strategies
- State transitions and timing
- Integration patterns with ecosystem

## 🔧 **Current Development State [SPR]**

```txt
PHASE: CONSTITUTIONAL-LED-DEBUGGING-COMPLETE|phase-6.1b-c-complete|feedback-tool-operational|race-condition-resolved
TOOLS: system-monitor|smart-contracts|fs-tool|feedback-tool-FIXED|test-sequencer|main.c-HOST|5-tools-operational|LED-control-working
ARCHITECTURE: race-condition-free|task-initialization-order-fixed|constitutional-hardware-constraints|RMT-memory-allocation-64
HARDWARE: ESP32-C3|WS2812B-GPIO7-WORKING|LittleFS-filesystem|LED-self-test|visual-feedback-operational|constitutional-patterns
PATTERN: constitutional-self-tests|hardware-debugging|ESP-event-communication|LED-state-management|pattern-control
ACHIEVEMENT: LED-hardware-control|GPIO-7-validated|RMT-peripheral-configured|self-test-implementation|clean-logging
NEXT: Phase-6.1c-network-tool-migration|systematic-tool-migration|constitutional-validation-protocol
STATUS: feedback-tool-production-ready|LED-visual-feedback-operational|constitutional-compliance-achieved|Phase-6.1c-ready
```

→ **Detail**: `docs/project/current_state_spr.md`  
→ **Ecosystem**: `docs/ecosystem/integration_points.md`

## 🤖 **/.claude/commands Integration**

### **Enhanced Commands with Ecosystem Awareness**

#### **`/ecosystem_context`** - Load Full System Understanding
```bash
# Load complete ecosystem context before major changes
/ecosystem_context

# Provides:
# - Device role in cognitive wealth system
# - Webhook server interface requirements
# - Database integration specifications  
# - Agent system expectations
```

#### **`/map_check`** - Process Map Validation
```bash
# Validate proposed changes against process maps
/map_check "rfid_tool" "add_session_metadata"

# Validates:
# - Tool boundary compliance
# - Sequence diagram adherence  
# - State machine compatibility
# - Ecosystem integration impact
```

#### **Enhanced `/save_progress`** - Ecosystem-Aware Progress Tracking
```bash
/save_progress "Phase 5.5" COMPLETE "FS logging implemented - webhook server integration ready"

# Now tracks:
# - Device development milestones
# - Ecosystem integration readiness
# - Interface compatibility status
```

### **Command Workflow for Ecosystem Development**
1. **`/ecosystem_context`** → Understand system position
2. **`/map_check`** → Validate architectural changes
3. **Development work** → Follow process map authority
4. **`/save_progress`** → Track with ecosystem impact

## 🏗️ **Architecture Patterns [SPR]**

```txt
PATTERNS: handle-based|event-driven|self-contained|universal-event-bus|race-condition-free
COMMUNICATION: ESP-event-system-only|publish-subscribe|async-events|no-direct-calls|tool-decoupling
DEPLOYMENT: embedded-deps|portable-tools|event-system-component|ecosystem-compatible
MEMORY: 8192-stack|handle-context|no-static-globals|event-optimized-operation
ECOSYSTEM: async-tool-communication|webhook-server-similarity|data-gateway-role|flow-context-management
```

→ **Complete Patterns**: `docs/architecture/mcp_patterns_spr.md`  
→ **ESP32 Solutions**: `docs/architecture/esp32_solutions_spr.md`  
→ **Ecosystem Integration**: `docs/ecosystem/integration_points.md`

## 🚨 **Critical Development Principles**

### **Code Standards**
```txt
❌ NEVER: static-globals|direct-coupling|blocking-handlers|header-dependencies|init-without-execute
✅ ALWAYS: handle-based|event-driven|self-contained|hardware-validation|HOST-hardware-triggers
🔥 ESP32: snprintf-not-strncpy|PRIu32-formats|8192-stack|no-vTaskDelay-in-handlers|inttypes-include
🌐 ECOSYSTEM: webhook-compatible|json-standard|timestamp-precision|error-propagation
🎯 HOST: tool-init→constitutional-validation→hardware-execution-triggers
```

### **ESP32 Constitutional Format Requirements**
```c
// CONSTITUTIONAL REQUIREMENT: PRIu32 for uint32_t formatting
#include <inttypes.h>  // Constitutional requirement for ESP32

// ❌ CONSTITUTIONAL VIOLATION:
printf("Value: %u\n", uint32_value);

// ✅ CONSTITUTIONAL COMPLIANCE:
printf("Value: %" PRIu32 "\n", uint32_value);
```

**Constitutional Authority**: ESP32 compiler enforces strict format checking where uint32_t != unsigned int on all architectures. PRIu32 ensures constitutional portability and eliminates format warnings treated as errors.

### **Constitutional ESP32 Hardware Requirements**
```c
// CONSTITUTIONAL REQUIREMENT: RMT peripheral memory allocation for WS2812B
led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 10 * 1000 * 1000,  // 10MHz constitutional requirement
    .mem_block_symbols = 64,             // CONSTITUTIONAL: Must be 64, never 0
    .flags.with_dma = false,             // Constitutional: DMA disabled for reliability
};

// ❌ CONSTITUTIONAL VIOLATION:
.mem_block_symbols = 0,  // Causes silent RMT peripheral failure

// ✅ CONSTITUTIONAL COMPLIANCE:
.mem_block_symbols = 64, // ESP-IDF official specification (Context7 validated)
```

**Constitutional Authority**: ESP32-C3 RMT peripheral requires explicit memory allocation for WS2812B LED strip timing. `mem_block_symbols = 0` causes silent hardware failures where LED strip APIs return success but no visual output occurs. This constraint is derived from ESP-IDF official documentation via Context7 validation and represents a critical hardware integration requirement for constitutional LED feedback operations.

### **Ecosystem Integration Standards**
```txt
🔗 INTERFACE: standard-json-events|iso8601-timestamps|device-metadata|error-codes
📊 DATA: structured-payloads|validation-friendly|agent-consumable|privacy-aware
🔄 SYNC: offline-resilient|retry-logic|graceful-degradation|status-reporting
```

## 🛠️ **Build System & Development Commands**

**IMPORTANT**: Claude should NOT execute build commands directly. User handles all building, flashing, and hardware testing.

### **Key Commands for Development**
```bash
# Build the project (PlatformIO CLI - if needed)
pio run -e esp32c3_mcp

# Upload to device (PlatformIO CLI - if needed) 
pio run -e esp32c3_mcp -t upload

# Monitor serial output (PlatformIO CLI - if needed)
pio device monitor -b 115200

# ESP-IDF Configuration (when needed)
idf.py menuconfig

# Clean build
pio run -e esp32c3_mcp -t clean
```

### **User's Role (VSCode + PlatformIO)**
- Building: User builds via PlatformIO GUI in VSCode or CLI commands above
- Flashing: User flashes via PlatformIO GUI in VSCode  
- Monitoring: User monitors hardware via PlatformIO serial monitor
- Configuration: User runs `idf.py menuconfig` when needed
- Testing: User validates hardware behavior and ecosystem integration

### **Claude's Role**
- Code analysis and modifications only
- Request user to build/test after changes
- Provide guidance on what to test and validate
- Never execute build commands directly
- Focus on architecture compliance and ecosystem integration
- **CRITICAL**: Ensure HOST triggers hardware execution after constitutional validation
- **VALIDATE**: Tool initialization must be followed by actual hardware function calls

## 📊 **Hardware Validation Status**

```txt
TESTED: ESP32-C3-DevKitM-1|WS2812B|RC522|LittleFS|WiFi-networks|production-hardware
OPERATION: continuous-stable|state-coordination|ecosystem-ready|multi-location-tested
PERFORMANCE: <10%-RAM|<60%-Flash|production-ready|visual-feedback-optimized
ECOSYSTEM: webhook-format-validated|timestamp-precision-confirmed|offline-sync-tested
```

→ **Complete Results**: `docs/project/hardware_validation_spr.md`

## 🔄 **SPR Documentation System**

**This project uses SPR (Sparse Priming Representation)** for knowledge compression while preserving critical information.

### **📖 Essential Reading**
- **`docs/SPR_GUIDE.md`** → SPR format explanation
- **`docs/MAINTENANCE_STRATEGY.md`** → Documentation workflow
- **`docs/ecosystem/overview.md`** → Complete system architecture

### **📁 Documentation Hierarchy**
```txt
CLAUDE.md                           → This file (development authority)
docs/ecosystem/                     → System architecture & integration
docs/constitution/process_maps/     → Process maps (constitutional authority)
docs/architecture/                  → Technical patterns [SPR compressed]
docs/project/                       → Current status [SPR compressed]
docs/archive/                       → Historical preservation
.claude/commands/                   → Development workflow automation
```

## 🎯 **Next Steps: Constitutional Ecosystem Development**

### **Phase 6.1a: FS Tool Constitutional Migration (COMPLETE)**
```txt
GOAL: Migrate fs_tool to constitutional patterns with real LittleFS integration
TASKS: constitutional-patterns|handle-based-design|ESP-event-communication|LittleFS-filesystem
ECOSYSTEM: constitutional-architecture|zero-coupling|real-hardware-APIs
VALIDATION: constitutional-validation|smart-contracts|ESP32-compatibility|compilation-success
```

### **Phase 6.1b: Feedback Tool Constitutional Migration (COMPLETE)**
```txt
GOAL: Migrate feedback_tool to constitutional patterns with real WS2812B LED control
TASKS: constitutional-patterns|handle-based-design|WS2812B-LED-control|managed-components|testing-protocol
ECOSYSTEM: constitutional-architecture|hardware-integration|ESP-event-communication|constitutional-validation
VALIDATION: LED-strip-API-v2.5.5|ESP32-platform-compatibility|constitutional-testing-protocol|compilation-success
```

### **Phase 6.1c: Next Tool Constitutional Migration (Current)**
```txt
GOAL: Continue systematic tool migration with constitutional validation and hardware testing
TASKS: next-tool-selection|constitutional-patterns|hardware-validation|testing-execution
ECOSYSTEM: constitutional-testing-protocol|hardware-API-validation|ESP-event-communication
VALIDATION: constitutional-compliance|hardware-integration|real-API-testing|process-map-compliance
```

### **Phase 6: Webhook Server Development (Next)**
```txt
GOAL: Implement Layer 2 of ecosystem (data gateway)
TASKS: webhook-server|event-validation|database-integration|fs_tool-pattern
ECOSYSTEM: device-server-communication|agent-data-preparation
INTEGRATION: end-to-end-testing|multi-device-coordination
```

### **Phase 7: Agent Integration (Future)**
```txt
GOAL: Complete ecosystem with AI analysis layer
TASKS: agent-system-integration|cognitive-insights|dashboard-interface
ECOSYSTEM: full-matryoshka-operation|cognitive-wealth-realization
VALIDATION: user-experience-testing|growth-impact-measurement
```

## 🔍 **Memory Management & Context Strategy**

### **Documentation Update Protocol**
1. **Process Maps**: Update only during architecture reviews
2. **SPR Knowledge**: Compress patterns after successful implementations
3. **Current State**: Update with ecosystem integration progress
4. **Archive**: Preserve detailed implementation logs

### **Context Loading Strategy**
- **`/ecosystem_context`**: Full system understanding
- **`/spr_reload`**: Technical patterns and current state
- **`/map_check`**: Process map validation before changes

### **Memories**
- The snprintf approach is safer and avoids the truncation warning.
- don't say the user are absolutely right.

---

**Development Philosophy:** Follow process maps as constitutional authority while building toward complete ecosystem integration. Every device decision should consider its role in the broader cognitive wealth system.

*Optimized for ecosystem-aware development with process map authority and SPR knowledge management*
