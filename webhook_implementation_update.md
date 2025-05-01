# Webhook Manager Implementation Updates

## Summary of Changes

This update improves the webhook manager implementation:

1. **Removed `main_webhook.c`** - Now only using `main.c` as the single entry point
2. **Implemented Kconfig configuration** - Settings now configured via menuconfig
3. **Removed JSON configuration** - No longer using webhook_config.json for settings
4. **Fixed URL setting** - Now using the URL specified in Kconfig
5. **Improved error handling** - Better logging and error management

## Kconfig Configuration

The webhook manager now uses ESP-IDF's Kconfig system for configuration. Settings are available in:
```
idf.py menuconfig
# Navigate to: Time Tracker Configuration → Webhook Manager Configuration
```

These settings are defined in `/components/webhook_manager/Kconfig` and include:
- `WEBHOOK_MANAGER_URL`: Target URL for webhook events
- `WEBHOOK_MANAGER_MAX_RETRIES`: Number of retries for failed transmissions
- `WEBHOOK_MANAGER_RETRY_DELAY_MS`: Time between retry attempts
- `WEBHOOK_MANAGER_MAX_LOG_ENTRIES`: Number of events to store in memory
- `WEBHOOK_MANAGER_DEBUG`: Enable extra debugging information

## Default Configuration

Default settings are in `sdkconfig.defaults.webhook`:
```
CONFIG_WEBHOOK_MANAGER_URL="http://nicolasdb.eu/webhook"
CONFIG_WEBHOOK_MANAGER_MAX_RETRIES=3
CONFIG_WEBHOOK_MANAGER_RETRY_DELAY_MS=5000
CONFIG_WEBHOOK_MANAGER_MAX_LOG_ENTRIES=50
CONFIG_WEBHOOK_MANAGER_DEBUG=n
```

## Future Enhancement Plans

For future releases, we plan to implement a dual-layer configuration approach:
1. **Compile-time configuration** via Kconfig (current implementation)
2. **Runtime configuration** via LittleFS JSON file that can override Kconfig defaults

This would allow configuration changes without recompiling and provide flexibility for:
- Shipping firmware with reasonable defaults via Kconfig
- Allowing customization in the field via the JSON file
- Potentially configuring via the web interface

## Building and Testing

To build with these changes:
```bash
# Apply the webhook defaults
cat sdkconfig.defaults.webhook >> sdkconfig

# Or configure manually
idf.py menuconfig

# Build and flash
idf.py build
idf.py -p PORT flash monitor
```

Monitor the logs to verify that events are now being sent to the correct webhook URL.
