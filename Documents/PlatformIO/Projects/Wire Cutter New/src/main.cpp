#include <Arduino.h>
#include "Shared.h"
#include "NetworkManager.h"
#include "MachineManager.h"
#include "StateMachine.h"
#include "ScreenManager.h"
#include "Rotary.h"
#include "SoundManager.h"

// ==========================================
// ประกาศตัวแปร Global
// ==========================================
QueueHandle_t jobQueue;
QueueHandle_t inputQueue;
QueueHandle_t soundQueue;
SemaphoreHandle_t displayMutex;
SemaphoreHandle_t queueMutex;      // 🔴 เพิ่มใหม่
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ==========================================
// ประกาศ Object
// ==========================================
NetworkManager network;
MachineManager machine;
Rotary rotary;
SoundManager soundSys;

StateMachine stateMachine(network, machine);
ScreenManager screen(stateMachine, network, machine);

// ==========================================
// FreeRTOS Task Entry Points
// ==========================================
void Task_Network(void *pvParameters) {
    network.begin();
    network.taskEntry();
}

void Task_Machine(void *pvParameters) {
    machine.begin();
    machine.taskEntry();
}

void Task_State(void *pvParameters) {
    stateMachine.begin();
    stateMachine.taskEntry();
}

void Task_Display(void *pvParameters) {
    screen.begin();
    screen.taskEntry();
}

void Task_Rotary(void *pvParameters) {
    rotary.begin();
    rotary.taskEntry();
}

void Task_Sound(void *pvParameters) {
    soundSys.begin();
    soundSys.taskEntry();
}

// ==========================================
// Setup & Loop
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    // 1. สร้าง OS Objects
    jobQueue     = xQueueCreate(10, sizeof(JobData));
    inputQueue   = xQueueCreate(10, sizeof(ButtonEvent));
    soundQueue   = xQueueCreate(5, sizeof(SoundEvent));
    displayMutex = xSemaphoreCreateMutex();
    queueMutex   = xSemaphoreCreateMutex();   // 🔴 สร้าง Mutex ใหม่

    // 2. สร้าง Tasks
    // Core 0: Network
    xTaskCreatePinnedToCore(Task_Network, "Network_Task", 8192,  NULL, 1, NULL, 0);

    // Core 1: Hardware, Logic, Display
    xTaskCreatePinnedToCore(Task_Sound,   "Sound_Task",   2048,  NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(Task_Machine, "Machine_Task", 4096,  NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(Task_State,   "State_Task",   4096,  NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(Task_Display, "Display_Task", 16384, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(Task_Rotary,  "Rotary_Task",  2048,  NULL, 3, NULL, 1);
}

void loop() { vTaskDelete(NULL); }
