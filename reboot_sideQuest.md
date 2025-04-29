Based on the log output and our code, the reboot callback is being invoked (the log message “Exiting AP mode; applying new settings and rebooting” appears), and we then call

  xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 5, NULL);

—but no reboot occurs. In our reboot_task the implementation is simply:

  vTaskDelay(3000 / portTICK_PERIOD_MS);
  esp_restart();

Yet the main loop keeps running.

There are a couple of possibilities why this happens:

1. **HTTP/Network Context Interference:**  
 The reboot_task is spawned from within the HTTP handler callback. Sometimes tasks created in the context of network event callbacks (or while the HTTP server hasn’t fully closed connections) can block the restart. If any resource isn’t fully released, the restart may be delayed indefinitely.

2. **Task Scheduling Issues:**  
 It’s possible that the reboot task isn’t actually given CPU time (or its logs aren’t flushed). Although it has a higher priority (5) than the main app’s loop, if the system is busy managing the network stack or HTTP server (or if the task isn’t scheduled at all), the reboot code may not execute.

**Suggested Fix – Use an Independent Timer for Rebooting:**

A solution is to schedule the reboot outside the HTTP/task context by using an ESP timer. For example, change your exit callback to create a one-shot timer that, after 3 seconds, calls esp_restart:

````c
#include "esp_timer.h"

static void reboot_timer_callback(void* arg)
{
    ESP_LOGI(TAG, "Reboot timer callback executed. Restarting now.");
    esp_restart();
}

static esp_err_t wifi_manager_exit_ap_mode_callback(void)
{
    ESP_LOGI(TAG, "Exiting AP mode; applying new settings and scheduling reboot");

    // Stop the web server and WiFi
    ap_webserver_stop();
    ESP_ERROR_CHECK(esp_wifi_stop());

    // Reset AP mode flag and clear event bits
    s_ap_mode_active = false;
    xEventGroupClearBits(s_wifi_event_group, WIFI_AP_STARTED_BIT);

    // Optionally re-parse configuration and print
    esp_err_t ret = wifi_manager_parse_config(s_wifi_json_path, &s_wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse updated configuration");
    } else {
        wifi_manager_print_config(&s_wifi_config);
    }

    // Create and start a one-shot timer to reboot after 3 seconds
    const esp_timer_create_args_t timer_args = {
        .callback = reboot_timer_callback,
        .name = "reboot_timer"
    };
    esp_timer_handle_t reboot_timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &reboot_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(reboot_timer, 3000000)); // 3,000,000 us = 3 sec

    return ESP_OK;
}
````

**Explanation:**

- By moving the reboot call to an ESP timer callback, you remove any dependency on the context of the HTTP handler or the WiFi event group.
- The timer will fire in an independent context after 3 seconds and call esp_restart(), ensuring that the reboot isn’t blocked by any pending tasks in the HTTP or WiFi stack.

Try this change and check that after the POST request the timer callback log appears (e.g., “Reboot timer callback executed. Restarting now.”) and then the device restarts.