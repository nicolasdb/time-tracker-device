# Reboot Loop Bug Fix

## Issue Description
The device was entering a reboot loop due to an issue with FreeRTOS event groups. The problem occurred when:
1. The WiFi manager tried to wait on event bits using `xEventGroupWaitBits` after starting AP mode
2. The event group handle `s_wifi_event_group` was potentially invalid or corrupted
3. The assertion `xEventGroupWaitBits event_groups.c:345 (xEventGroup)` failed

## Applied Fixes

### 1. Removed Event Group Wait in AP Mode Start
- Modified `wifi_manager_start_ap_mode()` to use a simple delay and status check instead of event group wait
- This avoids the problematic assertion that was causing the reboot loop

### 2. Implemented ESP Timer for Reboot
- Added ESP timer implementation for device reboot instead of using a task
- This avoids any potential issues with task scheduling during WiFi state transitions
- ESP timers operate independently of task scheduling

### 3. Added Event Group Validation
- Added null checks before all event group operations
- Improved error handling in event group creation
- Added logging to help diagnose any future event group issues

### 4. Optimization Settings
- Changed optimization from `-Os` (optimize for size) to `-O2` (optimize for performance)
- Added `-DESP_SYSTEM_CHECK_INT_LEVEL=5` for better memory corruption detection

## Expected Results
- The device should no longer crash when entering or exiting AP mode
- Reboots will be handled by the ESP timer, which is more reliable in this context
- If any memory corruption issues persist, they should be more clearly reported in the logs

## Testing Notes
- Test both normal startup (with saved WiFi credentials)
- Test AP mode activation when no known networks are available
- Test configuration saving and rebooting from AP mode
