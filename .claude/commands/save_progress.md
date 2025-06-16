# Save Progress Command

Save current development progress and update all tracking documents in the correct order.

## Usage

```
/save_progress <phase_name> <status> [validation_logs]
```

## Parameters

- `phase_name`: Current phase being completed (e.g., "Phase 3C", "Phase 3B")
- `status`: COMPLETE | PARTIAL | BLOCKED
- `validation_logs`: Optional hardware validation output to include

## What This Command Does

### 1. Update CLAUDE.md - Current State Section
- Mark specified phase as complete/partial/blocked
- Update RAM/Flash usage metrics if provided
- Note any dependency violations or blockers
- Update next steps priority order

### 2. Update docs/architecture_insights.md
- Add new technical patterns discovered
- Document compilation/runtime fixes applied
- Record performance metrics and memory usage
- Note any breaking changes or lessons learned

### 3. Update docs/phase_reports.md
- Add hardware validation logs and metrics
- Record tool initialization success/failure
- Document any crash analysis or debugging
- Include performance benchmarks

### 4. Update refactor_mission_brief.md
- Mark phase complete with success criteria met
- Update dependency hierarchy if changed
- Note any architecture replanning needed
- Update meta objectives progress

### 5. Compact and Reload State
- Run `/compact` to reduce context
- Provide concise summary of:
  - Current working system state
  - Next phase priorities
  - Key insights to prevent recurring mistakes
  - Critical architecture principles to maintain

## Example Usage

```bash
/save_progress "Phase 3C" COMPLETE "fs_tool initialized successfully, 5-tool architecture validated"
```

## Architecture Principles to Always Check

-  **No hardcoded dependencies**: Tools use other tool APIs, never direct system calls
-  **Handle-based design**: No static globals, proper context encapsulation  
-  **Event-driven communication**: ESP event system for inter-tool coordination
-  **Self-contained tools**: Each tool includes all dependencies
-  **MCP compliance**: Tool registry, capabilities discovery, clean lifecycle

## Common Mistakes to Prevent

Based on `docs/architecture_insights.md`:

1. **String Handling**: Always use `snprintf()` instead of `strncpy()` for ESP32
2. **Stack Size**: Monitor main task stack, increase if tools crash during init
3. **Dependency Violations**: Tools must use fs_tool APIs, not direct filesystem calls
4. **Format Specifiers**: Use `PRIu32` macros for uint32_t in printf statements
5. **Tool Configuration**: Each tool needs `/tools/tool_name/Kconfig` + `/tools/tool_name/CLAUDE.md`

## Success Criteria Checklist

- [ ] All compilation warnings resolved
- [ ] Tools initialize without crashes
- [ ] Memory usage documented and within limits
- [ ] MCP patterns followed consistently
- [ ] Hardware validation completed
- [ ] Next phase dependencies identified

This command ensures we maintain architectural integrity and avoid repeating solved problems across development sessions.