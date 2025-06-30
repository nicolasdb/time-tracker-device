# Claude Commands Overview - Ecosystem-Aware Development

> _Custom commands optimized for ecosystem-aware development with process map authority_

## 🌐 **Ecosystem-Aware Commands**

### 🔗 `/ecosystem_context` - Complete System Understanding
**Purpose**: Load full cognitive wealth ecosystem context before architectural changes

**When to use**:
- Starting work on ecosystem integration features
- Before making interface or protocol changes
- When developing webhook server components
- Before implementing new data formats

**What it loads**:
- Complete 5-layer system architecture (Device → Server → DB → Agents → Interface)
- Device role and responsibilities in ecosystem
- Integration requirements (webhook API, database schema)
- Agent system expectations and data needs
- Architectural patterns (fs_tool similarity with webhook server)

---

### 🗺️ `/map_check` - Process Map Validation
**Purpose**: Validate proposed changes against constitutional process maps

**Usage**: `/map_check <component> <proposed_change>`

**Examples**:
```bash
/map_check "rfid_tool" "add_session_metadata"
/map_check "fs_tool" "implement_event_logging"
/map_check "main.c" "modify_boot_sequence"
```

**Validation checks**:
- Process map sequence compliance
- Tool boundary respect
- Ecosystem integration compatibility
- State machine adherence
- Error handling preservation

---

## 📊 **Development Commands**

### 🔄 `/spr_reload` - Context Loading
**Purpose**: Load SPR-compressed knowledge for development session

**Enhanced with ecosystem awareness**:
- Current phase status with ecosystem integration readiness
- MCP architecture patterns with ecosystem context
- ESP32 solutions with webhook compatibility notes
- Tool ecosystem capabilities with integration status

---

### 📈 `/phase_status` - Development Health Check
**Purpose**: Current status with ecosystem integration tracking

**Enhanced information**:
- Development phase completion with ecosystem readiness
- Tool operational status with integration capabilities
- Hardware validation with ecosystem testing results
- Next steps with ecosystem impact assessment

---

### 💾 `/save_progress` - Ecosystem-Aware Progress Tracking
**Purpose**: Document milestones with ecosystem integration impact

**Enhanced tracking**:
```bash
/save_progress "Phase 5.5" COMPLETE "FS logging implemented - webhook server integration ready"
```

**What it now tracks**:
- Device development milestones
- Ecosystem integration readiness
- Interface compatibility status
- Cross-layer impact assessment

---

## 🏛️ **Constitutional Validation Enhancement**

### 🛡️ `/validate_constitutional` - Constitutional Compliance Validation
**Purpose**: Validate existing implementation against constitutional requirements

**When to use**:
- Before modifying existing implementation
- When constitutional violations suspected  
- For compliance verification of current code
- As enhancement to existing `/map_check` validation

**Validation areas**:
- Container isolation in existing code
- Process map compliance verification
- ESP_EVENT communication validation
- Constitutional authority adherence

**Integration**: Enhances existing ecosystem workflow, does not replace

---

### 🏗️ `/container_status` - Quick Container Health Check
**Purpose**: Fast assessment of container architecture in existing implementation

**Usage**: `/container_status`

**Provides**:
- Container isolation status in current code
- ESP_EVENT communication health
- Quick coupling detection
- Implementation compliance overview

**Integration**: Complements existing `/phase_status` with constitutional focus

---

## 🔄 **Enhanced Ecosystem-Aware Workflow**

### 🚀 **Starting Development Session**
1. **`/ecosystem_context`** → Understand device role in complete system
2. **`/spr_reload`** → Load technical patterns and current state  
3. **`/phase_status`** → Check current status and readiness
4. **`/container_status`** → Quick constitutional health check (optional)

### 🛠️ **Making Changes to Existing Implementation**
1. **`/map_check "component" "change"`** → Validate against process maps (primary)
2. **`/validate_constitutional`** → Check constitutional compliance (enhancement)
3. **Review ecosystem impact** → Consider webhook server, agents, database
4. **Proceed when validated** → Process map + constitutional compliance

### 🔄 **During Development**
- **`/phase_status`** → Quick health checks with ecosystem status (primary)
- **`/map_check`** → Validate architectural adjustments (primary)
- **`/container_status`** → Quick constitutional checks (enhancement)
- **Follow process maps** → Constitutional authority as defined in existing workflow

### 📈 **Completing Phases**
- **`/save_progress "phase" STATUS "achievement"`** → Document progress (existing pattern)
- **`/validate_constitutional`** → Optional constitutional validation
- **`/ecosystem_context`** → Verify readiness for next ecosystem layer

---

## 🎯 **Process Map Authority Integration**

### **Constitutional Documents**
Process maps in `/docs/constitution/process_maps/` are **constitutional authority** - they define fundamental system behavior:

- **`01_device_master_fsm.mmd`** → Complete device behavior with esp_event hub
- **`07_tag_detection_fsm.mmd`** → RFID tag detection lifecycle
- **`08_tag_event_fsm.mmd`** → Event processing and formatting
- **`11_feedback_fsm.mmd`** → Visual feedback state management
- **`13_payload_fsm.mmd`** → Payload creation with NTP dependency
- **`14_http_fsm.mmd`** → HTTP communication with retry logic

### **Process Map Rules**
1. **NEVER modify** during implementation phases
2. **ALWAYS validate** changes with `/map_check`
3. **FOLLOW exactly** during development
4. **UPDATE only** during architectural reviews

### **Ecosystem Integration Rules**
1. **Consider impact** on webhook server interface
2. **Maintain compatibility** with agent expectations
3. **Preserve data formats** for database integration
4. **Document changes** with ecosystem context

---

## 🏗️ **Benefits of Ecosystem-Aware Command System**

### ⚡ **Enhanced Development Speed**
- **Context loading**: Ecosystem awareness in seconds
- **Validation**: Automated process map compliance checking
- **Integration**: Clear understanding of system dependencies
- **Documentation**: Automatic ecosystem impact tracking

### 🎯 **Improved Development Quality**
- **Architectural consistency**: Process maps prevent drift
- **Integration confidence**: Ecosystem requirements validated
- **Future-proofing**: Changes consider multi-layer impact
- **Knowledge preservation**: SPR maintains institutional memory

### 🔄 **Sustainable Development Process**
- **Session continuity**: Commands bridge context windows
- **Team scalability**: Clear workflows for collaboration
- **System evolution**: Ecosystem context guides decisions
- **Documentation quality**: Automatic consistency maintenance

---

## 📚 **Command Reference Files**

### **Core Command Documentation**
- `/ecosystem_context.md` → Complete system understanding
- `/map_check.md` → Process map validation guide
- `/save_progress.md` → Progress tracking workflow
- `/spr_reload.md` → Context loading mechanism

### **Supporting Documentation**
- `docs/ecosystem/` → Complete system architecture
- `docs/constitution/process_maps/` → Process maps (constitutional authority)
- `docs/architecture/` → SPR-compressed technical patterns
- `docs/project/` → Current development status

### **Integration Points**
- `CLAUDE.md` → Development context and authority
- `README.md` → Ecosystem discovery for new developers
- `docs/SPR_GUIDE.md` → SPR format explanation
- `docs/MAINTENANCE_STRATEGY.md` → Documentation workflow

---

## 🚀 **Advanced Usage Patterns**

### **Ecosystem Integration Development**
```bash
# 1. Load complete system context
/ecosystem_context

# 2. Validate major interface changes  
/map_check "http_tool" "modify_webhook_payload"

# 3. Implement following process maps exactly
# [Development work with constitutional authority]

# 4. Track progress with ecosystem impact
/save_progress "Phase 5.5" COMPLETE "Webhook integration ready"
```

### **Cross-Layer Development Planning**
```bash
# 1. Understand current ecosystem readiness
/phase_status

# 2. Load patterns for next layer development
/spr_reload

# 3. Plan changes considering system impact
/ecosystem_context

# 4. Validate architectural decisions
/map_check "system" "add_webhook_server"
```

### **Multi-Session Development Continuity**
```bash
# Session Start
/ecosystem_context     # System understanding
/spr_reload           # Technical patterns
/phase_status         # Current state

# Session Work
/map_check [changes]  # Validate decisions

# Session End  
/save_progress [milestone] # Document with ecosystem context
```

---

## 🔮 **Future Command Evolution**

### **Planned Enhancements**
- **`/integration_test`** → Validate device-server communication
- **`/ecosystem_deploy`** → Coordinate multi-layer deployments  
- **`/agent_compatibility`** → Check data format compatibility
- **`/system_health`** → Monitor complete ecosystem status

### **Advanced Automation**
- **Automatic validation** → Process map compliance checking
- **Integration testing** → End-to-end ecosystem validation
- **Configuration management** → Dynamic settings coordination
- **Performance monitoring** → Cross-layer optimization

---

*Optimized for ecosystem-aware development with constitutional process map authority and seamless multi-layer integration*
