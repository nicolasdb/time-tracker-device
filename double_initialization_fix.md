# Event Loop Initialization Fix

## Issue Description
The device was encountering an error:

```
ESP_ERROR_CHECK failed: esp_err_t 0x103 (ESP_ERR_INVALID_STATE) at 0x42009ffc
file: "components/wifi_manager/wifi_manager.c" line 153
func: wifi_manager_init
expression: esp_event_loop_create_default()
```

This error occurs because the ESP-IDF components are being initialized twice:
1. Once in `main.c` 
2. And again in `wifi_manager.c`

## Applied Fixes

### 1. Improved Error Handling in Default Event Loop Creation
- Added checks for `ESP_ERR_INVALID_STATE` when creating the default event loop
- Instead of using `ESP_ERROR_CHECK` which causes abort, the code now handles the error gracefully
- Added logging to better understand the initialization flow

### 2. Avoided Duplicate Network Interface Creation
- Added checks to prevent creating duplicate STA and AP interfaces
- First checks if interfaces already exist using `esp_netif_get_handle_from_ifkey`
- Only creates new interfaces if needed

### 3. Improved WiFi Initialization
- Added checks for `ESP_ERR_WIFI_ALREADY_INIT` when initializing WiFi
- Better error reporting with error codes and descriptions
- Prevents duplicate initialization without crashing

### 4. Enhanced Error Reporting
- Added error code descriptions in log messages
- Shows specific error names instead of generic failure messages
- Makes debugging easier when issues occur

## Key Code Patterns

The main pattern used to fix this issue is to replace:

```c
ESP_ERROR_CHECK(esp_function_that_might_fail());
```

With:

```c
esp_err_t err = esp_function_that_might_fail();
if (err != ESP_OK && err != ESP_ERR_ALREADY_INITIALIZED) {
    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(err));
    return err;
} else if (err == ESP_ERR_ALREADY_INITIALIZED) {
    ESP_LOGW(TAG, "Already initialized, continuing");
}
```

This pattern allows the code to handle initialization failures gracefully, especially in cases where components are initialized multiple times across different modules.
