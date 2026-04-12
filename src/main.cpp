/*
 * ESP32 Hydroponic Controller - Phase 1 (Local Only)
 * Pin Mapping:
 * - Relay Pump (IN1): GPIO18
 * - Relay A (IN2): GPIO19
 * - Relay B (IN3): GPIO21
 * - Relay Down-pH (IN4): GPIO22
 * - LED Status: GPIO2
 * - DS18B20: GPIO4 (10k pull-up to 3.3V)
 * - TDS: GPIO32 (ADC1)
 * - pH: GPIO33 (ADC1) - with 100Ω series + 0.1µF to GND
 * - AC Current: GPIO34 (ADC1)
 */

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
//#include <BH1750.h>  // Tạm comment nếu không có cảm biến
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <FS.h>
#include <time.h>
#include <esp_task_wdt.h>  // Hardware Watchdog
#include <esp_system.h>

// ===== Pin Definitions =====
constexpr uint8_t PIN_RELAY_PUMP  = 18;  // Tuần hoàn
constexpr uint8_t PIN_RELAY_A     = 19;  // Bơm A
constexpr uint8_t PIN_RELAY_B     = 21;  // Bơm B
constexpr uint8_t PIN_RELAY_DOWNP = 22;  // Down-pH
constexpr uint8_t PIN_LED         = 2;

constexpr uint8_t PIN_TDS   = 32; // ADC1
constexpr uint8_t PIN_PH    = 33; // ADC1
constexpr uint8_t PIN_ZMCT  = 34; // ADC1
constexpr uint8_t PIN_DSBUS = 4;  // DS18B20 + 10k pull-up to 3.3V

// ===== WiFi Credentials =====
const char* WIFI_SSID = "haiquynh";
const char* WIFI_PASS = "12345687";

// ===== Sensor Objects =====
OneWire oneWire(PIN_DSBUS);
DallasTemperature ds(&oneWire);
//BH1750 lightMeter;  // Tạm comment nếu không có cảm biến
WebServer server(80);
Preferences prefs;

// ===== Configuration Structure =====
struct LoopCfg { 
  uint16_t on_min_day = 15;   // Bật (phút) - ban ngày
  uint16_t off_min_day = 45;  // Tắt (phút) - ban ngày
  uint16_t on_min_night = 10; // Bật (phút) - ban đêm
  uint16_t off_min_night = 50; // Tắt (phút) - ban đêm
  uint8_t light_start = 6;    // Giờ bắt đầu ban ngày (6h sáng)
  uint8_t light_end = 18;     // Giờ kết thúc ban ngày (6h tối)
} loopCfg;

struct TDSCfg { 
  uint16_t target = 800; 
  uint16_t hyst = 50; 
  uint16_t dose_ms = 700; 
  uint16_t lock_s = 90; 
  uint32_t max_ms_per_day = 60000; 
} tdsCfg;

struct PHCfg { 
  float target = 6.0f; 
  float hyst = 0.3f; 
  uint16_t dose_ms = 300; 
  uint16_t lock_s = 90; 
  uint32_t max_ms_per_day = 30000; 
} phCfg;

struct Calib { 
  float ph7 = 1.65f; 
  float ph4 = 2.10f; 
  float tds_k = 0.5f; 
  float zmctOffset = 1.65f;      // Điện áp offset ZMCT (V)
  float zmctSensitivity = 10.0f; // Hệ số A/V
} cal;

// ===== State Variables =====
unsigned long tLastLoop = 0;
bool loopOn = false;
unsigned long tLastDoseA = 0, tLastDoseB = 0, tLastDoseP = 0;
uint32_t doseA_today = 0, doseB_today = 0, doseP_today = 0;
unsigned long lastResetTime = 0;

float tempC = 0.0f;
float phVal = 0.0f;
float tdsVal = 0.0f;
float luxVal = 0.0f;
float acCurrent = 0.0f;
float acPower = 0.0f;      // Công suất (W)
float energyKwh = 0.0f;     // Tổng điện năng tiêu thụ (kWh)

int adcPH = 0, adcTDS = 0, adcZMCT = 0;

// Cấu hình ZMCT103C
constexpr float AC_VOLTAGE = 220.0f;  // Điện áp AC (V)
unsigned long lastEnergyUpdate = 0;

// Non-blocking dosing state
struct DosingState {
  uint8_t pin;
  bool active;
  unsigned long startTime;
  uint32_t *accum;
  uint32_t limit;
  uint16_t doseMs;  // Dose duration in ms
};
DosingState activeDosing = {0, false, 0, nullptr, 0, 0};

bool wifiConnected = false;
bool ledState = false;
unsigned long lastLedBlink = 0;
uint8_t ledPattern = 0; // 0: boot, 1: OK, 2: wifi error, 3: sensor error, 4: pumping, 5: wifi reconnecting
bool fsReady = false;    // LittleFS mount status

// Runtime diagnostics
constexpr uint8_t SENSOR_BAD_LATCH_COUNT = 3;
constexpr uint8_t SENSOR_GOOD_CLEAR_COUNT = 5;
constexpr size_t DIAG_EVENT_CAPACITY = 64;
constexpr size_t DIAG_EVENT_LEN = 96;

struct DiagCounters {
  uint32_t bootCount = 0;
  uint32_t powerOnCount = 0;
  uint32_t brownoutCount = 0;
  uint32_t wdtCount = 0;
  uint32_t sensorFaultCount = 0;
} diagCounters;

esp_reset_reason_t lastResetReason = ESP_RST_UNKNOWN;
const char* lastResetReasonText = "UNKNOWN";
bool sensorFaultLatched = false;
bool loopCommandedOn = false;
bool pumpRelayCommandOn = false;
uint8_t sensorBadStreak = 0;
uint8_t sensorGoodStreak = 0;
char lastSensorFaultReason[DIAG_EVENT_LEN] = "none";
char lastDiagEvent[DIAG_EVENT_LEN] = "";
char diagEvents[DIAG_EVENT_CAPACITY][DIAG_EVENT_LEN] = {{0}};
size_t diagEventWriteIndex = 0;
size_t diagEventTotal = 0;

// WiFi reconnect
unsigned long lastWiFiCheck = 0;
constexpr unsigned long WIFI_CHECK_INTERVAL = 30000; // Check mỗi 30 giây
constexpr unsigned long WIFI_RECONNECT_TIMEOUT = 15000; // Timeout reconnect 15s

// Watchdog
constexpr uint32_t WDT_TIMEOUT = 120; // 120 giây (đủ thời gian cho cả chu kỳ bơm dài nhất)

bool manualControl = false;
bool manualPump = false, manualA = false, manualB = false, manualDownpH = false;

// Auto control enable/disable (quan trọng khi hỏng cảm biến hoặc hết dung dịch)
bool autoLoopEnabled = true;   // Bơm tuần hoàn tự động ON/OFF
bool autoTDSEnabled = true;    // Châm TDS tự động ON/OFF
bool autoPHEnabled = true;     // Châm pH tự động ON/OFF

// Hysteresis state machines
bool tdsDosing = false;  // Đang trong chu kỳ châm TDS
bool phDosing = false;   // Đang trong chu kỳ châm pH

// ===== Function Prototypes =====
int readADCMedian(uint8_t pin);
float adcToVoltage(int raw);
float voltageToPH(float v);
float voltageToTDS(float v);
void setRelay(uint8_t pin, bool on);
bool startDosePump(uint8_t pin, uint16_t ms, uint32_t &accum, uint32_t limit, unsigned long &lastDoseTime);
void checkDosePump();
void ledPatternControl();
void checkWiFiAndReconnect();
void loadConfig();
void saveConfig();
void handleRoot();
void handleDashboard();
void handleConfig();
void handleCalibration();
void handleManualControl();
void handleSensorData();
void handleDiagData();
void sendJSON(const String& json);
String readFile(String path);
void handleStaticFile(String path);
void writeLog(String msg);
String generateConfigPage();
String generateCalibrationPage();
String generateManualPage();
void commandPumpRelay(bool on);
const char* resetReasonToString(esp_reset_reason_t reason);
void loadDiagCounters();
void recordBootDiagnostics();
void updateSensorHealth();
String jsonEscape(const char* text);
void validateLoadedConfig();
String jsonFloatOrNull(float value, int decimals);

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Booting pattern: nháy nhanh 5 lần
  pinMode(PIN_LED, OUTPUT);
  for(int i = 0; i < 5; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(100);
    digitalWrite(PIN_LED, LOW);
    delay(100);
  }
  
  // Relay pins (Low-trigger)
  pinMode(PIN_RELAY_PUMP, OUTPUT);
  pinMode(PIN_RELAY_A, OUTPUT);
  pinMode(PIN_RELAY_B, OUTPUT);
  pinMode(PIN_RELAY_DOWNP, OUTPUT);
  
  // Set all relays OFF (HIGH for low-trigger)
  commandPumpRelay(false);
  setRelay(PIN_RELAY_A, false);
  setRelay(PIN_RELAY_B, false);
  setRelay(PIN_RELAY_DOWNP, false);
  
  // ADC 12-bit
  analogReadResolution(12);
  
  // Initialize DS18B20
  ds.begin();
  
  // Initialize I2C for BH1750 (optional)
  //Wire.begin(25, 26);
  //if (!lightMeter.begin()) {
  //  Serial.println("BH1750 not found!");
  //}
  
  // Initialize preferences
  prefs.begin("farm365", false);
  loadConfig();
  loadDiagCounters();
  recordBootDiagnostics();
  
  // ===== CRITICAL: Initialize Hardware Watchdog =====
  // Watchdog sẽ reset ESP32 nếu bị treo > 120s
  Serial.println("Initializing Hardware Watchdog (120s timeout)...");
  esp_task_wdt_init(WDT_TIMEOUT, true);  // true = enable panic so ESP32 can reboot
  esp_task_wdt_add(NULL);  // Add current task to WDT
  Serial.println("Watchdog enabled - system will auto-reset if frozen");
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // Chúng ta tự quản lý reconnect
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  unsigned long wifiTimeout = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimeout < 10000)) {
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset(); // Feed watchdog trong khi đợi WiFi
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    ledPattern = 1; // OK
    
    // Configure NTP
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // UTC+7 (Vietnam)
    Serial.println("Waiting for NTP time sync...");
    delay(1000);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) { // Wait up to 10 seconds
      Serial.println("Time synchronized!");
      Serial.print("Current time: ");
      Serial.print(asctime(&timeinfo));
    } else {
      Serial.println("Failed to get NTP time (will retry in loop)");
    }
  } else {
    wifiConnected = false;
    Serial.println("WiFi connection failed!");
    Serial.println("⚠️ CRITICAL: Pump will continue working WITHOUT WiFi!");
    ledPattern = 2; // WiFi error
  }
  
  // Initialize LittleFS
  fsReady = LittleFS.begin();
  if (fsReady) {
    Serial.println("LittleFS mounted successfully");
    writeLog("System started");
  } else {
    Serial.println("LittleFS mount failed");
  }
  
  // Setup Web Server
  server.on("/", handleRoot);
  server.on("/dashboard", handleDashboard);
  server.on("/config", handleConfig);
  server.on("/calibration", handleCalibration);
  server.on("/manual", handleManualControl);
  server.on("/api/sensor", handleSensorData);
  server.on("/api/diag", handleDiagData);
  server.on("/api/config", []() {
    String json = "{";
    json += "\"loopOnDay\":" + String(loopCfg.on_min_day) + ",";
    json += "\"loopOffDay\":" + String(loopCfg.off_min_day) + ",";
    json += "\"loopOnNight\":" + String(loopCfg.on_min_night) + ",";
    json += "\"loopOffNight\":" + String(loopCfg.off_min_night) + ",";
    json += "\"lightStart\":" + String(loopCfg.light_start) + ",";
    json += "\"lightEnd\":" + String(loopCfg.light_end) + ",";
    json += "\"tdsTarget\":" + String(tdsCfg.target) + ",";
    json += "\"tdsHyst\":" + String(tdsCfg.hyst) + ",";
    json += "\"tdsDose\":" + String(tdsCfg.dose_ms) + ",";
    json += "\"tdsLock\":" + String(tdsCfg.lock_s) + ",";
    json += "\"phTarget\":" + String(phCfg.target, 2) + ",";
    json += "\"phHyst\":" + String(phCfg.hyst, 2) + ",";
    json += "\"phDose\":" + String(phCfg.dose_ms) + ",";
    json += "\"phLock\":" + String(phCfg.lock_s) + ",";
    json += "\"autoLoop\":" + String(autoLoopEnabled ? 1 : 0) + ",";
    json += "\"autoTDS\":" + String(autoTDSEnabled ? 1 : 0) + ",";
    json += "\"autoPH\":" + String(autoPHEnabled ? 1 : 0);
    json += "}";
    sendJSON(json);
  });
  server.on("/api/auto-control", HTTP_POST, []() {
    // Toggle auto control settings
    if (server.hasArg("autoLoop")) {
      autoLoopEnabled = server.arg("autoLoop").toInt() == 1;
    }
    if (server.hasArg("autoTDS")) {
      autoTDSEnabled = server.arg("autoTDS").toInt() == 1;
    }
    if (server.hasArg("autoPH")) {
      autoPHEnabled = server.arg("autoPH").toInt() == 1;
    }
    saveConfig();
    sendJSON("{\"status\":\"ok\"}");
  });
  server.onNotFound([]() {
    handleStaticFile(server.uri());
  });
  
  server.begin();
  Serial.println("Web server started");
  
  // Set LED pattern to OK
  if (ledPattern != 2) {
    ledPattern = 1;
  }
  lastLedBlink = millis();
  
  lastResetTime = millis();
}

// ===== Main Loop =====
void loop() {
  static unsigned long tSense = 0;
  static unsigned long tLockA = 0, tLockB = 0, tLockP = 0;
  bool sensorOk = !sensorFaultLatched;
  
  // ===== CRITICAL: Feed Watchdog =====
  // Reset watchdog mỗi vòng loop để tránh ESP32 tự reset
  esp_task_wdt_reset();
  
  // ===== CRITICAL: Check WiFi và Reconnect =====
  // Kiểm tra WiFi mỗi 30s và reconnect nếu mất kết nối
  checkWiFiAndReconnect();
  
  // Check for new day (reset daily counters)
  static unsigned long lastDayCheck = 0;
  if (millis() - lastDayCheck > 3600000) { // Check hourly
    lastDayCheck = millis();
    unsigned long uptimeDays = millis() / 86400000UL;
    if (uptimeDays != lastResetTime / 86400000UL) {
      doseA_today = 0;
      doseB_today = 0;
      doseP_today = 0;
      lastResetTime = millis();
      writeLog("Daily reset - counters cleared");
    }
  }
  
  // Read sensors every 500ms
  if (millis() - tSense > 500) {
    tSense = millis();
    
    // Read TDS and pH with median filter
    adcPH  = readADCMedian(PIN_PH);
    adcTDS = readADCMedian(PIN_TDS);
    
    float vPH  = adcToVoltage(adcPH);
    float vTDS = adcToVoltage(adcTDS);
    
    phVal  = voltageToPH(vPH);
    tdsVal = voltageToTDS(vTDS);
    
    // Read temperature
    ds.requestTemperatures();
    tempC = ds.getTempCByIndex(0);
    
    // Read AC Current from ZMCT103C (True RMS)
    // Sample multiple points over AC cycle for true RMS
    const int samples = 100;
    float sumSq = 0.0f;
    for (int i = 0; i < samples; i++) {
      int raw = analogRead(PIN_ZMCT);
      float v = adcToVoltage(raw);
      float vDiff = v - cal.zmctOffset;
      sumSq += vDiff * vDiff;
      delayMicroseconds(400); // ~2.5kHz sampling for 50Hz AC (100 samples per cycle)
    }
    float vRMS = sqrt(sumSq / samples);
    acCurrent = vRMS * cal.zmctSensitivity;  // Convert to Ampere
    
    // Calculate power (P = V × I)
    acPower = AC_VOLTAGE * acCurrent;
    
    // Accumulate energy (kWh) - tích lũy mỗi 500ms
    unsigned long now = millis();
    if (lastEnergyUpdate > 0) {
      float deltaTime = (now - lastEnergyUpdate) / 1000.0f / 3600.0f; // hours
      energyKwh += (acPower / 1000.0f) * deltaTime;  // kWh
    }
    lastEnergyUpdate = now;
    
    // Check for sensor errors with debounce/latch to avoid false alarms
    updateSensorHealth();
    sensorOk = !sensorFaultLatched;
    
    // Read light (optional - commented if no BH1750)
    //luxVal = lightMeter.readLightLevel();
  }
  
  // Control loop pump (day/night schedule)
  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo, 0); // Non-blocking
  bool isDaytime = false;
  if (timeValid && timeinfo.tm_year >= 70) { // Valid time (year >= 1970)
    int hour = timeinfo.tm_hour;
    isDaytime = (hour >= loopCfg.light_start && hour < loopCfg.light_end);
  } else {
    // Fallback: use millis() based 24h cycle if NTP unavailable
    unsigned long hours = (millis() / 3600000UL) % 24;
    isDaytime = (hours >= loopCfg.light_start && hours < loopCfg.light_end);
  }
  
  const unsigned long onMs  = isDaytime ? 
                               (unsigned long)loopCfg.on_min_day * 60000UL : 
                               (unsigned long)loopCfg.on_min_night * 60000UL;
  const unsigned long offMs = isDaytime ? 
                               (unsigned long)loopCfg.off_min_day * 60000UL : 
                               (unsigned long)loopCfg.off_min_night * 60000UL;
  
  if (!manualControl && autoLoopEnabled) {
    // Auto mode: circulation pump với day/night schedule
    if (loopOn) {
      loopCommandedOn = true;
      if (millis() - tLastLoop > onMs) {
        loopOn = false;
        loopCommandedOn = false;
        tLastLoop = millis();
        commandPumpRelay(false);
        ledPattern = 1;
        writeLog("Circulation pump OFF");
      } else {
        commandPumpRelay(true);  // Re-assert relay every loop in AUTO mode
        ledPattern = 4; // Pumping
      }
    } else {
      loopCommandedOn = false;
      commandPumpRelay(false);
      if (millis() - tLastLoop > offMs) {
        loopOn = true;
        loopCommandedOn = true;
        tLastLoop = millis();
        commandPumpRelay(true);
        ledPattern = 4; // Pumping
        writeLog("Circulation pump ON");
      } else if (ledPattern == 4) {
        ledPattern = 1;
      }
    }
  } else if (manualControl) {
    // Manual control
    loopCommandedOn = manualPump;
    commandPumpRelay(manualPump);
    loopOn = manualPump; // Đồng bộ trạng thái bơm cho LED logic phía sau
    if (manualPump) {
      ledPattern = 4;
    } else if (ledPattern == 4) {
      ledPattern = 1;
    }
  } else if (!autoLoopEnabled) {
    // Auto control DISABLED → tắt bơm tuần hoàn
    loopCommandedOn = false;
    commandPumpRelay(false);
    loopOn = false;
  }
  
  // Check non-blocking dosing
  checkDosePump();
  
  // TDS Control (Pump A & B) - Hysteresis 2 ngưỡng
  if (manualControl) {
    // MANUAL mode - điều khiển trực tiếp
    setRelay(PIN_RELAY_A, manualA);
    setRelay(PIN_RELAY_B, manualB);
    tdsDosing = false;  // Reset state khi manual
  } else if (autoTDSEnabled && sensorOk) {
    // Quyết định BẬT/TẮT chu kỳ châm dựa trên hysteresis 2 ngưỡng
    if (tdsVal < (tdsCfg.target - tdsCfg.hyst)) {
      // TDS < (target - hyst) → BẬT chu kỳ châm
      if (!tdsDosing) {
        tdsDosing = true;
        writeLog("TDS too low - START dosing cycle");
      }
    } 
    else if (tdsVal > (tdsCfg.target + tdsCfg.hyst)) {
      // TDS > (target + hyst) → TẮT chu kỳ châm
      if (tdsDosing) {
        tdsDosing = false;
        writeLog("TDS sufficient - STOP dosing cycle");
      }
    }
    // Vùng (target - hyst) đến (target + hyst): GIỮ NGUYÊN trạng thái
    
    // Nếu đang trong chu kỳ châm → châm định kỳ mỗi lock_s giây
    if (tdsDosing && !activeDosing.active) {
      if (millis() - tLockA > (unsigned long)tdsCfg.lock_s * 1000UL) {
        if (startDosePump(PIN_RELAY_A, tdsCfg.dose_ms, doseA_today, tdsCfg.max_ms_per_day, tLastDoseA)) {
          tLockA = millis();
          ledPattern = 4;
          writeLog("Dosed Pump A");
        }
      }
      if (millis() - tLockB > (unsigned long)tdsCfg.lock_s * 1000UL) {
        // Alternate with Pump A - wait if A is dosing
        if (!activeDosing.active || activeDosing.pin != PIN_RELAY_A) {
          if (startDosePump(PIN_RELAY_B, tdsCfg.dose_ms, doseB_today, tdsCfg.max_ms_per_day, tLastDoseB)) {
            tLockB = millis();
            ledPattern = 4;
            writeLog("Dosed Pump B");
          }
        }
      }
    }
  } else {
    // AUTO bị tắt hoặc sensor lỗi → đảm bảo dừng bơm châm
    setRelay(PIN_RELAY_A, false);
    setRelay(PIN_RELAY_B, false);
    tdsDosing = false;
  }
  
  // pH Control (Down-pH) - Hysteresis 2 ngưỡng
  if (manualControl) {
    setRelay(PIN_RELAY_DOWNP, manualDownpH);
    phDosing = false;  // Reset state khi manual
  } else if (autoPHEnabled && sensorOk) {
    // Quyết định BẬT/TẮT chu kỳ châm dựa trên hysteresis 2 ngưỡng
    if (phVal > (phCfg.target + phCfg.hyst)) {
      // pH > (target + hyst) → BẬT chu kỳ châm
      if (!phDosing) {
        phDosing = true;
        writeLog("pH too high - START dosing cycle");
      }
    }
    else if (phVal < (phCfg.target - phCfg.hyst)) {
      // pH < (target - hyst) → TẮT chu kỳ châm
      if (phDosing) {
        phDosing = false;
        writeLog("pH sufficient - STOP dosing cycle");
      }
    }
    // Vùng (target - hyst) đến (target + hyst): GIỮ NGUYÊN trạng thái
    
    // Nếu đang trong chu kỳ châm → châm định kỳ mỗi lock_s giây
    if (phDosing && !activeDosing.active) {
      if (millis() - tLockP > (unsigned long)phCfg.lock_s * 1000UL) {
        if (startDosePump(PIN_RELAY_DOWNP, phCfg.dose_ms, doseP_today, phCfg.max_ms_per_day, tLastDoseP)) {
          tLockP = millis();
          ledPattern = 4;
          writeLog("Dosed Down-pH");
        }
      }
    }
  } else {
    // AUTO bị tắt hoặc sensor lỗi → tắt bơm pH
    setRelay(PIN_RELAY_DOWNP, false);
    phDosing = false;
  }
  
  // LED pattern cho dosing pumps
  if (!loopOn) {  // Chỉ set nếu bơm tuần hoàn không chạy
    unsigned long now = millis();
    if (activeDosing.active || 
        (tLastDoseA > 0 && now - tLastDoseA < (tdsCfg.dose_ms + 1000)) ||
        (tLastDoseB > 0 && now - tLastDoseB < (tdsCfg.dose_ms + 1000)) ||
        (tLastDoseP > 0 && now - tLastDoseP < (phCfg.dose_ms + 1000))) {
      ledPattern = 4;
    } else if (ledPattern == 4 && sensorOk) {
      // Không còn bơm nào chạy hoặc vừa dose → trở về trạng thái OK
      ledPattern = 1;
    }
  }

  // Ưu tiên báo lỗi sensor nếu còn lỗi
  if (!sensorOk) {
    ledPattern = 3;
  }
  
  // LED pattern control
  ledPatternControl();
  
  // Handle web server
  server.handleClient();
}

// ===== Helper Functions =====
int readADCMedian(uint8_t pin) {
  const int N = 15;
  int v[N];
  for(int i = 0; i < N; i++) {
    v[i] = analogRead(pin);
    delay(3);
  }
  // Bubble sort
  for(int i = 0; i < N - 1; i++) {
    for(int j = i + 1; j < N; j++) {
      if(v[j] < v[i]) {
        int temp = v[i];
        v[i] = v[j];
        v[j] = temp;
      }
    }
  }
  return v[N / 2];
}

float adcToVoltage(int raw) {
  return (3.3f * raw) / 4095.0f;
}

float voltageToPH(float v) {
  // Linear interpolation: (ph7 -> cal.ph7), (ph4 -> cal.ph4)
  float m = (7.0f - 4.0f) / (cal.ph7 - cal.ph4);
  float b = 7.0f - m * cal.ph7;
  return m * v + b;
}

float voltageToTDS(float v) {
  // TDS calculation: voltage -> EC -> TDS
  float ec_uS = v * cal.tds_k * 1000.0f;
  float tds_ppm = ec_uS * 0.5f;
  return tds_ppm;
}

void setRelay(uint8_t pin, bool on) {
  // Low-trigger relay: LOW = ON, HIGH = OFF
  digitalWrite(pin, on ? LOW : HIGH);
}

void commandPumpRelay(bool on) {
  pumpRelayCommandOn = on;
  setRelay(PIN_RELAY_PUMP, on);
}

// Non-blocking dose pump - returns true if started, false if limit reached
bool startDosePump(uint8_t pin, uint16_t ms, uint32_t &accum, uint32_t limit, unsigned long &lastDoseTime) {
  if (activeDosing.active) {
    return false; // Already dosing
  }
  if (accum + ms > limit) {
    writeLog("Daily limit reached");
    return false;
  }
  activeDosing.pin = pin;
  activeDosing.active = true;
  activeDosing.startTime = millis();
  activeDosing.accum = &accum;
  activeDosing.limit = limit;
  activeDosing.doseMs = ms; // Store dose duration
  setRelay(pin, true);
  lastDoseTime = millis();
  return true;
}

// Check and stop dosing if time elapsed
void checkDosePump() {
  if (!activeDosing.active) return;
  
  unsigned long elapsed = millis() - activeDosing.startTime;
  if (elapsed >= activeDosing.doseMs) {
    setRelay(activeDosing.pin, false);
    if (activeDosing.accum) {
      *activeDosing.accum += activeDosing.doseMs;
    }
    activeDosing.active = false;
  }
}

const char* resetReasonToString(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "POWERON_RESET";
    case ESP_RST_EXT:       return "EXTERNAL_RESET";
    case ESP_RST_SW:        return "SW_RESET";
    case ESP_RST_PANIC:     return "PANIC_RESET";
    case ESP_RST_INT_WDT:   return "INT_WDT_RESET";
    case ESP_RST_TASK_WDT:  return "TASK_WDT_RESET";
    case ESP_RST_WDT:       return "OTHER_WDT_RESET";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP_RESET";
    case ESP_RST_BROWNOUT:  return "BROWNOUT_RESET";
    case ESP_RST_SDIO:      return "SDIO_RESET";
    default:                return "UNKNOWN_RESET";
  }
}

void loadDiagCounters() {
  diagCounters.bootCount = prefs.getUInt("bootCnt", 0);
  diagCounters.powerOnCount = prefs.getUInt("rstPwr", 0);
  diagCounters.brownoutCount = prefs.getUInt("rstBrn", 0);
  diagCounters.wdtCount = prefs.getUInt("rstWdt", 0);
  diagCounters.sensorFaultCount = prefs.getUInt("snsFault", 0);
}

void recordBootDiagnostics() {
  lastResetReason = esp_reset_reason();
  lastResetReasonText = resetReasonToString(lastResetReason);

  diagCounters.bootCount++;
  prefs.putUInt("bootCnt", diagCounters.bootCount);

  if (lastResetReason == ESP_RST_POWERON) {
    diagCounters.powerOnCount++;
    prefs.putUInt("rstPwr", diagCounters.powerOnCount);
  } else if (lastResetReason == ESP_RST_BROWNOUT) {
    diagCounters.brownoutCount++;
    prefs.putUInt("rstBrn", diagCounters.brownoutCount);
  } else if (lastResetReason == ESP_RST_INT_WDT ||
             lastResetReason == ESP_RST_TASK_WDT ||
             lastResetReason == ESP_RST_WDT) {
    diagCounters.wdtCount++;
    prefs.putUInt("rstWdt", diagCounters.wdtCount);
  }

  Serial.printf("Reset reason: %s (%d)\n", lastResetReasonText, static_cast<int>(lastResetReason));
  writeLog(String("Boot #") + String(diagCounters.bootCount) +
           " resetReason=" + lastResetReasonText +
           " powerOn=" + String(diagCounters.powerOnCount) +
           " brownout=" + String(diagCounters.brownoutCount) +
           " wdt=" + String(diagCounters.wdtCount));
}

void updateSensorHealth() {
  const bool tempDisconnected = (tempC == -127.0f);
  const bool tempPowerOnValue = (tempC == 85.0f);
  const bool phInvalid = !isfinite(phVal);
  const bool tdsInvalid = !isfinite(tdsVal);
  const bool rawFault = tempDisconnected || tempPowerOnValue || phInvalid || tdsInvalid;

  if (rawFault) {
    if (sensorBadStreak < 255) sensorBadStreak++;
    sensorGoodStreak = 0;
  } else {
    sensorBadStreak = 0;
    if (sensorGoodStreak < 255) sensorGoodStreak++;
  }

  if (rawFault) {
    String reason;
    if (tempDisconnected) reason += (reason.length() ? "," : "") + String("temp=-127");
    if (tempPowerOnValue) reason += (reason.length() ? "," : "") + String("temp=85");
    if (phInvalid)        reason += (reason.length() ? "," : "") + String("ph=nan");
    if (tdsInvalid)       reason += (reason.length() ? "," : "") + String("tds=nan");
    if (reason.length() == 0) reason = "unknown";
    snprintf(lastSensorFaultReason, sizeof(lastSensorFaultReason), "%s", reason.c_str());

    if (!sensorFaultLatched && sensorBadStreak >= SENSOR_BAD_LATCH_COUNT) {
      sensorFaultLatched = true;
      diagCounters.sensorFaultCount++;
      prefs.putUInt("snsFault", diagCounters.sensorFaultCount);
      writeLog(String("Sensor fault latched: ") + lastSensorFaultReason +
               " temp=" + String(tempC, 1) +
               " ph=" + String(phVal, 2) +
               " tds=" + String(tdsVal, 0));
    }
  } else if (sensorFaultLatched && sensorGoodStreak >= SENSOR_GOOD_CLEAR_COUNT) {
    sensorFaultLatched = false;
    snprintf(lastSensorFaultReason, sizeof(lastSensorFaultReason), "%s", "none");
    writeLog("Sensor fault cleared after stable readings");
  }
}

String jsonEscape(const char* text) {
  String escaped;
  if (!text) return escaped;

  for (const char* p = text; *p; ++p) {
    switch (*p) {
      case '\\': escaped += "\\\\"; break;
      case '"':  escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:   escaped += *p; break;
    }
  }
  return escaped;
}

void validateLoadedConfig() {
  bool changed = false;

  auto clampUInt = [&](uint16_t &value, uint16_t def, uint16_t minV, uint16_t maxV) {
    if (value < minV || value > maxV) {
      value = def;
      changed = true;
    }
  };

  clampUInt(loopCfg.on_min_day, 15, 1, 180);
  clampUInt(loopCfg.off_min_day, 45, 1, 180);
  clampUInt(loopCfg.on_min_night, 10, 1, 180);
  clampUInt(loopCfg.off_min_night, 50, 1, 180);
  clampUInt(tdsCfg.target, 800, 50, 5000);
  clampUInt(tdsCfg.hyst, 50, 1, 1000);
  clampUInt(tdsCfg.dose_ms, 700, 50, 10000);
  clampUInt(tdsCfg.lock_s, 90, 1, 3600);
  clampUInt(phCfg.dose_ms, 300, 50, 10000);
  clampUInt(phCfg.lock_s, 90, 1, 3600);

  if (loopCfg.light_start > 23) { loopCfg.light_start = 6; changed = true; }
  if (loopCfg.light_end > 23) { loopCfg.light_end = 18; changed = true; }
  if (loopCfg.light_start == loopCfg.light_end) { loopCfg.light_start = 6; loopCfg.light_end = 18; changed = true; }

  if (!isfinite(phCfg.target) || phCfg.target < 3.0f || phCfg.target > 10.0f) {
    phCfg.target = 6.0f;
    changed = true;
  }
  if (!isfinite(phCfg.hyst) || phCfg.hyst <= 0.0f || phCfg.hyst > 2.0f) {
    phCfg.hyst = 0.3f;
    changed = true;
  }
  if (tdsCfg.max_ms_per_day < 100 || tdsCfg.max_ms_per_day > 600000UL) {
    tdsCfg.max_ms_per_day = 60000;
    changed = true;
  }
  if (phCfg.max_ms_per_day < 100 || phCfg.max_ms_per_day > 600000UL) {
    phCfg.max_ms_per_day = 30000;
    changed = true;
  }

  if (!isfinite(cal.ph7) || cal.ph7 < 0.0f || cal.ph7 > 3.3f) {
    cal.ph7 = 1.65f;
    changed = true;
  }
  if (!isfinite(cal.ph4) || cal.ph4 < 0.0f || cal.ph4 > 3.3f) {
    cal.ph4 = 2.10f;
    changed = true;
  }
  if (fabs(cal.ph7 - cal.ph4) < 0.05f) {
    cal.ph7 = 1.65f;
    cal.ph4 = 2.10f;
    changed = true;
  }
  if (!isfinite(cal.tds_k) || cal.tds_k <= 0.0f || cal.tds_k > 10.0f) {
    cal.tds_k = 0.5f;
    changed = true;
  }
  if (!isfinite(cal.zmctOffset) || cal.zmctOffset < 0.0f || cal.zmctOffset > 3.3f) {
    cal.zmctOffset = 1.65f;
    changed = true;
  }
  if (!isfinite(cal.zmctSensitivity) || cal.zmctSensitivity <= 0.0f || cal.zmctSensitivity > 1000.0f) {
    cal.zmctSensitivity = 10.0f;
    changed = true;
  }

  if (changed) {
    saveConfig();
    prefs.putFloat("calPH7", cal.ph7);
    prefs.putFloat("calPH4", cal.ph4);
    prefs.putFloat("calTDS", cal.tds_k);
    prefs.putFloat("zmctOffset", cal.zmctOffset);
    prefs.putFloat("zmctSens", cal.zmctSensitivity);
    Serial.println("Invalid config/calibration detected -> restored safe defaults");
  }
}

String jsonFloatOrNull(float value, int decimals) {
  if (!isfinite(value)) {
    return "null";
  }
  return String(value, decimals);
}

void ledPatternControl() {
  static unsigned long ledOnTime = 0;
  unsigned long now = millis();
  
  switch (ledPattern) {
    case 0: // Booting - already done
      digitalWrite(PIN_LED, LOW);
      break;
    case 1: // OK - blink 100ms every 2 seconds
      if (now - lastLedBlink >= 2000) {
        digitalWrite(PIN_LED, HIGH);
        ledOnTime = now;
        lastLedBlink = now;
      } else if (now - ledOnTime < 100) {
        digitalWrite(PIN_LED, HIGH);
      } else {
        digitalWrite(PIN_LED, LOW);
      }
      break;
    case 2: // WiFi error - blink every 100ms
      if (now - lastLedBlink >= 100) {
        digitalWrite(PIN_LED, ledState ? LOW : HIGH);
        ledState = !ledState;
        lastLedBlink = now;
      }
      break;
    case 3: // Sensor error - 3 quick blinks every 2s
      {
        static unsigned long errStart = 0;
        static int blinkCount = 0;
        if (blinkCount == 0) {
          errStart = now;
          blinkCount = 1;
        }
        unsigned long elapsed = (now - errStart) % 2000;
        if (elapsed < 150 || (elapsed >= 200 && elapsed < 350) || (elapsed >= 400 && elapsed < 550)) {
          digitalWrite(PIN_LED, HIGH);
        } else {
          digitalWrite(PIN_LED, LOW);
        }
        if (now - errStart >= 2000) {
          blinkCount = 0;
        }
      }
      break;
    case 4: // Pumping - solid on
      digitalWrite(PIN_LED, HIGH);
      break;
    case 5: // WiFi reconnecting - nháy đều mỗi 0.5s
      if (now - lastLedBlink >= 500) {
        ledState = !ledState;
        digitalWrite(PIN_LED, ledState ? HIGH : LOW);
        lastLedBlink = now;
      }
      break;
    default:
      digitalWrite(PIN_LED, LOW);
      break;
  }
}

// ===== CRITICAL: WiFi Reconnect Function =====
void checkWiFiAndReconnect() {
  unsigned long now = millis();
  
  // Chỉ check mỗi 30s để tránh overhead
  if (now - lastWiFiCheck < WIFI_CHECK_INTERVAL) {
    return;
  }
  lastWiFiCheck = now;
  
  // Kiểm tra trạng thái WiFi
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      // Vừa mới kết nối lại thành công
      wifiConnected = true;
      Serial.println("✅ WiFi reconnected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      
      // Thử sync NTP lại
      configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
      
      if (ledPattern == 2 || ledPattern == 5) {
        ledPattern = 1; // Back to OK
      }
      writeLog("WiFi reconnected");
    }
  } else {
    // Mất kết nối WiFi
    if (wifiConnected) {
      wifiConnected = false;
      Serial.println("⚠️ WiFi disconnected! Attempting reconnect...");
      Serial.println("⚠️ CRITICAL: Pump continues working WITHOUT WiFi!");
      writeLog("WiFi disconnected");
      ledPattern = 5; // Reconnecting pattern
    }
    
    // Thử reconnect (không blocking)
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnecting WiFi...");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      
      // Đợi tối đa 15s (với watchdog feed)
      unsigned long startReconnect = millis();
      while (WiFi.status() != WL_CONNECTED && 
             (millis() - startReconnect < WIFI_RECONNECT_TIMEOUT)) {
        delay(500);
        Serial.print(".");
        esp_task_wdt_reset(); // Feed watchdog trong khi reconnect
      }
      Serial.println();
      
      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("✅ WiFi reconnected successfully!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        ledPattern = 1;
        writeLog("WiFi reconnected");
      } else {
        Serial.println("❌ WiFi reconnect failed - will retry in 30s");
        Serial.println("⚠️ PUMP STILL WORKING - System is SAFE!");
        ledPattern = 2; // WiFi error
      }
    }
  }
}

void loadConfig() {
  // Load configuration from NVS
  loopCfg.on_min_day = prefs.getUInt("loopOnDay", 15);
  loopCfg.off_min_day = prefs.getUInt("loopOffDay", 45);
  loopCfg.on_min_night = prefs.getUInt("loopOnNight", 10);
  loopCfg.off_min_night = prefs.getUInt("loopOffNight", 50);
  loopCfg.light_start = prefs.getUChar("lightStart", 6);
  loopCfg.light_end = prefs.getUChar("lightEnd", 18);
  
  tdsCfg.target = prefs.getUInt("tdsTarget", 800);
  tdsCfg.hyst = prefs.getUInt("tdsHyst", 50);
  tdsCfg.dose_ms = prefs.getUInt("tdsDose", 700);
  tdsCfg.lock_s = prefs.getUInt("tdsLock", 90);
  tdsCfg.max_ms_per_day = prefs.getUInt("tdsMax", 60000);
  
  phCfg.target = prefs.getFloat("phTarget", 6.0f);
  phCfg.hyst = prefs.getFloat("phHyst", 0.3f);
  phCfg.dose_ms = prefs.getUInt("phDose", 300);
  phCfg.lock_s = prefs.getUInt("phLock", 90);
  phCfg.max_ms_per_day = prefs.getUInt("phMax", 30000);
  
  cal.ph7 = prefs.getFloat("calPH7", 1.65f);
  cal.ph4 = prefs.getFloat("calPH4", 2.10f);
  cal.tds_k = prefs.getFloat("calTDS", 0.5f);
  cal.zmctOffset = prefs.getFloat("zmctOffset", 1.65f);
  cal.zmctSensitivity = prefs.getFloat("zmctSens", 10.0f);
  
  // Load auto control settings
  autoLoopEnabled = prefs.getBool("autoLoop", true);
  autoTDSEnabled = prefs.getBool("autoTDS", true);
  autoPHEnabled = prefs.getBool("autoPH", true);

  validateLoadedConfig();
}

void saveConfig() {
  prefs.putUInt("loopOnDay", loopCfg.on_min_day);
  prefs.putUInt("loopOffDay", loopCfg.off_min_day);
  prefs.putUInt("loopOnNight", loopCfg.on_min_night);
  prefs.putUInt("loopOffNight", loopCfg.off_min_night);
  prefs.putUChar("lightStart", loopCfg.light_start);
  prefs.putUChar("lightEnd", loopCfg.light_end);
  
  prefs.putUInt("tdsTarget", tdsCfg.target);
  prefs.putUInt("tdsHyst", tdsCfg.hyst);
  prefs.putUInt("tdsDose", tdsCfg.dose_ms);
  prefs.putUInt("tdsLock", tdsCfg.lock_s);
  prefs.putUInt("tdsMax", tdsCfg.max_ms_per_day);
  
  prefs.putFloat("phTarget", phCfg.target);
  prefs.putFloat("phHyst", phCfg.hyst);
  prefs.putUInt("phDose", phCfg.dose_ms);
  prefs.putUInt("phLock", phCfg.lock_s);
  prefs.putUInt("phMax", phCfg.max_ms_per_day);
  
  prefs.putFloat("calPH7", cal.ph7);
  prefs.putFloat("calPH4", cal.ph4);
  prefs.putFloat("calTDS", cal.tds_k);
  prefs.putFloat("zmctOffset", cal.zmctOffset);
  prefs.putFloat("zmctSens", cal.zmctSensitivity);
  
  // Save auto control settings
  prefs.putBool("autoLoop", autoLoopEnabled);
  prefs.putBool("autoTDS", autoTDSEnabled);
  prefs.putBool("autoPH", autoPHEnabled);
}

// ===== Web Server Handlers =====
void handleRoot() {
  String html = readFile("/index.html");
  server.send(200, "text/html", html);
}

void handleDashboard() {
  String html = readFile("/dashboard.html");
  server.send(200, "text/html", html);
}

void handleConfig() {
  if (server.method() == HTTP_GET) {
    // Return current config page
    String html = readFile("/config.html");
    if (html.length() == 0) {
      // Fallback HTML
      html = generateConfigPage();
    }
    server.send(200, "text/html", html);
  } else if (server.method() == HTTP_POST) {
    // Save config
    if (server.hasArg("loopOnDay")) {
      loopCfg.on_min_day = server.arg("loopOnDay").toInt();
    }
    if (server.hasArg("loopOffDay")) {
      loopCfg.off_min_day = server.arg("loopOffDay").toInt();
    }
    if (server.hasArg("loopOnNight")) {
      loopCfg.on_min_night = server.arg("loopOnNight").toInt();
    }
    if (server.hasArg("loopOffNight")) {
      loopCfg.off_min_night = server.arg("loopOffNight").toInt();
    }
    if (server.hasArg("lightStart")) {
      loopCfg.light_start = server.arg("lightStart").toInt();
    }
    if (server.hasArg("lightEnd")) {
      loopCfg.light_end = server.arg("lightEnd").toInt();
    }
    if (server.hasArg("tdsTarget")) {
      tdsCfg.target = server.arg("tdsTarget").toInt();
    }
    if (server.hasArg("tdsHyst")) {
      tdsCfg.hyst = server.arg("tdsHyst").toInt();
    }
    if (server.hasArg("tdsDose")) {
      tdsCfg.dose_ms = server.arg("tdsDose").toInt();
    }
    if (server.hasArg("tdsLock")) {
      tdsCfg.lock_s = server.arg("tdsLock").toInt();
    }
    if (server.hasArg("phTarget")) {
      phCfg.target = server.arg("phTarget").toFloat();
    }
    if (server.hasArg("phHyst")) {
      phCfg.hyst = server.arg("phHyst").toFloat();
    }
    if (server.hasArg("phDose")) {
      phCfg.dose_ms = server.arg("phDose").toInt();
    }
    if (server.hasArg("phLock")) {
      phCfg.lock_s = server.arg("phLock").toInt();
    }
    saveConfig();
    writeLog("Config saved");
    sendJSON("{\"status\":\"ok\"}");
  }
}

void handleCalibration() {
  if (server.method() == HTTP_GET) {
    String html = readFile("/calibration.html");
    if (html.length() == 0) {
      html = generateCalibrationPage();
    }
    server.send(200, "text/html", html);
  } else if (server.method() == HTTP_POST) {
    if (server.hasArg("setpH7")) {
      cal.ph7 = adcToVoltage(readADCMedian(PIN_PH));
      prefs.putFloat("calPH7", cal.ph7);
      writeLog("pH7 calibrated: " + String(cal.ph7));
    }
    if (server.hasArg("setpH4")) {
      cal.ph4 = adcToVoltage(readADCMedian(PIN_PH));
      prefs.putFloat("calPH4", cal.ph4);
      writeLog("pH4 calibrated: " + String(cal.ph4));
    }
    if (server.hasArg("setTDS")) {
      float v = adcToVoltage(readADCMedian(PIN_TDS));
      float knownEC = server.arg("knownEC").toFloat();
      cal.tds_k = knownEC / (v * 1000.0f);
      prefs.putFloat("calTDS", cal.tds_k);
      writeLog("TDS calibrated: k=" + String(cal.tds_k));
    }
    // ZMCT103C Calibration
    if (server.hasArg("setZMCTOffset")) {
      // Bước 1: Hiệu chuẩn offset (không có tải)
      cal.zmctOffset = adcToVoltage(readADCMedian(PIN_ZMCT));
      prefs.putFloat("zmctOffset", cal.zmctOffset);
      writeLog("ZMCT Offset calibrated: " + String(cal.zmctOffset) + "V");
    }
    if (server.hasArg("setZMCTSensitivity")) {
      // Bước 2: Hiệu chuẩn sensitivity (có tải đã biết dòng điện)
      float v = adcToVoltage(readADCMedian(PIN_ZMCT));
      float knownCurrent = server.arg("knownCurrent").toFloat();  // Dòng điện thực tế (A)
      float vDiff = abs(v - cal.zmctOffset);
      if (vDiff > 0.01f) {  // Tránh chia cho 0
        cal.zmctSensitivity = knownCurrent / vDiff;
        prefs.putFloat("zmctSens", cal.zmctSensitivity);
        writeLog("ZMCT Sensitivity calibrated: " + String(cal.zmctSensitivity) + "A/V");
      }
    }
    if (server.hasArg("resetEnergy")) {
      // Reset tổng điện năng
      energyKwh = 0.0f;
      writeLog("Energy counter reset to 0 kWh");
    }
    sendJSON("{\"status\":\"ok\"}");
  }
}

void handleManualControl() {
  if (server.method() == HTTP_GET) {
    String html = readFile("/manual.html");
    if (html.length() == 0) {
      html = generateManualPage();
    }
    server.send(200, "text/html", html);
  } else if (server.method() == HTTP_POST) {
    if (server.hasArg("pump")) {
      manualPump = server.arg("pump") == "1";
      loopCommandedOn = manualPump;
      commandPumpRelay(manualPump);
      Serial.printf("Manual Pump: %d\n", manualPump);
    }
    if (server.hasArg("a")) {
      manualA = server.arg("a") == "1";
      setRelay(PIN_RELAY_A, manualA);
      Serial.printf("Manual Pump A: %d\n", manualA);
    }
    if (server.hasArg("b")) {
      manualB = server.arg("b") == "1";
      setRelay(PIN_RELAY_B, manualB);
      Serial.printf("Manual Pump B: %d\n", manualB);
    }
    if (server.hasArg("downph")) {
      manualDownpH = server.arg("downph") == "1";
      setRelay(PIN_RELAY_DOWNP, manualDownpH);
      Serial.printf("Manual Down-pH: %d\n", manualDownpH);
    }
    if (server.hasArg("auto")) {
      manualControl = server.arg("auto") == "0";
      if (!manualControl) {
        writeLog("Switched to AUTO mode");
      }
    }
    if (server.hasArg("manual")) {
      manualControl = server.arg("manual") == "1";
      if (manualControl) {
        writeLog("Switched to MANUAL mode - All controls now direct");
      }
    }
    sendJSON("{\"status\":\"ok\"}");
  }
}

void handleSensorData() {
  // Trạng thái thực tế của bơm tuần hoàn
  bool pumpActualState = pumpRelayCommandOn;
  
  // Tính thời gian còn lại cho bơm tuần hoàn (ms)
  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo, 0);
  bool isDaytime = false;
  if (timeValid && timeinfo.tm_year >= 70) {
    int hour = timeinfo.tm_hour;
    isDaytime = (hour >= loopCfg.light_start && hour < loopCfg.light_end);
  } else {
    unsigned long hours = (millis() / 3600000UL) % 24;
    isDaytime = (hours >= loopCfg.light_start && hours < loopCfg.light_end);
  }
  
  const unsigned long onMs  = isDaytime ? 
                               (unsigned long)loopCfg.on_min_day * 60000UL : 
                               (unsigned long)loopCfg.on_min_night * 60000UL;
  const unsigned long offMs = isDaytime ? 
                               (unsigned long)loopCfg.off_min_day * 60000UL : 
                               (unsigned long)loopCfg.off_min_night * 60000UL;
  
  unsigned long loopRemainMs = 0;
  if (loopOn) {
    // Đang ON → thời gian còn lại cho ON
    unsigned long elapsed = millis() - tLastLoop;
    loopRemainMs = (elapsed < onMs) ? (onMs - elapsed) : 0;
  } else {
    // Đang OFF → thời gian còn lại cho OFF
    unsigned long elapsed = millis() - tLastLoop;
    loopRemainMs = (elapsed < offMs) ? (offMs - elapsed) : 0;
  }
  
  String json = "{";
  json += "\"temp\":" + jsonFloatOrNull(tempC, 1) + ",";
  json += "\"ph\":" + jsonFloatOrNull(phVal, 2) + ",";
  json += "\"tds\":" + jsonFloatOrNull(tdsVal, 0) + ",";
  json += "\"current\":" + jsonFloatOrNull(acCurrent, 2) + ",";
  json += "\"power\":" + jsonFloatOrNull(acPower, 1) + ",";
  json += "\"energy\":" + jsonFloatOrNull(energyKwh, 3) + ",";
  json += "\"pump\":" + String(pumpActualState ? 1 : 0) + ",";
  json += "\"loopOn\":" + String(loopOn ? 1 : 0) + ",";
  json += "\"loopRemainMs\":" + String(loopRemainMs) + ",";
  json += "\"manualMode\":" + String(manualControl ? 1 : 0) + ",";
  json += "\"manualPump\":" + String(manualPump ? 1 : 0) + ",";
  
  // Thêm trạng thái TDS và pH
  json += "\"tdsDosing\":" + String(tdsDosing ? 1 : 0) + ",";
  json += "\"phDosing\":" + String(phDosing ? 1 : 0) + ",";
  
  // Thêm thông tin daily dose (chuyển từ ms sang giây)
  json += "\"doseAToday\":" + String(doseA_today / 1000.0f, 1) + ",";
  json += "\"doseBToday\":" + String(doseB_today / 1000.0f, 1) + ",";
  json += "\"dosePToday\":" + String(doseP_today / 1000.0f, 1) + ",";
  
  // Thêm config để tính % daily limit
  json += "\"tdsTarget\":" + String(tdsCfg.target) + ",";
  json += "\"tdsHyst\":" + String(tdsCfg.hyst) + ",";
  json += "\"phTarget\":" + String(phCfg.target, 2) + ",";
  json += "\"phHyst\":" + String(phCfg.hyst, 2);
  
  json += "}";
  sendJSON(json);
}

void handleDiagData() {
  String json = "{";
  json += "\"uptimeMs\":" + String(millis()) + ",";
  json += "\"resetReason\":\"" + jsonEscape(lastResetReasonText) + "\",";
  json += "\"wifiConnected\":" + String(wifiConnected ? 1 : 0) + ",";
  json += "\"sensorFaultLatched\":" + String(sensorFaultLatched ? 1 : 0) + ",";
  json += "\"sensorFaultReason\":\"" + jsonEscape(lastSensorFaultReason) + "\",";
  json += "\"loopCommandedOn\":" + String(loopCommandedOn ? 1 : 0) + ",";
  json += "\"pumpRelayCommandOn\":" + String(pumpRelayCommandOn ? 1 : 0) + ",";
  json += "\"bootCount\":" + String(diagCounters.bootCount) + ",";
  json += "\"powerOnCount\":" + String(diagCounters.powerOnCount) + ",";
  json += "\"brownoutCount\":" + String(diagCounters.brownoutCount) + ",";
  json += "\"wdtCount\":" + String(diagCounters.wdtCount) + ",";
  json += "\"sensorFaultCount\":" + String(diagCounters.sensorFaultCount) + ",";
  json += "\"diagEventCount\":" + String(diagEventTotal) + ",";
  json += "\"lastEvent\":\"" + jsonEscape(lastDiagEvent) + "\"";
  json += "}";
  sendJSON(json);
}

void sendJSON(const String& json) {
  server.send(200, "application/json", json);
}

String readFile(String path) {
  File f = LittleFS.open(path, "r");
  if (!f) return String();
  String content = f.readString();
  f.close();
  return content;
}

void handleStaticFile(String path) {
  String mimetype = "text/plain";
  if (path.endsWith(".html")) mimetype = "text/html";
  else if (path.endsWith(".css")) mimetype = "text/css";
  else if (path.endsWith(".js")) mimetype = "application/javascript";
  
  // Read file from LittleFS and send content
  File f = LittleFS.open(path, "r");
  if (!f) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  
  server.streamFile(f, mimetype);
  f.close();
}

void writeLog(String msg) {
  char entry[DIAG_EVENT_LEN];
  snprintf(entry, sizeof(entry), "%lu,%s", millis(), msg.c_str());

  size_t slot = diagEventWriteIndex % DIAG_EVENT_CAPACITY;
  snprintf(diagEvents[slot], sizeof(diagEvents[slot]), "%s", entry);
  snprintf(lastDiagEvent, sizeof(lastDiagEvent), "%s", entry);

  diagEventWriteIndex = (diagEventWriteIndex + 1) % DIAG_EVENT_CAPACITY;
  if (diagEventTotal < DIAG_EVENT_CAPACITY) {
    diagEventTotal++;
  }

  Serial.println(entry);
}

String generateConfigPage() {
  String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Cấu Hình - Farm365</title></head><body><h1>Cấu Hình</h1><p>Trang này đang phát triển. Vui lòng upload file config.html vào LittleFS.</p></body></html>");
  return html;
}

String generateCalibrationPage() {
  String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Hiệu Chuẩn - Farm365</title></head><body><h1>Hiệu Chuẩn</h1><p>Trang này đang phát triển. Vui lòng upload file calibration.html vào LittleFS.</p></body></html>");
  return html;
}

String generateManualPage() {
  String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Điều Khiển - Farm365</title></head><body><h1>Điều Khiển Thủ Công</h1><p>Trang này đang phát triển. Vui lòng upload file manual.html vào LittleFS.</p></body></html>");
  return html;
}
