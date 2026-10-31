// =====================================================================
//  OBD2 ESP32 CYD -- ตัวจำลองรถ + อะแดปเตอร์ ELM327 (Bluetooth Classic SPP) + หน้าเว็บควบคุม
//
//  บอร์ด ESP32 ธรรมดา (ESP32-WROOM DevKit) ทำตัวเป็นอะแดปเตอร์ ELM327 ชื่อ "OBDII" รหัส 1234
//  ให้เกจ CYD (src/main.cpp ของโปรเจกต์หลัก) เชื่อมต่อและทดสอบได้ที่โต๊ะ ไม่ต้องมีรถ
//
//  - รับคำสั่งทีละบรรทัด (จบด้วย CR) ไม่สนตัวพิมพ์เล็ก/ใหญ่และช่องว่าง ทุกคำตอบจบด้วย "\r\r>"
//  - รถจำลองมีหลายสถานการณ์ (เดินเบา, ขับในเมือง, 0-100, รอบแดง, ร้อนจัด, แบตอ่อน, น้ำมันใกล้หมด, ปรับค่าเอง ...)
//  - ควบคุมได้ 3 ทาง: หน้าเว็บ (http://obd-sim.local), คำสั่งตัวอักษรเดียวทาง USB Serial, ปุ่ม BOOT
//  - log ทุกคำสั่งที่ได้รับและคำตอบที่ส่งกลับออก USB Serial (115200) และหน้าเว็บ
//
//  งาน 2 ส่วน: task "elm" (ลำดับความสำคัญสูง) รับคำสั่งจาก CYD และส่งคำตอบทาง Bluetooth
//              loop() อัปเดตรถจำลอง, หน้าเว็บ, Serial, ปุ่ม, LED
//  หน้าเว็บส่งข้อมูลช้า (WiFi ใช้วิทยุร่วมกับ Bluetooth) จึงต้องแยกไม่ให้ไปหน่วงคำตอบของ CYD
//  ข้อมูลที่ใช้ร่วมกันป้องกันด้วย stateMutex (StateLock)
// =====================================================================

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <initializer_list>
#include <vector>

#include "web_page.h"

// WiFi บ้าน (ไม่บังคับ): คัดลอก include/secrets.h.example เป็น include/secrets.h
// ไม่มีไฟล์นี้ = เปิด WiFi ของตัวเอง (AP) ชื่อ OBD-SIM
#if __has_include("secrets.h")
#include "secrets.h"
#else
inline constexpr char WIFI_SSID[] = "";
inline constexpr char WIFI_PASSWORD[] = "";
#endif

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED) || !defined(CONFIG_BT_SPP_ENABLED)
#error "ต้องใช้ ESP32 รุ่นที่มี Bluetooth Classic (ESP32 ดั้งเดิม ไม่ใช่ S3/C3)"
#endif

namespace cfg {
constexpr uint32_t SERIAL_BAUD = 115200;
inline constexpr char BT_NAME[] = "OBDII";  // ต้องตรงกับ app::DEFAULT_OBD_NAME ของ CYD
inline constexpr char BT_PIN[] = "1234";    // ต้องตรงกับ app::DEFAULT_OBD_PIN ของ CYD
inline constexpr char ELM_VERSION[] = "ELM327 v1.5";
constexpr int BOOT_PIN = 0;                  // ปุ่ม BOOT บน DevKit
constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;
constexpr int LED_PIN = 2;                   // LED สถานะ (LED สีน้ำเงินบน DevKit ส่วนใหญ่, HIGH = ติด)
constexpr uint32_t LED_SLOW_MS = 500;        // รอการเชื่อมต่อ: ติด 0.5 s / ดับ 0.5 s
constexpr uint32_t LED_FAST_MS = 100;        // Bluetooth เปิดไม่ได้ / จับคู่ไม่สำเร็จ: กะพริบเร็ว
constexpr uint32_t LED_ACTIVITY_MS = 40;     // เชื่อมต่อแล้ว: ดับแวบเมื่อได้รับคำสั่ง
constexpr uint32_t LED_AUTH_FAIL_MS = 3000;  // จับคู่ไม่สำเร็จ: กะพริบเร็วนานกี่ ms
constexpr uint32_t REPLY_DELAY_MIN_MS = 20;  // ค่าเริ่มต้นของการหน่วงคำตอบแบบสุ่ม (ปรับได้จากหน้าเว็บ)
constexpr uint32_t REPLY_DELAY_MAX_MS = 50;
constexpr uint32_t REPLY_DELAY_LIMIT_MS = 1000;  // หน้าเว็บตั้งได้สูงสุด (CYD หมดเวลาที่ 1500 ms)
constexpr uint32_t ATZ_DELAY_MS = 400;       // ATZ ของจริงใช้เวลารีเซ็ตนานกว่าคำสั่งอื่น
constexpr uint32_t SEARCH_DELAY_MS = 1500;   // 0100 ครั้งแรกหลัง ATSP0 ("SEARCHING...")
constexpr uint32_t SIM_STEP_MS = 50;         // อัปเดตรถจำลองทุกกี่ ms
constexpr uint32_t STATUS_PRINT_MS = 5000;
constexpr size_t CMD_MAX = 32;
constexpr float IDLE_RPM = 780.0f;
constexpr int MAX_FAULTS = 8;                // รหัสข้อผิดพลาดสูงสุด (เกิน 2 รหัสตอบแบบหลายเฟรม)

// ---- เครือข่าย / หน้าเว็บ ----
inline constexpr char AP_SSID[] = "OBD-SIM";
inline constexpr char AP_PASSWORD[] = "obdsim1234";  // 8 ตัวขึ้นไป
inline constexpr char MDNS_NAME[] = "obd-sim";       // http://obd-sim.local
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000;
constexpr int LOG_LINES = 80;                        // log ล่าสุดที่หน้าเว็บดึงได้
constexpr size_t LOG_LINE_LEN = 96;
constexpr int LOG_MAX_PER_REQUEST = 30;              // หน้าเว็บดึง log ได้ครั้งละไม่เกินกี่บรรทัด (ลด traffic)
constexpr UBaseType_t ELM_TASK_PRIORITY = 3;         // สูงกว่า loop() (1): ตอบ CYD ก่อนงานหน้าเว็บเสมอ
}  // namespace cfg

BluetoothSerial SerialBT;
WebServer server(80);

// ข้อมูลที่ task "elm" กับ loop() ใช้ร่วมกัน (รถ, รหัส, การตั้งค่า, log) ต้องถือ lock นี้ก่อนอ่าน/เขียน
SemaphoreHandle_t stateMutex = nullptr;
struct StateLock {
    StateLock() { xSemaphoreTakeRecursive(stateMutex, portMAX_DELAY); }
    ~StateLock() { xSemaphoreGiveRecursive(stateMutex); }
    StateLock(const StateLock &) = delete;
    StateLock &operator=(const StateLock &) = delete;
};

// =====================================================================
//  log: ออก USB Serial และเก็บล่าสุด LOG_LINES บรรทัดให้หน้าเว็บ (ring buffer)
// =====================================================================
char logLines[cfg::LOG_LINES][cfg::LOG_LINE_LEN];
uint32_t logSeq = 0;  // จำนวนบรรทัดทั้งหมดที่เคยเขียน (บรรทัดที่ n อยู่ช่อง n % LOG_LINES)

void webLog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void webLog(const char *fmt, ...)
{
    char line[cfg::LOG_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    StateLock lock;
    Serial.println(line);
    char *slot = logLines[logSeq % cfg::LOG_LINES];
    snprintf(slot, cfg::LOG_LINE_LEN, "%lu %s", static_cast<unsigned long>(millis() / 100), line);  // เวลาหน่วย 0.1 s
    logSeq++;
}

// =====================================================================
//  สถานะ Bluetooth + LED ที่ GPIO2
//
//    กะพริบเร็วตลอด (5 ครั้ง/วินาที)  = เปิด Bluetooth ไม่ได้
//    กะพริบช้า (ติด 0.5 s / ดับ 0.5 s) = Bluetooth พร้อม รอ CYD เชื่อมต่อ (ค้นหาเจอได้)
//    ติดค้าง                           = CYD เชื่อมต่ออยู่ (ดับแวบทุกครั้งที่ได้รับคำสั่ง)
//    กะพริบเร็ว 3 วินาที                = จับคู่ (pairing) ไม่สำเร็จ แล้วกลับไปกะพริบช้า
// =====================================================================
bool btReady = false;
uint32_t ledActivityAt = 0;
uint32_t authFailAt = 0;
bool authFailed = false;
char peerMac[18] = "";  // MAC ของ CYD ที่เชื่อมต่ออยู่ล่าสุด

// เหตุการณ์จาก Bluetooth task: เก็บไว้ก่อนแล้ว log ใน loop()
volatile bool sppOpenEvt = false;
volatile bool sppCloseEvt = false;
volatile bool authEvt = false;
volatile bool authOk = false;
uint8_t sppRemote[6] = {0};

void sppCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    if (event == ESP_SPP_SRV_OPEN_EVT) {
        memcpy(sppRemote, param->srv_open.rem_bda, sizeof(sppRemote));
        sppOpenEvt = true;
    } else if (event == ESP_SPP_CLOSE_EVT) {
        sppCloseEvt = true;
    }
}

void logBtEvents(uint32_t now)
{
    if (authEvt) {
        authEvt = false;
        webLog("[BT] pairing %s", authOk ? "ok" : "FAILED (check PIN / remove old pairing)");
        if (!authOk) {
            authFailed = true;
            authFailAt = now;
        }
    }
    if (sppOpenEvt) {
        sppOpenEvt = false;
        snprintf(peerMac, sizeof(peerMac), "%02X:%02X:%02X:%02X:%02X:%02X", sppRemote[0], sppRemote[1], sppRemote[2], sppRemote[3],
                 sppRemote[4], sppRemote[5]);
        webLog("[BT] SPP opened by %s", peerMac);
    }
    if (sppCloseEvt) {
        sppCloseEvt = false;
        webLog("[BT] SPP closed");
    }
}

void ledUpdate(uint32_t now)
{
    bool on;
    if (!btReady || (authFailed && now - authFailAt < cfg::LED_AUTH_FAIL_MS)) {
        on = (now / cfg::LED_FAST_MS) & 1;
    } else if (SerialBT.hasClient()) {
        on = now - ledActivityAt >= cfg::LED_ACTIVITY_MS;
    } else {
        on = (now / cfg::LED_SLOW_MS) & 1;
    }
    digitalWrite(cfg::LED_PIN, on ? HIGH : LOW);
}

// =====================================================================
//  รถจำลอง
// =====================================================================
enum class Scenario : uint8_t { Idle, City, Launch, Redline, Overheat, LowBattery, LowFuel, NoFuelPid, Manual, Count };

struct ScenarioInfo {
    char key;          // คำสั่ง Serial
    const char *id;    // ชื่อใน API ของหน้าเว็บ
    const char *name;
    const char *desc;
};
constexpr ScenarioInfo SCENARIOS[] = {
    {'i', "idle", "IDLE", "idle ~780 rpm, 0 km/h, coolant 90 C"},
    {'c', "city", "CITY", "city drive: stop / accelerate / cruise / brake, 30-60 km/h"},
    {'l', "launch", "LAUNCH", "0-100 km/h launch (~6.7 s, shifts at 6200 rpm), then brake and repeat"},
    {'r', "redline", "REDLINE", "rpm sweeps 3000 <-> 7200 (shift light / redline beep)"},
    {'o', "overheat", "OVERHEAT", "idle, coolant ramps up to 118 C (alert at >= 105)"},
    {'b', "lowbatt", "LOW BATT", "idle, battery 11.9 V"},
    {'f', "lowfuel", "LOW FUEL", "idle, fuel level 7 %"},
    {'n', "nofuel", "NO FUEL PID", "012F not supported (0120 mask without 2F, 012F -> NO DATA)"},
    {'m', "manual", "MANUAL", "every value set by hand from the web page"},
};
constexpr int SCENARIO_COUNT = static_cast<int>(Scenario::Count);
static_assert(sizeof(SCENARIOS) / sizeof(SCENARIOS[0]) == SCENARIO_COUNT, "SCENARIOS must list every Scenario");

struct Vehicle {
    float rpm = cfg::IDLE_RPM;
    float speed = 0;     // km/h
    float coolant = 85;  // C
    float throttle = 12; // %
    float load = 20;     // %
    float intake = 32;   // C
    float fuel = 62;     // %
    float volt = 14.1f;  // V
    int gear = 0;        // 0 = เกียร์ว่าง
};

Vehicle car;
Vehicle manualCar;  // ค่าที่ตั้งจากหน้าเว็บ (สถานการณ์ MANUAL)
Scenario scenario = Scenario::Idle;
uint32_t scenarioStart = 0;

// ---- รหัสข้อผิดพลาด (2 ไบต์ เช่น P0133 = 0x0133) ----
uint16_t faults[cfg::MAX_FAULTS];
int faultCount = 0;  // > 0 = ไฟ MIL ติด
constexpr uint16_t DEFAULT_FAULTS[] = {0x0133, 0x0420};

// ---- พฤติกรรมของอะแดปเตอร์ / ECU (ปรับจากหน้าเว็บ) ----
bool ecuOn = true;       // false = ดับเครื่อง/ECU ไม่ตอบ: คำสั่ง OBD ได้ NO DATA (CYD ขึ้น WAITING FOR ECU)
bool fuelPid = true;     // false = ECU ไม่รองรับ PID 2F (ระดับน้ำมัน)
uint32_t replyMinMs = cfg::REPLY_DELAY_MIN_MS;
uint32_t replyMaxMs = cfg::REPLY_DELAY_MAX_MS;
uint32_t cmdCount = 0;   // จำนวนคำสั่งที่ได้รับทั้งหมด

// km/h ต่อ 1000 rpm ของแต่ละเกียร์ (เกียร์ 1-6)
constexpr float KMH_PER_1000[] = {7.5f, 13.0f, 20.0f, 27.0f, 34.0f, 41.0f};
constexpr int GEARS = sizeof(KMH_PER_1000) / sizeof(KMH_PER_1000[0]);

float jitter(float amount)
{
    return (static_cast<float>(random(-1000, 1001)) / 1000.0f) * amount;
}

// ค่อย ๆ เข้าหาค่าเป้าหมาย (rate = หน่วยต่อวินาที)
float approach(float value, float target, float rate, float dt)
{
    const float step = rate * dt;
    if (value < target) return min(value + step, target);
    return max(value - step, target);
}

// เลือกเกียร์ต่ำสุดที่รอบไม่เกิน shiftRpm แล้วคำนวณรอบจากความเร็ว
void driveTrain(float shiftRpm)
{
    if (car.speed < 1.0f) {
        car.gear = 0;
        car.rpm = cfg::IDLE_RPM + jitter(15);
        return;
    }
    int g = 0;
    while (g < GEARS - 1 && car.speed / KMH_PER_1000[g] * 1000.0f > shiftRpm) g++;
    car.gear = g + 1;
    car.rpm = max(cfg::IDLE_RPM, car.speed / KMH_PER_1000[g] * 1000.0f) + jitter(20);
}

void setIdleOutputs()
{
    car.speed = 0;
    car.gear = 0;
    car.rpm = cfg::IDLE_RPM + jitter(15);
    car.throttle = 12 + jitter(0.5f);
    car.load = 21 + jitter(1);
}

// ขับในเมือง: วนรอบ จอด 6 s -> เร่งถึงเป้า -> วิ่งคงที่ 10 s -> เบรกถึง 0
void simCity(float dt)
{
    static const float TARGETS[] = {40, 60, 30, 50};
    static uint8_t phase = 0;   // 0 จอด, 1 เร่ง, 2 วิ่งคงที่, 3 เบรก
    static uint8_t leg = 0;
    static float phaseTime = 0;
    static uint32_t startedAt = 0;
    if (startedAt != scenarioStart) {  // เพิ่งเลือกสถานการณ์นี้: เริ่มรอบใหม่
        startedAt = scenarioStart;
        phase = 0;
        phaseTime = 0;
    }
    phaseTime += dt;
    const float target = TARGETS[leg % 4];
    switch (phase) {
        case 0:
            car.speed = approach(car.speed, 0, 8, dt);
            car.throttle = 12;
            car.load = 21;
            if (phaseTime > 6) { phase = 1; phaseTime = 0; }
            break;
        case 1:
            car.speed = approach(car.speed, target, 6, dt);
            car.throttle = 38;
            car.load = 62;
            if (car.speed >= target) { phase = 2; phaseTime = 0; }
            break;
        case 2:
            car.speed = target + jitter(1.0f);
            car.throttle = 16;
            car.load = 32;
            if (phaseTime > 10) { phase = 3; phaseTime = 0; }
            break;
        default:
            car.speed = approach(car.speed, 0, 7, dt);
            car.throttle = 0;
            car.load = 12;
            if (car.speed <= 0) { phase = 0; phaseTime = 0; leg++; }
            break;
    }
    car.throttle += jitter(1);
    car.load += jitter(2);
    driveTrain(2600);
}

// 0-100: จอด 4 s -> คันเร่งสุด เร่งถึง 105 km/h -> ค้าง 3 s -> เบรก -> วนใหม่
void simLaunch(float dt)
{
    static uint8_t phase = 0;
    static float phaseTime = 0;
    static uint32_t startedAt = 0;
    static uint32_t runStart = 0;
    if (startedAt != scenarioStart) {
        startedAt = scenarioStart;
        phase = 0;
        phaseTime = 0;
    }
    phaseTime += dt;
    switch (phase) {
        case 0:
            car.speed = approach(car.speed, 0, 12, dt);
            car.throttle = 12;
            car.load = 21;
            driveTrain(6200);
            if (phaseTime > 4 && car.speed <= 0) {
                phase = 1;
                phaseTime = 0;
                runStart = millis();
                webLog("[SIM] launch: GO");
            }
            break;
        case 1: {
            static const float ACCEL[] = {17, 15, 12, 9, 7, 5};  // km/h ต่อวินาที ของแต่ละเกียร์
            const int g = car.gear > 0 ? car.gear - 1 : 0;
            const float before = car.speed;
            car.speed += ACCEL[g] * dt;
            car.throttle = 100;
            car.load = 95 + jitter(3);
            driveTrain(6200);
            if (car.speed < 12) car.rpm = max(car.rpm, 3200.0f + car.speed * 150);  // คลัตช์ลื่นตอนออกตัว
            if (before < 100 && car.speed >= 100) webLog("[SIM] launch: 100 km/h in %.2f s", (millis() - runStart) / 1000.0f);
            if (car.speed >= 105) { phase = 2; phaseTime = 0; }
            break;
        }
        case 2:
            car.speed = 105 + jitter(0.5f);
            car.throttle = 20;
            car.load = 35;
            driveTrain(3000);
            if (phaseTime > 3) { phase = 3; phaseTime = 0; }
            break;
        default:
            car.speed = approach(car.speed, 0, 20, dt);
            car.throttle = 0;
            car.load = 10;
            driveTrain(2500);
            if (car.speed <= 0) { phase = 0; phaseTime = 0; }
            break;
    }
}

// รอบขึ้นลง 3000 <-> 7200 ในเกียร์ 3 (ช่วงละ 4 วินาที)
void simRedline()
{
    const float t = fmodf((millis() - scenarioStart) / 1000.0f, 8.0f);
    const bool rising = t < 4.0f;
    const float x = rising ? t / 4.0f : (8.0f - t) / 4.0f;
    car.gear = 3;
    car.rpm = 3000 + x * 4200 + jitter(20);
    car.speed = car.rpm / 1000.0f * KMH_PER_1000[2];
    car.throttle = rising ? 100 : 0;
    car.load = rising ? 92 + jitter(3) : 15;
}

void simUpdate(float dt)
{
    if (scenario == Scenario::Manual) {  // ค่าตามที่ตั้งจากหน้าเว็บ ไม่มีการแกว่ง
        car = manualCar;
        car.gear = 0;
        return;
    }
    switch (scenario) {
        case Scenario::City: simCity(dt); break;
        case Scenario::Launch: simLaunch(dt); break;
        case Scenario::Redline: simRedline(); break;
        default: setIdleOutputs(); break;
    }

    // น้ำหล่อเย็น: ร้อนจัดค่อย ๆ ขึ้นถึง 118 C, สถานการณ์อื่นกลับสู่ 90 C (ลงเร็วหน่อยให้ทดสอบซ้ำได้ไว)
    if (scenario == Scenario::Overheat) {
        car.coolant = approach(car.coolant, 118, 0.6f, dt);
    } else {
        car.coolant = approach(car.coolant, 90, car.coolant > 90 ? 1.5f : 0.4f, dt);
    }
    car.intake = 32 + (car.speed < 5 ? 4 : 0) + jitter(0.3f);

    // น้ำมัน: ปกติ 62% ลดลงช้า ๆ ระหว่างขับ, น้ำมันใกล้หมด = 7%
    if (scenario == Scenario::LowFuel) {
        car.fuel = 7;
    } else {
        if (car.fuel < 50) car.fuel = 62;
        car.fuel -= car.speed * 0.00002f * dt * 50;
    }

    car.volt = (scenario == Scenario::LowBattery ? 11.9f : 14.1f) + jitter(0.05f);
    car.throttle = constrain(car.throttle, 0.0f, 100.0f);
    car.load = constrain(car.load, 0.0f, 100.0f);
}

// =====================================================================
//  ELM327
// =====================================================================
struct Elm {
    bool echo = true;
    bool linefeed = false;
    bool spaces = true;
    bool headers = false;
    bool searchPending = true;  // ยังไม่เคยค้นหา protocol (0100 ครั้งแรกขึ้น "SEARCHING...")
};
Elm elm;

char rxBuf[cfg::CMD_MAX + 1];
size_t rxLen = 0;
bool rxOverflow = false;
char lastCmd[cfg::CMD_MAX + 1] = "";

String pendingReply;       // คำตอบที่รอส่ง (หน่วงเวลาให้เหมือนของจริง)
uint32_t replyDueAt = 0;
bool replyPending = false;
bool dropLinkAfterReply = false;
volatile bool dropRequested = false;  // loop() ขอตัดลิงก์ task "elm" เป็นคนตัด

void elmReset()
{
    elm = Elm();
}

const char *eol()
{
    return elm.linefeed ? "\r\n" : "\r";
}

// สร้างคำตอบจากหลายบรรทัด ปิดท้ายด้วยบรรทัดว่างและ '>' ("41 0C 0C 30\r\r>")
String finishReply(std::initializer_list<String> lines)
{
    String out;
    for (const String &l : lines) {
        out += l;
        out += eol();
    }
    out += eol();
    out += '>';
    return out;
}

// ไบต์ข้อมูล -> ข้อความ hex ตามการตั้งค่า ATS (ช่องว่าง) และ ATH (header แบบ CAN 11 บิต: 7E8 + ความยาว)
String formatFrame(const uint8_t *data, size_t len)
{
    String out;
    char hx[4];
    if (elm.headers) {
        out += "7E8";
        snprintf(hx, sizeof(hx), "%02X", static_cast<unsigned>(len));
        if (elm.spaces) out += ' ';
        out += hx;
    }
    for (size_t i = 0; i < len; i++) {
        if (elm.spaces && out.length() > 0) out += ' ';
        snprintf(hx, sizeof(hx), "%02X", data[i]);
        out += hx;
    }
    return out;
}

uint8_t pctByte(float pct)
{
    return static_cast<uint8_t>(constrain(lroundf(pct * 255.0f / 100.0f), 0L, 255L));
}

uint8_t tempByte(float c)
{
    return static_cast<uint8_t>(constrain(lroundf(c + 40.0f), 0L, 255L));
}

bool isHexString(const char *s)
{
    for (; *s != '\0'; s++) {
        if (!isxdigit(static_cast<unsigned char>(*s))) return false;
    }
    return true;
}

// ตอบ Mode 01 ด้วยสูตรเดียวกับที่ CYD ถอดใน decodePid(); false = ไม่รองรับ (NO DATA)
bool mode01(uint8_t pid, uint8_t *d, size_t &len)
{
    d[0] = 0x41;
    d[1] = pid;
    len = 3;
    switch (pid) {
        case 0x00:  // PID ที่รองรับ 01-20: 04 05 0C 0D 0F 11 20
            d[2] = 0x18; d[3] = 0x1A; d[4] = 0x80; d[5] = 0x01;
            len = 6;
            return true;
        case 0x20:  // PID ที่รองรับ 21-40: 2F (ระดับน้ำมัน) เมื่อเปิดใช้
            d[2] = 0x00; d[3] = fuelPid ? 0x02 : 0x00; d[4] = 0x00; d[5] = 0x00;
            len = 6;
            return true;
        case 0x01:  // A: bit7 = MIL, bit0-6 = จำนวนรหัส / B C D = สถานะการทดสอบ
            d[2] = faultCount > 0 ? static_cast<uint8_t>(0x80 | faultCount) : 0x00;
            d[3] = 0x07; d[4] = 0xE5; d[5] = 0x00;
            len = 6;
            return true;
        case 0x04: d[2] = pctByte(car.load); return true;
        case 0x05: d[2] = tempByte(car.coolant); return true;
        case 0x0C: {  // (256*A + B) / 4
            const long raw = constrain(lroundf(car.rpm * 4.0f), 0L, 65535L);
            d[2] = static_cast<uint8_t>(raw >> 8);
            d[3] = static_cast<uint8_t>(raw & 0xFF);
            len = 4;
            return true;
        }
        case 0x0D: d[2] = static_cast<uint8_t>(constrain(lroundf(car.speed), 0L, 255L)); return true;
        case 0x0F: d[2] = tempByte(car.intake); return true;
        case 0x11: d[2] = pctByte(car.throttle); return true;
        case 0x2F:
            if (!fuelPid) return false;
            d[2] = pctByte(car.fuel);
            return true;
        default: return false;
    }
}

// Mode 03 แบบ CAN: 43 [จำนวน] ตามด้วยรหัสทีละ 2 ไบต์ ("43 02 01 33 04 20" = P0133, P0420)
// เกิน 6 ไบต์ (มากกว่า 2 รหัส) ตอบแบบหลายเฟรมเหมือน ELM327 จริงตอน ATH0: "00A" / "0: ..." / "1: ..."
std::vector<String> mode03Lines()
{
    uint8_t d[2 + 2 * cfg::MAX_FAULTS];
    size_t len = 0;
    d[len++] = 0x43;
    d[len++] = static_cast<uint8_t>(faultCount);
    for (int i = 0; i < faultCount; i++) {
        d[len++] = static_cast<uint8_t>(faults[i] >> 8);
        d[len++] = static_cast<uint8_t>(faults[i] & 0xFF);
    }
    if (len <= 7 || elm.headers) return {formatFrame(d, min(len, static_cast<size_t>(7)))};  // ATH1: ตัดเหลือเฟรมเดียว

    std::vector<String> lines;
    char head[8];
    snprintf(head, sizeof(head), "%03X", static_cast<unsigned>(len));
    lines.push_back(head);
    size_t pos = 0;
    for (int frame = 0; pos < len; frame++) {
        const size_t n = min(frame == 0 ? static_cast<size_t>(6) : static_cast<size_t>(7), len - pos);
        char prefix[4];
        snprintf(prefix, sizeof(prefix), "%X:", frame & 0xF);
        String line = prefix;
        if (elm.spaces) line += ' ';
        line += formatFrame(d + pos, n);
        lines.push_back(line);
        pos += n;
    }
    return lines;
}

// คำสั่ง AT (cmd ไม่มีช่องว่าง ตัวพิมพ์ใหญ่ ขึ้นต้นด้วย "AT")
String handleAt(const char *cmd, uint32_t &delayMs)
{
    const char *a = cmd + 2;
    auto flag = [&](const char *name, bool &target) -> bool {
        const size_t n = strlen(name);
        if (strncmp(a, name, n) != 0 || (a[n] != '0' && a[n] != '1') || a[n + 1] != '\0') return false;
        target = a[n] == '1';
        return true;
    };

    if (strcmp(a, "Z") == 0) {
        elmReset();
        delayMs = cfg::ATZ_DELAY_MS;
        return finishReply({"", cfg::ELM_VERSION});
    }
    if (strcmp(a, "WS") == 0) {
        elmReset();
        return finishReply({"", cfg::ELM_VERSION});
    }
    if (strcmp(a, "D") == 0) {
        const bool e = elm.echo;
        elmReset();
        elm.echo = e;
        return finishReply({"OK"});
    }
    if (strcmp(a, "I") == 0) return finishReply({cfg::ELM_VERSION});
    if (strcmp(a, "@1") == 0) return finishReply({"OBDII to RS232 Interpreter"});
    if (strcmp(a, "RV") == 0) {  // อะแดปเตอร์วัดแรงดันเอง: ดับเครื่อง = แบตไม่ได้ชาร์จ
        char v[8];
        snprintf(v, sizeof(v), "%.1fV", ecuOn ? car.volt : min(car.volt, 12.4f));
        return finishReply({v});
    }
    if (strcmp(a, "DPN") == 0) return finishReply({"A6"});
    if (strcmp(a, "DP") == 0) return finishReply({"AUTO, ISO 15765-4 (CAN 11/500)"});
    bool dummy;
    if (flag("E", elm.echo) || flag("L", elm.linefeed) || flag("S", elm.spaces) || flag("H", elm.headers) || flag("M", dummy)) {
        return finishReply({"OK"});
    }
    if (strncmp(a, "AT", 2) == 0 && a[2] >= '0' && a[2] <= '2' && a[3] == '\0') return finishReply({"OK"});
    if (strncmp(a, "SP", 2) == 0 && a[2] != '\0' && a[3] == '\0' && isxdigit(static_cast<unsigned char>(a[2]))) {
        elm.searchPending = true;  // เลือก protocol ใหม่: 0100 ครั้งถัดไปต้องค้นหาอีก
        return finishReply({"OK"});
    }
    if (strncmp(a, "ST", 2) == 0 && strlen(a) == 4 && isHexString(a + 2)) return finishReply({"OK"});
    return finishReply({"?"});
}

// คำสั่ง OBD (เลขฐานสิบหก) เช่น "010C", "03", "04"
String handleObd(const char *cmd, uint32_t &delayMs)
{
    const size_t n = strlen(cmd);
    if (n < 2 || ((n & 1) != 0 && n != 5)) return finishReply({"?"});
    if (!isHexString(cmd)) return finishReply({"?"});

    std::vector<String> lines;
    if (elm.searchPending) {  // คำสั่ง OBD แรกหลัง ATZ/ATSP0: ค้นหา protocol ก่อน
        lines.push_back("SEARCHING...");
        delayMs += cfg::SEARCH_DELAY_MS;
        elm.searchPending = false;
    }
    auto reply = [&](std::vector<String> body) {
        String out;
        for (const String &l : lines) out += l + eol();
        for (const String &l : body) out += l + eol();
        out += eol();
        out += '>';
        return out;
    };

    // ECU ไม่ตอบ (ดับเครื่อง): ตอนค้นหา protocol = UNABLE TO CONNECT, หลังจากนั้น = NO DATA (ตอบเร็ว CYD จึงไม่หมดเวลา)
    if (!ecuOn) return reply({lines.empty() ? "NO DATA" : "UNABLE TO CONNECT"});

    const uint8_t mode = static_cast<uint8_t>(strtoul(String(cmd).substring(0, 2).c_str(), nullptr, 16));
    if (mode == 0x01 && (n == 4 || n == 5)) {  // ตัวที่ 5 = จำนวนคำตอบที่ต้องการ (เช่น "010C1")
        const uint8_t pid = static_cast<uint8_t>(strtoul(String(cmd).substring(2, 4).c_str(), nullptr, 16));
        uint8_t d[8];
        size_t len = 0;
        if (!mode01(pid, d, len)) return reply({"NO DATA"});
        return reply({formatFrame(d, len)});
    }
    if (mode == 0x03 && n == 2) return reply(mode03Lines());
    if (mode == 0x04 && n == 2) {
        if (faultCount > 0) webLog("[SIM] mode 04: %d fault code(s) cleared, MIL off", faultCount);
        faultCount = 0;
        const uint8_t d[1] = {0x44};
        return reply({formatFrame(d, 1)});
    }
    return reply({"NO DATA"});
}

// คำตอบแบบอ่านง่ายสำหรับ log: ตัด echo/\r/'>' แล้วคั่นบรรทัดด้วย " | "
String compactReply(const String &s)
{
    String out;
    String line;
    for (size_t i = 0; i <= s.length(); i++) {
        const char c = i < s.length() ? s[i] : '\r';
        if (c == '\r' || c == '\n') {
            if (line.length() > 0) {
                if (out.length() > 0) out += " | ";
                out += line;
                line = "";
            }
        } else if (c != '>') {
            line += c;
        }
    }
    return out;
}

void handleCommand(const char *raw)
{
    char cmd[cfg::CMD_MAX + 1];
    strlcpy(cmd, raw, sizeof(cmd));
    if (cmd[0] == '\0') strlcpy(cmd, lastCmd, sizeof(cmd));  // CR เปล่า = ทำคำสั่งเดิมซ้ำ (เหมือน ELM327)
    else strlcpy(lastCmd, cmd, sizeof(lastCmd));
    cmdCount++;

    const uint32_t lo = min(replyMinMs, replyMaxMs), hi = max(replyMinMs, replyMaxMs);
    uint32_t delayMs = static_cast<uint32_t>(random(lo, hi + 1));
    String body;
    if (cmd[0] == '\0') {
        body = finishReply({"?"});
    } else if (strncmp(cmd, "AT", 2) == 0) {
        body = handleAt(cmd, delayMs);
    } else {
        body = handleObd(cmd, delayMs);
    }
    String out;
    if (elm.echo) {
        out += raw;
        out += '\r';
    }
    out += body;

    webLog("[RX] %-8s +%ums  %s", cmd, static_cast<unsigned>(delayMs), compactReply(body).c_str());
    pendingReply = out;
    replyDueAt = millis() + delayMs;
    replyPending = true;
}

void pollBluetooth()
{
    while (SerialBT.available() > 0) {
        const int c = SerialBT.read();
        if (c < 0) break;
        if (c == '\r') {
            rxBuf[rxLen] = '\0';
            if (rxOverflow) {
                webLog("[RX] (command too long)");
                pendingReply = finishReply({"?"});
                replyDueAt = millis() + cfg::REPLY_DELAY_MIN_MS;
                replyPending = true;
            } else {
                StateLock lock;  // อ่านค่ารถ/รหัส และเขียน log
                handleCommand(rxBuf);
            }
            ledActivityAt = millis();
            rxLen = 0;
            rxOverflow = false;
        } else if (c == ' ' || c == '\n' || c < 0x20) {
            // ไม่สนช่องว่าง/LF/อักขระควบคุม
        } else if (rxLen < cfg::CMD_MAX) {
            rxBuf[rxLen++] = static_cast<char>(toupper(c));
        } else {
            rxOverflow = true;
        }
    }

    if (dropRequested) {
        dropRequested = false;
        if (replyPending) dropLinkAfterReply = true;
        else if (SerialBT.hasClient()) SerialBT.disconnect();
    }

    if (replyPending && static_cast<int32_t>(millis() - replyDueAt) >= 0) {
        SerialBT.print(pendingReply);
        replyPending = false;
        if (dropLinkAfterReply) {
            dropLinkAfterReply = false;
            vTaskDelay(pdMS_TO_TICKS(100));
            SerialBT.disconnect();
        }
    }
}

// task "elm": ตอบ CYD ทาง Bluetooth แยกจาก loop() (หน้าเว็บ) เพื่อไม่ให้การส่งหน้าเว็บที่ช้าไปหน่วงคำตอบ
void elmTask(void *)
{
    bool wasConnected = false;
    for (;;) {
        const bool connected = SerialBT.hasClient();
        if (connected != wasConnected) {
            wasConnected = connected;
            webLog("[BT] %s", connected ? "client connected (LED on)" : "client disconnected (LED blinks slowly)");
            if (!connected) {  // ลิงก์ใหม่ = อะแดปเตอร์เพิ่งเปิด
                elmReset();
                rxLen = 0;
                rxOverflow = false;
                replyPending = false;
                dropLinkAfterReply = false;
                dropRequested = false;
            }
        }
        pollBluetooth();
        vTaskDelay(1);  // ให้ loop() ได้ทำงาน (1 tick = 1 ms)
    }
}

// ขอตัดลิงก์ Bluetooth ให้ CYD เชื่อมต่อใหม่และถาม 0100/0120 อีกครั้ง (task "elm" ตัดให้หลังส่งคำตอบที่ค้าง)
void dropLink(const char *why)
{
    if (!SerialBT.hasClient()) return;
    webLog("[SIM] dropping the Bluetooth link: %s", why);
    dropRequested = true;
}

// =====================================================================
//  สถานการณ์ / ค่าที่ปรับได้ (ใช้ร่วมกันทั้ง Serial, ปุ่ม BOOT และหน้าเว็บ)
// =====================================================================
void setFuelPid(bool on)
{
    if (on == fuelPid) return;
    fuelPid = on;
    webLog("[SIM] fuel level PID 2F %s", on ? "supported" : "NOT supported (NO DATA)");
    // CYD ถาม PID ที่รองรับ (0100/0120) แค่ตอนเชื่อมต่อ: ต้องตัดลิงก์ให้ CYD ถามใหม่
    dropLink("supported-PID mask changed");
}

void setScenario(Scenario next)
{
    const Scenario prev = scenario;
    scenario = next;
    scenarioStart = millis();
    if (next == Scenario::Manual && prev != Scenario::Manual) manualCar = car;  // เริ่มจากค่าปัจจุบัน
    webLog("[SIM] scenario -> %s: %s", SCENARIOS[static_cast<int>(next)].name, SCENARIOS[static_cast<int>(next)].desc);
    if (next == Scenario::NoFuelPid) setFuelPid(false);
    else if (prev == Scenario::NoFuelPid) setFuelPid(true);
}

void setFaults(const uint16_t *codes, int count)
{
    faultCount = min(count, cfg::MAX_FAULTS);
    for (int i = 0; i < faultCount; i++) faults[i] = codes[i];
}

void formatDtc(uint16_t code, char *out, size_t size)
{
    static const char LETTERS[4] = {'P', 'C', 'B', 'U'};
    snprintf(out, size, "%c%04X", LETTERS[(code >> 14) & 3], code & 0x3FFF);
}

// "P0133" -> 0x0133, "U0100" -> 0xC100; false = รูปแบบไม่ถูกต้อง
bool parseDtc(const String &text, uint16_t &code)
{
    String t = text;
    t.trim();
    t.toUpperCase();
    if (t.length() != 5) return false;
    const char *letters = "PCBU";
    const char *p = strchr(letters, t[0]);
    if (p == nullptr || t[1] < '0' || t[1] > '3' || !isHexString(t.c_str() + 2)) return false;
    code = static_cast<uint16_t>(((p - letters) << 14) | ((t[1] - '0') << 12) | strtoul(t.c_str() + 2, nullptr, 16));
    return true;
}

void logFaults()
{
    String list;
    for (int i = 0; i < faultCount; i++) {
        char c[8];
        formatDtc(faults[i], c, sizeof(c));
        if (i > 0) list += ' ';
        list += c;
    }
    webLog("[SIM] fault codes: %s", faultCount > 0 ? (list + " (MIL on)").c_str() : "none");
}

// =====================================================================
//  คำสั่ง Serial / ปุ่ม BOOT
// =====================================================================
void printHelp()
{
    Serial.println();
    Serial.println("=== OBD-II / ELM327 simulator: commands (USB Serial) ===");
    for (int i = 0; i < SCENARIO_COUNT; i++) {
        Serial.printf("  %c  %-12s %s\n", SCENARIOS[i].key, SCENARIOS[i].name, SCENARIOS[i].desc);
    }
    Serial.println("  d  toggle fault codes (P0133, P0420 + MIL)");
    Serial.println("  e  toggle ECU on / off (off = NO DATA, CYD shows WAITING FOR ECU)");
    Serial.println("  x  drop the Bluetooth link (CYD reconnects and asks 0100/0120 again)");
    Serial.println("  s  print status");
    Serial.println("  h  this help (also '?')");
    Serial.println("  BOOT button = next scenario (MANUAL is skipped)");
    Serial.println("  Web page: see [NET] line for the address");
    Serial.println();
}

void printStatus()
{
    Serial.printf("[SIM] %-11s link=%s faults=%d ecu=%s | rpm=%.0f gear=%d speed=%.0f cool=%.1f thr=%.0f load=%.0f iat=%.0f fuel=%.1f%s volt=%.1f\n",
                  SCENARIOS[static_cast<int>(scenario)].name, SerialBT.hasClient() ? "CONNECTED" : "waiting", faultCount, ecuOn ? "on" : "OFF",
                  car.rpm, car.gear, car.speed, car.coolant, car.throttle, car.load, car.intake, car.fuel, fuelPid ? "" : "(N/A)", car.volt);
}

void handleSerialCommand(char c)
{
    c = static_cast<char>(tolower(c));
    for (int i = 0; i < SCENARIO_COUNT; i++) {
        if (SCENARIOS[i].key == c) {
            setScenario(static_cast<Scenario>(i));
            return;
        }
    }
    switch (c) {
        case 'd':
            if (faultCount > 0) setFaults(nullptr, 0);
            else setFaults(DEFAULT_FAULTS, sizeof(DEFAULT_FAULTS) / sizeof(DEFAULT_FAULTS[0]));
            logFaults();
            break;
        case 'e':
            ecuOn = !ecuOn;
            webLog("[SIM] ECU %s", ecuOn ? "on" : "OFF (NO DATA)");
            break;
        case 'x': dropLink("Serial command"); break;
        case 's': printStatus(); break;
        case 'h':
        case '?': printHelp(); break;
        case '\r':
        case '\n':
        case ' ': break;
        default: Serial.printf("[SIM] unknown command '%c' (h = help)\n", c); break;
    }
}

void pollButton(uint32_t now)
{
    static bool lastRaw = true;
    static bool stable = true;
    static uint32_t changedAt = 0;
    const bool raw = digitalRead(cfg::BOOT_PIN) == HIGH;  // HIGH = ไม่ได้กด
    if (raw != lastRaw) {
        lastRaw = raw;
        changedAt = now;
    }
    if (raw != stable && now - changedAt >= cfg::BUTTON_DEBOUNCE_MS) {
        stable = raw;
        if (!stable) {  // วนสถานการณ์อัตโนมัติ (ข้าม MANUAL ซึ่งต้องตั้งค่าจากหน้าเว็บ)
            int next = (static_cast<int>(scenario) + 1) % SCENARIO_COUNT;
            if (next == static_cast<int>(Scenario::Manual)) next = 0;
            setScenario(static_cast<Scenario>(next));
        }
    }
}

// =====================================================================
//  เครือข่าย: ต่อ WiFi บ้าน (secrets.h) ถ้าไม่ได้ภายใน 12 วินาที เปิด AP "OBD-SIM" ของตัวเอง
// =====================================================================
bool apMode = false;

void startNetwork()
{
    WiFi.persistent(false);
    if (strlen(WIFI_SSID) > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, strlen(WIFI_PASSWORD) > 0 ? WIFI_PASSWORD : nullptr);
        Serial.print("[NET] connecting to WiFi");
        const uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < cfg::WIFI_CONNECT_TIMEOUT_MS) {
            delay(250);
            Serial.print('.');
        }
        Serial.println();
    }
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.setAutoReconnect(true);
        webLog("[NET] WiFi connected -> http://%s  (http://%s.local)", WiFi.localIP().toString().c_str(), cfg::MDNS_NAME);
    } else {
        apMode = true;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(cfg::AP_SSID, cfg::AP_PASSWORD);
        webLog("[NET] AP \"%s\" password \"%s\" -> http://%s", cfg::AP_SSID, cfg::AP_PASSWORD, WiFi.softAPIP().toString().c_str());
    }
    if (MDNS.begin(cfg::MDNS_NAME)) MDNS.addService("http", "tcp", 80);
}

// =====================================================================
//  เว็บ API (ทำงานใน loop() เดียวกับรถจำลอง จึงไม่ต้องล็อกข้อมูล)
//    GET  /                หน้าควบคุม
//    GET  /api/state       ค่าทั้งหมด
//    GET  /api/log?since=N log ตั้งแต่บรรทัดที่ N
//    POST /api/scenario    id=city
//    POST /api/manual      rpm= speed= cool= thr= load= iat= fuel= volt= (ส่งเฉพาะที่เปลี่ยน)
//    POST /api/faults      codes=P0133,P0420 (ว่าง = ไม่มีรหัส)
//    POST /api/adapter     ecu=0|1 fuel=0|1 latmin= latmax=
//    POST /api/drop        ตัดลิงก์ Bluetooth
//  POST ต้องมี header "X-SIM: 1" (กันหน้าเว็บอื่นสั่งงานข้ามโดเมน)
// =====================================================================
void jsonEscape(const char *in, String &out)
{
    for (; *in != '\0'; in++) {
        const char c = *in;
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            out += ' ';
        } else {
            out += c;
        }
    }
}

bool postAllowed()
{
    if (server.header("X-SIM") == "1") return true;
    server.send(403, "application/json", "{\"error\":\"missing X-SIM header\"}");
    return false;
}

void sendOk()
{
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleIndex()
{
    server.send_P(200, "text/html; charset=utf-8", SIM_WEB_PAGE);
}

void handleState()
{
    char buf[1024];
    {
    StateLock lock;
    String codes;
    for (int i = 0; i < faultCount; i++) {
        char c[8];
        formatDtc(faults[i], c, sizeof(c));
        if (i > 0) codes += ',';
        codes += '"';
        codes += c;
        codes += '"';
    }
    const bool connected = SerialBT.hasClient();
    snprintf(buf, sizeof(buf),
             "{\"scenario\":\"%s\",\"bt\":{\"ready\":%s,\"connected\":%s,\"peer\":\"%s\",\"name\":\"%s\",\"mac\":\"%s\"},"
             "\"net\":{\"ap\":%s,\"ip\":\"%s\",\"ssid\":\"%s\"},\"uptime\":%lu,\"cmds\":%lu,"
             "\"car\":{\"rpm\":%.0f,\"speed\":%.0f,\"cool\":%.1f,\"thr\":%.0f,\"load\":%.0f,\"iat\":%.1f,\"fuel\":%.1f,\"volt\":%.2f,\"gear\":%d},"
             "\"manual\":{\"rpm\":%.0f,\"speed\":%.0f,\"cool\":%.0f,\"thr\":%.0f,\"load\":%.0f,\"iat\":%.0f,\"fuel\":%.0f,\"volt\":%.1f},"
             "\"faults\":[%s],\"ecu\":%s,\"fuelPid\":%s,\"latMin\":%lu,\"latMax\":%lu}",
             SCENARIOS[static_cast<int>(scenario)].id, btReady ? "true" : "false", connected ? "true" : "false", connected ? peerMac : "",
             cfg::BT_NAME, SerialBT.getBtAddressString().c_str(), apMode ? "true" : "false",
             (apMode ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str(), apMode ? cfg::AP_SSID : WIFI_SSID,
             static_cast<unsigned long>(millis() / 1000), static_cast<unsigned long>(cmdCount), car.rpm, car.speed, car.coolant,
             car.throttle, car.load, car.intake, car.fuel, car.volt, car.gear, manualCar.rpm, manualCar.speed, manualCar.coolant,
             manualCar.throttle, manualCar.load, manualCar.intake, manualCar.fuel, manualCar.volt, codes.c_str(), ecuOn ? "true" : "false",
             fuelPid ? "true" : "false", static_cast<unsigned long>(replyMinMs), static_cast<unsigned long>(replyMaxMs));
    }
    server.send(200, "application/json", buf);
}

// การวนถามค่าของ CYD (01xx ยกเว้น 0100/0120/0101 และ ATRV) มีหลายบรรทัดต่อวินาที: quiet=1 ไม่ส่งบรรทัดเหล่านี้
bool isPollLine(const char *line)
{
    const char *m = strstr(line, "[RX] ");
    if (m == nullptr) return false;
    m += 5;
    if (strncmp(m, "ATRV", 4) == 0) return true;
    return m[0] == '0' && m[1] == '1' && !(m[2] == '0' && (m[3] == '0' || m[3] == '1')) && !(m[2] == '2' && m[3] == '0');
}

void handleLog()
{
    const bool quiet = server.arg("quiet") == "1";
    String out;
    {
        StateLock lock;
        uint32_t since = server.hasArg("since") ? strtoul(server.arg("since").c_str(), nullptr, 10) : 0;
        if (since > logSeq) since = 0;  // ตัวจำลองรีบูต: เริ่มนับใหม่
        if (logSeq - since > cfg::LOG_LINES) since = logSeq - cfg::LOG_LINES;
        // ส่งไม่เกิน LOG_MAX_PER_REQUEST บรรทัด: "next" บอกว่าอ่านถึงไหนแล้ว ครั้งถัดไปค่อยอ่านต่อ
        const uint32_t end = min(logSeq, since + static_cast<uint32_t>(cfg::LOG_MAX_PER_REQUEST) * (quiet ? 4 : 1));
        String lines;
        lines.reserve(cfg::LOG_MAX_PER_REQUEST * 80);
        int sent = 0;
        uint32_t i = since;
        for (; i < end && sent < cfg::LOG_MAX_PER_REQUEST; i++) {
            const char *line = logLines[i % cfg::LOG_LINES];
            if (quiet && isPollLine(line)) continue;
            if (sent++ > 0) lines += ',';
            lines += '"';
            jsonEscape(line, lines);
            lines += '"';
        }
        out.reserve(lines.length() + 40);
        out += "{\"next\":";
        out += i;  // อ่านถึงบรรทัดนี้ (ครั้งถัดไปเริ่มจากตรงนี้)
        out += ",\"lines\":[";
        out += lines;
        out += "]}";
    }
    server.send(200, "application/json", out);
}

void handleScenario()
{
    if (!postAllowed()) return;
    StateLock lock;
    const String id = server.arg("id");
    for (int i = 0; i < SCENARIO_COUNT; i++) {
        if (id == SCENARIOS[i].id) {
            if (static_cast<int>(scenario) != i) setScenario(static_cast<Scenario>(i));
            sendOk();
            return;
        }
    }
    server.send(400, "application/json", "{\"error\":\"unknown scenario\"}");
}

void handleManual()
{
    if (!postAllowed()) return;
    StateLock lock;
    struct Field {
        const char *key;
        float *target;
        float lo, hi;
    };
    const Field fields[] = {
        {"rpm", &manualCar.rpm, 0, 8000},   {"speed", &manualCar.speed, 0, 255}, {"cool", &manualCar.coolant, -40, 215},
        {"thr", &manualCar.throttle, 0, 100}, {"load", &manualCar.load, 0, 100}, {"iat", &manualCar.intake, -40, 215},
        {"fuel", &manualCar.fuel, 0, 100},  {"volt", &manualCar.volt, 9, 16},
    };
    if (scenario != Scenario::Manual) setScenario(Scenario::Manual);  // ก่อน: setScenario คัดลอกค่าปัจจุบันเป็นค่าเริ่มต้น
    for (const Field &f : fields) {
        if (server.hasArg(f.key)) *f.target = constrain(server.arg(f.key).toFloat(), f.lo, f.hi);
    }
    sendOk();
}

void handleFaults()
{
    if (!postAllowed()) return;
    StateLock lock;
    uint16_t codes[cfg::MAX_FAULTS];
    int count = 0;
    String list = server.arg("codes");
    while (list.length() > 0) {
        const int comma = list.indexOf(',');
        const String item = comma < 0 ? list : list.substring(0, comma);
        list = comma < 0 ? "" : list.substring(comma + 1);
        if (item.length() == 0) continue;
        uint16_t code;
        if (!parseDtc(item, code)) {
            server.send(400, "application/json", "{\"error\":\"bad code: use P0133 / C0035 / B1000 / U0100\"}");
            return;
        }
        bool dup = false;
        for (int i = 0; i < count; i++) dup |= codes[i] == code;
        if (dup) continue;
        if (count >= cfg::MAX_FAULTS) {
            server.send(400, "application/json", "{\"error\":\"max 8 codes\"}");
            return;
        }
        codes[count++] = code;
    }
    setFaults(codes, count);
    logFaults();
    sendOk();
}

void handleAdapter()
{
    if (!postAllowed()) return;
    StateLock lock;
    if (server.hasArg("ecu")) {
        const bool on = server.arg("ecu") == "1";
        if (on != ecuOn) {
            ecuOn = on;
            webLog("[SIM] ECU %s", ecuOn ? "on" : "OFF (NO DATA)");
        }
    }
    if (server.hasArg("fuel")) setFuelPid(server.arg("fuel") == "1");
    if (server.hasArg("latmin") || server.hasArg("latmax")) {
        const long limit = cfg::REPLY_DELAY_LIMIT_MS;
        if (server.hasArg("latmin")) replyMinMs = static_cast<uint32_t>(constrain(server.arg("latmin").toInt(), 0L, limit));
        if (server.hasArg("latmax")) replyMaxMs = static_cast<uint32_t>(constrain(server.arg("latmax").toInt(), 0L, limit));
        if (replyMaxMs < replyMinMs) replyMaxMs = replyMinMs;
        webLog("[SIM] reply delay %lu-%lu ms", static_cast<unsigned long>(replyMinMs), static_cast<unsigned long>(replyMaxMs));
    }
    sendOk();
}

void handleDrop()
{
    if (!postAllowed()) return;
    StateLock lock;
    if (!SerialBT.hasClient()) {
        server.send(409, "application/json", "{\"error\":\"no Bluetooth client connected\"}");
        return;
    }
    dropLink("web page");
    sendOk();
}

void startWeb()
{
    static const char *HEADERS[] = {"X-SIM"};
    server.collectHeaders(HEADERS, 1);
    server.on("/", HTTP_GET, handleIndex);
    server.on("/api/state", HTTP_GET, handleState);
    server.on("/api/log", HTTP_GET, handleLog);
    server.on("/api/scenario", HTTP_POST, handleScenario);
    server.on("/api/manual", HTTP_POST, handleManual);
    server.on("/api/faults", HTTP_POST, handleFaults);
    server.on("/api/adapter", HTTP_POST, handleAdapter);
    server.on("/api/drop", HTTP_POST, handleDrop);
    server.onNotFound([] { server.send(404, "text/plain", "not found"); });
    server.begin();
}

// =====================================================================
//  setup / loop
// =====================================================================
void setup()
{
    Serial.begin(cfg::SERIAL_BAUD);
    delay(300);
    stateMutex = xSemaphoreCreateRecursiveMutex();
    pinMode(cfg::BOOT_PIN, INPUT_PULLUP);
    pinMode(cfg::LED_PIN, OUTPUT);
    digitalWrite(cfg::LED_PIN, HIGH);  // ทดสอบ LED: ติดระหว่างเริ่ม Bluetooth
    randomSeed(esp_random());

    Serial.println();
    Serial.println("OBD2 ESP32 CYD -- ELM327 / vehicle simulator");
    SerialBT.setPin(cfg::BT_PIN);  // legacy PIN (CYD จับคู่ด้วย SerialBT.setPin เหมือนกัน)
    // ESP32 กับ ESP32 อาจจับคู่แบบ SSP แทน PIN (ไลบรารียืนยันให้อัตโนมัติ) ทั้งสองแบบ log ผลไว้ที่นี่
    SerialBT.onAuthComplete([](boolean ok) {
        authOk = ok;
        authEvt = true;
    });
    SerialBT.register_callback(sppCallback);
    btReady = SerialBT.begin(cfg::BT_NAME, false);
    if (!btReady) {
        webLog("[BT] FAILED to start Bluetooth Classic (LED blinks fast)");
    } else {
        webLog("[BT] slave \"%s\" PIN %s, MAC %s", cfg::BT_NAME, cfg::BT_PIN, SerialBT.getBtAddressString().c_str());
    }
    manualCar = car;
    xTaskCreatePinnedToCore(elmTask, "elm", 6144, nullptr, cfg::ELM_TASK_PRIORITY, nullptr, 1);
    startNetwork();
    startWeb();
    scenarioStart = millis();
    printHelp();
    printStatus();
}

void loop()
{
    const uint32_t now = millis();

    logBtEvents(now);
    ledUpdate(now);
    server.handleClient();  // handler ถือ lock เฉพาะตอนอ่าน/เขียนข้อมูล ไม่ถือระหว่างส่งผ่าน WiFi

    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        StateLock lock;
        handleSerialCommand(c);
    }
    {
        StateLock lock;
        pollButton(now);
    }

    static uint32_t simAt = 0;
    if (now - simAt >= cfg::SIM_STEP_MS) {
        const float dt = simAt == 0 ? cfg::SIM_STEP_MS / 1000.0f : (now - simAt) / 1000.0f;
        simAt = now;
        StateLock lock;
        simUpdate(dt);
    }

    static uint32_t statusAt = 0;
    if (now - statusAt >= cfg::STATUS_PRINT_MS) {
        statusAt = now;
        StateLock lock;
        printStatus();
    }
    delay(1);
}
