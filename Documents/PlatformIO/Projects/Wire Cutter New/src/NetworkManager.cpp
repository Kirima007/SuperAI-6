#include "NetworkManager.h"

NetworkManager* globalNetworkInstance = nullptr;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (globalNetworkInstance) {
        globalNetworkInstance->handleMessage(topic, payload, length);
    }
}

NetworkManager::NetworkManager() : mqttClient(espClient) {
    globalNetworkInstance = this;
}

void NetworkManager::begin() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
}

bool NetworkManager::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::isMQTTConnected() {
    return mqttClient.connected();
}

void NetworkManager::publishStatus(const char* payload) {
    if (isMQTTConnected()) {
        mqttClient.publish("wirecutter/status", payload);
    }
}

void NetworkManager::connectWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));

        WiFi.begin(WIFI_SSID, WIFI_PASS);
        Serial.print("Connecting to WiFi...");

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            vTaskDelay(pdMS_TO_TICKS(500));
            Serial.print(".");
            attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
        } else {
            Serial.println("\nWiFi Connect Failed!");
        }
    }
}

void NetworkManager::connectMQTT() {
    if (isWiFiConnected() && !isMQTTConnected()) {
        Serial.print("Attempting MQTT connection... ");
        if (mqttClient.connect("ESP32S3_Master")) {
            Serial.println("CONNECTED!");
            mqttClient.subscribe("wirecutter/command");
        } else {
            Serial.printf("failed, rc=%d\n", mqttClient.state());
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

void NetworkManager::handleMessage(char* topic, byte* payload, unsigned int length) {
    Serial.printf("\n--- MQTT RX ---\nTopic: %s\nMessage: ", topic);
    for (unsigned int i = 0; i < length; i++) Serial.print((char)payload[i]);
    Serial.println("\n---------------");

    if (strcmp(topic, "wirecutter/command") != 0) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        Serial.printf("JSON Parse Error: %s\n", error.c_str());
        return;
    }

    const char* cmd = doc["command"];
    if (cmd == nullptr) return;

    if (strcmp(cmd, "add_queue") == 0) {
        JobData newJob;
        memset(&newJob, 0, sizeof(newJob));   // 🔴 เคลียร์ก่อนเซ็ต

        // 🔴 ใช้ strlcpy แทน String assignment
        strlcpy(newJob.size, doc["size"] | "N/A", sizeof(newJob.size));
        newJob.total_len = doc["total_length"] | 0.0f;
        newJob.front_len = doc["front_length"] | 0.0f;
        newJob.back_len  = doc["back_length"]  | 0.0f;
        newJob.qty       = doc["quantity"]      | 0;

        // 🔴 ใช้ queueMutex ปกป้องการเข้าถึง Queue
        if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (xQueueSend(jobQueue, &newJob, 0) == pdPASS) {
                Serial.println("Status: New Job Added from MQTT");
            } else {
                Serial.println("Status: Job Queue FULL!");
            }
            xSemaphoreGive(queueMutex);
        }
    }
    else if (strcmp(cmd, "clear_queue") == 0) {
        if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            xQueueReset(jobQueue);
            xSemaphoreGive(queueMutex);
        }
        Serial.println("Status: Queue Cleared via MQTT");
    }
}

void NetworkManager::taskEntry() {
    while (1) {
        connectWiFi();
        connectMQTT();

        if (isMQTTConnected()) {
            mqttClient.loop();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
