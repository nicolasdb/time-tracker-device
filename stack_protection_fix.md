# Stack Protection Fault Fix

## Issue Description
When a tag was removed from the RFID reader, the device experienced a stack protection fault:

```
Guru Meditation Error: Core 0 panic'ed (Stack protection fault).
Detected in task "rfid_event_loop" at 0x4038884e
```

This typically happens when a task's stack overflows or is corrupted during execution.

## Root Causes

1. **Insufficient Task Stack Size**: The RFID event loop task was only allocated 2048 bytes of stack memory, which was not enough for processing tag events, especially when event processing includes memory allocation and string formatting.

2. **Potential Event Queue Blocking**: Using `portMAX_DELAY` in the event posting could cause indefinite blocking if the event queue is full, potentially leading to recursive calls or deadlocks.

## Applied Fixes

### 1. Increased RFID Event Loop Task Stack Size
- Changed task stack size from 2048 bytes to 4096 bytes
- This provides more memory for function calls, local variables, and event data structures

### 2. Added Event Post Timeout
- Changed from `portMAX_DELAY` to a 100ms timeout (using `pdMS_TO_TICKS(100)`)
- Added error handling to log when event posting fails
- Implemented for both tag detection and removal events

### 3. Improved Error Handling
- Added logging when event post fails
- This helps identify issues with event processing in the future

## Expected Results
- The device should no longer crash when reading and removing RFID tags
- If the event queue becomes full, the system will gracefully fail instead of hanging
- We'll have better logging for diagnosing any future issues

These changes follow ESP-IDF best practices:
- Use adequate stack sizes for tasks with complex operations
- Avoid using `portMAX_DELAY` for event posting in ISR or event handler contexts
- Always handle error cases with proper logging
