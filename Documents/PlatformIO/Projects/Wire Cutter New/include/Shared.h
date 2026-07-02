#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <ArduinoJson.h>

// ==========================================
// 🔴 แก้ String → char[] ป้องกัน heap corruption ผ่าน Queue
// ==========================================
struct JobData {
    char size[16];        // เปลี่ยนจาก String → char array (Queue-safe)
    float total_len;
    float front_len;
    float back_len;
    int qty;
};

enum class ButtonEvent : uint8_t {
    SHORT_PRESS, LONG_PRESS, ROTATE_CW, ROTATE_CCW
};

enum class SoundEvent : uint8_t {
    SCROLL, SELECT, WARNING, SUCCESS, BACK
};

enum class UIState : uint8_t {
    DASHBOARD, LIST_QUEUE, MENU, EDIT
};

// ข้อมูลสำหรับส่งให้ ScreenManager วาดจอ (Thread-safe snapshot)
struct StateMachineDisplayData {
    UIState currentUIState;
    int menuIndex;
    float temp_total_len;
    float temp_front_len;
    float temp_back_len;
    int temp_qty;
    int clearMenuIndex;
    int scrollOffset;
    int snapshotCount;
    JobData snapshotJobs[10];
};

// ==========================================
// Global OS Objects
// ==========================================
extern QueueHandle_t jobQueue;
extern QueueHandle_t inputQueue;
extern QueueHandle_t soundQueue;
extern SemaphoreHandle_t displayMutex;
extern SemaphoreHandle_t queueMutex;   // 🔴 เพิ่มใหม่: ล็อค Queue แทน vTaskSuspend
extern portMUX_TYPE mux;
