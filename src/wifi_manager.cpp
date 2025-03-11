#include "wifi_manager.h"
#include <Adafruit_NeoPixel.h>

// External reference to NeoPixel object from main.cpp
extern Adafruit_NeoPixel pixels;

WiFiManager::WiFiManager() : _isConnected(false), _timeIsSynced(false), _lastSyncTime(0), _currentSSID("") {
}

void WiFiManager::updateLEDStatus(uint32_t color) {
    pixels.setPixelColor(0, color);
    pixels.show();
}

bool WiFiManager::begin() {
    DEBUG_SERIAL.println("\nInitializing WiFi...");
    WiFi.mode(WIFI_STA);
    
    if (!connect()) {
        return false;
    }
    
    // Once connected, sync time
    DEBUG_SERIAL.println("Syncing time with NTP server...");
    if (syncTime()) {
        DEBUG_SERIAL.printf("Time synced: %s\n", getFormattedTime().c_str());
        return true;
    } else {
        DEBUG_SERIAL.println("Failed to sync time!");
        return false;
    }
}

bool WiFiManager::connectToNetwork(const char* ssid, const char* password) {
    DEBUG_SERIAL.printf("Attempting to connect to %s\n", ssid);
    
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    
    // Connection attempt loop with LED feedback
    while (WiFi.status() != WL_CONNECTED && 
           millis() - startAttemptTime < WIFI_TIMEOUT) {
        updateLEDStatus(COLOR_WIFI_CONNECTING);
        delay(LED_FAST_BLINK_INTERVAL);
        updateLEDStatus(0);
        delay(LED_FAST_BLINK_INTERVAL);
        DEBUG_SERIAL.print(".");
    }
    DEBUG_SERIAL.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        _isConnected = true;
        _currentSSID = ssid;
        updateLEDStatus(COLOR_WIFI_CONNECTED);
        DEBUG_SERIAL.printf("Connected to %s\n", ssid);
        DEBUG_SERIAL.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    
    DEBUG_SERIAL.printf("Failed to connect to %s\n", ssid);
    return false;
}

bool WiFiManager::connect() {
    if (_isConnected) {
        return true;
    }
    
    DEBUG_SERIAL.println("Scanning available networks...");
    int numNetworks = WiFi.scanNetworks();
    
    if (numNetworks == 0) {
        DEBUG_SERIAL.println("No networks found!");
        updateLEDStatus(COLOR_ERROR);
        return false;
    }
    
    // Try to connect to each configured network in order
    for (int i = 0; i < WIFI_NETWORKS_COUNT; i++) {
        const char* targetSSID = WIFI_NETWORKS[i].ssid;
        const char* targetPass = WIFI_NETWORKS[i].password;
        
        // Check if this network is visible in scan results
        bool networkFound = false;
        for (int j = 0; j < numNetworks; j++) {
            if (WiFi.SSID(j) == targetSSID) {
                networkFound = true;
                DEBUG_SERIAL.printf("Found configured network: %s\n", targetSSID);
                break;
            }
        }
        
        if (networkFound && connectToNetwork(targetSSID, targetPass)) {
            return true;
        }
    }
    
    // If we get here, all connection attempts failed
    updateLEDStatus(COLOR_ERROR);
    return false;
}

void WiFiManager::disconnect() {
    if (_isConnected) {
        WiFi.disconnect();
        _isConnected = false;
        _currentSSID = "";
        updateLEDStatus(COLOR_ERROR);
        DEBUG_SERIAL.println("WiFi disconnected");
    }
}

String WiFiManager::getIPAddress() const {
    if (_isConnected) {
        return WiFi.localIP().toString();
    }
    return "Not Connected";
}

int WiFiManager::getRSSI() const {
    if (_isConnected) {
        return WiFi.RSSI();
    }
    return 0;
}

bool WiFiManager::checkConnection() {
    bool currentStatus = WiFi.status() == WL_CONNECTED;
    
    if (_isConnected != currentStatus) {
        _isConnected = currentStatus;
        if (!_isConnected) {
            _currentSSID = "";
            updateLEDStatus(COLOR_ERROR);
            DEBUG_SERIAL.println("WiFi connection lost!");
        }
    }
    
    return _isConnected;
}

bool WiFiManager::reconnectIfNeeded() {
    if (!checkConnection()) {
        DEBUG_SERIAL.println("Attempting to reconnect...");
        return connect();
    }
    return true;
}

bool WiFiManager::isDST(struct tm timeinfo) {
    // Implement your logic to determine if DST is in effect
    // For example, in Brussels, DST starts on the last Sunday of March and ends on the last Sunday of October
    if (timeinfo.tm_mon > 2 && timeinfo.tm_mon < 9) {
        return true;
    }
    if (timeinfo.tm_mon == 2 && (timeinfo.tm_mday - timeinfo.tm_wday) >= 25) {
        return true;
    }
    if (timeinfo.tm_mon == 9 && (timeinfo.tm_mday - timeinfo.tm_wday) < 25) {
        return true;
    }
    return false;
}

bool WiFiManager::syncTime() {
    if (!_isConnected) {
        DEBUG_SERIAL.println("Cannot sync time: WiFi not connected");
        return false;
    }
    
    configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);
    
    // Wait for sync with timeout
    int retry = 0;
    const int maxRetries = 10;  // 5 seconds total (500ms * 10)
    time_t now;
    
    while (time(&now) < 1000000000 && retry < maxRetries) {
        delay(500);
        retry++;
        DEBUG_SERIAL.print(".");
    }
    DEBUG_SERIAL.println();
    
    if (time(&now) > 1000000000) {
        // Check if DST is in effect and adjust the offset
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        
        if (isDST(timeinfo)) {
            configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
            DEBUG_SERIAL.println("Daylight Saving Time is in effect.");
        } else {
            configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);
            DEBUG_SERIAL.println("Standard Time is in effect.");
        }
        
        _timeIsSynced = true;
        _lastSyncTime = now;
        
        // Brief purple flash to indicate successful time sync
        updateLEDStatus(0xFF00FF);  // Purple
        delay(200);
        updateLEDStatus(_isConnected ? COLOR_WIFI_CONNECTED : COLOR_WIFI_CONNECTING);
        
        return true;
    }

    DEBUG_SERIAL.println("Time sync failed!");
    _timeIsSynced = false;
    return false;
}

time_t WiFiManager::getCurrentTime() const {
    time_t now;
    time(&now);
    return now;
}

String WiFiManager::getFormattedTime() const {
    if (!_timeIsSynced) {
        return "Time not synced";
    }
    
    time_t now;
    time(&now);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "Time error";
    }
    
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeStr);
}
