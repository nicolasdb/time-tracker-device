# SPR Maintenance Strategy
*How to keep documentation optimized as project evolves*

## Quick Reference: Update Decision Tree

```
New Information → Ask:
├─ "Will this pattern repeat in future tools?" → YES → SPR compress
├─ "Is this a broadly applicable fix?" → YES → SPR compress  
├─ "Is this architectural knowledge?" → YES → SPR compress
├─ "Is this one-time debugging detail?" → NO → Archive detailed
└─ "Is this tool-specific implementation?" → NO → Keep in tool docs
```

## SPR Update Workflow

### Step 1: Categorize Information
- **MCP patterns** → `docs/architecture/mcp_patterns_spr.md`
- **ESP32 fixes** → `docs/architecture/esp32_solutions_spr.md`  
- **Build knowledge** → `docs/architecture/build_system_spr.md`
- **Current status** → `docs/project/current_state_spr.md`
- **Hardware validation** → `docs/project/hardware_validation_spr.md`
- **Historical detail** → `docs/archive/`

### Step 2: Extend SPR Blocks (Don't Replace)
```markdown
# CORRECT - Extending existing pattern  
## Tool Validation [SPR]  
PHASE-5.1: wifi-multi-network|AP-fallback|production-ready
PHASE-5.2: ntp-tool-complete|time-sync-working|accurate-timestamps
PHASE-5.3: rfid-events|timestamped-sessions|work-tracking
```

### Step 3: Archive Detailed Logs
- Move verbose logs to `docs/archive/` with timestamps
- Keep SPR summary in active docs
- Reference archive location when detail needed

## SPR Quality Control

### ✅ Good SPR:
- Uses consistent vocabulary across files
- Compresses repeated patterns effectively  
- Remains meaningful when expanded mentally
- Follows category:keyword1|keyword2|keyword3 format

### ❌ Avoid:
- Overly cryptic abbreviations that lose meaning
- Inconsistent vocabulary across files
- Compressing unique details that need full explanation
- Making SPR blocks too long (>10 items per category)

## Standard SPR Vocabulary

### Core Patterns:
```
HANDLE: tool_init→context→tool_deinit|no-static-globals
EVENTS: publish-subscribe|ESP_EVENT_POST|no-coupling
TOOLS: tool-name(capabilities)|status|integration-level
VALIDATED: hardware-config|duration|performance|status
COMPLETE: phase-name|feature-achieved|validation-status
```

### Emergency Expansion:
If SPR becomes unclear, add example in comments:
```markdown
## ESP32 Solutions [SPR]
STRING: snprintf-not-strncpy|buffer-safety|avoid-truncation
# Example: snprintf(dest, sizeof(dest), "%s", src); // NOT strncpy()
```

## Integration with Save Progress

```bash
/save_progress "Phase 5.2" COMPLETE "ntp_tool working, WiFi-triggered sync"
```

This updates:
1. `docs/project/current_state_spr.md` → Add phase status in SPR
2. `docs/project/hardware_validation_spr.md` → Add results in SPR  
3. `CLAUDE.md` → Update current phase only
4. `docs/archive/` → Move detailed logs if verbose

See `docs/SPR_GUIDE.md` for complete SPR explanation and examples.
