## Time Tracker Color Code & Sequence Summary

### **Core Color Philosophy**
**Context-aware colors** - each color combination tells a story about **what** (issue type) affects **which system** (component).

### **Primary Color Meanings**
- **🔵 Blue**: Connectivity & Communication systems
- **🟢 Green**: Event management & Normal operations  
- **🔴 Red**: Errors & Failures
- **🟡 Yellow**: Warnings & Configuration issues
- **🟣 Purple**: Special states (AP mode)

---

### **State-Specific Implementations**

#### **Idle State**
- **Strong Blue (0,0,255)** with **breathing effect**
- Fade in/out over 2-second cycles
- Indicates "ready and waiting"

#### **Tag Events**
- **Green**: Tag detected/present
- **Blue**: Return to idle after tag removed
- **No red flash** on normal tag removal

#### **WiFi Connection States**
- **Blue blinking**: Attempting connection
- **Solid Blue**: Connected and ready

#### **AP Mode Sequence** 
**Pattern**: `Y→B→P→P` (1s, 1s, 2s cycle)
- **Yellow (1s)**: "Warning - no WiFi available"
- **Blue (1s)**: "Trying to connect" 
- **Purple (2s)**: "AP mode active" (emphasized duration)
- **Total cycle**: 4 seconds

#### **Webhook Error Patterns**
- **RED/Green alternating**: "Error sending (red) to webhook system (green)"
- **Yellow/Green alternating**: "Warning: queued (yellow) for webhook system (green)"

#### **System Errors**
- **Red**: Hardware failures, critical errors
- **Yellow**: Configuration issues, non-critical warnings

---

### **Visual Communication Logic**

**The pattern is**: `[ERROR_TYPE] + [SYSTEM_AFFECTED]`

Examples:
- `RED + GREEN` = Error from webhook system
- `YELLOW + BLUE` = Warning from com system  
- `YELLOW + BLUE + PURPLE` = Warning → from Wifi → AP fallback

This creates **intuitive visual language** where users learn to associate color combinations with specific system states and issues.