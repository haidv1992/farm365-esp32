# Tóm Tắt Các Cải Tiến

## ✅ Đã Hoàn Thành

### 1. Hysteresis 2 Ngưỡng cho TDS & pH
- **Vấn đề cũ**: Logic 1 ngưỡng gây dao động (chattering) khi giá trị ở quanh ngưỡng
- **Giải pháp**: 
  - TDS: Ngưỡng THẤP (target - hyst) BẬT, Ngưỡng CAO (target + hyst) TẮT
  - pH: Ngưỡng CAO (target + hyst) BẬT, Ngưỡng THẤP (target - hyst) TẮT
  - Vùng giữa: GIỮ NGUYÊN trạng thái
- **File**: `src/main.cpp` (lines 387-465)

### 2. Config Ngày/Đêm cho Bơm Tuần Hoàn
- **Vấn đề cũ**: Lịch bơm cố định, không phù hợp cho ngày/đêm
- **Giải pháp**:
  - `on_min_day`, `off_min_day`, `on_min_night`, `off_min_night`
  - `light_start`, `light_end` để xác định ban ngày/đêm
  - NTP sync tự động (UTC+7)
- **File**: `src/main.cpp` (lines 332-382), `data/config.html` (lines 66-101)

### 3. ZMCT103C True RMS Calculation
- **Vấn đề cũ**: Dùng DC midpoint → đo sai hoặc = 0
- **Giải pháp**:
  - Sample 100 điểm qua 1 cycle AC (400µs/sample = 2.5kHz)
  - Tính RMS = sqrt(Σ(v²) / n)
  - Calibration: Offset (no load) + Sensitivity (known current)
- **File**: `src/main.cpp` (lines 296-308)

### 4. Non-Blocking Dosing
- **Vấn đề cũ**: `delay()` trong `dosePump` block main loop → Web UI đơ
- **Giải pháp**:
  - State machine: `startDosePump()` + `checkDosePump()`
  - `activeDosing` struct lưu trạng thái đang châm
  - Check trong `loop()`, không block
- **File**: `src/main.cpp` (lines 548-580)

### 5. Dashboard Tối Ưu
- **Vấn đề cũ**: Polling 5s, overlapping requests, dashboard lag
- **Giải pháp**:
  - Polling interval: 3 giây
  - Debounce: `isUpdating` flag tránh overlapping
  - `requestAnimationFrame` batch DOM updates
  - Retry mechanism khi lỗi network
- **File**: `data/dashboard.html` (lines 247-346)

### 6. Trạng Thái Chi Tiết
- **Vấn đề cũ**: "⚙️ Sẵn sàng" không rõ ràng
- **Giải pháp**:
  - TDS: ⚠️ Thấp / 🔄 Đang châm / ✅ Ổn định
  - pH: ⚠️ Cao / 🔄 Đang châm / ✅ Ổn định
  - Hiển thị ngưỡng và giá trị hiện tại
  - Tổng liều hôm nay (giây)
- **File**: `data/dashboard.html` (lines 283-331), `src/main.cpp` (lines 873-886)

### 7. Responsive UI
- **Vấn đề cũ**: Header không responsive trên mobile
- **Giải pháp**:
  - `flex-wrap: wrap` cho navigation
  - Media queries cho desktop/mobile
  - Font-size và padding adaptive
- **File**: `data/*.html` (all pages)

### 8. Hướng Dẫn Hiệu Chuẩn
- **Vấn đề cũ**: Không rõ cách hiệu chuẩn, đặc biệt với chiết áp
- **Giải pháp**:
  - File `CALIBRATION_GUIDE.md` với hướng dẫn chi tiết
  - Cập nhật `calibration.html` với notes về chiết áp
  - NVS không reset khi nạp firmware
- **File**: `CALIBRATION_GUIDE.md`, `data/calibration.html` (lines 84-95, 142-151)

### 9. API Improvements
- **Thêm `/api/config`**: Trả về config hiện tại để populate form
- **Cập nhật `/api/sensor`**: Thêm `tdsDosing`, `phDosing`, `doseAToday`, etc.
- **File**: `src/main.cpp` (lines 246-263, 857-890)

---

## ⚠️ Vấn Đề Còn Lại (Đang Fix)

### 1. `handleSensorData` Dùng `loopOn` Thay Vì Actual State
- **Vấn đề**: Manual mode → `loopOn` không update → API trả về sai
- **File**: `src/main.cpp` (line 859)
- **Fix**: Đã có `pumpActualState` nhưng cần verify

### 2. `dosePump` Vẫn Có `delay()` (Legacy Code?)
- **Vấn đề**: Nếu code cũ còn sót, vẫn block
- **File**: Cần kiểm tra lại toàn bộ
- **Fix**: Đảm bảo chỉ dùng `startDosePump()` + `checkDosePump()`

---

## 📊 Performance

- **Build size**: 886KB Flash (67.6%), 46KB RAM (14.1%)
- **Upload time**: ~57s firmware + ~9s filesystem
- **API response**: < 100ms
- **Dashboard update**: 3s interval

---

## 🔧 Cần Test

1. ✅ Hysteresis 2 ngưỡng: TDS từ <450 → 450-550 → >550
2. ✅ Config ngày/đêm: Kiểm tra lịch bơm theo giờ
3. ✅ ZMCT: Đo dòng điện thực tế với tải
4. ⚠️ Non-blocking: Test dashboard khi đang châm (xem có đơ không)
5. ⚠️ Manual mode: Toggle manual → check API `/api/sensor`

---

## 📝 Note

- NVS lưu config persistent → không mất khi nạp firmware
- Daily limits reset mỗi 24h (dựa trên `millis()`)
- NTP sync khi WiFi connect → fallback `millis()` nếu lỗi

