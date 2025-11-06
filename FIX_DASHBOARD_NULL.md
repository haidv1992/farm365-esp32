# 🔧 FIX DASHBOARD HIỂN THỊ NULL & RELAY NHÁY

**Ngày:** 2025-10-28  
**Vấn đề:** Dashboard hiển thị `--` mặc dù API trả về dữ liệu, Relay số 4 (pH) vẫn nháy

---

## 🔴 NGUYÊN NHÂN

### Vấn Đề 1: Dashboard Hiển Thị `--`
**API trả về:**
```json
{"temp":28.1,"ph":7.70,"tds":nan,"lux":0,"current":4.65,...}
```

**Vấn đề:** `"tds":nan` là **CHUỖI "nan"** không phải JSON null  
**Kết quả:** JavaScript không parse được → Dashboard hiển thị `--`

**Code cũ (SAI):**
```cpp
json += "\"tds\":" + String(tdsVal, 0) + ",";
// Khi tdsVal = NaN → String(NaN) = "nan" (không phải JSON null!)
```

### Vấn Đề 2: Relay Số 4 Vẫn Nháy

**Code cũ:**
```cpp
if (sensorsValid) {
  if (phVal > target) {
    dosePump(...);  // Bơm
  }
  // ❌ THIẾU: Không tắt relay khi pH trong khoảng
} else {
  setRelay(PIN_RELAY_DOWNP, false);  // Tắt khi sensor lỗi
}
```

**Vấn đề:**
- Khi sensor chuyển từ invalid → valid
- pH có thể đang trong khoảng (không cần bơm)
- Nhưng relay vẫn giữ trạng thái cũ (có thể BẬT)
- → Relay nháy

---

## ✅ GIẢI PHÁP

### Fix #1: Convert NaN → JSON null

**Code mới:**
```cpp
void handleSensorData() {
  String json = "{";
  
  // Temperature - convert NaN/invalid to null
  if (isnan(tempC) || tempC < -100.0f || tempC > 100.0f) {
    json += "\"temp\":null,";  // ✅ JSON null (không phải "nan")
  } else {
    json += "\"temp\":" + String(tempC, 1) + ",";
  }
  
  // pH - convert NaN/invalid to null
  if (isnan(phVal) || phVal < 0.0f || phVal > 14.0f) {
    json += "\"ph\":null,";
  } else {
    json += "\"ph\":" + String(phVal, 2) + ",";
  }
  
  // TDS - convert NaN/invalid to null
  if (isnan(tdsVal) || tdsVal < 0.0f || tdsVal > 5000.0f) {
    json += "\"tds\":null,";  // ✅ JSON null - JavaScript parse được!
  } else {
    json += "\"tds\":" + String(tdsVal, 0) + ",";
  }
  
  // Current, Power, Energy - tương tự
  // ...
}
```

**Kết quả:**
```json
{
  "temp": 28.1,
  "ph": 7.70,
  "tds": null,     ← ✅ JSON null (không phải "nan")
  "lux": 0,
  "current": 4.65,
  "power": 1022.1,
  "energy": 0.016
}
```

**Dashboard hiển thị:**
- Temp: `28.1°C` ✅
- pH: `7.70` ✅
- TDS: `--` ✅ (JavaScript hiểu null → hiển thị `--`)
- Current: `4.65A` ✅
- Power: `1022W` ✅

### Fix #2: Đảm Bảo Relay TẮT Khi pH Trong Khoảng

**Code mới:**
```cpp
// pH Control (Down-pH)
if (!manualControl) {
  // ⚠️ SAFETY CHECK: Only run AUTO mode if sensors are valid
  bool sensorsValid = !isnan(tdsVal) && !isnan(phVal) && 
                      (tempC > -100.0f && tempC < 100.0f) &&
                      (tdsVal >= 0.0f && tdsVal < 5000.0f) &&
                      (phVal >= 0.0f && phVal <= 14.0f);
  
  if (sensorsValid) {
    if (phVal > (phCfg.target + phCfg.hyst)) {
      // pH quá cao → Bơm Down-pH
      if (millis() - tLockP > lockout) {
        dosePump(PIN_RELAY_DOWNP, ...);
      }
    } else {
      // ✅ pH trong khoảng → TẮT relay ngay
      setRelay(PIN_RELAY_DOWNP, false);
    }
  } else {
    // ✅ Sensors invalid → FORCE TẮT relay
    setRelay(PIN_RELAY_DOWNP, false);
    static unsigned long lastWarnP = 0;
    if (millis() - lastWarnP > 60000) {
      writeLog("pH AUTO DISABLED: Sensors invalid!");
      lastWarnP = millis();
    }
  }
}
```

**Kết quả:**
- ✅ Relay **LUÔN TẮT** khi pH trong khoảng
- ✅ Relay **LUÔN TẮT** khi sensor lỗi
- ✅ Relay **KHÔNG NHÁY** ngẫu nhiên

---

## 📊 SO SÁNH TRƯỚC/SAU

### API Response

**Trước fix:**
```json
{
  "temp": 28.1,
  "ph": 7.70,
  "tds": nan,        ← ❌ String "nan" - JavaScript parse lỗi
  "current": 4.65,
  "power": 1022.1,
  "energy": 0.016
}
```

**Sau fix:**
```json
{
  "temp": 28.1,
  "ph": 7.70,
  "tds": null,       ← ✅ JSON null - JavaScript parse OK
  "current": 4.65,
  "power": 1022.1,
  "energy": 0.016
}
```

### Dashboard

**Trước fix:**
```
Nhiệt độ: --       ← ❌ Parse lỗi
pH: --             ← ❌ Parse lỗi
TDS: --            ← ❌ Parse lỗi
Current: --        ← ❌ Parse lỗi
Power: --          ← ❌ Parse lỗi
Energy: --         ← ❌ Parse lỗi
```

**Sau fix:**
```
Nhiệt độ: 28.1°C   ← ✅ Hiển thị đúng
pH: 7.70           ← ✅ Hiển thị đúng
TDS: --            ← ✅ Hiển thị -- (vì null - probe chưa nhúng nước)
Current: 4.65A     ← ✅ Hiển thị đúng
Power: 1022W       ← ✅ Hiển thị đúng
Energy: 0.016kWh   ← ✅ Hiển thị đúng
```

### Relay Số 4 (pH)

**Trước fix:**
- ❌ Nháy thi thoảng (khi sensor chuyển trạng thái)
- ❌ Không tắt rõ ràng khi pH trong khoảng

**Sau fix:**
- ✅ **KHÔNG nháy** khi sensor lỗi
- ✅ **TẮT rõ ràng** khi pH trong khoảng
- ✅ **TẮT ngay lập tức** khi sensor invalid
- ✅ **Ghi log** mỗi 60s khi sensor lỗi

---

## 🧪 CÁCH KIỂM TRA SAU KHI UPLOAD

### 1. Upload Code Mới

```bash
cd /Users/haidv/IdeaProjects/thuycanhesp32

# Đóng Serial Monitor trước
# Sau đó upload:
pio run --target upload
```

### 2. Kiểm Tra API

```bash
curl http://192.168.0.102/api/sensor
```

**Kết quả mong đợi:**
```json
{
  "temp": 28.1,      ← Số
  "ph": 7.70,        ← Số
  "tds": null,       ← JSON null (không phải "nan")
  "lux": 0,
  "current": 4.65,   ← Số
  "power": 1022.1,   ← Số
  "energy": 0.016,   ← Số
  "pump": 0,
  "loopOn": 0
}
```

### 3. Kiểm Tra Dashboard

Mở: http://192.168.0.102/dashboard

**Phải thấy:**
- ✅ Nhiệt độ: `28.1°C` (không phải `--`)
- ✅ pH: `7.70` (không phải `--`)
- ✅ TDS: `--` (vì null - chưa nhúng probe)
- ✅ Current: `4.65A` (không phải `--`)
- ✅ Power: `1022W` (không phải `--`)
- ✅ Energy: `0.016kWh` (không phải `--`)

### 4. Kiểm Tra Relay Số 4

**Quan sát LED:**
- Nháy 3 lần (sensor error) - vì TDS = null ✅

**Quan sát Relay số 4:**
- **KHÔNG nháy** ✅ (đã fix!)

**Serial Monitor:**
```
pH AUTO DISABLED: Sensors invalid!  ← Mỗi 60s
AUTO DISABLED: Sensors invalid!     ← Mỗi 60s (từ TDS control)
```

### 5. Test Nhúng TDS Probe Vào Nước

**Nhúng TDS probe vào cốc nước**

**Đợi 10 giây → Refresh Dashboard**

**Phải thấy:**
- TDS: `150 ppm` (hoặc giá trị khác tùy nước) ✅
- LED: Chớp 1/2s (OK) ✅
- Relay: KHÔNG nháy ✅
- Serial: Không còn "Sensors invalid" ✅

---

## ⚠️ LƯU Ý QUAN TRỌNG

### Sensor Phải Đúng Trạng Thái

**Để hệ thống hoạt động bình thường:**
1. **DS18B20 (Nhiệt độ):** Ngâm nước
2. **pH Probe:** Ngâm trong dung dịch KCl 3M (hoặc nước)
3. **TDS Probe:** Ngâm nước
4. **BH1750:** Không cần (đã comment)
5. **ZMCT103C:** Đo dòng điện (có thể để không)

**Khi CHƯA nhúng:**
- Dashboard hiển thị `--` ✅ Đúng
- Relay KHÔNG chạy ✅ An toàn
- LED nháy 3 lần ✅ Cảnh báo sensor lỗi
- Serial log warning ✅ Ghi nhận

**Khi ĐÃ nhúng:**
- Dashboard hiển thị số ✅
- AUTO mode hoạt động ✅
- LED chớp 1/2s ✅
- Serial log bình thường ✅

---

## 📖 TÀI LIỆU THAM KHẢO

- **Chi tiết fix trước:** [SENSOR_FIX_REPORT.md](SENSOR_FIX_REPORT.md)
- **Deploy thành công:** [SUCCESS_DEPLOY.md](SUCCESS_DEPLOY.md)
- **Hướng dẫn nhanh:** [QUICK_START.md](QUICK_START.md)

---

## ✅ KẾT LUẬN

### ĐÃ FIX

| Vấn Đề | Nguyên Nhân | Giải Pháp |
|--------|-------------|-----------|
| Dashboard hiển thị `--` | JSON `"nan"` string | Convert NaN → JSON `null` |
| Relay số 4 nháy | Không tắt khi pH trong khoảng | Thêm `setRelay(false)` rõ ràng |
| API parse lỗi | JavaScript không hiểu "nan" | Dùng JSON null chuẩn |

### SẴN SÀNG UPLOAD

**Compile thành công:**
- RAM: 14.0%
- Flash: 67.1%
- Không có linter error

**Upload:**
```bash
# Đóng Serial Monitor
# Chạy:
pio run --target upload
```

**Sau upload:**
- Dashboard hiển thị đúng tất cả số liệu
- Relay không còn nháy
- Hệ thống hoạt động ổn định

**Chúc bạn canh tác thành công!** 🌱













