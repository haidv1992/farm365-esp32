# ⚡ UPLOAD CODE ĐÃ FIX - HƯỚNG DẪN NHANH

**Lỗi đã sửa:**
- ✅ Dashboard hiển thị "--" → Đã fix API
- ✅ Relay pH nháy rồi tắt → Đã thêm sensor validation
- ✅ BH1750 crash → Đã comment
- ✅ ZMCT103C không đọc → Đã thêm lại

---

## 📤 UPLOAD NGAY (Chọn 1 trong 2)

### Cách 1: PlatformIO (Khuyến nghị)

```bash
# Vào thư mục project
cd /Users/haidv/IdeaProjects/thuycanhesp32

# Upload code
pio run --target upload

# Upload web UI (nếu cần)
pio run --target uploadfs
```

### Cách 2: Arduino IDE

```
1. Mở file: src/main.cpp
2. Board: ESP32 Dev Module
3. Partition Scheme: Default 4MB with spiffs
4. Upload Speed: 115200
5. Port: Chọn đúng cổng COM
6. Click Upload (Ctrl+U)
```

---

## ✅ SAU KHI UPLOAD

### 1. Reset ESP32
Ấn nút **RESET** trên board

### 2. Mở Serial Monitor (115200 baud)
Quan sát:
```
WiFi connected!
IP address: 192.168.x.x
LittleFS mounted successfully
System started
Web server started
```

### 3. Kiểm Tra Dashboard
Mở: `http://IP-ESP32/dashboard`

**Phải thấy:**
- Nhiệt độ: `25.3 °C` (không phải `--`)
- pH: `6.12` (không phải `--`)
- TDS: `850 ppm` (không phải `--`)
- Current: `0.15 A` ← **MỚI**
- Power: `33.0 W` ← **MỚI**
- Energy: `0.003 kWh` ← **MỚI**

### 4. Test Sensor Validation
**Rút probe pH ra** → Để khô 10 giây

**Quan sát LED:**
- Nháy 3 lần (sensor error) ✅
- Relay pH **KHÔNG nháy** ✅ (đã fix!)

**Serial Monitor:**
```
AUTO DISABLED: Sensors invalid!
```

**Nhúng lại probe** → LED chớp 1/2s (OK) ✅

---

## 🔧 NẾU VẪN LỖI

### Lỗi: "Dashboard vẫn hiển thị --"

**Nguyên nhân:** Cache browser

**Fix:**
```
1. Ctrl+F5 (hard refresh)
2. Hoặc: Ctrl+Shift+Del → Xóa cache
3. Hoặc: Dùng Incognito/Private mode
```

### Lỗi: "Compilation error"

**Nguyên nhân:** Thiếu thư viện

**Fix:**
```bash
# PlatformIO tự động cài
pio lib install

# Arduino IDE
Tools → Manage Libraries → Cài:
- OneWire
- DallasTemperature
- LittleFS
```

### Lỗi: "Upload failed"

**Fix:**
```
1. Giữ nút BOOT trên ESP32
2. Click Upload
3. Khi thấy "Connecting...", nhả nút BOOT
```

---

## 📊 KIỂM TRA HOÀN CHỈNH

- [ ] Dashboard hiển thị đầy đủ số liệu (không có `--`)
- [ ] Relay pH **KHÔNG nháy** khi sensor lỗi
- [ ] LED chớp 1/2s (OK)
- [ ] Manual control vẫn hoạt động
- [ ] Serial Monitor không có error

**NẾU TẤT CẢ ĐÃ OK → HỆ THỐNG ĐÃ FIX XONG!** ✅

---

## 📖 Tham Khảo

- Chi tiết fix: [SENSOR_FIX_REPORT.md](SENSOR_FIX_REPORT.md)
- Hướng dẫn nhanh: [QUICK_START.md](QUICK_START.md)
- Sơ đồ đấu nối: [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md)










