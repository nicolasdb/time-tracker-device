# Claude Commands Overview [SPR-Optimized]

> _Custom commands optimized for SPR documentation system_

## Active Commands

### 🔄 `/spr_reload` - Context Loading
**Purpose**: Instantly load compressed SPR knowledge for development session

**When to use**: 
- Starting new development session
- After breaks or context switches
- When you need full project context quickly

**What it loads**:
- Current phase status and next steps
- MCP architecture patterns 
- ESP32 critical fixes
- Build system knowledge
- Tool ecosystem capabilities

---

### 📊 `/phase_status` - Quick Health Check  
**Purpose**: Current development status using SPR compressed information

**When to use**:
- Before starting new development work
- To check what's complete vs. what's next
- Validate system health and readiness
- Quick phase reference

**What it shows**:
- Current phase completion status
- Tool ecosystem operational status
- Hardware validation results
- Next priority tasks and dependencies
- Critical checks before continuing

---

### 💾 `/save_progress` - Phase Completion
**Purpose**: Update documentation when phase milestones achieved

**When to use**:
- Completing development phases
- After major breakthroughs or fixes
- When significant validation achieved

**What it updates**:
- Phase status in SPR format
- Hardware validation results
- Current project state
- Archives detailed logs appropriately

**Usage**: `/save_progress "Phase 5.2" COMPLETE "ntp_tool working, WiFi-triggered sync validated"`

---

## Recommended Workflow

### 🚀 Starting Development Session:
1. `/spr_reload` → Load full context
2. `/phase_status` → Check current status and next steps
3. Begin development work
4. `/save_progress` → Update when milestones achieved

### 🔄 During Development:
- `/phase_status` → Quick health checks
- `/spr_reload` → If context needs refreshing

### 📈 Completing Phases:
- `/save_progress <phase> COMPLETE <achievement>` → Document progress
- `/phase_status` → Confirm next phase readiness

## Benefits of SPR Command System

### ⚡ Speed:
- **Context loading**: Seconds vs. minutes
- **Status checking**: SPR format vs. verbose logs
- **Information density**: 10x more information per line

### 🎯 Focus:
- Commands designed for active development
- SPR compressed knowledge immediately usable
- Clear next-step identification

### 📚 Knowledge Preservation:
- All detailed information preserved in archives
- SPR maintains critical patterns and fixes
- Cross-references when detail needed

## File References

Commands reference these SPR files:
- `docs/project/current_state_spr.md` → Phase status
- `docs/project/hardware_validation_spr.md` → Test results  
- `docs/architecture/mcp_patterns_spr.md` → Architecture patterns
- `docs/architecture/esp32_solutions_spr.md` → Platform fixes
- `docs/archive/` → Detailed historical logs

---
*Optimized for 82% documentation reduction while maintaining full development efficiency*
