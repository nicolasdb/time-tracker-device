# State Ownership Categories Design [Phase 5.5.1]

> *Complete namespace allocation and ownership enforcement for 8-tool MCP system*

## Namespace Allocation Strategy

**Design principle:** Each tool owns a 256-state namespace (0x??00-0x??FF) to prevent conflicts and enable clear ownership.

### Namespace Map

| Range | Tool | Namespace Base | Owner ID | States Reserved |
|-------|------|----------------|----------|-----------------|
| `0x0000-0x00FF` | **System** | `0x0000` | `"system"` | 16 core states |
| `0x0100-0x01FF` | **WiFi Tool** | `0x0100` | `"wifi"` | 16 network states |
| `0x0200-0x02FF` | **NTP Tool** | `0x0200` | `"ntp"` | 8 time states |
| `0x0300-0x03FF` | **RFID Tool** | `0x0300` | `"rfid"` | 12 hardware states |
| `0x0400-0x04FF` | **Webhook Tool** | `0x0400` | `"webhook"` | 16 transmission states |
| `0x0500-0x05FF` | **Webserver Tool** | `0x0500` | `"webserver"` | 8 server states |
| `0x0600-0x06FF` | **FS Tool** | `0x0600` | `"fs"` | 12 filesystem states |
| `0x0700-0x07FF` | **Feedback Tool** | `0x0700` | `"feedback"` | 8 self-management states |

---

## Complete State Enum Design

```c
/**
 * @brief State ownership categories for 8-tool MCP system
 * Each tool owns 256-state namespace for conflict prevention
 */
typedef enum {
    // =================================================================
    // SYSTEM STATES (0x0000-0x00FF) - Owner: "system" (main.c)
    // =================================================================
    FEEDBACK_STATE_SYSTEM_BASE         = 0x0000,
    
    // Core System States
    FEEDBACK_STATE_BOOTING            = 0x0000,  ///< System boot sequence
    FEEDBACK_STATE_IDLE               = 0x0001,  ///< Normal operation (blue breathing)
    FEEDBACK_STATE_ERROR              = 0x0002,  ///< System error (red solid)
    FEEDBACK_STATE_SHUTDOWN           = 0x0003,  ///< Graceful shutdown
    
    // Initialization Sequence States
    FEEDBACK_STATE_INIT_START         = 0x0010,  ///< Initialization begun
    FEEDBACK_STATE_INIT_FS            = 0x0011,  ///< Filesystem initialization
    FEEDBACK_STATE_INIT_WIFI_PREP     = 0x0012,  ///< WiFi preparation
    FEEDBACK_STATE_INIT_TIME          = 0x0013,  ///< Time sync preparation
    FEEDBACK_STATE_INIT_WEBHOOK       = 0x0014,  ///< Webhook initialization
    FEEDBACK_STATE_INIT_RFID          = 0x0015,  ///< RFID initialization
    FEEDBACK_STATE_INIT_COMPLETE      = 0x0016,  ///< All tools initialized
    
    // Tool Registry States
    FEEDBACK_STATE_TOOL_REGISTERED    = 0x0020,  ///< Tool registration success
    FEEDBACK_STATE_TOOL_ERROR         = 0x0021,  ///< Tool registration error
    FEEDBACK_STATE_TOOL_DISCONNECTED  = 0x0022,  ///< Tool communication failure
    
    // =================================================================
    // WIFI TOOL STATES (0x0100-0x01FF) - Owner: "wifi"
    // =================================================================
    FEEDBACK_STATE_WIFI_BASE          = 0x0100,
    
    // Connection States
    FEEDBACK_STATE_WIFI_CONNECTING    = 0x0100,  ///< Attempting connection (blue blinking)
    FEEDBACK_STATE_WIFI_CONNECTED     = 0x0101,  ///< Connected to network (blue solid)
    FEEDBACK_STATE_WIFI_FAILED        = 0x0102,  ///< Connection failed (red blink)
    FEEDBACK_STATE_WIFI_DISCONNECTED  = 0x0103,  ///< Disconnected from network
    
    // AP Mode States
    FEEDBACK_STATE_WIFI_AP_MODE       = 0x0110,  ///< AP mode active (orange blinking)
    FEEDBACK_STATE_WIFI_AP_CLIENT_CONNECTED = 0x0111,  ///< Client connected to AP
    FEEDBACK_STATE_WIFI_AP_STOPPING   = 0x0112,  ///< AP mode stopping
    
    // Configuration States
    FEEDBACK_STATE_WIFI_CONFIG_LOADING = 0x0120,  ///< Loading config from FS
    FEEDBACK_STATE_WIFI_CONFIG_ERROR   = 0x0121,  ///< Config load failed
    
    // =================================================================
    // NTP TOOL STATES (0x0200-0x02FF) - Owner: "ntp"
    // =================================================================
    FEEDBACK_STATE_NTP_BASE           = 0x0200,
    
    // Time Sync States
    FEEDBACK_STATE_TIME_SYNCING       = 0x0200,  ///< Syncing with NTP server
    FEEDBACK_STATE_TIME_SYNCED        = 0x0201,  ///< Time sync successful
    FEEDBACK_STATE_TIME_SYNC_FAILED   = 0x0202,  ///< Time sync failed
    FEEDBACK_STATE_TIME_UPDATING      = 0x0203,  ///< Periodic time update
    
    // =================================================================
    // RFID TOOL STATES (0x0300-0x03FF) - Owner: "rfid"
    // =================================================================
    FEEDBACK_STATE_RFID_BASE          = 0x0300,
    
    // Hardware States
    FEEDBACK_STATE_RFID_INITIALIZING  = 0x0300,  ///< Hardware initialization
    FEEDBACK_STATE_RFID_ACTIVE        = 0x0301,  ///< Ready for scanning
    FEEDBACK_STATE_RFID_ERROR         = 0x0302,  ///< Hardware error
    FEEDBACK_STATE_RFID_DISABLED      = 0x0303,  ///< Scanning disabled
    
    // Tag Detection States
    FEEDBACK_STATE_TAG_DETECTED       = 0x0310,  ///< Tag on reader (green solid)
    FEEDBACK_STATE_TAG_READ_ERROR     = 0x0311,  ///< Tag read failed
    FEEDBACK_STATE_TAG_PROCESSING     = 0x0312,  ///< Tag data processing
    
    // =================================================================
    // WEBHOOK TOOL STATES (0x0400-0x04FF) - Owner: "webhook"
    // =================================================================
    FEEDBACK_STATE_WEBHOOK_BASE       = 0x0400,
    
    // Transmission States
    FEEDBACK_STATE_WEBHOOK_SENDING    = 0x0400,  ///< Sending HTTP request
    FEEDBACK_STATE_WEBHOOK_SUCCESS    = 0x0401,  ///< Transmission successful
    FEEDBACK_STATE_WEBHOOK_ERROR      = 0x0402,  ///< Transmission failed
    FEEDBACK_STATE_WEBHOOK_QUEUED     = 0x0403,  ///< Queued for retry
    FEEDBACK_STATE_WEBHOOK_TIMEOUT    = 0x0404,  ///< Request timeout
    
    // Phase 5.6: Session Intelligence States
    FEEDBACK_STATE_WEBHOOK_READING_LOGS = 0x0410,  ///< Reading FS logs
    FEEDBACK_STATE_WEBHOOK_CALCULATING_SESSION = 0x0411,  ///< Processing session data
    FEEDBACK_STATE_WEBHOOK_BUILDING_PAYLOAD = 0x0412,  ///< Creating enhanced payload
    
    // =================================================================
    // WEBSERVER TOOL STATES (0x0500-0x05FF) - Owner: "webserver"
    // =================================================================
    FEEDBACK_STATE_WEBSERVER_BASE     = 0x0500,
    
    // Server States
    FEEDBACK_STATE_WEBSERVER_STARTING = 0x0500,  ///< Starting HTTP server
    FEEDBACK_STATE_WEBSERVER_ACTIVE   = 0x0501,  ///< Server running
    FEEDBACK_STATE_WEBSERVER_ERROR    = 0x0502,  ///< Server error
    FEEDBACK_STATE_WEBSERVER_STOPPING = 0x0503,  ///< Graceful shutdown
    
    // =================================================================
    // FS TOOL STATES (0x0600-0x06FF) - Owner: "fs" 
    // =================================================================
    FEEDBACK_STATE_FS_BASE            = 0x0600,
    
    // Phase 5.5: Event Logging States
    FEEDBACK_STATE_FS_WRITING_RFID_LOG = 0x0600,  ///< Writing RFID event to JSON
    FEEDBACK_STATE_FS_WRITING_NTP_LOG = 0x0601,   ///< Writing NTP event to JSON
    FEEDBACK_STATE_FS_READING_LOGS    = 0x0602,   ///< Reading log files
    FEEDBACK_STATE_FS_LOG_ERROR       = 0x0603,   ///< Log operation failed
    
    // Filesystem States
    FEEDBACK_STATE_FS_MOUNTING        = 0x0610,   ///< Mounting filesystem
    FEEDBACK_STATE_FS_MOUNTED         = 0x0611,   ///< Filesystem ready
    FEEDBACK_STATE_FS_UNMOUNTING      = 0x0612,   ///< Unmounting filesystem
    FEEDBACK_STATE_FS_DISK_FULL       = 0x0613,   ///< Storage space exhausted
    FEEDBACK_STATE_FS_CORRUPTION      = 0x0614,   ///< Filesystem corruption detected
    
    // =================================================================
    // FEEDBACK TOOL SELF-STATES (0x0700-0x07FF) - Owner: "feedback"
    // =================================================================
    FEEDBACK_STATE_FEEDBACK_BASE      = 0x0700,
    
    // Queue Management States
    FEEDBACK_STATE_QUEUE_FULL         = 0x0700,   ///< Priority queue full
    FEEDBACK_STATE_QUEUE_ERROR        = 0x0701,   ///< Queue operation failed
    
    // LED Hardware States
    FEEDBACK_STATE_LED_ERROR          = 0x0710,   ///< LED hardware failure
    FEEDBACK_STATE_LED_CALIBRATING    = 0x0711,   ///< LED brightness calibration
    
    // Dashboard States
    FEEDBACK_STATE_DASHBOARD_UPDATING = 0x0720,   ///< Generating dashboard
    FEEDBACK_STATE_DASHBOARD_ERROR    = 0x0721,   ///< Dashboard generation failed
    
    // Must be last
    FEEDBACK_STATE_MAX = 0xFFFF
} feedback_state_t;
```

---

## Ownership Validation Structure

```c
/**
 * @brief State ownership mapping for validation
 */
typedef struct {
    uint16_t namespace_base;    ///< Namespace base (e.g., 0x0100 for WiFi)
    uint16_t namespace_mask;    ///< Namespace mask (0xFF00 for all tools)
    const char* owner_tool;     ///< Tool ID that owns this namespace
    const char* description;    ///< Human-readable description
} state_ownership_entry_t;

/**
 * @brief Complete ownership map for all 8 tools
 */
static const state_ownership_entry_t STATE_OWNERSHIP_MAP[] = {
    {0x0000, 0xFF00, "system",    "System core states and initialization"},
    {0x0100, 0xFF00, "wifi",      "WiFi connectivity and AP mode states"}, 
    {0x0200, 0xFF00, "ntp",       "NTP time synchronization states"},
    {0x0300, 0xFF00, "rfid",      "RFID hardware and tag detection states"},
    {0x0400, 0xFF00, "webhook",   "HTTP transmission and session intelligence"},
    {0x0500, 0xFF00, "webserver", "HTTP server and configuration interface"},
    {0x0600, 0xFF00, "fs",        "Filesystem operations and event logging"},
    {0x0700, 0xFF00, "feedback",  "LED feedback and queue management"},
    {0x0000, 0x0000, NULL,        NULL}  // Terminator
};
```

---

## Ownership Enforcement Functions

```c
/**
 * @brief Validate tool ownership of a state
 * @param state The state to validate
 * @param source_tool Tool requesting the state change
 * @return true if tool owns the state, false otherwise
 */
static bool validate_state_ownership(feedback_state_t state, const char* source_tool)
{
    if (!source_tool) {
        return false;
    }
    
    uint16_t namespace = state & 0xFF00;
    
    for (int i = 0; STATE_OWNERSHIP_MAP[i].owner_tool != NULL; i++) {
        if (STATE_OWNERSHIP_MAP[i].namespace_base == namespace) {
            return (strcmp(STATE_OWNERSHIP_MAP[i].owner_tool, source_tool) == 0);
        }
    }
    
    // Unknown namespace - reject
    return false;
}

/**
 * @brief Remove all states in the same namespace
 * @param tool Feedback tool handle
 * @param namespace Namespace to clear (e.g., 0x0100)
 */
static void remove_namespace_states(struct feedback_tool *tool, uint16_t namespace)
{
    if (!tool) return;
    
    for (uint8_t i = 0; i < tool->queue_count; i++) {
        if ((tool->state_queue[i].state & 0xFF00) == namespace) {
            // Remove this entry by shifting remaining entries
            for (uint8_t j = i; j < tool->queue_count - 1; j++) {
                tool->state_queue[j] = tool->state_queue[j + 1];
            }
            tool->queue_count--;
            i--; // Check the same index again since we shifted
        }
    }
}

/**
 * @brief Get owner tool for a namespace
 * @param namespace Namespace to query
 * @return Owner tool ID or NULL if unknown
 */
static const char* get_namespace_owner(uint16_t namespace)
{
    for (int i = 0; STATE_OWNERSHIP_MAP[i].owner_tool != NULL; i++) {
        if (STATE_OWNERSHIP_MAP[i].namespace_base == namespace) {
            return STATE_OWNERSHIP_MAP[i].owner_tool;
        }
    }
    return NULL;
}
```

---

## Enhanced Queue State Change Function

```c
/**
 * @brief Enhanced queue_state_change with ownership enforcement
 * @param tool Feedback tool handle
 * @param state State to set
 * @param priority Priority level
 * @param duration_ms Duration (0 = permanent)
 * @param source_tool Tool requesting the change
 * @return ESP_OK on success, ESP_ERR_NOT_ALLOWED on ownership violation
 */
static esp_err_t queue_state_change(struct feedback_tool *tool, 
                                   feedback_state_t state,
                                   feedback_priority_t priority,
                                   uint32_t duration_ms,
                                   const char* source_tool)
{
    if (!tool || !source_tool) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Phase 5.5.3: Ownership validation
    if (!validate_state_ownership(state, source_tool)) {
        ESP_LOGW(TAG, "Ownership violation: tool '%s' cannot set state 0x%04X (owner: %s)", 
                 source_tool, state, get_namespace_owner(state & 0xFF00));
        return ESP_ERR_NOT_ALLOWED;
    }
    
    // Take queue mutex
    if (xSemaphoreTake(tool->queue_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Phase 5.5.3: Replace same-namespace states
    uint16_t namespace = state & 0xFF00;
    remove_namespace_states(tool, namespace);
    
    // Check queue space
    if (tool->queue_count >= FEEDBACK_QUEUE_SIZE) {
        ESP_LOGW(TAG, "Queue full, cannot add state 0x%04X", state);
        xSemaphoreGive(tool->queue_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Add new state entry
    feedback_state_entry_t* entry = &tool->state_queue[tool->queue_count];
    entry->state = state;
    entry->priority = priority;
    entry->timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    entry->duration_ms = duration_ms;
    entry->source_tool = source_tool;  // Store for debugging
    
    tool->queue_count++;
    
    // Sort queue by priority (higher priority first)
    for (uint8_t i = tool->queue_count - 1; i > 0; i--) {
        if (tool->state_queue[i].priority > tool->state_queue[i-1].priority) {
            feedback_state_entry_t temp = tool->state_queue[i];
            tool->state_queue[i] = tool->state_queue[i-1];
            tool->state_queue[i-1] = temp;
        } else {
            break;
        }
    }
    
    xSemaphoreGive(tool->queue_mutex);
    
    ESP_LOGD(TAG, "State 0x%04X queued by %s (priority %d, %lums)", 
             state, source_tool, priority, duration_ms);
    
    return ESP_OK;
}
```

---

## Migration Impact Analysis

### API Changes Required

```c
// OLD API (Phase 5.4)
esp_err_t feedback_tool_set_state(feedback_tool_handle_t handle, 
                                  feedback_state_t state, 
                                  feedback_priority_t priority, 
                                  uint32_t duration_ms);

// NEW API (Phase 5.5.3+)
esp_err_t feedback_tool_set_state(feedback_tool_handle_t handle, 
                                  feedback_state_t state, 
                                  feedback_priority_t priority, 
                                  uint32_t duration_ms,
                                  const char* source_tool);  // NEW PARAMETER
```

### Tool Migration Sequence

1. **Phase 5.5.2:** Update feedback_tool.h with new enum
2. **Phase 5.5.3:** Update feedback_tool.c with ownership enforcement  
3. **Phase 5.5.4:** Update all tool calls to include source_tool parameter

### Backward Compatibility

```c
// Convenience macro for simple state changes
#define feedback_tool_set_state_simple(handle, state) \
    feedback_tool_set_state(handle, state, FEEDBACK_PRIORITY_MEDIUM, 0, __TOOL_ID__)

// Tool-specific helper (each tool defines __TOOL_ID__)
#define __TOOL_ID__ "rfid"  // In rfid_tool.c
#define __TOOL_ID__ "wifi"  // In wifi_tool.c
// etc.
```

---

## Benefits Analysis

### Conflict Prevention
- **Before:** Multiple tools could set same states → accumulation/conflicts
- **After:** Only owner can modify states in their namespace → clean separation

### Debugging Enhancement  
- **Before:** Unknown who set a state → hard to debug
- **After:** source_tool logged → clear ownership trail

### Scalability
- **Before:** Adding new states risked conflicts with existing ones
- **After:** Each tool has 256-state namespace → room for expansion

### Phase 5.5+ Foundation
- **FS Tool:** Clean ownership of logging states (0x0600-0x06FF)
- **Webhook Tool:** Session intelligence states (0x0410-0x0412) 
- **System:** Initialization coordination without tool conflicts

---

## Next Steps

1. **Phase 5.5.2:** Implement this enum in `feedback_tool.h`
2. **Phase 5.5.3:** Add ownership enforcement to `feedback_tool.c`
3. **Phase 5.5.4:** Update all 8 tools with new API + source_tool parameter
4. **Phase 5.5.5:** Implement FS tool event logging using clean state ownership

This design provides the foundation for conflict-free state management across the entire 8-tool MCP ecosystem.