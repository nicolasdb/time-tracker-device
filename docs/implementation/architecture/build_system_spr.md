# Build System & Deployment [SPR]
*Compressed build knowledge from architecture insights and mission brief*

## PlatformIO Integration
```
CONFIG: platformio.ini|esp32c3_mcp|espressif32-platform|espidf-framework
BUILD-FLAGS: -I-tools/tool/component/internal|private-include-resolution
LIB-DIRS: tools|managed_components|self-contained-tools-only
BOARD: esp32-c3-devkitm-1|target-hardware-specific
```

## ESP-IDF Component System
```
COMPONENTS: idf_component_register|SRCS|INCLUDE_DIRS|PRIV_INCLUDE_DIRS
DEPENDENCIES: managed-components|idf_component.yml|version-pinning
RESOLUTION: PRIV_INCLUDE_DIRS|private-headers|embedded-deps
REGISTRATION: self-contained|no-external-components-dir
```

## Managed Dependencies
```
LITTLEFS: joltwallet/littlefs-^1.14.8|fs-tool-dependency
LED-STRIP: espressif/led_strip-^3.0.0|feedback-tool-WS2812B
COMPONENT-MANAGER: ESP-IDF-automatic|version-resolution|conflict-detection
```

## Tool Deployment Pattern
```
STRUCTURE: tools/tool_name/include|src|CMakeLists.txt|embedded_deps|Kconfig
PACKAGING: tar-czf-tool_name_v1.0.0.tar.gz|portable-archive
INTEGRATION: extract-to-tools/|lib_extra_dirs|private-includes-if-needed
REUSABILITY: cross-project|self-contained|no-external-coupling
```

## Partition Configuration
```
FLASH-LAYOUT: 2MB-app|1536K-LittleFS|expanded-from-1MB|nvs-phy-factory-storage
PARTITIONS-CSV: factory-app-0x10000-2M|storage-data-0x210000-1.5M
USAGE: 53.8%-flash|9.6%-RAM|adequate-expansion-space
VALIDATION: production-tested|multi-tool-ecosystem|stable-operation
```

## Private Include Resolution
```
PROBLEM: embedded-components|internal-headers|not-in-public-include-path
SOLUTION: build_flags=-I-tools/tool_name/component/internal
ALTERNATIVE: PRIV_INCLUDE_DIRS|component-registration|ESP-IDF-native
PATTERN: self-contained-tools|no-external-header-dependencies
```

## Build Commands & Workflow
```
USER-ROLE: VSCode-PlatformIO-GUI|build-flash-monitor|hardware-interaction
CLAUDE-ROLE: code-analysis-only|no-direct-build-execution|guidance-provision
COMMANDS: user-builds-via-GUI|user-flashes-hardware|user-monitors-serial
COLLABORATION: Claude-provides-code|user-tests-hardware|iterative-development
```

## Tool Archive Creation
```bash
# Standard tool packaging
tar -czf tool_name_v1.0.0.tar.gz tools/tool_name/

# Cross-project deployment
tar -xzf tool_name_v1.0.0.tar.gz -C new_project/tools/
# Add to new_project/platformio.ini lib_extra_dirs = tools
```

## Validation Protocol
```
COMPILE: all-tools-building|no-missing-dependencies|private-includes-resolved
FLASH: hardware-deployment|target-ESP32-C3|partition-table-correct
RUNTIME: tool-initialization|event-coordination|stable-operation|memory-tracking
INTEGRATION: cross-tool-communication|event-system|capabilities-discovery
```

---
*References: platformio.ini, CMakeLists.txt configurations*
