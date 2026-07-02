#pragma once
#include "Shared.h"

class MachineManager {
public:
    MachineManager();
    void begin();
    void taskEntry();

    bool isJobActive();
    int  getCurrentCutQty();
    JobData getActiveJob();
    void abortCurrentJob();

private:
    volatile bool _isJobActive;
    volatile int  _currentCutQty;
    volatile bool _reqAbortJob;
    JobData _activeJob;
};
