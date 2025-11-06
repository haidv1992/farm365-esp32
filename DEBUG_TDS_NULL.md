# 🔍 DEBUG TDS VẪN NULL

**Hiện trạng:** Đã nhúng TDS probe vào nước nhưng vẫn hiển thị null

---

## 🔴 NGUYÊN NHÂN KHẢ DỊ

### 1. **cal.tds_k = 0.5 (Giá trị mặc định - CÓ THỂ SAI)**

Xem code line 74:
```cpp
struct Calib { 
  float ph7 = 1.65f; 
  float ph4 = 2.10f; 
  float tds_k = 0.5f;  ← Giá trị mặc định
}
```

Hàm tính TDS (line 435-439):
```cpp
float voltageToTDS(float v) {
  float ec_uS = v * cal.tds_k * 1000.0f;  // v * 0.5 * 1000
  float tds_ppm = ec_uS * 0.5f;           // * 0.5
  return tds_ppm;
}
```

**Ví dụ:**
- TDS probe đọc được: `vTDS = 1.5V`
- Tính toán: `ec_uS = 1.5 * 0.5 * 1000 = 750 µS/cm`
- Tính toán: `tds_ppm = 750 * 0.5 = 375 ppm`

**Nếu sensor thực tế khác:**
- Một số TDS sensor output 0-2.3V cho 0-1000ppm
- `cal.tds_k` cần điều chỉnh theo sensor cụ thể

### 2. **TDS Probe Chưa Kết Nối Đúng**

Kiểm tra:
- VCC → 3.3V hoặc 5V (tùy module)
- GND → GND
- OUT → GPIO32

### 3. **TDS Probe Hỏng hoặc Cần Bảo Dưỡng**

- Electrode bị bẩn
- Dây tín hiệu đứt
- Module hỏng

### 4. **Validation Quá Chặt**

Code validation (line 682-686):
```cpp
if (isnan(tdsVal) || tdsVal < 0.0f || tdsVal > 5000.0f) {
  json += "\"tds\":null,";
} else {
  json += "\"tds\":" + String(tdsVal, 0) + ",";
}
```

Nếu `tdsVal` âm hoặc > 5000 → null

---

## ✅ GIẢI PHÁP

### Bước 1: Upload Code Debug

Code đã thêm debug log (line 259-265):
```cpp
// Debug TDS (print every 5 seconds)
static unsigned long lastDebug = 0;
if (millis() - lastDebug > 5000) {
  Serial.printf("TDS Debug - ADC: %d, Voltage: %.3fV, cal.tds_k: %.3f, TDS: %.1f ppm\n", 
                adcTDS, vTDS, cal.tds_k, tdsVal);
  lastDebug = millis();
}
```

**Upload:**
```bash
# Đóng Serial Monitor
cd /Users/haidv/IdeaProjects/thuycanhesp32
pio run --target upload
```

### Bước 2: Mở Serial Monitor (115200 baud)

Sau khi upload, mở Serial Monitor và quan sát:

**Kết quả mong đợi:**
```
TDS Debug - ADC: 1234, Voltage: 0.995V, cal.tds_k: 0.500, TDS: 247.5 ppm
TDS Debug - ADC: 1250, Voltage: 1.008V, cal.tds_k: 0.500, TDS: 252.0 ppm
```

**Phân tích các trường hợp:**

#### Trường Hợp 1: ADC = 0 hoặc rất thấp
```
TDS Debug - ADC: 0, Voltage: 0.000V, cal.tds_k: 0.500, TDS: 0.0 ppm
```
→ **Sensor không kết nối** hoặc **chưa cấp nguồn**

**Fix:**
- Kiểm tra VCC → 3.3V hoặc 5V
- Kiểm tra dây OUT → GPIO32
- Kiểm tra GND

#### Trường Hợp 2: ADC = 4095 (max)
```
TDS Debug - ADC: 4095, Voltage: 3.300V, cal.tds_k: 0.500, TDS: 1650.0 ppm
```
→ **Sensor ngắn mạch** hoặc **cấp nguồn sai**

**Fix:**
- Kiểm tra nguồn (phải đúng 3.3V hoặc 5V tùy module)
- Kiểm tra không bị ngắn mạch

#### Trường Hợp 3: TDS = số âm
```
TDS Debug - ADC: 500, Voltage: 0.403V, cal.tds_k: 0.500, TDS: -50.0 ppm
```
→ **Công thức tính sai** hoặc **cal.tds_k sai**

**Fix:** Điều chỉnh `cal.tds_k` hoặc công thức

#### Trường Hợp 4: TDS > 5000
```
TDS Debug - ADC: 3000, Voltage: 2.418V, cal.tds_k: 2.000, TDS: 6045.0 ppm
```
→ **cal.tds_k quá cao** hoặc **validation quá chặt**

**Fix:** Tăng giới hạn validation hoặc giảm `cal.tds_k`

---

## 🔧 FIX NGAY (Không Cần Hiệu Chuẩn)

Nếu bạn chưa hiệu chuẩn TDS, công thức mặc định có thể sai.

### Fix Tạm Thời: Loại Bỏ Validation Âm

**Thay đổi validation (line 682):**

**Cũ:**
```cpp
if (isnan(tdsVal) || tdsVal < 0.0f || tdsVal > 5000.0f) {
```

**Mới:**
```cpp
if (isnan(tdsVal) || tdsVal > 10000.0f) {  // Cho phép cả số âm tạm thời
```

**Lý do:** Nếu công thức tính sai, TDS có thể âm. Tạm chấp nhận để debug.

### Fix Đúng: Sửa Công Thức TDS

Hầu hết TDS sensor dùng công thức:

**TDS Sensor v1.0 (phổ biến):**
```cpp
float voltageToTDS(float v) {
  // Sensor output: 0-2.3V for 0-1000ppm (typical)
  // Formula: TDS = voltage * 133 * temperature_compensation
  float tempCoef = 1.0f + 0.02f * (tempC - 25.0f);  // Temperature compensation
  float tds_ppm = (v * 133.0f) / tempCoef;
  return tds_ppm;
}
```

**TDS Sensor Gravity (DFRobot):**
```cpp
float voltageToTDS(float v) {
  float compensationCoefficient = 1.0 + 0.02 * (tempC - 25.0);
  float compensationVolatge = v / compensationCoefficient;
  float tds_ppm = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge 
                 - 255.86 * compensationVolatge * compensationVolatge 
                 + 857.39 * compensationVolatge) * 0.5;
  return tds_ppm;
}
```

---

## 📊 BẢNG THAM KHẢO TDS

| Nước | TDS (ppm) | EC (µS/cm) |
|------|-----------|------------|
| Nước cất | 0-10 | 0-20 |
| Nước uống RO | 10-50 | 20-100 |
| Nước máy | 50-300 | 100-600 |
| Nước giếng | 300-800 | 600-1600 |
| Dung dịch dinh dưỡng thủy canh | 500-1500 | 1000-3000 |

**Nước thường (nước máy):** 150-250 ppm

---

## 🚀 HÀNH ĐỘNG NGAY

### 1. Upload Code Debug (Đã Compile OK)

```bash
# Đóng Serial Monitor
cd /Users/haidv/IdeaProjects/thuycanhesp32
pio run --target upload
```

### 2. Mở Serial Monitor (115200 baud)

Quan sát output:
```
TDS Debug - ADC: ???, Voltage: ???V, cal.tds_k: ???, TDS: ??? ppm
```

### 3. Gửi Kết Quả Cho Tôi

Ví dụ:
```
TDS Debug - ADC: 0, Voltage: 0.000V, cal.tds_k: 0.500, TDS: 0.0 ppm
```

→ Tôi sẽ phân tích và fix chính xác!

---

## 📖 CHECKLIST KIỂM TRA TDS SENSOR

**Phần cứng:**
- [ ] VCC sensor → 3.3V hoặc 5V (kiểm tra datasheet)
- [ ] GND sensor → GND ESP32
- [ ] OUT sensor → GPIO32 ESP32
- [ ] Probe đã nhúng hoàn toàn trong nước
- [ ] Electrode sạch (không bị gỉ/bẩn)

**Phần mềm:**
- [ ] Code debug đã upload
- [ ] Serial Monitor mở (115200 baud)
- [ ] Thấy dòng "TDS Debug..." mỗi 5 giây

**Nếu thấy debug output → Gửi cho tôi để phân tích!**

---

**Compile đã thành công - Đóng Serial Monitor và upload để xem debug log!** 🔍













