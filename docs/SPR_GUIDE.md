# What is SPR? (Sparse Priming Representation)
*Essential reading for anyone working with this project's documentation*

## Quick Overview
**SPR** compresses verbose explanations into dense, scannable patterns that preserve all essential information. Instead of writing 50-line explanations repeatedly, we create 5-line compressed blocks that experienced developers can rapidly expand into full context.

## SPR Format Rules

### Basic Structure
```
CATEGORY: keyword1-description|keyword2-description|keyword3-description
```

### Separator Meanings
- **Pipe (|)** = separate related concepts
- **Hyphen (-)** = connects problem-solution pairs  
- **Colon (:)** = starts category grouping
- **ALL-CAPS** = category headers

## Real Example from This Project

### Before SPR (50+ lines):
```markdown
### Common ESP32 Fixes

**String Handling (Critical):**
The ESP32 compiler has strict warnings that become errors. Never use strncpy() 
because it triggers -Werror=stringop-truncation warnings. The issue is that 
strncpy() doesn't guarantee null termination, which can lead to buffer overflows.

Always use snprintf() instead because:
1. It provides proper buffer safety
2. It guarantees null termination  
3. It avoids compiler warnings
4. It's more secure for embedded systems

**Format Specifiers:**
When logging uint32_t values, don't use %d format specifier as it causes 
compilation errors on ESP32. You must include <inttypes.h> and use the 
PRIu32 macro for proper cross-platform formatting.

**Stack Management:**  
The default ESP-IDF main task stack is only 3584 bytes, which is insufficient 
for complex applications with multiple tool initialization. You must explicitly 
set the stack size to 8192 bytes to prevent stack overflow crashes during 
tool initialization sequences.

[... continues for 30+ more lines]
```

### After SPR (4 lines):
```markdown
## ESP32 Critical Solutions [SPR]
STRING: snprintf-not-strncpy|buffer-safety|avoid-stringop-truncation
FORMAT: PRIu32-macros|inttypes-include|no-bare-integers|proper-specifiers
STACK: 8192-main-task|explicit-sizes|no-default-3584|prevent-overflow
EVENTS: no-vTaskDelay|immediate-return|event-loop-safe|no-blocking-handlers
```

## How to Read SPR

### Mental Expansion Process:
1. **See category**: `STRING:` → "This is about string handling"
2. **Read pattern**: `snprintf-not-strncpy` → "Use snprintf, not strncpy"  
3. **Add context**: `buffer-safety` → "Because of buffer safety concerns"
4. **Full reconstruction**: "Use snprintf() instead of strncpy() for buffer safety and to avoid stringop-truncation warnings"

### Practice Examples:
```markdown
TOOLS: feedback(0x1F)|wifi(0x7F)|rfid(0x6F)
→ "Tools in system: feedback_tool with capabilities 0x1F, wifi_tool with 0x7F, rfid_tool with 0x6F"

VALIDATED: 60s-continuous|stable-memory|production-ready  
→ "Validated with 60 seconds continuous operation, stable memory usage, confirmed production ready"

HANDLE: tool_init→context→tool_deinit|no-static-globals
→ "Handle-based design: call tool_init to get context, use context for operations, call tool_deinit for cleanup, never use static global variables"
```

## When to Use SPR vs. Detailed Documentation

### ✅ USE SPR FOR:
- **Repeated patterns** across multiple tools/phases
- **Critical fixes** that apply broadly (like ESP32 solutions)
- **Architecture principles** used throughout project
- **Status updates** that follow consistent format
- **Validation results** with standard structure

### ❌ DON'T USE SPR FOR:
- **Complex code examples** requiring exact syntax
- **Unique debugging sessions** specific to one issue
- **Detailed implementation guides** for new team members
- **Historical logs** with specific timestamps and metrics
- **One-time discoveries** that don't create patterns

## SPR Vocabulary for This Project

### Core MCP Concepts:
```
HANDLE: tool_init→context→tool_deinit|no-static-globals|opaque-pointers
EVENTS: publish-subscribe|ESP_EVENT_POST|no-coupling|immediate-return
REGISTRY: capabilities-bitmask|metadata-discovery|tool-version|health-status
LIFECYCLE: init→register→subscribe→operate→publish→deinit
```

### ESP32 Standards:
```
STRING: snprintf-not-strncpy|buffer-safety|avoid-truncation
FORMAT: PRIu32-macros|inttypes-include|proper-specifiers
MEMORY: 8192-stack|handle-context|no-leaks|partition-planning
BUILD: private-includes|managed-deps|platformio-flags|self-contained
```

### Project States:
```
COMPLETE: phase-name|feature-achieved|validation-status
VALIDATED: hardware-config|duration|performance-metrics|operation-stability
NEXT: target-phase|dependencies|success-criteria
TOOLS: tool-name(capabilities)|status|integration-level
```

## Working with SPR Documentation

### Reading SPR Files:
1. **Scan categories** to find relevant section
2. **Expand keywords** mentally based on context
3. **Reference archives** when full detail needed
4. **Use cross-references** to related SPR blocks

### Updating SPR Files:
1. **Check existing patterns** before adding new information
2. **Extend categories** rather than replacing them
3. **Maintain vocabulary consistency** with existing blocks
4. **Archive detailed logs** while keeping SPR summary

### Creating New SPR Blocks:
1. **Identify repeated pattern** across multiple instances
2. **Extract core concepts** into keyword format
3. **Test readability** - can others expand the pattern?
4. **Add to appropriate category** in existing SPR files

## Benefits of SPR in This Project

### Before SPR (Problems):
- 2,800+ lines of documentation to maintain
- Critical information scattered across multiple files
- Redundant explanations in every phase report
- Difficult to find specific technical fixes quickly
- Documentation growing larger with each development phase

### After SPR (Solutions):
- 500 lines of active documentation (82% reduction)
- Patterns immediately scannable and recognizable
- Single source of truth for technical knowledge
- Sub-30-second information lookup
- Documentation grows smarter, not larger

## Quick Reference Card

### SPR Reading Cheat Sheet:
```
CATEGORY: concept1|concept2|concept3
         ↑        ↑       ↑        ↑
    Group type   Main    Related  Additional
                concept  concept  context
                
problem-solution = "Use solution instead of problem"
concept1→concept2 = "Concept1 leads to concept2"  
feature(value) = "Feature has specific value"
status|duration|result = "Three related pieces of information"
```

### Most Common Patterns:
- `tool-name(capabilities)` = Tool with specific capability bitmask
- `phase-X.Y-complete` = Development phase finished
- `hardware|duration|status` = Validation results summary
- `problem-solution` = Fix for specific issue
- `concept1→concept2→concept3` = Process flow or sequence

---
*This SPR system enables 82% documentation reduction while preserving 100% of critical information*
