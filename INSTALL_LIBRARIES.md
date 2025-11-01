# 📚 Cài Đặt Thư Viện - Farm365

## Arduino IDE

### Cách 1: Cài Qua Library Manager (Đề Xuất)

1. Mở Arduino IDE
2. Vào `Tools → Manage Libraries...`
3. Tìm và cài đặt từng thư viện sau:

| Thư Viện | Tác Giả | Link |
|-----------|---------|------|
| OneWire | Paul Stoffregen | [GitHub](https://github.com/paulstoffregen/OneWire) |
| DallasTemperature | Paul Stoffregen | [GitHub](https://github.com/paulstoffregen/DallasTemperature) |
| BH1750 | Christopher Laws | [GitHub](https://github.com/chrislaws/BH1750) |

### Cách 2: Cài Qua GitHub (Nếu Library Manager không có)

```bash
# Vào thư mục Libraries của Arduino IDE
# Windows: Documents/Arduino/libraries/
# Mac: ~/Documents/Arduino/libraries/
# Linux: ~/Arduino/libraries/

cd ~/Documents/Arduino/libraries/

# Clone các thư viện
git clone https://github.com/paulstoffregen/OneWire.git
git clone https://github.com/paulstoffregen/DallasTemperature.git
git clone https://github.com/chrislaws/BH1750.git
```

### Cách 3: Cài Zip File

1. Download từ GitHub:
   - [OneWire](https://github.com/paulstoffregen/OneWire/archive/master.zip)
   - [DallasTemperature](https://github.com/paulstoffregen/DallasTemperature/archive/master.zip)
   - [BH1750](https://github.com/chrislaws/BH1750/archive/master.zip)

2. Vào `Sketch → Include Library → Add .ZIP Library...`

3. Chọn file zip đã tải về

## PlatformIO (Nếu Dùng Thay Arduino IDE)

### Cách 1: Tạo Project Mới

```bash
# Cài PlatformIO CLI
pip install platformio

# Hoặc dùng VS Code extension
# PlatformIO IDE in VS Code

# Tạo project (đã có sẵn platformio.ini trong repo)
cd /Users/haidv/IdeaProjects/thuycanhesp32
pio run -e esp32dev
```

### Cách 2: Dùng File `platformio.ini` Có Sẵn

File `platformio.ini` đã được tạo sẵn trong repo này. Đơn giản chạy:

```bash
pio run
```

## ✅ Kiểm Tra Thư Viện Đã Cài

Mở Arduino IDE → `File → Examples` → Xem có các thư viện không:

- OneWire → examples → OneWire
- DallasTemperature → examples → Dallas → Simple
- BH1750 → examples → BH1750test

## 🚨 Nếu Vẫn Lỗi

### Lỗi "No such file or directory"

1. Khởi động lại Arduino IDE
2. Kiểm tra `Sketch → Include Library → Manage Libraries...` xem thư viện đã cài chưa
3. Nếu dùng ESP32, đảm bảo đã cài ESP32 Board Manager:
   - `File → Preferences → Additional Boards Manager URLs`
   - Thêm: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Vào `Tools → Board → Boards Manager...`
   - Tìm "ESP32" → Install

### Lỗi "Multiple definition"

- Xóa các version cũ của thư viện
- Chỉ giữ 1 version trong folder libraries/

## 📦 Thư Viện Bắt Buộc

| Thư Viện | Mục Đích | Bắt Buộc |
|----------|----------|----------|
| OneWire | Giao tiếp DS18B20 | ✅ |
| DallasTemperature | Đọc DS18B20 | ✅ |
| BH1750 | Cảm biến ánh sáng | ⚠️ (Tùy chọn) |

**Lưu ý:** Nếu không dùng BH1750, comment dòng `#include <BH1750.h>` trong code.

## 🔧 Cài Đặt ESP32 Board (Lần Đầu)

Nếu chưa có ESP32 trong Arduino IDE:

1. `File → Preferences`
2. Thêm URL này vào "Additional Boards Manager URLs":
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
3. `Tools → Board → Boards Manager...`
4. Tìm "esp32" → Install
5. Chọn board: `Tools → Board → ESP32 Arduino → ESP32 Dev Module`

## ✨ Thư Viện ESP32 Sẵn Có (Không Cần Cài)

- WiFi
- WebServer
- Preferences
- LittleFS
- ESP32FS

Những thư viện này đã được tích hợp sẵn trong ESP32 Arduino Core.

## 📝 Quick Install (Copy & Paste)

```bash
# Vào Arduino IDE
Tools → Manage Libraries → Tìm và cài:
- OneWire
- DallasTemperature  
- BH1750

# Hoặc dùng PlatformIO
cd /Users/haidv/IdeaProjects/thuycanhesp32
pio run
```

---

**💡 Tip:** Nếu vẫn gặp lỗi, hãy khởi động lại Arduino IDE và kiểm tra lại sketch.




