# NFC Time Tracking Device with M5Stamp C3U

## Overview

This project implements an NFC-based time tracking device using the M5Stamp C3U microcontroller and PN532 RFID module. The device tracks work sessions by detecting NFC tags and sends event data to n8n via webhooks.

## Why This Matters

Tracking work sessions accurately is crucial for productivity and project management. This device provides a reliable and automated way to log work hours using NFC technology.

## Our Approach

We focus on three key elements:

1. **Real-time NFC Detection**: Immediate response to NFC tag interactions
2. **WiFi Connectivity**: Seamless integration with web services
3. **Health Monitoring**: Continuous device health metrics

## Features

- Real-time NFC tag detection with multiple tag type support
- Multi-network WiFi connectivity with automatic selection
- LED status indicators for visual feedback
- Webhook integration with n8n for workflow automation
- Automatic health metrics in every event
- NTP time synchronization

## Current Status

The project is currently in Phase 2 of development. See [progress.md](./progress.md) for detailed implementation status and upcoming features.

## Documentation

- [Getting Started Guide](./docs/guides/getting-started.md) - Setup instructions and configuration
- [Progress Tracking](./progress.md) - Development phases and implementation status
- [Database Setup](./SQLquery.md) - Supabase integration for event storage

## License

Copyright 2025 Nicolas de Barquin

Licensed under the Apache License, Version 2.0. See `./LICENSE` for the full license text.

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](./docs/CONTRIBUTING.md) for guidelines.

For community standards and expectations, refer to [CODE_OF_CONDUCT.md](./CODE_OF_CONDUCT.md)

[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](CODE_OF_CONDUCT.md)

## Contact

[@nicolasdb](https://github.com/nicolasdb)
