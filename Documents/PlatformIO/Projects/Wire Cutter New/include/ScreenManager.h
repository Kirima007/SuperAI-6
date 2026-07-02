#pragma once
#include <Arduino.h>
#include "Shared.h"
#include "StateMachine.h"
#include "NetworkManager.h"
#include "MachineManager.h"

class ScreenManager {
public:
    ScreenManager(StateMachine& sm, NetworkManager& net, MachineManager& mac);
    void begin();
    void taskEntry();

private:
    StateMachine& _sm;
    NetworkManager& _net;
    MachineManager& _mac;

    // State snapshot and old values for flicker-free drawing
    StateMachineDisplayData _sm_data;
    StateMachineDisplayData _sm_data_old;
    bool forceFullRedraw;

    // Dashboard specific old values
    bool oldWiFi, oldMQTT, oldJobActive;
    int oldQueueCount, oldCutQty;
    bool reqJobUIUpdate;

    // ฟังก์ชันวาดจอแยกตามหมวดหมู่
    void _drawDashboard();
    void _drawListQueue();
    void _drawMenuAndEdit();
};