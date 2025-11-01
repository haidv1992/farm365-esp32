# Farm365 ESP32 - Hydroponic Controller

🌱 Hệ thống điều khiển thủy canh tự động với ESP32 - Giám sát và điều chỉnh pH, TDS, nhiệt độ, dòng điện

![ESP32](https://img.shields.io/badge/ESP32-DevKit-blue)
![Arduino](https://img.shields.io/badge/Arduino-Framework-00979D)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Community-orange)
![License](https://img.shields.io/badge/License-MIT-green)

## ✨ Features

### Điều Khiển Thông Minh
- ✅ **Hysteresis 2-threshold control** - Tránh dao động (chattering) cho TDS & pH
- ✅ **Day/Night pump scheduling** - Lịch bơm tuần hoàn phân biệt ngày/đêm
- ✅ **NTP Time Sync** - Đồng bộ thời gian tự động (UTC+7)
- ✅ **Non-blocking dosing system** - Không block Web UI khi đang châm

### Giám Sát Cảm Biến
- 🌡️ **Nhiệt độ** - DS18B20 (±0.5°C)
- 🧪 **pH** - Aideepen/LM358 module (2-point calibration)
- ⚖️ **TDS** - Electrical Conductivity sensor
- ⚡ **Dòng điện AC** - ZMCT103C với True RMS calculation
- 📊 **Công suất & Điện năng** - Real-time monitoring

### Web Interface
- 📱 **Responsive UI** - Tối ưu cho mobile và desktop
- 📊 **Dashboard** - Hiển thị real-time sensors với trạng thái chi tiết
- ⚙️ **Configuration** - Cấu hình TDS/pH target, hysteresis, pump schedules
- 🔬 **Calibration** - Hiệu chuẩn pH (2-point), TDS, ZMCT103C
- 🎮 **Manual Control** - Điều khiển thủ công từng bơm
- 💾 **NVS Persistent Storage** - Config không mất khi nạp firmware

---

## 🛠️ Hardware Requirements

### ESP32 & Power
- ESP32 DevKit (240MHz, 4MB Flash)
- Nguồn 3.3V cho ESP32 (không dùng VIN)
- GND chung cho toàn hệ thống

### Sensors
- **pH Sensor** (Aideepen/LM358) → GPIO33 (ADC1) + RC filter
- **TDS Sensor** → GPIO32 (ADC1)
- **DS18B20** Temperature → GPIO4 + 10kΩ pull-up
- **ZMCT103C** AC Current → GPIO34 (ADC1)
- *(Optional)* BH1750 Light Sensor → I²C (SDA: GPIO25, SCL: GPIO26)

### Actuators
- **4-Channel Relay Module** (Low-trigger):
  - IN1: Bơm tuần hoàn → GPIO18
  - IN2: Bơm A (Dinh dưỡng) → GPIO19
  - IN3: Bơm B (Dinh dưỡng) → GPIO21
  - IN4: Bơm Down-pH → GPIO22

### Status LED
- LED + 220-330Ω resistor → GPIO2

---

## 📋 Pin Mapping

| Component | Pin | Note |
|-----------|-----|------|
| Relay Pump | GPIO18 | Bơm tuần hoàn 12V/5A |
| Relay A | GPIO19 | Bơm dinh dưỡng A |
| Relay B | GPIO21 | Bơm dinh dưỡng B |
| Relay Down-pH | GPIO22 | Bơm giảm pH |
| LED Status | GPIO2 | 220-330Ω resistor |
| TDS Sensor | GPIO32 | ADC1 |
| pH Sensor | GPIO33 | ADC1 + RC filter (100Ω + 0.1µF) |
| ZMCT103C | GPIO34 | ADC1 |
| DS18B20 | GPIO4 | 10kΩ pull-up to 3.3V |
| BH1750 SDA | GPIO25 | (Optional) |
| BH1750 SCL | GPIO26 | (Optional) |

---

## 🚀 Quick Start

### 1. Clone Repository
```bash
git clone git@github.com:haidv1992/farm365-esp32.git
cd farm365-esp32
```

### 2. Install PlatformIO
```bash
# Using pip
pip install platformio

# Or using Homebrew (macOS)
brew install platformio
```

### 3. Update WiFi Credentials
Edit `src/main.cpp`:
```cpp
const char* WIFI_SSID = "your_wifi_ssid";
const char* WIFI_PASS = "your_wifi_password";
```

### 4. Build & Upload
```bash
# Build firmware
pio run

# Upload firmware
pio run --target upload

# Upload filesystem (HTML files)
pio run --target uploadfs
```

### 5. Access Web Interface
1. Mở Serial Monitor để xem IP address:
```bash
pio device monitor
```

2. Truy cập Web UI:
```
http://[ESP32_IP_ADDRESS]/
```

---

## 📚 Documentation

- **[CALIBRATION_GUIDE.md](CALIBRATION_GUIDE.md)** - Hướng dẫn hiệu chuẩn pH, TDS, ZMCT103C chi tiết
- **[UPGRADE_SUMMARY.md](UPGRADE_SUMMARY.md)** - Tóm tắt các cải tiến và tính năng
- **[WIRING_DIAGRAM.md](WIRING_DIAGRAM.md)** - Sơ đồ đấu dây chi tiết (nếu có)

---

## 🎯 Key Features Explained

### Hysteresis 2-Threshold Control

**TDS Control:**
- Ngưỡng THẤP: `target - hysteresis` → **BẬT** chu kỳ châm
- Ngưỡng CAO: `target + hysteresis` → **TẮT** chu kỳ châm
- Vùng giữa: **GIỮ NGUYÊN** trạng thái

**Ví dụ:** Target = 800 ppm, Hysteresis = 50 ppm
- TDS < 750 → Bắt đầu châm
- TDS > 850 → Dừng châm
- 750 ≤ TDS ≤ 850 → Giữ nguyên trạng thái

**pH Control:**
- Ngưỡng CAO: `target + hysteresis` → **BẬT** chu kỳ châm Down-pH
- Ngưỡng THẤP: `target - hysteresis` → **TẮT** chu kỳ châm

### True RMS Calculation (ZMCT103C)

Thay vì đo DC midpoint, hệ thống:
1. Sample 100 điểm trong 1 cycle AC (400µs/sample = 2.5kHz)
2. Tính RMS: `sqrt(Σ(v²) / n)`
3. Chuyển đổi sang dòng điện (A) qua hệ số calibration
4. Tính công suất: P = V × I
5. Tích lũy điện năng: E = Σ(P × Δt)

---

## ⚙️ Configuration

### Default Settings

**TDS (Dinh Dưỡng):**
- Target: 800 ppm
- Hysteresis: 50 ppm
- Dose time: 700 ms
- Lockout: 90 seconds
- Daily limit: 60 seconds

**pH:**
- Target: 6.0
- Hysteresis: 0.3
- Dose time: 300 ms
- Lockout: 90 seconds
- Daily limit: 30 seconds

**Bơm Tuần Hoàn:**
- Ngày (6h-18h): 15 phút ON / 45 phút OFF
- Đêm (18h-6h): 10 phút ON / 50 phút OFF

---

## 🔧 Calibration

### pH Sensor (2-point)
1. Nhúng probe vào pH 6.86 → Bấm "Set pH7"
2. Nhúng probe vào pH 4.00 → Bấm "Set pH4"

### TDS Sensor
1. Nhúng probe vào dung dịch chuẩn (1413 µS/cm)
2. Nhập EC chuẩn → Bấm "Set TDS"

### ZMCT103C (AC Current)
1. Tắt tất cả tải → Bấm "Set Offset"
2. Bật tải đã biết dòng (VD: 0.45A) → Bấm "Set Sensitivity"

📖 Chi tiết xem [CALIBRATION_GUIDE.md](CALIBRATION_GUIDE.md)

---

## 📊 API Endpoints

- `GET /` - Homepage
- `GET /dashboard` - Real-time sensor dashboard
- `GET /config` - Configuration page
- `GET /calibration` - Calibration page
- `GET /manual` - Manual control page
- `GET /api/sensor` - JSON sensor data
- `GET /api/config` - JSON configuration
- `POST /config` - Save configuration
- `POST /calibration` - Calibration actions
- `POST /manual` - Manual control

---

## 🛡️ Safety Features

- ✅ **Daily dose limits** - Tránh bơm quá liều
- ✅ **Lockout periods** - Thời gian chờ giữa các lần châm
- ✅ **Brownout detector** - Phát hiện điện áp thấp
- ✅ **Watchdog timer** - Tự động reset nếu treo
- ✅ **Fail-safe logic** - Dừng bơm khi vượt giới hạn

---

## 📱 Web UI Screenshots

### Dashboard
- Real-time sensor readings
- Pump status với trạng thái chi tiết
- Daily dose counters
- TDS/pH status với ngưỡng hysteresis

### Configuration
- TDS target & hysteresis
- pH target & hysteresis
- Day/Night pump schedules
- Dose time & lockout periods

### Calibration
- pH 2-point calibration
- TDS calibration
- ZMCT103C offset & sensitivity
- Reset functions

---

## 🧰 Tech Stack

- **Framework:** Arduino (ESP32)
- **Build System:** PlatformIO
- **Filesystem:** LittleFS
- **Web Server:** ESP32 WebServer
- **Time Sync:** NTP (pool.ntp.org)
- **Storage:** NVS (Non-Volatile Storage)
- **Libraries:**
  - OneWire (2.3.8)
  - DallasTemperature (3.11.0)
  - BH1750 (1.3.0) - Optional

---

## 📈 Performance

- **Flash Usage:** 886 KB (67.6%)
- **RAM Usage:** 46 KB (14.1%)
- **API Response:** < 100 ms
- **Dashboard Update:** 3 seconds interval
- **True RMS Sampling:** 2.5 kHz (100 samples/cycle)

---

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

---

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details

---

## 👤 Author

**Hai DV** - [haidv1992](https://github.com/haidv1992)

---

## 🙏 Acknowledgments

- ESP32 Community
- PlatformIO Team
- Arduino Framework Contributors

---

## 📞 Support

For issues, questions, or suggestions:
- Open an issue on GitHub
- Email: haidv1992@example.com (update your email)

---

Made with ❤️ for smart farming 🌱
