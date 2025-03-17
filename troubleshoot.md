# M5Stamp-C3U with PN532 RFID Troubleshooting Guide

This guide provides troubleshooting steps for common issues with the M5Stamp-C3U RFID project.

## Hardware Setup

### PN532 Module Configuration

**Critical: Set the Interface Switch to I2C Mode**

The PN532 module has a physical interface selection switch or jumpers that must be set correctly:
- Ensure the switch is set to **I2C mode** (not SPI or HSU/UART)
- This is the most common cause of initialization failures

![PN532 Interface Switch](https://i.imgur.com/example-image.jpg)

### Wiring Connections

Verify the following connections between the M5Stamp-C3U and PN532:

| M5Stamp-C3U Pin | PN532 Pin | Notes |
|-----------------|-----------|-------|
| 4 (GPIO4)       | SDA       | I2C Data Line |
| 3 (GPIO3)       | SCL       | I2C Clock Line |
| 3.3V            | VCC       | Power (3.3V only, not 5V) |
| GND             | GND       | Ground |

**Note:** The IRQ and RESET pins are not used in this project (set to -1 in the code).

## LED Status Indicators

The built-in NeoPixel LED provides status information:

| Color | Pattern | Meaning |
|-------|---------|---------|
| Red | Blinking | Error condition (PN532 initialization failure, WiFi connection failure) |
| Blue | Solid | WiFi connected |
| Blue | Blinking | WiFi connecting |
| Green | Solid | Tag present and detected |
| Purple | Brief flash | Time synced with NTP server |

## Serial Monitor Issues

### Configuration

- Ensure monitor speed is set to **115200** baud (as defined in config.h and platformio.ini)
- USB CDC must be enabled (already set in platformio.ini with these flags):
  ```
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1
  ```

### No Serial Output

If you don't see any serial output:

1. Check if the correct COM port is selected
2. Try pressing the reset button after the serial monitor is open
3. Verify the USB cable supports data (not just power)
4. Try a different USB port or cable
5. Ensure the ESP32-C3 USB driver is installed on your computer

## WiFi Connectivity

### Credential Verification

- Double-check SSID and password in credentials.h
- Verify the network is 2.4GHz (ESP32-C3 doesn't support 5GHz)
- Check if the network is visible in range

### Signal Strength

- Position the device closer to the WiFi router during testing
- Poor signal strength can cause intermittent connectivity issues

## I2C Communication

### Pin Configuration

- The project uses GPIO4 (SDA) and GPIO3 (SCL) for I2C communication
- If using a different board layout, update these in config.h:
  ```cpp
  #define PN532_SDA 4
  #define PN532_SCL 3
  ```

### Clock Speed

- The code sets I2C clock to 100kHz (standard mode)
- If experiencing reliability issues, try lowering the clock speed:
  ```cpp
  Wire.setClock(50000); // Try 50kHz instead of 100kHz
  ```

### I2C Troubleshooting

1. Use an I2C scanner sketch to verify the PN532 is detected
2. Check for proper pull-up resistors on SDA/SCL lines
3. Keep I2C wires short and away from interference sources
4. Try adding a small delay (10-50ms) between I2C operations

## General Debugging Tips

### Minimal Test Code

Test with a minimal sketch to isolate issues:

```cpp
#include <Wire.h>
#include <Adafruit_PN532.h>

#define PN532_SDA 4
#define PN532_SCL 3
#define LED_PIN 2

Adafruit_PN532 nfc(-1, -1);  // Use I2C, disable IRQ and RESET pins

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("PN532 I2C Test");
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // Turn on LED
  
  Wire.begin(PN532_SDA, PN532_SCL);
  Wire.setClock(100000);
  
  if (!nfc.begin()) {
    Serial.println("Couldn't find PN532 board!");
    while (1) {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
  }
  
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Couldn't find PN532 board!");
    while (1) {
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
    }
  }
  
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  nfc.SAMConfig();
  Serial.println("Setup complete!");
  digitalWrite(LED_PIN, LOW);  // Turn off LED
}

void loop() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;
  
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  if (success) {
    Serial.println("Found a card!");
    Serial.print("UID Length: "); Serial.print(uidLength, DEC);
    Serial.println(" bytes");
    Serial.print("UID Value: ");
    for (uint8_t i=0; i < uidLength; i++) {
      Serial.print(" 0x"); Serial.print(uid[i], HEX);
    }
    Serial.println("");
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
  }
  
  delay(1000);
}
```

### Component Isolation

1. Test the M5Stamp-C3U alone with a simple blink sketch
2. Test the PN532 with a basic read example
3. Test WiFi connectivity separately
4. Add components one by one to identify the problematic part

### Firmware and Library Versions

- Ensure you're using compatible versions of:
  - ESP32 Arduino core
  - Adafruit PN532 library (v1.3.4 as specified in platformio.ini)
  - Adafruit NeoPixel library (v1.12.4 as specified in platformio.ini)

## Power Considerations

- Ensure stable power supply (USB power might be insufficient for both ESP32 and PN532)
- Consider adding a capacitor (100μF) between VCC and GND to stabilize power
- If using battery power, ensure adequate voltage and current capacity

## Reset Procedure

If the device is in an unknown state:

1. Disconnect power
2. Hold the reset button (if available)
3. Reconnect power while holding reset
4. Release reset after 2 seconds
5. Monitor serial output for initialization messages

## Common Error Scenarios

### PN532 Not Detected

**Symptoms:**
- Red blinking LED
- "Error: Could not initialize PN532!" or "Error: Did not find PN532 board!" in serial output

**Solutions:**
- Check interface switch setting (must be I2C)
- Verify wiring connections
- Try a different PN532 module
- Check power supply stability

### WiFi Connection Failure

**Symptoms:**
- Red blinking LED after initial blue blinking
- "Failed to connect to [SSID]" in serial output

**Solutions:**
- Verify credentials
- Check network availability
- Position closer to router
- Try connecting to a different network

### Serial Monitor Not Working

**Symptoms:**
- No output in serial monitor
- Unable to see debug messages

**Solutions:**
- Check baud rate (115200)
- Verify USB connection
- Try a different USB cable
- Reinstall USB drivers
