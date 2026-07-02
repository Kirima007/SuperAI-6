#pragma once
#include <Arduino.h>
#include "Shared.h"

#define BUZZER_PIN 14

class SoundManager {
public:
    void begin();
    void taskEntry();

private:
    void beepScroll();
    void beepSelect();
    void beepWarning();
    void beepSuccess();
    void beepBack();
};