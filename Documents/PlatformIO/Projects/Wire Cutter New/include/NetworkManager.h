#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Shared.h"

// ตั้งค่า WiFi และ MQTT ของคุณที่นี่
#define WIFI_SSID "weerapat"
#define WIFI_PASS "12345678"
#define MQTT_SERVER "192.168.137.242"
#define MQTT_PORT 1883

class NetworkManager {
public:
    NetworkManager();
    void begin();
    void taskEntry();
    
    // Getter ให้หน้าจอหรือ StateMachine ดึงสถานะไปแสดงผล
    bool isWiFiConnected();
    bool isMQTTConnected();
    
    // ไว้ให้คลาสอื่นเรียกใช้เพื่อส่งข้อความกลับไปที่ Node-RED
    void publishStatus(const char* payload);
    
    // ฟังก์ชันจัดการข้อความที่รับมาจาก MQTT
    void handleMessage(char* topic, byte* payload, unsigned int length);

private:
    WiFiClient espClient;
    PubSubClient mqttClient;
    
    void connectWiFi();
    void connectMQTT();
};