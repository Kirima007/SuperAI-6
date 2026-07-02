#include "StateMachine.h"

StateMachine::StateMachine(NetworkManager& net, MachineManager& mac) : _net(net), _mac(mac) {
    currentUIState = UIState::DASHBOARD;
    menuIndex = 0;
    temp_total_len = 50.0;
    temp_front_len = 5.0;
    temp_back_len = 5.0;
    temp_qty = 10;
    clearMenuIndex = 0;
    scrollOffset = 0;
    snapshotCount = 0;
}

void StateMachine::begin() {}

void StateMachine::_requestSound(SoundEvent ev) {
    xQueueSend(soundQueue, &ev, 0);
}

// 🔴 ฟังก์ชันใหม่: ถ่ายภาพ Queue อย่างปลอดภัย (ใช้ queueMutex แทน vTaskSuspend)
void StateMachine::_snapshotQueue() {
    if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        snapshotCount = uxQueueMessagesWaiting(jobQueue);
        for (int i = 0; i < snapshotCount; i++) {
            xQueueReceive(jobQueue, &snapshotJobs[i], 0);
            xQueueSend(jobQueue, &snapshotJobs[i], 0);
        }
        xSemaphoreGive(queueMutex);
    } else {
        snapshotCount = 0;   // Timeout → แสดงว่า Queue ไม่ว่าง
        Serial.println("[State] WARNING: queueMutex timeout during snapshot");
    }
}

// 🔴 ฟังก์ชันใหม่: ลบรายการจาก Queue อย่างปลอดภัย
bool StateMachine::_deleteFromQueue(int indexToDelete) {
    bool success = false;
    if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        int count = uxQueueMessagesWaiting(jobQueue);
        for (int i = 0; i < count; i++) {
            JobData temp;
            xQueueReceive(jobQueue, &temp, 0);
            if (i != indexToDelete) {
                xQueueSend(jobQueue, &temp, 0);
            }
        }
        // Re-snapshot หลังลบ
        snapshotCount = uxQueueMessagesWaiting(jobQueue);
        for (int i = 0; i < snapshotCount; i++) {
            xQueueReceive(jobQueue, &snapshotJobs[i], 0);
            xQueueSend(jobQueue, &snapshotJobs[i], 0);
        }
        success = true;
        xSemaphoreGive(queueMutex);
    }
    return success;
}

void StateMachine::taskEntry() {
    ButtonEvent ev;
    for (;;) {
        if (xQueueReceive(inputQueue, &ev, portMAX_DELAY) == pdTRUE) {
            switch (currentUIState) {
                case UIState::DASHBOARD:  _handleDashboard(ev);  break;
                case UIState::LIST_QUEUE: _handleListQueue(ev);  break;
                case UIState::MENU:       _handleMenu(ev);       break;
                case UIState::EDIT:       _handleEdit(ev);       break;
            }
        }
    }
}

void StateMachine::getDisplayData(StateMachineDisplayData& data) {
    xSemaphoreTake(displayMutex, portMAX_DELAY);
    data.currentUIState = currentUIState;
    data.menuIndex      = menuIndex;
    data.temp_total_len = temp_total_len;
    data.temp_front_len = temp_front_len;
    data.temp_back_len  = temp_back_len;
    data.temp_qty       = temp_qty;
    data.clearMenuIndex = clearMenuIndex;
    data.scrollOffset   = scrollOffset;
    data.snapshotCount  = snapshotCount;
    // 🔴 ใช้ memcpy ปลอดภัยแล้วเพราะ JobData ไม่มี String
    memcpy(data.snapshotJobs, snapshotJobs, sizeof(JobData) * data.snapshotCount);
    xSemaphoreGive(displayMutex);
}

// ==========================================
// Dashboard: กดสั้น → Menu, กดค้าง → Queue List
// ==========================================
void StateMachine::_handleDashboard(ButtonEvent ev) {
    if (ev == ButtonEvent::LONG_PRESS) {
        _requestSound(SoundEvent::SELECT);

        // 🔴 Snapshot Queue โดยไม่ต้อง Suspend task (ปลอดภัยจาก Deadlock)
        _snapshotQueue();

        xSemaphoreTake(displayMutex, portMAX_DELAY);
        currentUIState = UIState::LIST_QUEUE;
        clearMenuIndex = 0;
        scrollOffset   = 0;
        xSemaphoreGive(displayMutex);
    }
    else if (ev == ButtonEvent::SHORT_PRESS) {
        _requestSound(SoundEvent::SELECT);
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        currentUIState = UIState::MENU;
        menuIndex = 0;
        xSemaphoreGive(displayMutex);
    }
}

// ==========================================
// Queue List: หมุน → เลื่อน, กดสั้น → ลบ/กลับ/หยุดงาน
// ==========================================
void StateMachine::_handleListQueue(ButtonEvent ev) {
    if (ev == ButtonEvent::LONG_PRESS) {
        _requestSound(SoundEvent::BACK);
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        currentUIState = UIState::DASHBOARD;
        xSemaphoreGive(displayMutex);
    }
    else if (ev == ButtonEvent::ROTATE_CW || ev == ButtonEvent::ROTATE_CCW) {
        _requestSound(SoundEvent::SCROLL);
        int diff = (ev == ButtonEvent::ROTATE_CW) ? 1 : -1;
        int maxIndex = snapshotCount + 1;   // BACK + STOP = 2 รายการพิเศษ

        xSemaphoreTake(displayMutex, portMAX_DELAY);
        clearMenuIndex += diff;
        if (clearMenuIndex < 0)        clearMenuIndex = 0;
        if (clearMenuIndex > maxIndex) clearMenuIndex = maxIndex;

        const int VISIBLE_ROWS = 5;
        if (clearMenuIndex < scrollOffset)
            scrollOffset = clearMenuIndex;
        if (clearMenuIndex >= scrollOffset + VISIBLE_ROWS)
            scrollOffset = clearMenuIndex - VISIBLE_ROWS + 1;
        xSemaphoreGive(displayMutex);
    }
    else if (ev == ButtonEvent::SHORT_PRESS) {
        if (clearMenuIndex == snapshotCount) {
            // === BACK / CANCEL ===
            _requestSound(SoundEvent::BACK);
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            currentUIState = UIState::DASHBOARD;
            xSemaphoreGive(displayMutex);
        }
        else if (clearMenuIndex == snapshotCount + 1) {
            // === STOP CURRENT JOB ===
            _requestSound(SoundEvent::WARNING);
            _mac.abortCurrentJob();
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            currentUIState = UIState::DASHBOARD;
            xSemaphoreGive(displayMutex);
        }
        else {
            // === DELETE QUEUE ITEM ===
            _requestSound(SoundEvent::SUCCESS);

            // 🔴 ลบงานจาก Queue อย่างปลอดภัยด้วย Mutex (ไม่ Suspend task)
            _deleteFromQueue(clearMenuIndex);

            xSemaphoreTake(displayMutex, portMAX_DELAY);
            if (clearMenuIndex > snapshotCount + 1)
                clearMenuIndex = snapshotCount + 1;
            xSemaphoreGive(displayMutex);
        }
    }
}

// ==========================================
// Menu: เลือกพารามิเตอร์ / เพิ่มงาน / ยกเลิก
// ==========================================
void StateMachine::_handleMenu(ButtonEvent ev) {
    if (ev == ButtonEvent::LONG_PRESS) {
        _requestSound(SoundEvent::BACK);
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        currentUIState = UIState::DASHBOARD;
        xSemaphoreGive(displayMutex);
    }
    else if (ev == ButtonEvent::ROTATE_CW || ev == ButtonEvent::ROTATE_CCW) {
        _requestSound(SoundEvent::SCROLL);
        int diff = (ev == ButtonEvent::ROTATE_CW) ? 1 : -1;
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        menuIndex += diff;
        if (menuIndex < 0) menuIndex = 0;
        if (menuIndex > 5) menuIndex = 5;
        xSemaphoreGive(displayMutex);
    }
    else if (ev == ButtonEvent::SHORT_PRESS) {
        if (menuIndex <= 3) {
            _requestSound(SoundEvent::SELECT);
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            currentUIState = UIState::EDIT;
            xSemaphoreGive(displayMutex);
        }
        else if (menuIndex == 4) {
            // === ADD TO QUEUE ===
            JobData newJob;
            memset(&newJob, 0, sizeof(newJob));
            strlcpy(newJob.size, "Manual", sizeof(newJob.size));
            newJob.total_len = temp_total_len;
            newJob.front_len = temp_front_len;
            newJob.back_len  = temp_back_len;
            newJob.qty       = temp_qty;

            bool sent = false;
            if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                sent = (xQueueSend(jobQueue, &newJob, 0) == pdPASS);
                xSemaphoreGive(queueMutex);
            }

            if (sent) {
                _requestSound(SoundEvent::SUCCESS);
                xSemaphoreTake(displayMutex, portMAX_DELAY);
                currentUIState = UIState::DASHBOARD;
                xSemaphoreGive(displayMutex);
            } else {
                _requestSound(SoundEvent::WARNING);   // คิวเต็ม หรือ mutex timeout
            }
        }
        else if (menuIndex == 5) {
            _requestSound(SoundEvent::BACK);
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            currentUIState = UIState::DASHBOARD;
            xSemaphoreGive(displayMutex);
        }
    }
}

// ==========================================
// Edit: หมุนเปลี่ยนค่า, กดสั้นยืนยัน
// ==========================================
void StateMachine::_handleEdit(ButtonEvent ev) {
    if (ev == ButtonEvent::LONG_PRESS) {
        _requestSound(SoundEvent::BACK);
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        currentUIState = UIState::DASHBOARD;
        xSemaphoreGive(displayMutex);
    }
    else if (ev == ButtonEvent::ROTATE_CW || ev == ButtonEvent::ROTATE_CCW) {
        _requestSound(SoundEvent::SCROLL);
        int diff = (ev == ButtonEvent::ROTATE_CW) ? 1 : -1;

        xSemaphoreTake(displayMutex, portMAX_DELAY);
        switch (menuIndex) {
            case 0: temp_total_len += diff * 5.0f; if (temp_total_len < 10.0f) temp_total_len = 10.0f; break;
            case 1: temp_front_len += diff * 1.0f; if (temp_front_len < 0.0f)  temp_front_len = 0.0f;  break;
            case 2: temp_back_len  += diff * 1.0f; if (temp_back_len  < 0.0f)  temp_back_len  = 0.0f;  break;
            case 3: temp_qty       += diff;        if (temp_qty < 1)           temp_qty = 1;           break;
        }
        xSemaphoreGive(displayMutex);
    }
    else if (ev == ButtonEvent::SHORT_PRESS) {
        _requestSound(SoundEvent::SELECT);
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        currentUIState = UIState::MENU;
        xSemaphoreGive(displayMutex);
    }
}
