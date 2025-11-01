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
uint8_t ledPattern = 0; // 0: boot, 1: OK, 2: wifi error, 3: sensor error, 4: pumping

bool manualControl = false;
bool manualPump = false, manualA = false, manualB = false, manualDownpH = false;

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
void loadConfig();
void saveConfig();
void handleRoot();
void handleDashboard();
void handleConfig();
void handleCalibration();
void handleManualControl();
void handleSensorData();
void sendJSON(const String& json);
String readFile(String path);
void handleStaticFile(String path);
void writeLog(String msg);
String generateConfigPage();
String generateCalibrationPage();
String generateManualPage();

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
  setRelay(PIN_RELAY_PUMP, false);
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
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  unsigned long wifiTimeout = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimeout < 10000)) {
    delay(500);
    Serial.print(".");
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
    ledPattern = 2; // WiFi error
  }
  
  // Initialize LittleFS
  if (LittleFS.begin()) {
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
    json += "\"phLock\":" + String(phCfg.lock_s);
    json += "}";
    sendJSON(json);
  });
  server.onNotFound([]() {
    handleStaticFile(server.uri());
  });
  
  server.begin();
  Serial.println("Web server started");
  
  // Set LED pattern to OK
  ledPattern = 1;
  lastLedBlink = millis();
  
  lastResetTime = millis();
}

// ===== Main Loop =====
void loop() {
  static unsigned long tSense = 0;
  static unsigned long tLockA = 0, tLockB = 0, tLockP = 0;
  
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
    
    // Check for sensor errors
    if (tempC == -127.0f || tempC == 85.0f || isnan(phVal) || isnan(tdsVal)) {
      ledPattern = 3; // Sensor error
    } else if (ledPattern == 3) {
      ledPattern = 1; // Recover to OK
    }
    
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
  
  if (!manualControl) {
    if (loopOn) {
      if (millis() - tLastLoop > onMs) {
        loopOn = false;
        tLastLoop = millis();
        setRelay(PIN_RELAY_PUMP, false);
        ledPattern = 1;
        writeLog("Circulation pump OFF");
      } else {
        ledPattern = 4; // Pumping
      }
    } else {
      if (millis() - tLastLoop > offMs) {
        loopOn = true;
        tLastLoop = millis();
        setRelay(PIN_RELAY_PUMP, true);
        ledPattern = 4; // Pumping
        writeLog("Circulation pump ON");
      } else if (ledPattern == 4) {
        ledPattern = 1;
      }
    }
  } else {
    // Manual control
    setRelay(PIN_RELAY_PUMP, manualPump);
    if (manualPump) {
      ledPattern = 4;
    } else if (ledPattern == 4) {
      ledPattern = 1;
    }
  }
  
  // Check non-blocking dosing
  checkDosePump();
  
  // TDS Control (Pump A & B) - Hysteresis 2 ngưỡng
  if (!manualControl) {
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
    // MANUAL mode - điều khiển trực tiếp
    setRelay(PIN_RELAY_A, manualA);
    setRelay(PIN_RELAY_B, manualB);
    tdsDosing = false;  // Reset state khi manual
  }
  
  // pH Control (Down-pH) - Hysteresis 2 ngưỡng
  if (!manualControl) {
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
    setRelay(PIN_RELAY_DOWNP, manualDownpH);
    phDosing = false;  // Reset state khi manual
  }
  
  // LED pattern cho dosing pumps
  if (!loopOn) {  // Chỉ set nếu bơm tuần hoàn không chạy
    if (activeDosing.active || 
        (millis() - tLastDoseA < (tdsCfg.dose_ms + 1000)) ||
        (millis() - tLastDoseB < (tdsCfg.dose_ms + 1000)) ||
        (millis() - tLastDoseP < (phCfg.dose_ms + 1000))) {
      ledPattern = 4;
    }
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
    default:
      digitalWrite(PIN_LED, LOW);
      break;
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
      setRelay(PIN_RELAY_PUMP, manualPump);
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
  bool pumpActualState = manualControl ? manualPump : loopOn;
  
  String json = "{";
  json += "\"temp\":" + String(tempC, 1) + ",";
  json += "\"ph\":" + String(phVal, 2) + ",";
  json += "\"tds\":" + String(tdsVal, 0) + ",";
  json += "\"current\":" + String(acCurrent, 2) + ",";
  json += "\"power\":" + String(acPower, 1) + ",";
  json += "\"energy\":" + String(energyKwh, 3) + ",";
  json += "\"pump\":" + String(pumpActualState ? 1 : 0) + ",";
  json += "\"loopOn\":" + String(loopOn ? 1 : 0) + ",";
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
  File f = LittleFS.open("/log.txt", "a");
  if (f) {
    f.print(millis());
    f.print(",");
    f.println(msg);
    f.close();
  }
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

