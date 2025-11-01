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
#include <BH1750.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <FS.h>

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
BH1750 lightMeter;
WebServer server(80);
Preferences prefs;

// ===== Configuration Structure =====
struct LoopCfg { 
  uint16_t on_min = 15; 
  uint16_t off_min = 45; 
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

int adcPH = 0, adcTDS = 0, adcZMCT = 0;

bool wifiConnected = false;
bool ledState = false;
unsigned long lastLedBlink = 0;
uint8_t ledPattern = 0; // 0: boot, 1: OK, 2: wifi error, 3: sensor error, 4: pumping

bool manualControl = false;
bool manualPump = false, manualA = false, manualB = false, manualDownpH = false;

// ===== Function Prototypes =====
int readADCMedian(uint8_t pin);
float adcToVoltage(int raw);
float voltageToPH(float v);
float voltageToTDS(float v);
void setRelay(uint8_t pin, bool on);
void dosePump(uint8_t pin, uint16_t ms, uint32_t &accum, uint32_t limit);
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
  
  // Initialize I2C for BH1750
  Wire.begin(25, 26);
  if (!lightMeter.begin()) {
    Serial.println("BH1750 not found!");
  }
  
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
    
    // Check for sensor errors
    if (tempC == -127.0f || tempC == 85.0f || isnan(phVal) || isnan(tdsVal)) {
      ledPattern = 3; // Sensor error
    } else if (ledPattern == 3) {
      ledPattern = 1; // Recover to OK
    }
    
    // Read light (optional)
    luxVal = lightMeter.readLightLevel();
  }
  
  // Control loop pump (every minute)
  const unsigned long onMs  = (unsigned long)loopCfg.on_min * 60000UL;
  const unsigned long offMs = (unsigned long)loopCfg.off_min * 60000UL;
  
  if (!manualControl) {
    if (loopOn) {
      if (millis() - tLastLoop > onMs) {
        loopOn = false;
        tLastLoop = millis();
        setRelay(PIN_RELAY_PUMP, false);
        ledPattern = 1;
        writeLog("Circulation pump OFF");
      }
    } else {
      if (millis() - tLastLoop > offMs) {
        loopOn = true;
        tLastLoop = millis();
        setRelay(PIN_RELAY_PUMP, true);
        ledPattern = 4; // Pumping
        writeLog("Circulation pump ON");
      }
    }
  } else {
    // Manual control
    setRelay(PIN_RELAY_PUMP, manualPump);
  }
  
  // TDS Control (Pump A & B)
  if (!manualControl) {
    if (tdsVal < (tdsCfg.target - tdsCfg.hyst)) {
      if (millis() - tLockA > (unsigned long)tdsCfg.lock_s * 1000UL) {
        dosePump(PIN_RELAY_A, tdsCfg.dose_ms, doseA_today, tdsCfg.max_ms_per_day);
        tLockA = millis();
        ledPattern = 4;
        writeLog("Dosed Pump A");
      }
    }
    
    if (tdsVal < (tdsCfg.target - tdsCfg.hyst)) {
      if (millis() - tLockB > (unsigned long)tdsCfg.lock_s * 1000UL) {
        dosePump(PIN_RELAY_B, tdsCfg.dose_ms, doseB_today, tdsCfg.max_ms_per_day);
        tLockB = millis();
        ledPattern = 4;
        writeLog("Dosed Pump B");
      }
    }
  } else {
    setRelay(PIN_RELAY_A, manualA);
    setRelay(PIN_RELAY_B, manualB);
  }
  
  // pH Control (Down-pH)
  if (!manualControl) {
    if (phVal > (phCfg.target + phCfg.hyst)) {
      if (millis() - tLockP > (unsigned long)phCfg.lock_s * 1000UL) {
        dosePump(PIN_RELAY_DOWNP, phCfg.dose_ms, doseP_today, phCfg.max_ms_per_day);
        tLockP = millis();
        ledPattern = 4;
        writeLog("Dosed Down-pH");
      }
    }
  } else {
    setRelay(PIN_RELAY_DOWNP, manualDownpH);
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

void dosePump(uint8_t pin, uint16_t ms, uint32_t &accum, uint32_t limit) {
  if (accum + ms > limit) {
    writeLog("Daily limit reached");
    return;
  }
  setRelay(pin, true);
  delay(ms);
  setRelay(pin, false);
  accum += ms;
}

void ledPatternControl() {
  unsigned long now = millis();
  
  switch (ledPattern) {
    case 0: // Booting - already done
      break;
    case 1: // OK - blink once every 2 seconds
      if (now - lastLedBlink > 2000) {
        digitalWrite(PIN_LED, HIGH);
        delay(80);
        digitalWrite(PIN_LED, LOW);
        lastLedBlink = now;
      }
      break;
    case 2: // WiFi error - double blink
      if (now - lastLedBlink > 100) {
        digitalWrite(PIN_LED, ledState ? LOW : HIGH);
        ledState = !ledState;
        lastLedBlink = now;
      }
      break;
    case 3: // Sensor error - 3 quick blinks
      static unsigned long t = 0;
      static int count = 0;
      if (now - t > 2000) {
        if (count < 3) {
          if ((now - lastLedBlink) > 100) {
            digitalWrite(PIN_LED, HIGH);
            delay(50);
            digitalWrite(PIN_LED, LOW);
            lastLedBlink = now;
            count++;
          }
        } else {
          count = 0;
          t = now;
        }
      }
      break;
    case 4: // Pumping - solid on
      digitalWrite(PIN_LED, HIGH);
      break;
  }
}

void loadConfig() {
  // Load configuration from NVS
  loopCfg.on_min = prefs.getUInt("loopOn", 15);
  loopCfg.off_min = prefs.getUInt("loopOff", 45);
  
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
}

void saveConfig() {
  prefs.putUInt("loopOn", loopCfg.on_min);
  prefs.putUInt("loopOff", loopCfg.off_min);
  
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
    if (server.hasArg("loopOn")) {
      loopCfg.on_min = server.arg("loopOn").toInt();
    }
    if (server.hasArg("loopOff")) {
      loopCfg.off_min = server.arg("loopOff").toInt();
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
    }
    if (server.hasArg("a")) {
      manualA = server.arg("a") == "1";
      setRelay(PIN_RELAY_A, manualA);
    }
    if (server.hasArg("b")) {
      manualB = server.arg("b") == "1";
      setRelay(PIN_RELAY_B, manualB);
    }
    if (server.hasArg("downph")) {
      manualDownpH = server.arg("downph") == "1";
      setRelay(PIN_RELAY_DOWNP, manualDownpH);
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
        writeLog("Switched to MANUAL mode");
      }
    }
    sendJSON("{\"status\":\"ok\"}");
  }
}

void handleSensorData() {
  String json = "{";
  json += "\"temp\":" + String(tempC, 1) + ",";
  json += "\"ph\":" + String(phVal, 2) + ",";
  json += "\"tds\":" + String(tdsVal, 0) + ",";
  json += "\"lux\":" + String(luxVal, 0) + ",";
  json += "\"pump\":" + String(loopOn ? 1 : 0) + ",";
  json += "\"loopOn\":" + String(loopOn ? 1 : 0);
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

