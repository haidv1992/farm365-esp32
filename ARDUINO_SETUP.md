# 🛠️ Hướng Dẫn Setup Arduino IDE - Farm365

## Bước 1: Cài ESP32 Board Manager

1. Mở Arduino IDE
2. Vào `File → Preferences`
3. Tìm ô "Additional Boards Manager URLs"
4. Thêm URL sau (nếu chưa có):
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
5. Nhấn OK

## Bước 2: Cài ESP32 Board

1. Vào `Tools → Board → Boards Manager...`
2. Tìm "esp32"
3. Tìm "ESP32" bởi "Espressif Systems"
4. Nhấn "Install" (chờ vài phút)
5. Hoàn thành!

## Bước 3: Cài Thư Viện Cần Thiết

Vào `Tools → Manage Libraries...` và cài:

### 1. OneWire
- Tìm "OneWire" bởi "Paul Stoffregen"
- Nhấn Install

### 2. DallasTemperature
- Tìm "DallasTemperature" bởi "Paul Stoffregen"
- Nhấn Install

### 3. BH1750 (Tùy chọn - nếu dùng cảm biến ánh sáng)
- Tìm "BH1750" bởi "Christopher Laws"
- Nhấn Install

## Bước 4: Chọn Board

1. Vào `Tools → Board`
2. Chọn `ESP32 Arduino → ESP32 Dev Module`

## Bước 5: Cấu Hình Upload

Đảm bảo các cài đặt sau:

```
Board: "ESP32 Dev Module"
Upload Speed: "115200"
CPU Frequency: "240MHz (WiFi/BT)"
Flash Frequency: "80MHz"
Flash Mode: "QIO"
Flash Size: "4MB (32Mb)"
Partition Scheme: "Default 4MB with spiffs (3MB APP/1.5MB SPIFFS)"
Port: [Chọn đúng cổng COM/USB]
```

## Bước 6: Upload Sketch

1. Mở file: `main/main.ino`
2. Vào `Sketch → Upload`
3. Chờ upload hoàn thành (khoảng 30-60 giây)

## Bước 7: Cài ESP32FS Plugin (Để Upload Web UI)

### Download Plugin:
https://github.com/me-no-dev/arduino-esp32fs-plugin/releases/latest

### Cài Đặt:

**Windows:**
```
C:\Users\<tên người dùng>\Documents\Arduino\tools\ESP32FS\tool\
```
Copy file `esp32fs.jar` vào thư mục này

**Mac:**
```
~/Documents/Arduino/tools/ESP32FS/tool/
```

**Linux:**
```
~/.arduino/tools/ESP32FS/tool/
```

### Tạo cấu trúc thư mục nếu chưa có:
- Tạo folder: `tools`
- Trong `tools`, tạo folder: `ESP32FS`
- Trong `ESP32FS`, tạo folder: `tool`
- Copy `esp32fs.jar` vào `tool`

### Khởi Động Lại Arduino IDE

Sau khi cài plugin, khởi động lại Arduino IDE để thấy menu mới:
`Tools → ESP32 LittleFS Data Upload`

## Bước 8: Upload Web UI (LittleFS Data)

1. Đảm bảo có thư mục `data/` với các file HTML:
   - `index.html`
   - `dashboard.html`
   - `config.html`
   - `calibration.html`
   - `manual.html`

2. Vào `Tools → ESP32 LittleFS Data Upload`

3. Đợi upload hoàn thành

4. Nếu thành công: "LittleFS Image Uploaded"

## Bước 9: Kiểm Tra Kết Nối

1. Mở Serial Monitor: `Tools → Serial Monitor`
2. Baud rate: 115200
3. ESP32 sẽ hiển thị:
   ```
   WiFi connected!
   IP address: 192.168.x.x
   Web server started
   ```

## Bước 10: Truy Cập Web UI

1. Ghi lại IP address từ Serial Monitor
2. Mở trình duyệt
3. Truy cập: `http://IP-address`
4. Bạn sẽ thấy trang chủ Farm365!

## 🚨 Troubleshooting

### Lỗi "No such file or directory"
→ Xem [INSTALL_LIBRARIES.md](INSTALL_LIBRARIES.md)

### Lỗi "A fatal error occurred"
→ Kiểm tra cài đặt board ESP32

### Lỗi Upload Failed
→ Nhấn nút BOOT trên ESP32 khi upload

### Serial Monitor Không Hiện Gì
→ Kiểm tra Baud rate = 115200
→ Kiểm tra cáp USB data (không phải chỉ sạc)

### Web UI Không Mở Được
→ Upload lại LittleFS data
→ Kiểm tra ESP32 đã connect WiFi chưa

## ✅ Checklist Cuối Cùng

- [ ] ESP32 Board đã cài
- [ ] Thư viện OneWire + DallasTemperature đã cài
- [ ] Upload sketch thành công
- [ ] Upload LittleFS data thành công
- [ ] Serial Monitor hiện IP address
- [ ] Web UI mở được trên trình duyệt

---

**🎉 Hoàn thành!** Bây giờ bạn có thể bắt đầu sử dụng hệ thống!




