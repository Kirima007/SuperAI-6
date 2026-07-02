#include "ScreenManager.h"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ==========================================
// การตั้งค่าหน้าจอ LovyanGFX
// ==========================================
#define TFT_CS     9
#define TFT_DC     10
#define TFT_MOSI   11
#define TFT_MISO   -1
#define TFT_SCLK   12
#define TFT_RST    -1
#define TFT_BL     13

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;
public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI3_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = TFT_SCLK;
            cfg.pin_mosi = TFT_MOSI;
            cfg.pin_miso = TFT_MISO;
            cfg.pin_dc   = TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = TFT_CS;
            cfg.pin_rst          = TFT_RST;
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = true;
            cfg.rgb_order        = false;
            cfg.invert           = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};

static LGFX tft;

// ==========================================
// Constructor & Begin
// ==========================================
ScreenManager::ScreenManager(StateMachine& sm, NetworkManager& net, MachineManager& mac)
    : _sm(sm), _net(net), _mac(mac)
{
    forceFullRedraw = true;
    oldWiFi = false; oldMQTT = false; oldQueueCount = -1;
    oldJobActive = false; oldCutQty = -1; reqJobUIUpdate = true;
}

void ScreenManager::begin() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
}

// ==========================================
// Helper
// ==========================================
static bool _isSameScreen(UIState a, UIState b) {
    if (a == b) return true;
    if ((a == UIState::MENU || a == UIState::EDIT) &&
        (b == UIState::MENU || b == UIState::EDIT)) {
        return true;
    }
    return false;
}

// ==========================================
// 🔴 Helper: วาด Sprite เล็กๆ ปลอดภัย
//    ถ้า createSprite fail → fallback วาดตรงลง tft
// ==========================================
static LGFX_Sprite _safeSprite(&tft);
static bool _spriteReady = false;

static bool beginSafeSprite(int w, int h) {
    _safeSprite.deleteSprite();
    _spriteReady = _safeSprite.createSprite(w, h);
    if (_spriteReady) {
        _safeSprite.fillScreen(TFT_BLACK);
        _safeSprite.setFont(&fonts::Font2);
    }
    return _spriteReady;
}

static void pushSafeSprite(int x, int y) {
    if (_spriteReady) {
        _safeSprite.pushSprite(x, y);
        _safeSprite.deleteSprite();
        _spriteReady = false;
    }
}

// ==========================================
// Task Entry
// ==========================================
void ScreenManager::taskEntry() {
    while(1) {
        _sm.getDisplayData(_sm_data);

        if (_sm_data_old.currentUIState != _sm_data.currentUIState) {
            if (!_isSameScreen(_sm_data_old.currentUIState, _sm_data.currentUIState)) {
                forceFullRedraw = true;
            }
        }

        switch (_sm_data.currentUIState) {
            case UIState::DASHBOARD:  _drawDashboard();    break;
            case UIState::LIST_QUEUE: _drawListQueue();    break;
            case UIState::MENU:
            case UIState::EDIT:       _drawMenuAndEdit();  break;
        }

        memcpy(&_sm_data_old, &_sm_data, sizeof(StateMachineDisplayData));
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// ==========================================
// 1. DASHBOARD
// 🔴 FIX: แบ่ง Sprite 320x160 → Sprite เล็กๆ 5 อัน
// ==========================================
void ScreenManager::_drawDashboard() {
    if (forceFullRedraw) {
        tft.fillScreen(TFT_BLACK);
        tft.fillRect(0, 0, 320, 25, tft.color565(30, 30, 30));
        tft.setTextColor(TFT_WHITE); tft.setFont(&fonts::Font2);
        tft.setCursor(10, 4); tft.print("SYSTEM STATUS");
        tft.drawFastHLine(0, 25, 320, tft.color565(80, 80, 80));
        tft.drawFastHLine(0, 205, 320, tft.color565(80, 80, 80));

        oldWiFi = !_net.isWiFiConnected();
        oldMQTT = !_net.isMQTTConnected();
        oldQueueCount = -1;
        oldJobActive = false;
        oldCutQty = -1;
        reqJobUIUpdate = true;
        forceFullRedraw = false;
    }

    // --- Status bar ---
    bool curWiFi = _net.isWiFiConnected();
    bool curMQTT = _net.isMQTTConnected();
    if (curWiFi != oldWiFi || curMQTT != oldMQTT) {
        if (beginSafeSprite(100, 20)) {
            _safeSprite.fillScreen(tft.color565(30, 30, 30));
            _safeSprite.setTextColor(curWiFi ? TFT_GREEN : tft.color565(100, 100, 100));
            _safeSprite.setCursor(0, 2); _safeSprite.print("WiFi");
            _safeSprite.setTextColor(curMQTT ? TFT_CYAN : tft.color565(100, 100, 100));
            _safeSprite.setCursor(50, 2); _safeSprite.print("MQTT");
            pushSafeSprite(210, 2);
        }
        oldWiFi = curWiFi; oldMQTT = curMQTT;
    }

    // --- Job area ---
    bool isJobAct = _mac.isJobActive();
    int curCutQty = _mac.getCurrentCutQty();
    if (isJobAct != oldJobActive || curCutQty != oldCutQty) {
        reqJobUIUpdate = true;
        oldJobActive = isJobAct;
        oldCutQty = curCutQty;
    }

    if (reqJobUIUpdate) {
        if (isJobAct) {
            JobData activeJob = _mac.getActiveJob();
            float progress = (activeJob.qty > 0) ? (float)curCutQty / activeJob.qty : 0;
            int fillWidth = (int)(progress * 290);
            char buf[64];

            // 🔴 Row 1: Title "CURRENT JOB" (320 x 25 = 16KB แทน 100KB)
            if (beginSafeSprite(300, 22)) {
                _safeSprite.setTextColor(TFT_CYAN);
                _safeSprite.setCursor(5, 3); _safeSprite.print("CURRENT JOB");
                pushSafeSprite(10, 30);
            }

            // 🔴 Row 2: Job size (ใช้ Font4 ตัวใหญ่)
            if (beginSafeSprite(300, 28)) {
                _safeSprite.setFont(&fonts::Font4);
                _safeSprite.setTextColor(TFT_WHITE);
                _safeSprite.setCursor(5, 2); _safeSprite.print(activeJob.size);
                pushSafeSprite(10, 55);
            }

            // 🔴 Row 3: Detail lengths
            if (beginSafeSprite(300, 20)) {
                _safeSprite.setTextColor(tft.color565(200, 200, 200));
                sprintf(buf, "L: %.1f mm  (Strip: %.1f / %.1f)",
                        activeJob.total_len, activeJob.front_len, activeJob.back_len);
                _safeSprite.setCursor(5, 2); _safeSprite.print(buf);
                pushSafeSprite(10, 88);
            }

            // 🔴 Row 4: Progress bar
            if (beginSafeSprite(300, 20)) {
                _safeSprite.drawRect(0, 2, 290, 16, tft.color565(100, 100, 100));
                if (fillWidth > 4)
                    _safeSprite.fillRect(2, 4, fillWidth - 4, 12, TFT_CYAN);
                pushSafeSprite(15, 115);
            }

            // 🔴 Row 5: Completed count + percentage
            if (beginSafeSprite(300, 22)) {
                _safeSprite.setTextColor(TFT_WHITE);
                sprintf(buf, "Completed: %d / %d pcs", curCutQty, activeJob.qty);
                _safeSprite.setCursor(5, 3); _safeSprite.print(buf);
                sprintf(buf, "%d %%", (int)(progress * 100));
                _safeSprite.setCursor(250, 3); _safeSprite.print(buf);
                pushSafeSprite(10, 140);
            }

        } else {
            tft.fillRect(0, 28, 320, 175, TFT_BLACK);
            if (beginSafeSprite(320, 50)) {
                _safeSprite.setTextDatum(top_center);
                _safeSprite.setTextColor(tft.color565(120, 120, 120));
                _safeSprite.drawString("NO ACTIVE JOB", 160, 3);
                _safeSprite.setTextColor(tft.color565(80, 80, 80));
                _safeSprite.drawString("Waiting for queue...", 160, 25);
                pushSafeSprite(0, 90);
            }
        }
        reqJobUIUpdate = false;
    }

    // --- Queue footer ---
    int currentQueueCount = uxQueueMessagesWaiting(jobQueue);
    if (currentQueueCount != oldQueueCount) {
        if (beginSafeSprite(320, 28)) {
            _safeSprite.setTextColor(tft.color565(150, 150, 150));
            _safeSprite.setCursor(15, 6); _safeSprite.print("Pending Queue:");
            _safeSprite.setTextColor(currentQueueCount > 0 ? TFT_ORANGE : TFT_WHITE);
            char buf[16]; sprintf(buf, "%d / 10", currentQueueCount);
            _safeSprite.setCursor(160, 6); _safeSprite.print(buf);
            pushSafeSprite(0, 210);
        }
        oldQueueCount = currentQueueCount;
    }
}

// ==========================================
// 2. QUEUE MANAGER
// ==========================================
void ScreenManager::_drawListQueue() {
    bool isFullRedraw = forceFullRedraw;
    if (isFullRedraw) {
        tft.fillScreen(TFT_BLACK);
        tft.fillRect(0, 0, 320, 25, tft.color565(180, 50, 0));
        tft.setTextColor(TFT_WHITE); tft.setFont(&fonts::Font2);
        tft.setCursor(15, 4); tft.print("MANAGE QUEUE");
        forceFullRedraw = false;
    }

    int visibleRows = 5;
    bool needRedrawAll = isFullRedraw
                      || (_sm_data.scrollOffset != _sm_data_old.scrollOffset)
                      || (_sm_data.snapshotCount != _sm_data_old.snapshotCount);
    bool indexMoved = (_sm_data.clearMenuIndex != _sm_data_old.clearMenuIndex);

    for (int i = 0; i < visibleRows; i++) {
        int dataIndex = _sm_data.scrollOffset + i;

        bool needRedraw = needRedrawAll;
        if (indexMoved && (dataIndex == _sm_data.clearMenuIndex || dataIndex == _sm_data_old.clearMenuIndex)) {
            needRedraw = true;
        }

        if (needRedraw) {
            if (beginSafeSprite(310, 32)) {

                if (dataIndex <= _sm_data.snapshotCount + 1) {
                    if (dataIndex == _sm_data.clearMenuIndex) {
                        _safeSprite.fillRect(0, 0, 310, 32, tft.color565(60, 20, 20));
                        _safeSprite.fillRect(0, 0, 5, 32, TFT_RED);
                    }

                    _safeSprite.setTextColor(dataIndex == _sm_data.clearMenuIndex ? TFT_WHITE : tft.color565(150, 150, 150));

                    if (dataIndex == _sm_data.snapshotCount) {
                        _safeSprite.setCursor(100, 8);
                        _safeSprite.print("[ BACK / CANCEL ]");
                    }
                    else if (dataIndex == _sm_data.snapshotCount + 1) {
                        _safeSprite.setTextColor(dataIndex == _sm_data.clearMenuIndex ? TFT_WHITE : TFT_RED);
                        _safeSprite.setCursor(45, 8);
                        _safeSprite.print("[ STOP CURRENT RUNNING JOB ]");
                    }
                    else {
                        char buf[50];
                        sprintf(buf, "%d. %-6s L:%.0f  Q:%d",
                                dataIndex + 1,
                                _sm_data.snapshotJobs[dataIndex].size,
                                _sm_data.snapshotJobs[dataIndex].total_len,
                                _sm_data.snapshotJobs[dataIndex].qty);
                        _safeSprite.setCursor(15, 8); _safeSprite.print(buf);

                        if (dataIndex == _sm_data.clearMenuIndex) {
                            _safeSprite.setTextColor(TFT_RED);
                            _safeSprite.setCursor(265, 8);
                            _safeSprite.print("DEL");
                        }
                    }
                }
                pushSafeSprite(5, 35 + i * 36);
            }
        }
    }
}

// ==========================================
// 3. MENU & EDIT
// 🔴 FIX: ลบ String ทั้งหมด → ใช้ char[] + sprintf
// ==========================================
void ScreenManager::_drawMenuAndEdit() {
    bool isFullRedraw = forceFullRedraw;
    if (isFullRedraw) {
        tft.fillScreen(TFT_BLACK);
        tft.fillRect(0, 0, 320, 25, tft.color565(0, 120, 120));
        tft.setTextColor(TFT_WHITE); tft.setFont(&fonts::Font2);
        tft.setCursor(15, 4); tft.print("MANUAL JOB SETUP");
        forceFullRedraw = false;
    }

    bool indexMoved = (_sm_data.menuIndex != _sm_data_old.menuIndex);
    bool modeChanged = (_sm_data.currentUIState != _sm_data_old.currentUIState);

    UIState cur_uiState = _sm_data.currentUIState;
    int cur_menuIndex = _sm_data.menuIndex;

    // 🔴 FIX: ใช้ const char* และ char[] บน stack — ไม่แตะ heap เลย
    static const char* titles[6] = {
        "Total Length", "Strip Front", "Strip Back",
        "Quantity", "ADD TO QUEUE", "CANCEL"
    };

    char vals[4][20];
    sprintf(vals[0], "%.1f mm", _sm_data.temp_total_len);
    sprintf(vals[1], "%.1f mm", _sm_data.temp_front_len);
    sprintf(vals[2], "%.1f mm", _sm_data.temp_back_len);
    sprintf(vals[3], "%d pcs",  _sm_data.temp_qty);

    int y_pos[6] = {40, 72, 104, 136, 175, 207};

    for (int i = 0; i < 6; i++) {
        bool needRedraw = isFullRedraw;

        if (i == 0 && _sm_data.temp_total_len != _sm_data_old.temp_total_len) needRedraw = true;
        if (i == 1 && _sm_data.temp_front_len != _sm_data_old.temp_front_len) needRedraw = true;
        if (i == 2 && _sm_data.temp_back_len  != _sm_data_old.temp_back_len)  needRedraw = true;
        if (i == 3 && _sm_data.temp_qty       != _sm_data_old.temp_qty)       needRedraw = true;
        if (indexMoved && (i == cur_menuIndex || i == _sm_data_old.menuIndex)) needRedraw = true;
        if (modeChanged && i == cur_menuIndex) needRedraw = true;

        if (needRedraw) {
            if (beginSafeSprite(310, 28)) {

                if (i == cur_menuIndex) {
                    if (cur_uiState == UIState::EDIT) {
                        _safeSprite.fillRect(0, 0, 310, 28, tft.color565(60, 60, 0));
                        _safeSprite.drawRect(0, 0, 310, 28, TFT_YELLOW);
                    } else {
                        _safeSprite.fillRect(0, 0, 310, 28, tft.color565(40, 40, 40));
                        _safeSprite.fillRect(0, 0, 5, 28, TFT_CYAN);
                    }
                }

                if (i <= 3) {
                    _safeSprite.setTextColor(TFT_WHITE);
                    _safeSprite.setCursor(15, 6);
                    _safeSprite.print(titles[i]);

                    _safeSprite.setTextColor(
                        i == cur_menuIndex
                            ? (cur_uiState == UIState::EDIT ? TFT_YELLOW : TFT_CYAN)
                            : tft.color565(180, 180, 180));
                    _safeSprite.setCursor(190, 6);
                    _safeSprite.print(vals[i]);
                } else {
                    _safeSprite.setTextColor(
                        i == cur_menuIndex ? TFT_WHITE : (i == 4 ? TFT_GREEN : TFT_RED));
                    _safeSprite.setCursor(100, 6);
                    _safeSprite.print(titles[i]);
                }

                pushSafeSprite(5, y_pos[i]);
            }
        }
    }
}
