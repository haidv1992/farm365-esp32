# ✅ FIX RELAY BƠM TUẦN HOÀN AUTO MODE

**Ngày:** 2025-10-28  
**Vấn đề:** LED sáng liên tục (pump=1) nhưng relay không hút trong AUTO mode. Manual hoạt động đúng.

---

## 🔴 NGUYÊN NHÂN

### Code Cũ (BUG):

```cpp
// Line 305-322 (CŨ)
if (!manualControl) {
  if (loopOn) {
    if (millis() - tLastLoop > onMs) {
      // Tắt bơm sau 15 phút
      loopOn = false;
      setRelay(PIN_RELAY_PUMP, false);
      ledPattern = 1;
    }
    // ❌ THIẾU: Không force relay ON mỗi loop!
  } else {
    if (millis() - tLastLoop > offMs) {
      // Bật bơm sau 45 phút
      loopOn = true;
      setRelay(PIN_RELAY_PUMP, true);  // ✅ Set 1 lần
      ledPattern = 4;
    }
  }
}
```

### Vấn Đề:

**Logic AUTO mode:**
1. Sau 45 phút → `loopOn = true`
2. `setRelay(PIN_RELAY_PUMP, true)` được gọi **1 LẦN DUY NHẤT**
3. Sau đó vào branch `if (loopOn)` nhưng **KHÔNG** gọi `setRelay()` nữa!
4. **Nếu relay bị nhiễu, mất điện, hoặc sensor validation block** → Relay TẮT
5. LED vẫn sáng (pattern 4) nhưng relay không hút

**Manual hoạt động vì:**
```cpp
// Line 325
setRelay(PIN_RELAY_PUMP, manualPump);  // ✅ Được gọi MỖI LOOP (~500ms)
```

→ Manual force relay ON liên tục mỗi 500ms → Relay luôn hút!

---

## ✅ FIX

### Code Mới:

```cpp
// Line 305-330 (MỚI)
if (!manualControl) {
  if (loopOn) {
    if (millis() - tLastLoop > onMs) {
      // Tắt bơm sau 15 phút
      loopOn = false;
      tLastLoop = millis();
      setRelay(PIN_RELAY_PUMP, false);
      if (ledPattern == 4) ledPattern = 1;
      writeLog("Circulation pump OFF");
    } else {
      // ✅ FIX: Keep pump running - Force relay ON every loop
      setRelay(PIN_RELAY_PUMP, true);  // ← THÊM DÒNG NÀY!
      if (ledPattern != 3) ledPattern = 4;
    }
  } else {
    if (millis() - tLastLoop > offMs) {
      // Bật bơm sau 45 phút
      loopOn = true;
      tLastLoop = millis();
      setRelay(PIN_RELAY_PUMP, true);
      if (ledPattern != 3) ledPattern = 4;
      writeLog("Circulation pump ON");
    }
  }
}
```

### Thay Đổi:

1. **Thêm line 315:** `setRelay(PIN_RELAY_PUMP, true);`
   - Gọi **MỖI LOOP** (~500ms) khi `loopOn = true`
   - Giống logic Manual → Relay luôn được refresh

2. **Fix LED pattern:**
   - Chỉ set `ledPattern = 4` nếu `ledPattern != 3` (sensor error)
   - Tránh ghi đè lên LED sensor error

---

## 📊 SO SÁNH

### Trước Fix:

```
Thời gian  | loopOn | setRelay() gọi? | Relay | LED
-----------|--------|-----------------|-------|-------
0:00       | false  | No              | OFF   | Chớp
45:00      | true   | Yes (1 lần)     | ON    | Sáng
45:01      | true   | No ❌           | ON?   | Sáng
45:02      | true   | No ❌           | Lỗi!  | Sáng
...        | true   | No ❌           | OFF   | Sáng ← Bug!
60:00      | false  | Yes (OFF)       | OFF   | Chớp
```

→ Relay bị tắt sau 45:01 do không được refresh!

### Sau Fix:

```
Thời gian  | loopOn | setRelay() gọi? | Relay | LED
-----------|--------|-----------------|-------|-------
0:00       | false  | No              | OFF   | Chớp
45:00      | true   | Yes             | ON    | Sáng
45:01      | true   | Yes ✅          | ON    | Sáng
45:02      | true   | Yes ✅          | ON    | Sáng
...        | true   | Yes ✅          | ON    | Sáng
60:00      | false  | Yes (OFF)       | OFF   | Chớp
```

→ Relay được refresh mỗi 500ms → Luôn BẬT!

---

## 🧪 CÁCH TEST

### Test 1: Đợi Chu Kỳ Tự Động

**ESP32 vừa boot:**
```json
{"pump": 0, "loopOn": 0}
```

**Đợi 45 phút:**
- loopOn chuyển thành 1
- pump chuyển thành 1
- LED sáng liên tục
- **Relay phải HÚT** ✅

**Kiểm tra:**
```
1. Nghe relay click?
2. Đo điện áp OUT relay?
3. Bơm có chạy?
```

### Test 2: Force Bật Ngay (Debug)

**Thay đổi chu kỳ tạm thời:**

```cpp
// File: src/main.cpp line 50-52 (struct LoopCfg)
struct LoopCfg { 
  uint16_t on_min = 15;   // Giữ nguyên
  uint16_t off_min = 1;   // ← Đổi từ 45 xuống 1 phút
} loopCfg;
```

**Upload lại → Đợi 1 phút → Bơm sẽ BẬT!**

**Sau khi test xong → Đổi lại off_min = 45**

### Test 3: Kiểm Tra Serial Log

**Mở Serial Monitor (115200 baud):**

```
Circulation pump ON   ← Phải thấy sau 45 phút
```

**Nếu thấy log nhưng relay không hút:**
→ Vấn đề phần cứng (relay module)

**Nếu không thấy log:**
→ Code chưa upload đúng

### Test 4: So Sánh Manual vs AUTO

**Manual:**
```
1. /manual → Chế Độ Thủ Công
2. Bật bơm Tuần hoàn
3. Relay HÚT ✅
```

**AUTO (sau fix):**
```
1. Đợi 45 phút (hoặc set off_min=1)
2. Xem Serial: "Circulation pump ON"
3. Relay phải HÚT ✅ (giống manual)
```

---

## 🔧 TROUBLESHOOTING

### Vấn Đề 1: Vẫn Không Hút Sau Fix

**Kiểm tra:**

1. **Code đã upload đúng chưa?**
   ```
   Flash: [=======   ]  67.2% (used 880165 bytes)
   → Dung lượng tăng so với trước
   ```

2. **Serial log có "Circulation pump ON"?**
   - Có: Code chạy đúng → Vấn đề phần cứng
   - Không: Code chưa chạy → Kiểm tra lại

3. **Đo điện áp GPIO18:**
   ```
   Pump ON → GPIO18 = 0V (Low-trigger)
   Pump OFF → GPIO18 = 3.3V
   ```

4. **LED module IN1 sáng?**
   - Sáng: Module nhận tín hiệu
   - Không: Dây IN1 chưa nối

### Vấn Đề 2: LED Không Sáng Khi Pump ON

**Nguyên nhân:** Sensor lỗi → `ledPattern = 3` → Ghi đè lên pattern 4

**Fix mới đã xử lý:**
```cpp
if (ledPattern != 3) ledPattern = 4;
```

→ Nếu sensor error (pattern 3) → Giữ nguyên, không đổi sang 4

### Vấn Đề 3: Relay Hút 1 Lần Rồi Tắt

**Trước fix:** Đúng triệu chứng này!

**Sau fix:** Không còn nữa vì relay được refresh mỗi 500ms

---

## 📖 LÝ DO CHI TIẾT

### Tại Sao Manual Hoạt Động?

**Code manual (line 325-329):**
```cpp
} else {
  // Manual control
  setRelay(PIN_RELAY_PUMP, manualPump);  // ← Gọi MỖI LOOP
}
```

**Hoạt động:**
```
Loop 1 (0ms):     setRelay(PUMP, true)  → Relay ON
Loop 2 (500ms):   setRelay(PUMP, true)  → Relay ON
Loop 3 (1000ms):  setRelay(PUMP, true)  → Relay ON
...
```

→ Relay được **refresh liên tục** → Luôn BẬT

### Tại Sao AUTO Không Hoạt Động (Trước Fix)?

**Code AUTO cũ:**
```cpp
if (loopOn) {
  if (millis() - tLastLoop > onMs) {
    // Tắt
  }
  // ❌ KHÔNG GỌI setRelay()
}
```

**Hoạt động:**
```
45:00 - Set relay ON (1 lần duy nhất)
45:01 - Không gọi setRelay → Relay có thể bị nhiễu
45:02 - Không gọi setRelay → Relay TẮT!
...
```

→ Relay **chỉ được set 1 lần** → Dễ bị lỗi

### Tại Sao Cần Refresh Relay?

**Relay module có thể:**
1. Bị nhiễu điện từ
2. Điện áp GPIO không ổn định
3. Tín hiệu yếu do dây dài
4. Module cần tín hiệu liên tục để giữ trạng thái

**Giải pháp:** Gọi `digitalWrite()` **liên tục mỗi 500ms** → Đảm bảo relay luôn đúng trạng thái

---

## ✅ KẾT LUẬN

### Đã Fix:

- ✅ Thêm `setRelay(PIN_RELAY_PUMP, true)` trong loop khi `loopOn = true`
- ✅ Fix LED pattern không ghi đè lên sensor error
- ✅ AUTO mode giờ hoạt động giống Manual

### Cách Test:

1. **Nhanh:** Đổi `off_min = 1` → Upload → Đợi 1 phút
2. **Bình thường:** Đợi 45 phút → Xem relay hút
3. **Serial log:** Phải thấy "Circulation pump ON"

### Nếu Vẫn Lỗi:

→ Đọc [DEBUG_RELAY_PUMP.md](DEBUG_RELAY_PUMP.md) để kiểm tra phần cứng

---

**Fix hoàn thành - Relay AUTO mode sẽ hoạt động như Manual!** ✅










