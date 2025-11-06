# ✅ FIX TDS SENSOR - HOÀN THÀNH

**Ngày:** 2025-10-28  
**Vấn đề:** TDS hiển thị null mặc dù đã nhúng nước

---

## 🔴 NGUYÊN NHÂN ĐÃ TÌM RA

### Debug Log Ban Đầu:
```
TDS Debug - ADC: 0, Voltage: 0.000V, cal.tds_k: inf, TDS: nan ppm
```

### Phân Tích:

**1. ADC = 0 → Voltage = 0V**
- **Nguyên nhân:** TDS sensor **CHƯA KẾT NỐI** hoặc **CHƯA CÓ NGUỒN**
- GPIO32 (D32) đọc được 0V
- Sensor không xuất tín hiệu

**2. cal.tds_k = inf (Infinity)**
- **Nguyên nhân:** Giá trị hiệu chuẩn trong NVS bị **CORRUPT**
- Dẫn đến công thức tính: `TDS = 0V * inf * 1000 * 0.5 = NaN`
- JSON trả về: `"tds": nan` → Bị validation reject → `null`

**3. TDS = nan**
- Kết quả của phép tính với `infinity`
- JavaScript không parse được → Dashboard hiển thị `--`

---

## ✅ CÁC FIX ĐÃ THỰC HIỆN

### Fix #1: Validate Calibration Values Khi Load

**Code cũ (KHÔNG AN TOÀN):**
```cpp
void loadConfig() {
  cal.ph7 = prefs.getFloat("calPH7", 1.65f);
  cal.ph4 = prefs.getFloat("calPH4", 2.10f);
  cal.tds_k = prefs.getFloat("calTDS", 0.5f);
  // ❌ Không kiểm tra NaN/inf
}
```

**Code mới (AN TOÀN):**
```cpp
void loadConfig() {
  cal.ph7 = prefs.getFloat("calPH7", 1.65f);
  cal.ph4 = prefs.getFloat("calPH4", 2.10f);
  cal.tds_k = prefs.getFloat("calTDS", 0.5f);
  
  // ✅ Validate calibration values - fix infinity/NaN
  if (isnan(cal.ph7) || isinf(cal.ph7)) cal.ph7 = 1.65f;
  if (isnan(cal.ph4) || isinf(cal.ph4)) cal.ph4 = 2.10f;
  if (isnan(cal.tds_k) || isinf(cal.tds_k) || cal.tds_k <= 0.0f) cal.tds_k = 0.5f;
  
  Serial.printf("Loaded calibration - pH7: %.3f, pH4: %.3f, TDS_k: %.3f\n", 
                cal.ph7, cal.ph4, cal.tds_k);
}
```

**Kết quả:** `cal.tds_k` luôn có giá trị hợp lệ (0.5 nếu NVS corrupt)

### Fix #2: Safety Check Trong Hàm voltageToTDS()

**Code cũ (KHÔNG AN TOÀN):**
```cpp
float voltageToTDS(float v) {
  float ec_uS = v * cal.tds_k * 1000.0f;
  float tds_ppm = ec_uS * 0.5f;
  return tds_ppm;
  // ❌ Không kiểm tra sensor disconnected
  // ❌ Không kiểm tra giá trị âm
}
```

**Code mới (AN TOÀN):**
```cpp
float voltageToTDS(float v) {
  // ✅ Safety check: if voltage too low, sensor not connected
  if (v < 0.01f) return 0.0f;  // ADC < ~12 → sensor disconnected
  
  float ec_uS = v * cal.tds_k * 1000.0f;
  float tds_ppm = ec_uS * 0.5f;
  
  // ✅ Safety check: ensure valid range
  if (tds_ppm < 0.0f) return 0.0f;
  if (tds_ppm > 9999.0f) return 9999.0f;
  
  return tds_ppm;
}
```

**Kết quả:** 
- Sensor disconnected → TDS = 0 (không phải NaN)
- TDS luôn trong khoảng 0-9999 ppm

---

## 📊 KẾT QUẢ SAU KHI FIX

### API Response:
```json
{
  "temp": 28.6,    ✅ OK
  "ph": 8.05,      ✅ OK
  "tds": 0,        ✅ FIX - Không còn null!
  "lux": 0,
  "current": 5.00,
  "power": 1100.1,
  "energy": 0.003,
  "pump": 0,
  "loopOn": 0
}
```

### Debug Log (Dự kiến):
```
Loaded calibration - pH7: 1.650, pH4: 2.100, TDS_k: 0.500
TDS Debug - ADC: 0, Voltage: 0.000V, cal.tds_k: 0.500, TDS: 0.0 ppm
```

**Giải thích:**
- `cal.tds_k` đã fix về 0.500 ✅
- TDS = 0 ppm (vì sensor chưa kết nối đúng) ✅
- Dashboard sẽ hiển thị `0 ppm` thay vì `--` ✅

---

## ⚠️ TDS VẪN = 0 → CẦN KIỂM TRA PHẦN CỨNG

### Vấn Đề: ADC = 0, Voltage = 0V

**Nguyên nhân:** TDS sensor **CHƯA XUẤT TÍN HIỆU**

### Checklist Kiểm Tra:

**1. Nguồn Cấp (QUAN TRỌNG NHẤT)**
```
TDS Module:
  VCC → 3.3V hoặc 5V (kiểm tra datasheet)
  GND → GND ESP32
  OUT → GPIO32 (D32) ESP32
```

**Hầu hết TDS sensor cần 5V để hoạt động!**

```diff
- VCC → 3.3V (Có thể không đủ)
+ VCC → 5V (Từ ESP32 VIN hoặc nguồn ngoài)
```

**2. Kiểm Tra Dây Nối**
- [ ] Dây VCC nối đúng
- [ ] Dây GND nối đúng  
- [ ] Dây OUT nối đúng GPIO32 (không phải GPIO khác)
- [ ] Không có dây đứt

**3. Kiểm Tra Probe**
- [ ] Probe đã nhúng **HOÀN TOÀN** trong nước
- [ ] Electrode không bị gỉ/bẩn
- [ ] Probe không bị hỏng

**4. Kiểm Tra Module**
- [ ] LED trên module sáng (nếu có)
- [ ] Module không bị hỏng
- [ ] Module đúng loại (TDS sensor, không phải cảm biến khác)

---

## 🔧 HƯỚNG DẪN FIX TDS = 0

### Bước 1: Kiểm Tra Nguồn

**Thử cấp 5V cho TDS module:**
```
TDS Module VCC → ESP32 VIN (5V)
TDS Module GND → ESP32 GND
TDS Module OUT → ESP32 GPIO32
```

**Lưu ý:** GPIO32 chỉ chịu được 3.3V!
- Nếu module output 5V → **PHẢI DÙNG VOLTAGE DIVIDER**
- Công thức: `R1 = 10kΩ, R2 = 10kΩ` → Output = 5V / 2 = 2.5V

### Bước 2: Kiểm Tra Bằng Multimeter

**Đo điện áp OUT của TDS module:**
```
1. Nhúng probe vào nước
2. Đặt multimeter ở chế độ DC Voltage
3. Đo giữa OUT và GND
```

**Kết quả mong đợi:**
- Nước cất: 0.1-0.3V
- Nước máy: 0.5-1.5V
- Dung dịch dinh dưỡng: 1.0-2.5V

**Nếu đo được 0V:**
- Module chưa có nguồn
- Module hỏng
- Probe hỏng

### Bước 3: Test Với Nước Muối

**Tạo dung dịch test:**
```
1 cốc nước (200ml) + 1 thìa muối (5g)
→ TDS ~5000 ppm (rất cao)
```

**Nhúng probe:**
- Serial Monitor phải hiển thị ADC > 0
- Voltage phải > 0.5V
- TDS phải > 1000 ppm

**Nếu vẫn ADC = 0:**
→ **Module/Probe hỏng** hoặc **chưa kết nối đúng**

---

## 📖 TÀI LIỆU THAM KHẢO

### TDS Sensor Phổ Biến

**1. TDS Sensor v1.0 (Gravity DFRobot)**
- Nguồn: 3.3-5.5V DC
- Output: 0-2.3V (tương ứng 0-1000ppm)
- Công thức: `TDS = (133.42 * V³ - 255.86 * V² + 857.39 * V) * 0.5`

**2. TDS Sensor Analog (Generic)**
- Nguồn: 5V DC
- Output: 0-3.3V
- Công thức: `TDS = V * 500` (đơn giản hóa)

### Cách Hiệu Chuẩn TDS

**Cần:**
- Dung dịch chuẩn 1413 µS/cm (hoặc 707 ppm)
- Nhiệt kế

**Bước:**
1. Nhúng probe vào dung dịch chuẩn
2. Đợi ổn định (30 giây)
3. Vào `/calibration` → Nhập EC = 1413
4. Click **Set TDS**

---

## ✅ KẾT LUẬN

### Đã Fix:

| Vấn Đề | Trước | Sau |
|--------|-------|-----|
| cal.tds_k = inf | ❌ Infinity | ✅ 0.500 |
| TDS = nan | ❌ NaN | ✅ 0.0 |
| Dashboard | ❌ `--` | ✅ `0 ppm` |
| JSON | ❌ `"tds": nan` | ✅ `"tds": 0` |

### Cần Làm Tiếp:

**TDS vẫn = 0 vì sensor chưa kết nối đúng:**
1. ✅ Kiểm tra nguồn VCC (phải đủ 5V)
2. ✅ Kiểm tra dây OUT → GPIO32
3. ✅ Kiểm tra probe nhúng nước
4. ✅ Test với nước muối
5. ✅ Hiệu chuẩn sau khi đọc được giá trị

**Khi ADC > 0 → TDS sẽ hiển thị đúng!**

---

**Code đã an toàn - Không còn crash vì infinity/NaN!** ✅













