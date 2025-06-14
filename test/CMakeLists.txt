# Test Infrastructure for MCP-Inspired Refactoring
# Phase 0: Component Isolation and Integration Testing

cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF project template
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Set components for testing
set(COMPONENTS main)

# Add mock drivers for hardware abstraction
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/mocks/driver/")

# Add custom component directories for testing
list(APPEND EXTRA_COMPONENT_DIRS "../components")

# Test project name
project(time_tracker_tests)