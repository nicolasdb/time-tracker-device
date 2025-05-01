# ESP32 Stack Protection Fault Fix

## Issue Description

The project was experiencing a `Stack protection fault` crash during initialization of the webhook manager component. The error occurred because:

1. The webhook initialization was trying to load and parse large JSON files on the main task's stack
2. The default stack size was insufficient for the JSON parsing operations
3. All operations were attempted sequentially without proper memory management

## Solution Implemented

We've implemented a multi-part fix to address the stack overflow issue:

### 1. Minimized Webhook Manager Initialization

- Modified the webhook manager to initialize with minimal configuration
- Separated configuration loading and log file loading into explicit operations
- Added new public API functions:
  - `webhook_manager_load_configuration()`
  - `webhook_manager_load_log_file()`

### 2. Increased Task Stack Size

- Increased the webhook task stack size from 4096 to 8192 bytes
- This provides more headroom for JSON parsing operations

### 3. Deferred Loading of Heavy Operations

- Moved the loading of configuration and logs to the dedicated webhook task
- Added delays between heavy operations to prevent stack exhaustion
- Ensured the main application stack isn't compromised

### 4. Improved Memory Management

- Added proper cleanup in error paths
- Implemented better string handling
- Added validation for parsing operations

### 5. Added Defensive Programming

- Added NULL checks throughout the code
- Implemented proper error handling for file operations
- Created default configuration and log files if not found

## Technical Details

The ESP32-C3 has limited stack space, and JSON parsing (especially with cJSON) can be memory-intensive. The stack protection mechanism in ESP-IDF detects when the stack is approaching overflow and triggers a protection fault.

Our solution separates heavy operations into a dedicated task with adequate stack space and spreads operations over time to prevent memory pressure.

## Testing

The solution has been tested with:
- Various configuration file sizes
- Missing configuration files (default creation)
- Startup sequences with and without configuration

## Best Practices for ESP-IDF Development

1. Avoid heavy operations (file I/O, JSON parsing) on the main task
2. Allocate sufficient stack for tasks performing complex operations
3. Spread resource-intensive operations over time
4. Use explicit heap allocations for large objects
5. Implement proper cleanup in all error paths
