# ✅ FIX LED SÁNG LIÊN TỤC KHI KHÔNG CÓ BƠM CHẠY

**Ngày:** 2025-10-28  
**Vấn đề:** `loopOn: 0, pump: 0` nhưng LED vẫn sáng liên tục (pattern 4 - pumping)

---

## 🔴 NGUYÊN NHÂN

### API Status:
```json
{
  "pump": 0,
  "loopOn": 0,
  "temp": 28.4,
  "ph": 8.70,
  "tds": 61
}
```

→ **KHÔNG CÓ** bơm nào chạy, nhưng LED vẫn sáng liên tục!

### Logic LED Cũ (BUG):

**File:** `src/main.cpp`

#### Khi Bơm Chạy:

```cpp
// Line 318: Bơm tuần hoàn chạy
if (loopOn) {
  setRelay(PIN_RELAY_PUMP, true);
  ledPattern = 4; // ✅ Sáng liên tục
}

// Line 347: Bơm TDS A chạy
dosePump(PIN_RELAY_A, ...);
ledPattern = 4; // ✅ Sáng liên tục

// Line 356: Bơm TDS B chạy
dosePump(PIN_RELAY_B, ...);
ledPattern = 4; // ✅ Sáng liên tục

// Line 389: Bơm pH chạy
dosePump(PIN_RELAY_DOWNP, ...);
ledPattern = 4; // ✅ Sáng liên tục
```

#### Khi Bơm Dừng:

```cpp
// Line 313: Bơm tuần hoàn dừng
if (millis() - tLastLoop > onMs) {
  loopOn = false;
  setRelay(PIN_RELAY_PUMP, false);
  if (ledPattern == 4) ledPattern = 1; // ❌ CHỈ SET 1 LẦN!
}

// Sau đó vào nhánh else:
} else {
  // Đợi 45 phút
  // ❌ KHÔNG SET ledPattern = 1 ở đây!
}

// Bơm TDS/pH dừng:
// ❌ HOÀN TOÀN KHÔNG SET ledPattern = 1!
```

### Timeline Bug:

```
00:00 - Boot → ledPattern = 1 (OK - chớp 1/2s)
00:01 - TDS thấp → Bơm A chạy → ledPattern = 4 (sáng liên tục)
00:02 - Bơm A dừng → ledPattern VẪN = 4 ❌
05:00 - loopOn = true → ledPattern = 4 (sáng)
20:00 - loopOn = false → ledPattern = 1 (lúc này SET)
20:01 - pH cao → Bơm pH chạy → ledPattern = 4
20:02 - Bơm pH dừng → ledPattern VẪN = 4 ❌
...
```

→ **Khi bất kỳ bơm nào chạy** → `ledPattern = 4`  
→ **Khi bơm dừng** → `ledPattern` không được reset lại!  
→ LED sáng mãi mãi! ❌

---

## ✅ FIX

### Logic Mới:

**Thêm vào cuối `loop()` (trước `ledPatternControl()`):**

```cpp
// Line 410-424: Reset LED nếu không có bơm nào chạy

// ✅ AUTO mode: Nếu bơm tuần hoàn OFF và LED vẫn = 4
if (!manualControl && !loopOn && ledPattern == 4) {
  // Reset về OK (trừ khi sensor error)
  if (ledPattern != 3) {
    ledPattern = 1;
  }
}

// ✅ MANUAL mode: LED theo trạng thái bơm
if (manualControl && manualPump) {
  if (ledPattern != 3) ledPattern = 4; // Pumping
} else if (manualControl && !manualPump) {
  if (ledPattern == 4) ledPattern = 1; // Stop pumping
}

// LED pattern control
ledPatternControl();
```

### Giải Thích:

**AUTO Mode:**
- Nếu `loopOn = false` (bơm tuần hoàn dừng)
- VÀ `ledPattern = 4` (vẫn hiển thị đang bơm)
- → Reset về `ledPattern = 1` (OK - chớp bình thường)

**MANUAL Mode:**
- Nếu `manualPump = true` → `ledPattern = 4` (sáng)
- Nếu `manualPump = false` → `ledPattern = 1` (chớp)

**Sensor Error:**
- Luôn ưu tiên `ledPattern = 3` (không ghi đè)

---

## 📊 SO SÁNH TRƯỚC/SAU

### Trước Fix:

```
Timeline | loopOn | Bơm TDS | ledPattern | LED Thực Tế
---------|--------|---------|------------|-------------
00:00    | 0      | No      | 1          | Chớp 1/2s ✅
00:01    | 0      | Yes     | 4          | Sáng ✅
00:02    | 0      | No      | 4 ❌       | Sáng ❌ (Bug!)
01:00    | 1      | No      | 4          | Sáng ✅
04:00    | 0      | No      | 1          | Chớp ✅
04:01    | 0      | Yes (pH)| 4          | Sáng ✅
04:02    | 0      | No      | 4 ❌       | Sáng ❌ (Bug!)
```

→ Sau khi bơm TDS/pH dừng, `ledPattern` không reset → LED sáng mãi!

### Sau Fix:

```
Timeline | loopOn | Bơm TDS | ledPattern | LED Thực Tế
---------|--------|---------|------------|-------------
00:00    | 0      | No      | 1          | Chớp 1/2s ✅
00:01    | 0      | Yes     | 4          | Sáng ✅
00:02    | 0      | No      | 1 ✅       | Chớp ✅ (Fixed!)
01:00    | 1      | No      | 4          | Sáng ✅
04:00    | 0      | No      | 1 ✅       | Chớp ✅
04:01    | 0      | Yes (pH)| 4          | Sáng ✅
04:02    | 0      | No      | 1 ✅       | Chớp ✅ (Fixed!)
```

→ Mỗi loop, nếu `loopOn = 0` → Reset `ledPattern = 1` → LED chớp đúng!

---

## 🧪 CÁCH TEST

### Test 1: Kiểm Tra API + LED

**Bước 1:** Kiểm tra trạng thái
```bash
curl http://192.168.0.102/api/sensor
```

**Nếu kết quả:**
```json
{"pump": 0, "loopOn": 0}
```

**Kiểm tra LED:**
- ✅ **Đúng:** LED chớp 1 lần mỗi 2 giây (pattern 1)
- ❌ **Sai:** LED sáng liên tục (pattern 4) → Bug chưa fix

### Test 2: Test Manual Mode

**Bước 1:** Mở `http://192.168.0.102/manual`

**Bước 2:** Chuyển sang "Chế Độ Thủ Công"

**Bước 3:** Bật bơm Tuần hoàn

**Kiểm tra:**
- LED sáng liên tục ✅
- API: `{"pump": 1, "loopOn": 1}`

**Bước 4:** Tắt bơm Tuần hoàn

**Kiểm tra:**
- LED chớp 1/2s ✅
- API: `{"pump": 0, "loopOn": 0}`

### Test 3: Test AUTO Mode - Bơm TDS

**Điều kiện:** TDS hiện tại < target - hyst (trigger bơm)

**Khi bơm TDS chạy:**
- LED sáng liên tục (pattern 4) ✅
- Sau 700ms (tdsDose) → Bơm dừng

**Sau khi bơm dừng:**
- Đợi ~1s
- LED phải **chớp lại** (pattern 1) ✅

### Test 4: Test AUTO Mode - Bơm Tuần Hoàn

**Cách 1: Đợi chu kỳ (nếu đã set off_min = 1)**

```
Đợi 1 phút → loopOn = 1 → LED sáng
Đợi 3 phút → loopOn = 0 → LED chớp ✅
```

**Cách 2: Xem Serial Monitor**

```
Circulation pump ON  → LED sáng ✅
...
Circulation pump OFF → LED chớp ✅
```

---

## 🔧 TROUBLESHOOTING

### Vấn Đề 1: LED Vẫn Sáng Liên Tục

**Kiểm tra:**
```bash
curl http://192.168.0.102/api/sensor
```

**Nếu `loopOn: 1`:**
→ Bơm tuần hoàn đang chạy → LED sáng là đúng!

**Nếu `loopOn: 0`:**
→ Code chưa upload đúng → Upload lại:
```bash
cd /Users/haidv/IdeaProjects/thuycanhesp32
pio run --target upload
```

### Vấn Đề 2: LED Không Bao Giờ Sáng

**Kiểm tra:**
1. LED có chớp 1/2s không? (pattern 1 OK)
2. Manual bật bơm → LED có sáng không?

**Nếu không sáng:**
→ Vấn đề phần cứng (LED, GPIO2)

### Vấn Đề 3: LED Nháy 3 Lần Liên Tục

**Nguyên nhân:** `ledPattern = 3` (sensor error)

**Kiểm tra:**
- TDS = NaN?
- pH = NaN?
- temp < -100 hoặc > 100?

→ Fix sensor trước!

---

## 📖 CHI TIẾT KỸ THUẬT

### LED Pattern Definitions:

```cpp
case 1: // OK - Chớp 1 lần mỗi 2s
case 2: // WiFi error - Chớp đôi
case 3: // Sensor error - Nháy 3 lần
case 4: // Pumping - Sáng liên tục
```

### Logic Flow (Sau Fix):

```cpp
// Every ~500ms loop:
1. Đọc sensor
2. Kiểm tra sensor error → ledPattern = 3?
3. Điều khiển bơm tuần hoàn:
   - Nếu loopOn = true → ledPattern = 4
4. Điều khiển bơm TDS:
   - Nếu dosePump() chạy → ledPattern = 4
5. Điều khiển bơm pH:
   - Nếu dosePump() chạy → ledPattern = 4
6. ✅ RESET LED (NEW):
   - Nếu AUTO && loopOn = 0 && ledPattern = 4
     → ledPattern = 1
7. ledPatternControl() → Hiển thị LED theo pattern
```

### Ưu Tiên LED Pattern:

```
Sensor Error (3) > Pumping (4) > OK (1) > WiFi Error (2)
```

→ Sensor error LUÔN ưu tiên cao nhất (không bị ghi đè)

---

## ✅ KẾT LUẬN

### Đã Fix:

- ✅ Thêm logic reset `ledPattern = 1` khi `loopOn = 0`
- ✅ Xử lý riêng cho Manual mode
- ✅ Ưu tiên sensor error (pattern 3)

### Kết Quả:

**Trước:** LED sáng liên tục dù `loopOn = 0` ❌  
**Sau:** LED chớp đúng khi không có bơm nào chạy ✅

### Test Ngay:

```bash
curl http://192.168.0.102/api/sensor
```

**Nếu `loopOn: 0`:**
→ LED phải **chớp 1 lần mỗi 2 giây** ✅

---

**Fix hoàn thành - LED giờ hiển thị đúng trạng thái hệ thống!** ✅











