#include "webhook_manager.h"
#include "config.h"
#include <WiFiClient.h>

WebhookManager::WebhookManager() {
    webhookUrl = WEBHOOK_URL;
}

bool WebhookManager::testConnection(const String& host, int port) {
    WiFiClient client;
    DEBUG_SERIAL.printf("Testing connection to %s:%d...\n", host.c_str(), port);
    
    unsigned long timeout = millis();
    bool connected = client.connect(host.c_str(), port);
    unsigned long connectionTime = millis() - timeout;
    
    if (connected) {
        DEBUG_SERIAL.printf("Connection successful! Response time: %lu ms\n", connectionTime);
        client.stop();
        return true;
    } else {
        DEBUG_SERIAL.println("Connection failed!");
        return false;
    }
}

bool WebhookManager::begin() {
    DEBUG_SERIAL.println("\nInitializing Webhook Manager...");
    DEBUG_SERIAL.printf("Webhook URL: %s\n", webhookUrl.c_str());
    
    // Extract host and port from URL for connection testing
    String host = "";
    int port = 80;
    
    // Simple URL parsing (assumes http:// format)
    int protocolEnd = webhookUrl.indexOf("://");
    if (protocolEnd > 0) {
        int hostStart = protocolEnd + 3;
        int hostEnd = webhookUrl.indexOf(":", hostStart);
        if (hostEnd < 0) {
            hostEnd = webhookUrl.indexOf("/", hostStart);
        }
        
        if (hostEnd > 0) {
            host = webhookUrl.substring(hostStart, hostEnd);
            
            // Try to extract port
            int portStart = webhookUrl.indexOf(":", hostStart);
            if (portStart > 0 && portStart < webhookUrl.indexOf("/", hostStart)) {
                int portEnd = webhookUrl.indexOf("/", portStart);
                String portStr = webhookUrl.substring(portStart + 1, portEnd);
                port = portStr.toInt();
            }
        }
    }
    
    DEBUG_SERIAL.printf("Webhook Host: %s, Port: %d\n", host.c_str(), port);
    
    // Test connection to the webhook server
    if (!host.isEmpty() && port > 0) {
        bool connectionSuccess = testConnection(host, port);
        if (!connectionSuccess) {
            DEBUG_SERIAL.println("Warning: Could not connect to webhook server!");
            DEBUG_SERIAL.println("Webhook calls may fail. Check if n8n is running and accessible.");
        }
    }
    
    DEBUG_SERIAL.println("Webhook Manager initialized");
    return true;
}

void WebhookManager::printWebhookStatus() {
    DEBUG_SERIAL.println("\n--- Webhook Status ---");
    DEBUG_SERIAL.printf("Webhook URL: %s\n", webhookUrl.c_str());
    
    // Extract host and port from URL
    String host = "";
    int port = 80;
    String path = "/";
    
    // Simple URL parsing (assumes http:// format)
    int protocolEnd = webhookUrl.indexOf("://");
    if (protocolEnd > 0) {
        int hostStart = protocolEnd + 3;
        int hostEnd = webhookUrl.indexOf(":", hostStart);
        if (hostEnd < 0) {
            hostEnd = webhookUrl.indexOf("/", hostStart);
        }
        
        if (hostEnd > 0) {
            host = webhookUrl.substring(hostStart, hostEnd);
            
            // Try to extract port
            int portStart = webhookUrl.indexOf(":", hostStart);
            if (portStart > 0 && portStart < webhookUrl.indexOf("/", hostStart)) {
                int portEnd = webhookUrl.indexOf("/", portStart);
                String portStr = webhookUrl.substring(portStart + 1, portEnd);
                port = portStr.toInt();
                
                // Extract path
                path = webhookUrl.substring(portEnd);
            } else {
                // Extract path without port
                int pathStart = webhookUrl.indexOf("/", hostStart);
                if (pathStart > 0) {
                    path = webhookUrl.substring(pathStart);
                }
            }
        }
    }
    
    DEBUG_SERIAL.printf("Host: %s\n", host.c_str());
    DEBUG_SERIAL.printf("Port: %d\n", port);
    DEBUG_SERIAL.printf("Path: %s\n", path.c_str());
    
    // Test connection
    if (!host.isEmpty() && port > 0) {
        testConnection(host, port);
    }
    
    DEBUG_SERIAL.println("--- End Webhook Status ---\n");
}

bool WebhookManager::sendPollResult(bool tagPresent, String currentTagId, String lastTagId,
                                  String tagType, String wifiStatus, String timeStatus,
                                  String timestamp) {
    DEBUG_SERIAL.println("\n--- Webhook Call ---");
    DEBUG_SERIAL.printf("Webhook URL: %s\n", webhookUrl.c_str());
    
    // Create JSON document
    StaticJsonDocument<512> doc;
    JsonObject rfidPollResult = doc.createNestedObject("rfid_poll_result");
    
    rfidPollResult["timestamp"] = timestamp;
    rfidPollResult["event_type"] = tagPresent ? "tag_insert" : "tag_removed";
    rfidPollResult["tag_present"] = tagPresent;
    rfidPollResult["tag_id"] = tagPresent ? currentTagId : lastTagId;
    
    if (tagPresent) {
        rfidPollResult["tag_type"] = tagType;
        rfidPollResult["wifi_status"] = wifiStatus;
        rfidPollResult["time_status"] = timeStatus;
    }

    // Serialize JSON to string
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Debug output - print JSON payload
    DEBUG_SERIAL.println("JSON Payload:");
    DEBUG_SERIAL.println(jsonString);

    // Send HTTP POST request
    http.begin(webhookUrl);
    http.addHeader("Content-Type", "application/json");
    
    DEBUG_SERIAL.println("Sending webhook POST request...");
    int httpResponseCode = http.POST(jsonString);
    
    // Process response
    bool success = (httpResponseCode > 0 && httpResponseCode < 300);
    
    DEBUG_SERIAL.printf("HTTP Response Code: %d\n", httpResponseCode);
    
    if (httpResponseCode > 0) {
        if (success) {
            DEBUG_SERIAL.println("Webhook call successful!");
            String response = http.getString();
            if (response.length() > 0) {
                DEBUG_SERIAL.println("Response:");
                DEBUG_SERIAL.println(response);
            }
        } else {
            // Error handling based on status code
            if (httpResponseCode == 404) {
                DEBUG_SERIAL.println("Error: Webhook URL not found (404)");
                DEBUG_SERIAL.println("Check if the n8n webhook URL is correct and the server is running");
            } else if (httpResponseCode >= 500) {
                DEBUG_SERIAL.println("Error: Server error on n8n side");
            } else {
                DEBUG_SERIAL.printf("Error: HTTP request failed with code %d\n", httpResponseCode);
            }
        }
    } else {
        DEBUG_SERIAL.println("Error: Connection failed");
        DEBUG_SERIAL.println("Possible causes:");
        DEBUG_SERIAL.println("- n8n server is not running");
        DEBUG_SERIAL.println("- IP address in webhook URL is incorrect");
        DEBUG_SERIAL.println("- Network connectivity issues");
        DEBUG_SERIAL.println("- Firewall blocking the connection");
    }
    
    DEBUG_SERIAL.println("--- End Webhook Call ---\n");
    
    http.end();
    return success;
}
