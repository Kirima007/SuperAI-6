#include "SoundManager.h"

void SoundManager::begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW); // กันเสียงค้าง
}

void SoundManager::beepScroll() {
    digitalWrite(BUZZER_PIN, HIGH); vTaskDelay(pdMS_TO_TICKS(2)); digitalWrite(BUZZER_PIN, LOW);
}

void SoundManager::beepSelect() {
    digitalWrite(BUZZER_PIN, HIGH); vTaskDelay(pdMS_TO_TICKS(15)); digitalWrite(BUZZER_PIN, LOW);
}

void SoundManager::beepWarning() {
    // เสียงเตือนยาวๆ ตอนกดค้างหรือลบคิว
    digitalWrite(BUZZER_PIN, HIGH); vTaskDelay(pdMS_TO_TICKS(100)); digitalWrite(BUZZER_PIN, LOW);
}

void SoundManager::beepSuccess() {
    // เสียงปี๊บสองครั้งติด (ทำงานสำเร็จ)
    digitalWrite(BUZZER_PIN, HIGH); vTaskDelay(pdMS_TO_TICKS(30)); digitalWrite(BUZZER_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(30));
    digitalWrite(BUZZER_PIN, HIGH); vTaskDelay(pdMS_TO_TICKS(50)); digitalWrite(BUZZER_PIN, LOW);
}

void SoundManager::beepBack() {
    // เสียง ติ๊ด-ตื๊ด (กดยกเลิก / ถอยกลับ)
    digitalWrite(BUZZER_PIN, HIGH); vTaskDelay(pdMS_TO_TICKS(40)); digitalWrite(BUZZER_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(60));
    digitalWrite(BUZZER_PIN, HIGH); vTaskDelay(pdMS_TO_TICKS(80)); digitalWrite(BUZZER_PIN, LOW);
}

void SoundManager::taskEntry() {
    SoundEvent ev;

    for (;;) {
        // รอรับคำสั่งให้เล่นเสียง (ถ้าไม่มีก็นอนรอ ไม่กิน CPU)
        if (xQueueReceive(soundQueue, &ev, portMAX_DELAY) == pdTRUE) {
            switch (ev) {
                case SoundEvent::SCROLL:  beepScroll();  break;
                case SoundEvent::SELECT:  beepSelect();  break;
                case SoundEvent::WARNING: beepWarning(); break;
                case SoundEvent::SUCCESS: beepSuccess(); break;
                case SoundEvent::BACK:    beepBack();    break;
            }
        }
    }
}