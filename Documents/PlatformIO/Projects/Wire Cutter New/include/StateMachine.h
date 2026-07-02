#pragma once
#include "Shared.h"
#include "NetworkManager.h"
#include "MachineManager.h"

// 🔴 ลบ struct StateMachineDisplayData ออกจากไฟล์นี้ทั้งหมด
//    เพราะย้ายไปประกาศใน Shared.h แล้ว

class StateMachine {
public:
    StateMachine(NetworkManager& net, MachineManager& mac);
    void begin();
    void taskEntry();
    void getDisplayData(StateMachineDisplayData& data);

private:
    NetworkManager& _net;
    MachineManager& _mac;

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

    void _requestSound(SoundEvent ev);
    void _snapshotQueue();                    // 🔴 เพิ่มใหม่
    bool _deleteFromQueue(int indexToDelete); // 🔴 เพิ่มใหม่

    void _handleDashboard(ButtonEvent ev);
    void _handleListQueue(ButtonEvent ev);
    void _handleMenu(ButtonEvent ev);
    void _handleEdit(ButtonEvent ev);
};
