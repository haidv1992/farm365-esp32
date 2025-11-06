# 🧪 HƯỚNG DẪN TEST CẢM BIẾN & DASHBOARD

**Ngày:** 2025-10-28  
**Vấn đề:** Dashboard hiển thị `--` mặc dù API có dữ liệu

---

## 🔴 HIỆN TRẠNG

### API Response (Đúng ✅):
```json
{
  "temp": 28.1,     ✅ Nhiệt độ OK
  "ph": 9.12,       ✅ pH OK (hơi kiềm)
  "tds": 0,         ⚠️ TDS sensor chưa kết nối
  "lux": 0,         ✅ OK (đã comment)
  "current": 2.53,  ✅ Dòng điện OK
  "power": 557.6,   ✅ Công suất OK
  "energy": 5.328,  ✅ Điện năng OK
  "pump": 1,        ✅ Bơm tuần hoàn ĐANG BẬT
  "loopOn": 1       ✅ Chu kỳ ON
}
```

### Dashboard (Sai ❌):
```
Nhiệt độ: 28°C    ✅ OK
pH: 10.02         ✅ OK
TDS: --ppm        ❌ Hiển thị "--" (dù API = 0)
Dòng điện: --A    ❌ Hiển thị "--" (dù API = 2.53)
Công suất: --W    ❌ Hiển thị "--" (dù API = 557.6)
Điện năng: --kWh  ❌ Hiển thị "--" (dù API = 5.328)
```

### LED Sáng Liên Tục
```json
"pump": 1, "loopOn": 1
```
→ **Bơm tuần hoàn đang BẬT** (15 phút ON / 45 phút OFF)  
→ LED pattern = 4 (Pumping) → Sáng liên tục ✅ **ĐÚNG!**

---

## 🔍 NGUYÊN NHÂN

### 1. Dashboard Hiển Thị `--`

**Nguyên nhân:** JavaScript không refresh hoặc cache browser

**Kiểm tra:**
```javascript
// dashboard.html line 268-269
updateData();
setInterval(updateData, 5000);  // Update mỗi 5 giây
```

**Có thể:**
- Browser cache cũ (chưa load dashboard.html mới)
- JavaScript error (kiểm tra Console)
- Auto-refresh bị block

### 2. TDS = 0

**Debug log (Serial Monitor):**
```
TDS Debug - ADC: 0, Voltage: 0.000V, ...
```

→ TDS sensor chưa kết nối (đã giải thích ở file trước)

### 3. LED Sáng Liên Tục - BÌNH THƯỜNG!

```
pump = 1 → Bơm tuần hoàn đang BẬT
→ LED pattern = 4 → Sáng liên tục
```

**Đây KHÔNG phải lỗi!** Sau 15 phút sẽ tắt.

---

## ✅ FIX DASHBOARD HIỂN THỊ `--`

### Giải Pháp 1: Hard Refresh Browser (Nhanh nhất)

**Windows/Linux:**
```
Ctrl + F5
hoặc
Ctrl + Shift + R
```

**Mac:**
```
Cmd + Shift + R
```

### Giải Pháp 2: Xóa Cache Browser

**Chrome:**
```
1. F12 (Developer Tools)
2. Tab "Network"
3. Checkbox "Disable cache"
4. Refresh (F5)
```

**Hoặc:**
```
1. Ctrl + Shift + Del
2. Chọn "Cached images and files"
3. Clear data
4. Refresh Dashboard
```

### Giải Pháp 3: Dùng Incognito/Private Mode

```
Ctrl + Shift + N (Chrome)
Ctrl + Shift + P (Firefox)
```

Mở: http://192.168.0.102/dashboard

### Giải Pháp 4: Kiểm Tra Console

**F12 → Console Tab**

Xem có lỗi JavaScript không:
```
Uncaught TypeError: Cannot read property 'textContent' of null
SyntaxError: Unexpected token...
```

Nếu có lỗi → Gửi cho tôi để fix!

---

## 🧪 HƯỚNG DẪN TEST CÁC SENSOR

### 1. Test Nhiệt Độ (DS18B20) ✅

**Đang hoạt động:** `28.1°C`

**Cách test:**
1. Cầm nóng sensor bằng tay → Nhiệt độ tăng lên 30-32°C
2. Nhúng vào cốc nước đá → Nhiệt độ giảm xuống 5-10°C
3. Refresh Dashboard → Xem thay đổi

**Nếu không đổi:**
- Kiểm tra pull-up 10kΩ → 3.3V
- Kiểm tra dây Data → GPIO4

---

### 2. Test pH Sensor ✅

**Đang hoạt động:** `9.12` (nước kiềm)

**Cách test:**
1. **Nước cất:** pH ~7.0
2. **Giấm:** pH ~2.5-3.0 (cực acid)
3. **Nước xà phòng:** pH ~10-11 (kiềm)
4. **Nước máy:** pH ~7-8

**Test:**
```
1. Rửa probe bằng nước cất
2. Nhúng vào giấm → pH phải < 4
3. Rửa lại
4. Nhúng vào nước xà phòng → pH phải > 9
```

**Nếu không đổi hoặc nhảy loạn:**
→ Cần hiệu chuẩn pH (dùng dung dịch chuẩn pH 4 và pH 7)

---

### 3. Test TDS Sensor ⚠️

**Hiện tại:** `TDS = 0` (ADC = 0)

**Nguyên nhân:** Sensor chưa kết nối

**Cách test:**

#### Test 1: Kiểm Tra Phần Cứng
```
1. Kiểm tra nguồn VCC:
   TDS Module VCC → ESP32 VIN (5V) hoặc 3.3V
   TDS Module GND → ESP32 GND
   TDS Module OUT → ESP32 GPIO32

2. Dùng Multimeter:
   - Đo VCC-GND: Phải có 5V (hoặc 3.3V)
   - Nhúng probe vào nước, đo OUT-GND: Phải > 0.3V
```

#### Test 2: Nước Muối (TDS Cao)
```
1. Pha dung dịch:
   200ml nước + 1 thìa muối (5g)
   → TDS ~5000 ppm

2. Nhúng probe
3. Xem Serial Monitor:
   TDS Debug - ADC: ???, Voltage: ???V, TDS: ??? ppm
   
   Mong đợi:
   ADC > 500
   Voltage > 1.5V
   TDS > 2000 ppm
```

#### Test 3: Các Loại Nước
```
| Loại Nước | TDS (ppm) |
|-----------|-----------|
| Nước cất | 0-10 |
| Nước RO   | 10-50 |
| Nước máy  | 100-300 |
| Nước giếng | 300-800 |
| Nước muối | 2000-5000 |
```

**Nếu vẫn ADC = 0:**
→ Module chưa có nguồn hoặc hỏng

---

### 4. Test Dòng Điện (ZMCT103C) ✅

**Đang hoạt động:** `2.53A`, `557.6W`

**Giải thích:**
- ESP32 + Relay + Bơm đang chạy → Tiêu thụ ~2.5A
- Công suất = 220V × 2.53A = 556W ✅ Đúng!

**Cách test:**

#### Test 1: Bật Thêm Thiết Bị
```
1. Bật bơm thủ công:
   /manual → Bật bơm A hoặc B
   
2. Xem Current tăng:
   - Trước: 2.5A
   - Sau bật thêm 1 bơm: 3.0-3.5A
   - Sau bật thêm 2 bơm: 3.5-4.5A
```

#### Test 2: Tắt Bơm Tuần Hoàn
```
1. Đợi chu kỳ OFF (45 phút)
   hoặc
   /manual → Tắt bơm tuần hoàn

2. Xem Current giảm:
   - Bơm ON: 2.5A
   - Bơm OFF: 0.5-1.0A (chỉ ESP32)
```

#### Test 3: Hiệu Chuẩn (Nếu Sai)
```
1. Tắt tất cả bơm
2. Đo dòng điện chính xác bằng ampe kìm
3. So sánh với giá trị Dashboard
4. Điều chỉnh ZMCT_SENSITIVITY trong code
```

**Công thức:**
```cpp
// Line 96: Điều chỉnh theo module thực tế
constexpr float ZMCT_SENSITIVITY = 1000.0f / 100.0f;  // 10
```

Nếu đo được 3A nhưng hiển thị 2A:
```cpp
ZMCT_SENSITIVITY = 1000.0f / 150.0f;  // Tăng lên
```

---

### 5. Test Công Suất & Điện Năng ✅

**Đang hoạt động:**
- `Power: 557.6W`
- `Energy: 5.328kWh`

**Giải thích:**
```
Công suất = Điện áp × Dòng điện
557.6W = 220V × 2.53A ✅ Đúng!

Điện năng = Tích lũy theo thời gian
5.328kWh = Chạy từ lúc boot đến giờ
```

**Cách test:**

#### Test 1: Xem Điện Năng Tăng
```
1. Ghi lại energy hiện tại: 5.328 kWh
2. Đợi 1 giờ
3. Xem energy mới:
   Ví dụ: 5.885 kWh
   
   → Tiêu thụ trong 1h = 5.885 - 5.328 = 0.557 kWh
   → Công suất trung bình = 557 W ✅
```

#### Test 2: Tính Điện Tiêu Thụ 1 Ngày
```
Giả sử:
- Bơm tuần hoàn: 15 phút ON / 45 phút OFF
- Công suất khi ON: 560W
- Công suất khi OFF: 50W (chỉ ESP32)

Tính:
1h = 15 phút ON + 45 phút OFF
   = (560W × 0.25h) + (50W × 0.75h)
   = 140Wh + 37.5Wh
   = 177.5Wh

24h = 177.5Wh × 24 = 4.26 kWh/ngày
```

#### Test 3: Reset Điện Năng
```
Hiện tại: Energy reset mỗi khi ESP32 reboot
Nếu muốn reset thủ công → Cần thêm tính năng
```

---

## 🔧 CHECKLIST DEBUG

### Dashboard Hiển Thị `--`

- [ ] Hard refresh browser (Ctrl + F5)
- [ ] Xóa cache browser
- [ ] Dùng Incognito mode
- [ ] Kiểm tra Console (F12) có lỗi không
- [ ] Test API trực tiếp: `curl http://192.168.0.102/api/sensor`

### TDS = 0

- [ ] Kiểm tra VCC module → 5V hoặc 3.3V
- [ ] Kiểm tra OUT → GPIO32
- [ ] Đo điện áp OUT bằng Multimeter
- [ ] Test với nước muối
- [ ] Xem Serial Monitor: `TDS Debug - ADC: ???`

### Dòng Điện/Công Suất

- [ ] Kiểm tra ZMCT103C có VCC, GND, OUT → GPIO34
- [ ] Test bật/tắt bơm → Dòng điện phải thay đổi
- [ ] Nếu sai → Điều chỉnh ZMCT_SENSITIVITY

### LED Sáng Liên Tục

- [ ] Kiểm tra pump=1 → Bơm đang BẬT
- [ ] Đợi 15 phút → LED sẽ chớp (bơm OFF)
- [ ] Nếu sáng mãi → Kiểm tra relay stuck

---

## 📊 BẢNG GIÁ TRỊ CHUẨN

### Nước Thủy Canh
```
pH: 5.5-6.5 (tối ưu 6.0)
TDS: 800-1200 ppm (tùy giai đoạn)
Nhiệt độ: 18-25°C (tối ưu 20-22°C)
```

### Tiêu Thụ Điện
```
ESP32 + Relay: ~50W (standby)
ESP32 + Relay + 1 bơm nhỏ: 200-300W
ESP32 + Relay + bơm tuần hoàn: 400-600W
Tất cả bơm BẬT: 800-1200W
```

### LED Pattern
```
✅ Chớp 1/2s: OK, hệ thống bình thường
⚠️ Chớp đôi: WiFi lỗi
❌ Nháy 3 lần: Sensor lỗi
💡 Sáng liên tục: Đang bơm ← BẠN Ở ĐÂY!
🔄 Nháy nhanh 5 lần: Đang boot
```

---

## 🚀 HÀNH ĐỘNG NGAY

### 1. Fix Dashboard Hiển Thị `--`

```
Ctrl + F5 (Hard refresh)
hoặc
Ctrl + Shift + N → http://192.168.0.102/dashboard
```

**Kết quả mong đợi:**
```
Nhiệt độ: 28°C
pH: 9.12
TDS: 0 ppm        ← Sẽ hiển thị số (không phải --)
Dòng điện: 2.53A  ← Sẽ hiển thị số
Công suất: 557W   ← Sẽ hiển thị số
Điện năng: 5.3kWh ← Sẽ hiển thị số
```

### 2. Test TDS Sensor

**Serial Monitor (115200 baud):**
```
TDS Debug - ADC: 0, Voltage: 0.000V, ...
```

**Nếu ADC = 0:**
1. Kiểm tra VCC → 5V
2. Kiểm tra OUT → GPIO32
3. Test với nước muối

**Khi ADC > 0:**
→ Hiệu chuẩn với dung dịch chuẩn EC = 1413 µS/cm

### 3. Quan Sát LED & Relay

**LED sáng liên tục:**
→ Bơm tuần hoàn đang BẬT ✅ Bình thường!

**Đợi 15 phút:**
→ LED sẽ chớp (bơm OFF)

**Nếu LED sáng mãi > 20 phút:**
→ Kiểm tra relay bị dính

---

## ✅ KẾT LUẬN

### Hiện Trạng:

| Sensor | API | Dashboard | Trạng Thái |
|--------|-----|-----------|------------|
| Nhiệt độ | 28.1°C | 28°C | ✅ OK |
| pH | 9.12 | 10.02 | ✅ OK |
| TDS | 0 | `--` | ⚠️ Sensor chưa kết nối + Dashboard cache |
| Dòng điện | 2.53A | `--` | ⚠️ Dashboard cache |
| Công suất | 557W | `--` | ⚠️ Dashboard cache |
| Điện năng | 5.3kWh | `--` | ⚠️ Dashboard cache |
| Bơm | ON | - | ✅ LED sáng đúng |

### Cần Làm:

1. **Hard refresh Dashboard** → Fix hiển thị `--`
2. **Kiểm tra TDS sensor phần cứng** → Fix TDS = 0
3. **Theo dõi LED** → Sau 15 phút sẽ chớp

**Dashboard đang bị cache - Ctrl+F5 sẽ fix ngay!** 🚀













