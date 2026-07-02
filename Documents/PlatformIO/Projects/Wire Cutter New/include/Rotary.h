#pragma once
#include <Arduino.h>
#include <ESP32Encoder.h>
#include "Shared.h"

#define ENCODER_CLK 5  
#define ENCODER_DT  4  
#define ENCODER_SW  8 

class Rotary {
public:
    void begin();
    void taskEntry();

private:
    ESP32Encoder encoder;
    int encoder_value;
};