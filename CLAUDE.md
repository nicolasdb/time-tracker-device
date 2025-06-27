# Development Context & Architecture Authority

> *This file provides guidance to Claude Code when working on the Time Tracker Device*

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

**🚨 CRITICAL for Claude Code:** Process maps in `/docs/charts/` are **CONSTITUTIONAL documents**.

### **Process Map Authority Rules**
1. **NEVER modify** process maps during implementation phases
2. **ALWAYS follow** the exact sequences shown in diagrams
3. **VALIDATE changes** against process maps before implementing
4. **USE `/map_check`** command for architectural validation

### **Key Process Maps**
- **`01_boot_sequence.mmd`** → Tool initialization order and grace period
- **`02_tag_placement_happy_path.mmd`** → Normal operation flow
- **`03_error_handling_http_retry.mmd`** → Error recovery patterns
- **`04_circular_buffer_stress_test.mmd`** → Load handling and debounce
- **`05_device_state_machine.mmd`** → Overall device behavior

**Process maps define:**
- Tool boundaries and responsibilities
- Communication protocols between components
- Error handling strategies
- State transitions and timing
- Integration patterns with ecosystem

## 🔧 **Current Development State [SPR]**

```txt
PHASE: 5.7-complete|system-monitor-fixes|dashboard-metrics-corrected|architectural-boundaries-restored|memory-corruption-eliminated
TOOLS: rfid_tool(95%)|payload_tool(85%)|fs_tool(100%)|feedback_tool(100%)|network_tool(98%)|ntp_tool(88%)|system_monitor_tool(100%)|http_tool(95%)|webserver_tool(100%)|event_system(100%)
FIXED: storage-calculation-overflow-protection|webhook-counter-bounds-checking|system-monitor-struct-alignment|dashboard-accuracy|memory-safety
HARDWARE: ESP32-C3|WS2812B-GPIO7|RC522-SPI|LittleFS-1536K|storage-1%-accurate|webhook-counters-reset|dashboard-functional
ECOSYSTEM: system-monitor-compliant|tool-boundaries-enforced|memory-corruption-resolved|dashboard-metrics-accurate|system-compliance-98%
ISSUES: resolved|no-critical-issues|ready-for-phase-6
NEXT: Phase-6.0|webhook-server-layer-2|ecosystem-integration|agent-system-preparation
STATUS: system-monitor-production-ready|dashboard-accurate|memory-safe|architecturally-compliant
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
❌ NEVER: static-globals|direct-coupling|blocking-handlers|header-dependencies
✅ ALWAYS: handle-based|event-driven|self-contained|hardware-validation
🔥 ESP32: snprintf-not-strncpy|PRIu32-formats|8192-stack|no-vTaskDelay-in-handlers
🌐 ECOSYSTEM: webhook-compatible|json-standard|timestamp-precision|error-propagation
```

### **Ecosystem Integration Standards**
```txt
🔗 INTERFACE: standard-json-events|iso8601-timestamps|device-metadata|error-codes
📊 DATA: structured-payloads|validation-friendly|agent-consumable|privacy-aware
🔄 SYNC: offline-resilient|retry-logic|graceful-degradation|status-reporting
```

## 🛠️ **Build System & User/Claude Separation**

**IMPORTANT**: Claude should NOT execute build commands directly.

### **User's Role (VSCode + PlatformIO)**
- Building: User builds via PlatformIO GUI in VSCode
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
docs/charts/                        → Process maps (constitutional authority)
docs/architecture/                  → Technical patterns [SPR compressed]
docs/project/                       → Current status [SPR compressed]
docs/archive/                       → Historical preservation
.claude/commands/                   → Development workflow automation
```

## 🎯 **Next Steps: Ecosystem Integration**

### **Phase 5.6b: Event-Driven Architecture (COMPLETE)**
```txt
GOAL: Eliminate race conditions through async event-driven architecture
TASKS: universal-event-system|ESP-event-communication|tool-decoupling|flow-context-race-fix
ECOSYSTEM: async-tool-communication|event-driven-patterns|process-map-compliance
VALIDATION: compilation-clean|race-condition-eliminated|orange-green-issue-resolved
```

### **Phase 5.7: Async Flow Testing (Current)**
```txt
GOAL: Validate async event-driven flow awareness without race conditions
TASKS: hardware-testing|flow-timing-validation|orange-green-verification|performance-testing
ECOSYSTEM: end-to-end-validation|ecosystem-readiness-confirmation
VALIDATION: place-tag→GREEN|flow-awareness→orange-breathing|flow-urgency→orange-pulsing|remove-tag→idle|new-tag→GREEN
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
