# Save Progress Command [SPR-Optimized]

> _Maintain documentation consistency with optimized SPR structure_

## 📖 Essential Background

- **docs/SPR_GUIDE.md** → What is SPR? How to read/write SPR format
- **docs/MAINTENANCE_STRATEGY.md** → Detailed update workflow and quality control

## Usage

```bash
/save_progress <phase_name> <status>
```

## Document Roles [Updated Structure]

### CLAUDE.md - Current Context Only (150 lines max)

- Current phase status and immediate next steps [SPR format]
- Build commands and user/Claude role separation
- Critical architecture principles [SPR references]
- Hardware validation status [SPR summary]

### docs/architecture/ - Technical Knowledge Base [SPR Compressed]

- **mcp_patterns_spr.md**: Universal tool patterns (handle-based, event-driven)
- **esp32_solutions_spr.md**: Platform-specific fixes (string handling, events, memory)
- **build_system_spr.md**: Deployment patterns (PlatformIO, dependencies, archives)

### docs/project/ - Project Context [SPR Compressed]

- **current_state_spr.md**: Live phase status, tool ecosystem, capabilities
- **hardware_validation_spr.md**: Essential test results, performance metrics
- **development_summary.md**: High-level progress tracking

### docs/archive/ - Historical Preservation

- **development_history_complete.md**: Complete phase logs and validation details
- **architecture_insights_complete.md**: Full technical evolution history
- **original_mission_brief.md**: Complete transformation plan and objectives

### tools/*/CLAUDE.md - Tool Integration Guides

- Tool-specific configuration and integration patterns
- Keep current structure (unchanged)

## SPR Update Protocol

### Phase Completion Updates [In Order]

1. **docs/project/current_state_spr.md**:

   ```txt
   COMPLETE: Phase-X.Y|feature-achieved|validation-status
   TOOLS: updated-capabilities|new-integrations|performance
   NEXT: Phase-X.Z|target-feature|dependencies
   ```

2. **docs/project/hardware_validation_spr.md**:

   ```txt
   VALIDATED: Phase-X.Y|hardware-config|duration|performance-metrics
   RESULTS: tool-coordination|memory-usage|operation-stability
   ```

3. **CLAUDE.md**: Update "Current State [SPR]" section only

4. **Archive if needed**: Move detailed logs to docs/archive/

## SPR Format Guidelines

### Use SPR Compression For

- **Repeated patterns** across multiple tools
- **Technical fixes** that apply broadly  
- **Validation results** with consistent structure
- **Architecture principles** used project-wide

### SPR Pattern Examples

```txt
TOOLS: tool1(caps)|tool2(caps)|tool3(caps)|status
VALIDATION: hardware|duration|performance|status
PATTERNS: principle1|principle2|principle3|application
FIXES: problem1-solution|problem2-solution|context
```

### Keep Detailed For

- **Unique implementation details** specific to single components
- **Debug logs** that may be referenced later
- **Performance metrics** requiring precise values
- **Integration commands** requiring exact syntax

## Quick Reference: Core SPR Vocabulary

### MCP Architecture

```txt
HANDLE: tool_init→context→tool_deinit|no-static-globals
EVENTS: publish-subscribe|ESP_EVENT_POST|no-coupling
REGISTRY: capabilities-bitmask|metadata-discovery|tool-version
```

### ESP32 Critical Fixes

```txt  
STRING: snprintf-not-strncpy|buffer-safety|avoid-truncation
EVENTS: no-vTaskDelay|immediate-return|event-loop-safe
MEMORY: 8192-stack|handle-context|no-leaks
```

### Tool Development

```txt
LIFECYCLE: init→register→subscribe→operate→publish→deinit
TESTING: isolation-first|integration-second|hardware-final
DEPLOYMENT: self-contained|embedded-deps|portable-archive
```

## Example Usage

```bash
/save_progress "Phase 5.2" COMPLETE "ntp_tool implemented, time sync working, WiFi-triggered sync validated"
```

This command now maintains SPR-optimized documentation that grows smarter, not larger, with each development phase.

---

> Optimized for documentation size reduction while preserving all critical information
