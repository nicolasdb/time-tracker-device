#ifndef WEBHOOK_MANAGER_H
#define WEBHOOK_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "credentials.h"

class WebhookManager {
    private:
        HTTPClient http;
        String webhookUrl;
        bool testConnection(const String& host, int port);

    public:
        WebhookManager();
        bool begin();
        bool sendPollResult(bool tagPresent, String currentTagId, String lastTagId, 
                          String tagType, String wifiStatus, String timeStatus,
                          String timestamp);
        void printWebhookStatus();
};

#endif // WEBHOOK_MANAGER_H
