#include "MachineManager.h"
#include "NetworkManager.h"

extern NetworkManager network;

MachineManager::MachineManager() {
    _isJobActive = false;
    _currentCutQty = 0;
    _reqAbortJob = false;
}

void MachineManager::begin() {
    Serial2.begin(9600, SERIAL_8N1, 6, 7);
}

bool MachineManager::isJobActive() {
    xSemaphoreTake(displayMutex, portMAX_DELAY);
    bool status = _isJobActive;
    xSemaphoreGive(displayMutex);
    return status;
}

int MachineManager::getCurrentCutQty() {
    xSemaphoreTake(displayMutex, portMAX_DELAY);
    int qty = _currentCutQty;
    xSemaphoreGive(displayMutex);
    return qty;
}

JobData MachineManager::getActiveJob() {
    xSemaphoreTake(displayMutex, portMAX_DELAY);
    JobData job = _activeJob;
    xSemaphoreGive(displayMutex);
    return job;
}

void MachineManager::abortCurrentJob() {
    _reqAbortJob = true;     // volatile write, ปลอดภัยโดยไม่ต้องล็อค
}

void MachineManager::taskEntry() {
    JobData currentJob;
    String rxBuffer;
    rxBuffer.reserve(128);   // 🔴 จองหน่วยความจำล่วงหน้า ลด heap fragmentation

    while (1) {
        // ============================================
        // Phase 1: พยายามดึงงานจาก Queue (ใช้ queueMutex)
        // ============================================
        bool gotJob = false;

        if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (xQueueReceive(jobQueue, &currentJob, 0) == pdPASS) {
                gotJob = true;
            }
            xSemaphoreGive(queueMutex);
        }

        if (!gotJob) {
            // ไม่มีงาน → อัปเดตสถานะแล้ววนรอ
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            if (_isJobActive) _isJobActive = false;   // เขียนเฉพาะตอนเปลี่ยน
            xSemaphoreGive(displayMutex);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // ============================================
        // Phase 2: เริ่มทำงาน — อัปเดตสถานะ + ส่งคำสั่งไป STM32
        // ============================================
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        _activeJob    = currentJob;
        _isJobActive  = true;
        _currentCutQty = 0;
        _reqAbortJob  = false;
        xSemaphoreGive(displayMutex);

        {
            JsonDocument doc;
            doc["size"]         = currentJob.size;
            doc["total_length"] = currentJob.total_len;
            doc["front_length"] = currentJob.front_len;
            doc["back_length"]  = currentJob.back_len;
            doc["quantity"]     = currentJob.qty;

            String payload;
            serializeJson(doc, payload);
            Serial.printf("\n[Machine] Send to STM32: %s\n", payload.c_str());
            Serial2.println(payload);
        }

        // ============================================
        // Phase 3: รอผลลัพธ์จาก STM32 (ลบ goto ใช้ flag แทน)
        // ============================================
        bool jobRunning = true;
        rxBuffer = "";

        while (jobRunning) {
            // เช็คว่ามีคำสั่ง Abort หรือไม่
            if (_reqAbortJob) {
                _reqAbortJob = false;
                Serial2.println("{\"command\":\"abort\"}");
                Serial.println("[Machine] Job aborted by user");
                break;   // ออกจาก while(jobRunning) ไปทำ Phase 4
            }

            // อ่านข้อมูลจาก STM32
            while (Serial2.available()) {
                char c = Serial2.read();
                if (c == '\n') {
                    rxBuffer.trim();
                    if (rxBuffer.length() == 0) {
                        rxBuffer = "";
                        continue;
                    }

                    if (rxBuffer == "done") {
                        network.publishStatus("{\"status\":\"done\"}");
                        jobRunning = false;   // 🔴 ใช้ flag แทน goto
                    }
                    else if (rxBuffer.startsWith("{")) {
                        JsonDocument rxDoc;
                        DeserializationError error = deserializeJson(rxDoc, rxBuffer);
                        if (!error) {
                            if (rxDoc["pg"].is<int>()) {
                                xSemaphoreTake(displayMutex, portMAX_DELAY);
                                _currentCutQty = rxDoc["pg"].as<int>();
                                xSemaphoreGive(displayMutex);
                            }
                            network.publishStatus(rxBuffer.c_str());
                        }
                    }
                    rxBuffer = "";
                } else {
                    rxBuffer += c;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // ============================================
        // Phase 4: งานจบ — อัปเดตสถานะ
        // ============================================
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        _isJobActive = false;
        xSemaphoreGive(displayMutex);

        Serial.println("[Machine] Job finished, ready for next");
    }
}
