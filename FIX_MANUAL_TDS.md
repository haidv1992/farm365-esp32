# 🔧 FIX MANUAL PUMP A & TDS HIỆU CHUẨN

**Ngày:** 2025-10-28  
**Vấn đề:** 
1. Manual click bơm A không chạy
2. TDS hiệu chuẩn 120 nhưng vẫn hiển thị 0
3. Ẩn Ánh sáng trên Dashboard

---

## ✅ ĐÃ FIX

### 1. Manual Pump A Không Chạy ✅

**Nguyên nhân:** Bug trong `manual.html` - Không gửi parameter `manual=1` khi chuyển mode

**Code cũ (SAI):**
```javascript
// Line 120-124 - Switch to auto
fetch('/manual', {
    method: 'POST',
    body: new FormData(document.createElement('form'))  // ❌ EMPTY - không gửi auto=1
});

// Line 126-130 - Switch to manual
// ❌ KHÔNG GỬI manual=1
```

**Code mới (ĐÚNG):**
```javascript
// Switch to auto
const formData = new FormData();
formData.append('auto', '1');  // ✅ Gửi auto=1
fetch('/manual', { method: 'POST', body: formData });

// Switch to manual
const formData = new FormData();
formData.append('manual', '1');  // ✅ Gửi manual=1
fetch('/manual', { method: 'POST', body: formData });
```

**Kết quả:** Manual mode sẽ hoạt động đúng!

### 2. Ẩn Ánh Sáng Trên Dashboard ✅

**File:** `data/dashboard.html`

**Code cũ:**
```html
<div class="card">
    <div class="card-icon">💡</div>
    <div class="card-title">Ánh Sáng</div>
    <div class="card-value">
        <span id="lux">--</span><span class="card-unit">lux</span>
    </div>
</div>
```

**Code mới:**
```html
<!-- Ánh sáng tạm ẩn -->
<!--
<div class="card">
    <div class="card-icon">💡</div>
    <div class="card-title">Ánh Sáng</div>
    <div class="card-value">
        <span id="lux">--</span><span class="card-unit">lux</span>
    </div>
</div>
-->
```

**Kết quả:** Card Ánh sáng không hiển thị trên Dashboard

---

## ⚠️ VẤN ĐỀ TDS VẪN = 0 DÙ HIỆU CHUẨN

### Hiện Trạng

**API:**
```json
{"tds": 0}
```

**Debug Log (Serial Monitor):**
```
TDS Debug - ADC: 0, Voltage: 0.000V, cal.tds_k: 0.500, TDS: 0.0 ppm
```

### Phân Tích

**1. Bạn Đã Hiệu Chuẩn Đúng Chưa?**

Bạn nói: "Nhúng TDS vào nước và set hiệu chuẩn 120"

**Câu hỏi:** 120 là gì?
- EC = 120 µS/cm? (Rất thấp - nước gần như cất)
- TDS = 120 ppm? (Tương đương EC = 240 µS/cm)

**QUAN TRỌNG:** Trang hiệu chuẩn yêu cầu **EC (µS/cm)**, KHÔNG phải TDS (ppm)!

**Ví dụ:**
- Nước máy thường: EC = 300-600 µS/cm → TDS = 150-300 ppm
- Dung dịch chuẩn: EC = 1413 µS/cm → TDS = 707 ppm

### 2. Hiệu Chuẩn KHÔNG FIX ADC = 0!

**Hiểu sai:**
```
Hiệu chuẩn → TDS sẽ hiển thị giá trị ✗
```

**Đúng:**
```
ADC đọc được giá trị → Hiệu chuẩn → TDS hiển thị chính xác ✓
```

**Hiệu chuẩn CHỈ điều chỉnh công thức tính, KHÔNG sửa sensor disconnected!**

### 3. ADC = 0 → Sensor Chưa Kết Nối

**Debug log cho thấy:**
```
ADC: 0 → Voltage: 0.000V
```

**Nguyên nhân:**
- TDS module **CHƯA CẤP NGUỒN** (VCC)
- Dây OUT **CHƯA NỐI** hoặc nối sai
- TDS module **HỎNG**

---

## 🔧 CÁCH FIX TDS = 0

### Bước 1: Kiểm Tra Kết Nối Phần Cứng

**TDS Module cần 3 dây:**
```
VCC → 5V (hoặc 3.3V tùy module)
GND → GND
OUT → GPIO32 (D32)
```

**QUAN TRỌNG:** Hầu hết TDS sensor cần **5V** để hoạt động!

**Kiểm tra:**
```
1. Dùng Multimeter đo giữa VCC và GND module
   → Phải có 5V (hoặc 3.3V)

2. Nhúng probe vào nước, đo giữa OUT và GND
   → Nước máy: Phải có 0.5-1.5V
   → Nước cất: 0.1-0.3V
   → Nếu 0V → Module chưa có nguồn hoặc hỏng
```

### Bước 2: Test Với Nước Muối

**Tạo dung dịch test:**
```
1 cốc nước (200ml) + 1 thìa muối (5g)
→ TDS ~5000 ppm (rất cao)
```

**Nhúng probe và xem Serial Monitor:**
```
TDS Debug - ADC: ???, Voltage: ???V, ...
```

**Kết quả mong đợi:**
- ADC > 500 (ví dụ: 1500-2000)
- Voltage > 1.5V (ví dụ: 1.8-2.5V)
- TDS > 2000 ppm

**Nếu vẫn ADC = 0:**
→ Module chưa kết nối hoặc hỏng

### Bước 3: Kiểm Tra Module TDS

**Thử cấp nguồn từ ESP32:**
```diff
- TDS VCC → ESP32 3.3V (có thể không đủ)
+ TDS VCC → ESP32 VIN (5V)
  TDS GND → ESP32 GND
  TDS OUT → ESP32 GPIO32
```

**Lưu ý:** Nếu OUT của module xuất 5V:
- **CẤM** nối trực tiếp vào GPIO32 (chỉ chịu 3.3V)
- **PHẢI** dùng voltage divider:
```
OUT ─── [10kΩ] ─── GPIO32
                │
             [10kΩ]
                │
              GND
```

### Bước 4: Sau Khi ADC > 0 → Hiệu Chuẩn

**CHỈ KHI ADC > 0, bạn mới hiệu chuẩn!**

**Chuẩn bị:**
- Dung dịch chuẩn EC = 1413 µS/cm (mua sẵn hoặc pha)

**Các bước:**
1. Nhúng TDS probe vào dung dịch chuẩn
2. Đợi 30 giây cho ổn định
3. Vào `/calibration`
4. Nhập EC = 1413 (KHÔNG phải 120!)
5. Click **Set TDS**
6. Refresh Dashboard → TDS phải hiển thị ~700 ppm

---

## 📊 BẢNG TÀI LIỆU THAM KHẢO

### EC vs TDS

| EC (µS/cm) | TDS (ppm) | Loại Nước |
|------------|-----------|-----------|
| 0-50 | 0-25 | Nước cất |
| 50-200 | 25-100 | Nước uống RO |
| 200-600 | 100-300 | Nước máy |
| 600-1200 | 300-600 | Nước giếng |
| 1413 | 707 | Dung dịch chuẩn |
| 2000-3000 | 1000-1500 | Dinh dưỡng thủy canh |

**Công thức:** `TDS (ppm) = EC (µS/cm) × 0.5`

### Dung Dịch Hiệu Chuẩn

**Mua sẵn (Khuyến nghị):**
- EC 1413 µS/cm (707 ppm) - Phổ biến nhất
- EC 2764 µS/cm (1382 ppm)
- EC 12880 µS/cm (6440 ppm)

**Tự pha (Không chính xác):**
```
1 lít nước cất + 0.5g muối → EC ~1000 µS/cm
```

---

## 🚀 HÀNH ĐỘNG NGAY

### 1. Upload File Web UI Đã Fix

```bash
cd /Users/haidv/IdeaProjects/thuycanhesp32

# Upload LittleFS (dashboard.html, manual.html đã fix)
pio run --target uploadfs
```

**Sau upload:**
- Dashboard không còn hiển thị Ánh sáng ✅
- Manual pump A sẽ chạy được ✅

### 2. Kiểm Tra TDS Sensor

**Mở Serial Monitor (115200 baud) và xem:**
```
TDS Debug - ADC: 0, Voltage: 0.000V, cal.tds_k: 0.500, TDS: 0.0 ppm
```

**Nếu ADC = 0:**
1. Kiểm tra VCC → 5V
2. Kiểm tra dây OUT → GPIO32
3. Đo điện áp OUT bằng Multimeter
4. Test với nước muối

**Khi ADC > 0:**
→ Hiệu chuẩn lại với EC = 1413 µS/cm
→ TDS sẽ hiển thị đúng

### 3. Test Manual Pump A

**Sau khi upload uploadfs:**
1. Vào `/manual`
2. Click **Chế Độ Thủ Công**
3. Click bơm A (⚖️)
4. Relay phải BẬT ✅

---

## ✅ TÓM TẮT

### Đã Fix:
1. ✅ Manual pump A - Fix bug không gửi `manual=1`
2. ✅ Ẩn Ánh sáng - Comment code trong dashboard.html
3. ✅ Giải thích TDS = 0

### Cần Làm:
1. **Upload uploadfs** để áp dụng fix manual.html và dashboard.html
2. **Kiểm tra TDS sensor** - ADC phải > 0 thì mới hiệu chuẩn được
3. **Hiệu chuẩn đúng** - Dùng EC = 1413 µS/cm, không phải 120

### Hiểu Đúng:
```
Hiệu chuẩn KHÔNG sửa sensor disconnected!
Hiệu chuẩn CHỈ điều chỉnh công thức tính!

ADC = 0 → Sensor chưa kết nối → PHẢI fix phần cứng trước!
ADC > 0 → Sensor kết nối OK → Hiệu chuẩn để chính xác!
```

---

**Upload uploadfs ngay để fix manual pump A và ẩn ánh sáng!** 🚀













