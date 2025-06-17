# Time Tracker Color Code & Sequence Summary v2.0

## **Core Color Philosophy**

**Context-aware colors** - each color combination tells a story about **what** (issue type) affects **which system** (component).

## **Primary Color Meanings**

- **🔵 Blue**: Connectivity & Communication systems
- **🟢 Green**: Event management & Normal operations  
- **🟠 Orange**: Cognitive wellness & Timing systems
- **🔴 Red**: Errors & Failures
- **🟡 Yellow**: Warnings & Configuration issues
- **🟣 Purple**: Special states (AP mode)

---

## **Configuration Advantages**

### **Complete Timing Control**

- **Deployment Flexibility**: Different environments may need different feedback speeds
- **User Preferences**: Some users prefer faster/slower visual cues
- **Accessibility**: Adjust timing for different cognitive or visual needs
- **Testing & Tuning**: Easy to experiment with optimal feedback patterns

### **Practical Tuning Examples**

- **High-stress environments**: Reduce flash durations, speed up breathing cycles
- **Calm workspaces**: Longer breathing cycles, slower error patterns
- **Accessibility needs**: Longer flash durations for easier perception
- **Battery optimization**: Adjust cycle durations to balance feedback vs power consumption

### **Consistency Benefits**

- **Single configuration approach**: All timing in one place
- **Maintainable code**: No hardcoded magic numbers scattered through codebase
- **Documentation sync**: Config values match implementation exactly

---

## **State-Specific Implementations**

### **Idle State**

- **Strong Blue (0,0,255)** with **breathing effect**
- Fade in/out cycles (CONFIG_IDLE_BREATHING_CYCLE)
- Indicates "ready and waiting"

### **Tag Events & Flow Awareness**

**Initial Tag Detection:**

- **Green flash**: Tag detected, payload processing (CONFIG_TAG_FLASH_DURATION)
- **Solid Green**: Webhook successful, active session started

**Active Session Flow States:**

- **Solid Green**: Normal active session (0-60 minutes)
- **Orange breathing (4-7-8 pattern)**: 60+ minutes - mindful awareness invitation
  - Configurable inhale → hold → exhale cycle (CONFIG_BREATHING_*)
  - Replaces green state during breathing sequence
- **Orange pulsating**: 90+ minutes - gentle rest suggestion
  - Pulse cycles (CONFIG_URGENCY_PULSE_DURATION) with gentle urgency
  - Replaces green state during pulsing

**Session End:**

- **Green flash**: Tag removed, payload processing (CONFIG_TAG_FLASH_DURATION)
- **Blue breathing**: Return to idle state after webhook successful

### **WiFi Connection States**

- **Blue blinking**: Attempting connection (CONFIG_WIFI_BLINK_INTERVAL)
- **Solid Blue**: Connected and ready

### **AP Mode Sequence**

**Pattern**: `Y→B→P→P` (fully configurable cycle)

- **Yellow**: "Warning - no WiFi available" fast flash (CONFIG_AP_MODE_WARNING_FLASH_INTERVAL)
- **Blue**: "Trying to connect" (CONFIG_AP_MODE_BLUE_DURATION)
- **Purple**: "AP mode active" (CONFIG_AP_MODE_PURPLE_DURATION - emphasized)
- **Total cycle**: CONFIG_AP_MODE_TOTAL_CYCLE

### **Webhook Error Patterns**

- **RED/Green alternating**: "Error sending to webhook" (CONFIG_ERROR_BLINK_INTERVAL)
- **Yellow/Green alternating**: "Warning: queued for webhook" (CONFIG_WARNING_BLINK_INTERVAL)
- **Pattern duration**: CONFIG_ERROR_PATTERN_DURATION

### **System Errors**

- **Red**: Hardware failures, critical errors
- **Yellow**: Configuration issues, non-critical warnings

---

## **Visual Communication Logic**

**The pattern is**: `[ERROR_TYPE] + [SYSTEM_AFFECTED]`

Examples:

- `RED + GREEN` = Error from webhook system
- `YELLOW + BLUE` = Warning from communication system  
- `YELLOW + BLUE + PURPLE` = Warning → from WiFi → AP fallback
- `ORANGE` = Cognitive wellness timing system active

---

## **Flow Awareness Implementation**

### **Complete Timing Configuration (kconfig)**

```kconfig
// === BASIC OPERATIONS ===
CONFIG_TAG_FLASH_DURATION            = 200ms      // Tag detection/removal flash
CONFIG_IDLE_BREATHING_CYCLE          = 2000ms     // Idle blue breathing cycle
CONFIG_SESSION_TIMEOUT               = 300000ms   // Tag absence = session end

// === FLOW AWARENESS ===
CONFIG_FLOW_AWARENESS_60MIN_TIMER    = 3600000ms  // 60 minutes - breathing trigger
CONFIG_FLOW_AWARENESS_90MIN_TIMER    = 5400000ms  // 90 minutes - urgency trigger
CONFIG_BREATHING_INHALE_DURATION     = 4000ms     // 4-7-8 breathing: inhale
CONFIG_BREATHING_HOLD_DURATION       = 7000ms     // 4-7-8 breathing: hold
CONFIG_BREATHING_EXHALE_DURATION     = 8000ms     // 4-7-8 breathing: exhale
CONFIG_URGENCY_PULSE_DURATION        = 2000ms     // 90min+ pulsing cycle

// === CONNECTION STATES ===
CONFIG_WIFI_BLINK_INTERVAL           = 500ms      // WiFi connection attempt blink
CONFIG_AP_MODE_WARNING_FLASH_INTERVAL = 100ms      // AP sequence: warning flash speed
CONFIG_AP_MODE_BLUE_DURATION         = 1000ms     // AP sequence: attempt phase  
CONFIG_AP_MODE_PURPLE_DURATION       = 2000ms     // AP sequence: active phase
CONFIG_AP_MODE_TOTAL_CYCLE           = 4000ms     // Complete AP mode cycle

// === ERROR PATTERNS ===
CONFIG_ERROR_BLINK_INTERVAL          = 1000ms     // Red/Green error alternating
CONFIG_WARNING_BLINK_INTERVAL        = 1500ms     // Yellow/Green warning alternating
CONFIG_ERROR_PATTERN_DURATION        = 10000ms    // How long to show error pattern
```

### **Session Definition**

- **Session Start**: First tag detection after idle
- **Session Continue**: Tag present or absent <5 minutes
- **Session End**: Tag absent >5 minutes → reset all timers

### **State Priority** (single LED system)

1. **System Errors** (Red/Yellow) - highest priority
2. **Flow Awareness** (Orange) - overrides normal operations
3. **Normal Operations** (Green) - baseline active state
4. **Connectivity** (Blue) - idle/connection states
5. **Special States** (Purple) - AP mode

---

## **Future Enhancement Options**

### **Dual LED Configuration**

- **Primary LED**: Maintains current system (operations/errors)
- **Secondary LED**: Dedicated cognitive wellness indicator
- **Allows**: Simultaneous green (active) + orange (flow awareness)

### **Advanced Flow Patterns**

- **Breathing sync**: Match user's actual breath rate
- **Circadian integration**: Adjust timing based on time of day
- **Personal calibration**: Learn individual optimal flow periods

---

## **Design Philosophy Principles**

1. **Intuitive Visual Language**: Color combinations tell clear stories
2. **Non-Disruptive Awareness**: Information without interruption  
3. **Escalating Guidance**: Gentle suggestions that increase appropriately
4. **Biological Respect**: Technology that works with human rhythms
5. **Configurable Intelligence**: Adaptable to individual needs
