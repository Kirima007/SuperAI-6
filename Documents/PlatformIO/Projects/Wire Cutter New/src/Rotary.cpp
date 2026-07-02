#include "Rotary.h"

void Rotary::begin() {
    pinMode(ENCODER_SW, INPUT_PULLUP);
    
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(ENCODER_DT, ENCODER_CLK);
    encoder.setCount(0);
    encoder_value = 0;
}

void Rotary::taskEntry() {
    bool last_btn_state = HIGH;
    uint32_t press_start_time = 0;
    bool long_press_triggered = false; 

    while(1) {
        // --- 1. อ่านค่าการหมุน (ลื่นปรื๊ดเพราะไม่มี delay เสียง) ---
        long currentPosition = encoder.getCount();
        if (currentPosition != encoder_value) {
            int diff = currentPosition - encoder_value;
            encoder_value = currentPosition;
            
            ButtonEvent dir = (diff > 0) ? ButtonEvent::ROTATE_CW : ButtonEvent::ROTATE_CCW;
            int steps = abs(diff);
            for(int i = 0; i < steps; i++) {
                xQueueSend(inputQueue, &dir, 0);
            }
        }

        // --- 2. อ่านค่าการกดปุ่ม ---
        bool current_btn_state = digitalRead(ENCODER_SW);
        
        // เพิ่งเริ่มกดปุ่มลงไป
        if (current_btn_state == LOW && last_btn_state == HIGH) {
            press_start_time = millis();
            long_press_triggered = false;
        } 
        // แช่ปุ่มค้างไว้ครบ 1.5 วิ
        else if (current_btn_state == LOW && last_btn_state == LOW) {
            if (!long_press_triggered && (millis() - press_start_time >= 1500)) { 
                ButtonEvent ev = ButtonEvent::LONG_PRESS;
                xQueueSend(inputQueue, &ev, 0); 
                long_press_triggered = true; 
            }
        }
        // ปล่อยปุ่ม
        else if (current_btn_state == HIGH && last_btn_state == LOW) {
            uint32_t press_duration = millis() - press_start_time; 
            if (!long_press_triggered && press_duration > 20) {
                ButtonEvent ev = ButtonEvent::SHORT_PRESS;
                xQueueSend(inputQueue, &ev, 0); 
            }
        }
        
        last_btn_state = current_btn_state;
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}