# MCP Architecture Patterns [SPR]
*Compressed knowledge from 500+ lines of original documentation*

## Core Interface Pattern
```
HANDLE: tool_init→context→tool_deinit|no-static-globals|opaque-pointers
INTERFACE: capabilities-bitmask|status-health|registry-metadata|get-status
LIFECYCLE: init→register→subscribe→operate→publish→deinit
COMMUNICATION: publish-subscribe|ESP_EVENT_POST|handler-register|no-coupling
```

## Implementation Standards
```
VIOLATIONS: static-globals|direct-calls|header-includes|blocking-handlers
DEPLOYMENT: self-contained|embedded-deps|tool-archive|portable-reusable
BUILD: private-includes|managed-components|platformio-flags|no-external-deps
REGISTRY: bitmask-enum|metadata-discovery|capabilities-query|tool-version
```

## Tool Development Checklist
```
REQUIRED: handle-based|event-driven|self-contained|registry-entry
OPTIONAL: background-tasks|configuration|health-monitoring|status-export
TESTING: isolation-first|integration-second|hardware-validation-final
DEPENDENCIES: embedded-within-tool|idf-component-yml|managed-deps-only
```

## Universal Tool Context Structure
```c
typedef struct {
    tool_config_t config;
    tool_capabilities_t capabilities;  // Bitmask enumeration
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    // Tool-specific state...
} tool_context_t;
```

## Event-Driven Communication
```
EVENTS: ESP_EVENT_DEFINE_BASE|esp_event_post|handler_register
PATTERN: publish-state-changes|subscribe-to-dependencies|no-direct-calls
TIMING: immediate-return|no-blocking|event-loop-safe
```

## Proven Tool Capabilities
```
feedback_tool: 0x1F (PRIORITY_QUEUE|AUTO_EXPIRE|THREAD_SAFE|LED_CONTROL)
wifi_tool: 0x7F (STA|AP|MULTI_NETWORK|EVENT_PUBLISH|AUTO_CONNECT|CONFIG_MGMT|HEALTH_MONITOR)
rfid_tool: 0x6F (TAG_DETECTION|AUTO_SCAN|EVENT_PUBLISH|UID_EXTRACTION|TYPE_DETECTION|HEALTH_MONITOR)
fs_tool: 0xFF (MOUNT|JSON_CONFIG|JSON_LOGS|EVENT_PUBLISH|HEALTH_MONITOR)
webhook_tool: 0x9F (HTTP_POST|RETRY_QUEUE|EVENT_SUBSCRIBE|JSON_PAYLOAD|HEALTH_MONITOR)
webserver_tool: 0x8F (HTTP_SERVER|CONFIG_MGMT|CAPTIVE_PORTAL|EVENT_SUBSCRIBE)
```

---
*References: Original docs/architecture_insights.md, CLAUDE.md, refactor_mission_brief.md*
