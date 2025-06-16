# Save Progress Command

Maintain documentation consistency across project tracking files.

## Usage

```
/save_progress <phase_name> <status>
```

## Document Roles

### CLAUDE.md - Project Memory & Guidelines
- Current system state and next priorities
- Build commands and development workflow
- Critical architecture principles and common mistakes
- Configuration management patterns

### docs/architecture_insights.md - Technical Patterns
- Reusable MCP architecture patterns
- Critical technical fixes and solutions
- Hardware integration patterns
- Build system and deployment knowledge

### docs/phase_reports.md - Historical Development Log
- Hardware validation results by phase
- Memory usage progression
- Performance metrics and benchmarks
- Detailed timeline and achievements

### refactor_mission_brief.md - Master Plan & Objectives
- Overall mission goals and success criteria
- Phase breakdown and dependencies
- Architecture transformation strategy
- Long-term vision and roadmap

## Example Usage

```bash
/save_progress "Phase 3C" COMPLETE "fs_tool initialized successfully, 5-tool architecture validated"
```

## Quick Reference

### MCP Architecture Principles (Always Check)
- Handle-based design (no static globals)
- Event-driven communication (no direct coupling)
- Self-contained tools (embedded dependencies)
- Tool registry and capabilities discovery

### Common ESP32 Fixes
- Use `snprintf()` not `strncpy()` for strings
- Use `PRIu32` macros for format specifiers
- Set main task stack to 8192 bytes minimum
- Use fs_tool APIs, not direct filesystem calls

### Tool Development Checklist
- [ ] MCP interface implemented
- [ ] Handle-based context structure
- [ ] Event publishing/subscribing
- [ ] Kconfig + CLAUDE.md documentation
- [ ] Hardware validation completed

This command maintains information distribution across documents to optimize for their distinct purposes while preventing duplication.