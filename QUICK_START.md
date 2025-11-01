# Hướng Dẫn Nhanh - Farm365 ESP32

## 🚀 Bắt Đầu Trong 5 Phút

### 1. Upload Code

```bash
# Mở Arduino IDE
# Board: ESP32 Dev Module
# Partition Scheme: Default 4MB with spiffs (3MB APP/1.5MB SPIFFS)
# Upload Speed: 115200
# Cổng COM: Chọn đúng cổng

# Upload sketch
File → Open → main/main.ino
Sketch → Upload
```

### 2. Upload Web UI (LittleFS)

```
# Cài ESP32FS plugin: https://github.com/me-no-dev/arduino-esp32fs-plugin
# Khởi động lại Arduino IDE

# Upload files
Tools → ESP32 LittleFS Data Upload
```

### 3. Kết Nối

- ESP32 sẽ kết nối WiFi: `haiquynh`
- Mở Serial Monitor (115200 baud) để xem IP
- Truy cập: `http://IP-ESP32`

### 4. Hiệu Chuẩn Lần Đầu

#### pH (2 điểm):
1. Vào `/calibration`
2. Nhúng probe vào pH7 → Bấm **Set pH7**
3. Rửa probe
4. Nhúng vào pH4 → Bấm **Set pH4**

#### TDS:
1. Nhúng probe vào dung dịch chuẩn 1413 µS/cm
2. Nhập EC: 1413
3. Bấm **Set TDS**

### 5. Điều Khiển

- **Tự động**: `/` → Mặc định chạy auto
- **Thủ công**: `/manual` → Bật/tắt từng bơm
- **Cấu hình**: `/config` → Đặt mục tiêu pH/TDS

## 📍 Các Trang Chính

| Trang | URL | Chức Năng |
|-------|-----|-----------|
| Trang chủ | `/` | Tổng quan hệ thống |
| Dashboard | `/dashboard` | Dữ liệu real-time |
| Cấu hình | `/config` | Đặt pH/TDS, lịch tuần hoàn |
| Điều khiển | `/manual` | Bật/tắt manual |
| Hiệu chuẩn | `/calibration` | Calibrate cảm biến |

## ⚙️ Cấu Hình Mặc Định

```json
{
  "loop": { "on_min": 15, "off_min": 45 },
  "tds": { "target": 800, "hyst": 50, "dose_ms": 700, "lock_s": 90 },
  "ph": { "target": 6.0, "hyst": 0.3, "dose_ms": 300, "lock_s": 90 }
}
```

## 🎯 Quick Actions

### Đặt pH mục tiêu 6.0:
```
/config → pH Target: 6.0 → Save
```

### Đặt TDS mục tiêu 800 ppm:
```
/config → TDS Target: 800 → Save
```

### Bật bơm thủ công 10 giây:
```
/manual → Switch to Manual → Toggle Pump → Tắt sau 10s
```

### Xem dữ liệu real-time:
```
/dashboard → Auto refresh mỗi 5 giây
```

## 🐛 Fix Nhanh

| Vấn Đề | Giải Pháp Nhanh |
|--------|----------------|
| Web UI không mở | Upload lại LittleFS data |
| pH/TDS đọc sai | Hiệu chuẩn lại |
| LED chớp đôi | WiFi lỗi → Kiểm tra SSID |
| Reset liên tục | Thêm tụ lọc 100µF |
| Bơm không chạy | Kiểm tra relay trigger |

## 📊 LED Status Guide

```
✅ Chớp 1/2s: OK, đang chạy bình thường
⚠️ Chớp đôi: WiFi lỗi
❌ Nháy 3 lần: Sensor lỗi
💡 Sáng liên tục: Đang bơm
🔄 Nháy nhanh 5 lần: Đang boot
```

## 🔗 Tham Khảo

- [README.md](README.md) - Tài liệu đầy đủ
- [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md) - Sơ đồ kết nối

---

**💡 Tip:** Lưu Serial Monitor để debug nhanh khi có lỗi!




