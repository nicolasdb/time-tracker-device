# Getting Started with NFC Time Tracking Device

This guide helps you set up and use the NFC Time Tracking Device with M5Stamp C3U.

## Hardware Setup

1. **Components Required**
   - M5Stamp C3U
   - PN532 NFC Module
   - Connecting wires
   - USB cable for power/programming

2. **Wiring Connections**
   - I2C SDA: Pin 4
   - I2C SCL: Pin 3
   - LED: Pin 2 (built-in)

3. **Power Supply**
   - Connect via USB for development
   - For deployment, a 5V power supply is recommended

## Software Setup

1. **Development Environment**
   - Install PlatformIO (recommended)
   - Clone this repository
   - Open the project in PlatformIO

2. **Configuration**
   - Create `src/credentials.h` with your settings:

   ```cpp
   #ifndef CREDENTIALS_H
   #define CREDENTIALS_H

   // WiFi Networks Configuration
   struct WiFiNetwork {
       const char* ssid;
       const char* password;
   };

   const WiFiNetwork WIFI_NETWORKS[] = {
       {"YourHomeWiFi", "password1"},
       {"YourOfficeWiFi", "password2"},
       {"YourMobileHotspot", "password3"}
       // Add more networks as needed
   };

   const int WIFI_NETWORKS_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WiFiNetwork);

   // Webhook Configuration
   #define WEBHOOK_URL "http://your-server:port/webhook/your-webhook-id"

   #endif // CREDENTIALS_H
   ```

3. **Upload the Code**
   - Connect the M5Stamp C3U to your computer
   - Build and upload the code using PlatformIO
   - Monitor serial output for debugging

## Using the Device

1. **LED Indicators**
   - Blue: Waiting for tag
   - Green: Tag present
   - Red: Error state (WiFi disconnection, NFC module not found)

2. **NFC Tag Interaction**
   - Place an NFC tag on the reader to start tracking
   - Remove the tag to end tracking
   - The device sends events to the configured webhook

3. **Webhook Integration**
   - Configure n8n to process the webhook data
   - Set up workflows based on tag events
   - See SQLquery.md for database integration

## Troubleshooting

1. **Device Not Responding**
   - Check power connections
   - Verify the NFC module is properly connected
   - Monitor serial output for error messages

2. **WiFi Connection Issues**
   - Verify WiFi credentials in credentials.h
   - Ensure the device is within range of the WiFi networks
   - Check if the networks require additional authentication

3. **Webhook Not Receiving Data**
   - Verify the webhook URL is correct
   - Check if the server is accessible from the device's network
   - Monitor serial output for webhook response codes

## Need Help?

1. Check [progress.md](../../progress.md) for current development status
2. Review [CONTRIBUTING.md](../CONTRIBUTING.md) for how to contribute
3. Create an issue using the appropriate template
4. Contact [@nicolasdb](https://github.com/nicolasdb) for direct assistance
