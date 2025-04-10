# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands
- Build project: `pio run`
- Upload to device: `pio run -t upload`
- Monitor serial output: `pio run -t monitor`
- Clean build files: `pio run -t clean`
- Build and upload in one step: `pio run -t upload -t monitor`

## Code Style Guidelines
- Use camelCase for variables and methods
- Use PascalCase for class names
- Use UPPER_SNAKE_CASE for constants
- 4-space indentation
- Opening braces on same line as declaration
- Descriptive comments preceding functions
- Meaningful variable/function names

## Error Handling
- Use DEBUG_SERIAL for error reporting
- Provide visual feedback with LED indicators
- Check return values of all hardware operations
- Include context in error messages

## Configuration
- All configuration parameters in config.h
- Use #define for constants
- Use consistent naming patterns in config.h

## Module Organization
- Header (.h) files for interface definitions
- Implementation (.cpp) files for logic
- Keep modules with clear single responsibilities