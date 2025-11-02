# 🔧 DEBUG RELAY BƠM TUẦN HOÀN KHÔNG HÚT

**Vấn đề:** LED sáng liên tục (pump=1) nhưng relay không hút

---

## 🔍 PHÂN TÍCH

### API Cho Thấy:
```json
{"pump": 1, "loopOn": 1}
```
→ Code đang gọi `setRelay(PIN_RELAY_PUMP, true)` ✅

### Code Logic:
```cpp
// Line 318-320
setRelay(PIN_RELAY_PUMP, true);   // Bật relay
ledPattern = 4;                    // LED sáng
writeLog("Circulation pump ON");   // Ghi log
```

### Hàm setRelay:
```cpp
void setRelay(uint8_t pin, bool on) {
  // Low-trigger relay: LOW = ON, HIGH = OFF
  digitalWrite(pin, on ? LOW : HIGH);
}
```

→ Khi `on = true` → `digitalWrite(PIN_RELAY_PUMP, LOW)`

---

## 🔴 NGUYÊN NHÂN KHẢ DỊ

### 1. **Relay Module SAI TRIGGER (Phổ biến nhất!)**

**Module Low-trigger (Phổ biến):**
- IN = LOW (0V) → Relay BẬT
- IN = HIGH (3.3V) → Relay TẮT

**Module High-trigger (Ít gặp):**
- IN = HIGH (3.3V) → Relay BẬT
- IN = LOW (0V) → Relay TẮT

**Kiểm tra:**
```
Code hiện tại: Low-trigger
Nếu module là High-trigger → Phải đổi code!
```

### 2. **Dây Nối Sai**

**Kiểm tra kết nối:**
```
ESP32 GPIO18 → Relay Module IN1 (Tuần hoàn)
ESP32 GND    → Relay Module GND
Relay VCC    → 5V (không phải 3.3V!)
```

**Lưu ý:** 
- GPIO18 phải nối đúng IN1 (Tuần hoàn)
- Không nối ngược với IN2, IN3, IN4

### 3. **Relay Hỏng Hoặc Hàn Lỗi**

- LED trên module IN1 sáng nhưng relay không click
- Tiếp điểm relay bị dính
- Module hỏng

### 4. **GPIO18 Không Xuất Tín Hiệu**

- ESP32 hỏng GPIO18
- GPIO18 bị xung đột với module khác
- Dùng cho mục đích khác

---

## ✅ CÁCH KIỂM TRA

### Test 1: Đo Điện Áp GPIO18

**Công cụ:** Multimeter

**Bước 1 - Đo Khi Bơm ON:**
```
1. Xem Serial Monitor: "Circulation pump ON"
2. Đo điện áp GPIO18 → GND
   
Kết quả mong đợi (Low-trigger):
- Bơm ON → GPIO18 = 0V (LOW) ✅
```

**Bước 2 - Đo Khi Bơm OFF:**
```
1. Đợi 15 phút hoặc vào /manual tắt bơm
2. Đo điện áp GPIO18 → GND
   
Kết quả mong đợi:
- Bơm OFF → GPIO18 = 3.3V (HIGH) ✅
```

**Nếu điện áp không đổi:**
→ ESP32 GPIO18 hỏng hoặc code không chạy

### Test 2: Kiểm Tra LED Relay Module

**Relay module 4 kênh thường có LED chỉ thị:**

```
IN1 (GPIO18) → LED1 trên module
IN2 (GPIO19) → LED2
IN3 (GPIO21) → LED3
IN4 (GPIO22) → LED4
```

**Kiểm tra:**
```
1. Khi pump ON → LED1 phải SÁNG
2. Khi pump OFF → LED1 phải TẮT
```

**Nếu LED sáng nhưng relay không click:**
→ Relay trên module hỏng

**Nếu LED không sáng:**
→ Dây IN1 chưa nối hoặc nối sai

### Test 3: Test Manual Control

**Vào `/manual` và test:**

```
1. Click "Chế Độ Thủ Công"
2. Click bơm Tuần hoàn (💧)
3. Nghe relay có click không
4. Đo điện áp GPIO18
```

**Nếu manual cũng không chạy:**
→ Vấn đề phần cứng (dây nối, relay, GPIO)

**Nếu manual chạy được:**
→ Vấn đề logic AUTO (ít khả năng vì code đơn giản)

### Test 4: Swap Relay Channel

**Thử đổi kênh relay:**

```cpp
// File: src/main.cpp line 27
// Đổi từ:
constexpr uint8_t PIN_RELAY_PUMP = 18;  // Tuần hoàn

// Sang:
constexpr uint8_t PIN_RELAY_PUMP = 19;  // Dùng kênh A thử
```

**Sau đó:**
1. Upload code
2. Nối dây bơm tuần hoàn vào relay OUT2 (thay vì OUT1)
3. Test xem relay IN2 có chạy không

**Nếu relay IN2 chạy:**
→ Relay IN1 hỏng

**Nếu vẫn không chạy:**
→ Relay module hoặc code sai trigger

---

## 🔧 FIX - ĐỔI TRIGGER RELAY

### Nếu Module Là High-Trigger

**File:** `src/main.cpp` line 458-461

**Code cũ (Low-trigger):**
```cpp
void setRelay(uint8_t pin, bool on) {
  // Low-trigger relay: LOW = ON, HIGH = OFF
  digitalWrite(pin, on ? LOW : HIGH);
}
```

**Code mới (High-trigger):**
```cpp
void setRelay(uint8_t pin, bool on) {
  // High-trigger relay: HIGH = ON, LOW = OFF
  digitalWrite(pin, on ? HIGH : LOW);
}
```

**Upload lại code và test!**

---

## 🧪 HƯỚNG DẪN DEBUG TỪNG BƯỚC

### Bước 1: Kiểm Tra Code Đang Chạy

**Mở Serial Monitor (115200 baud):**

```
Quan sát log:
Circulation pump ON   ← Phải thấy dòng này khi bơm bật
Circulation pump OFF  ← Sau 15 phút
```

**Nếu không thấy log:**
→ Code không chạy logic bơm tuần hoàn

### Bước 2: Kiểm Tra GPIO18

**Dùng Multimeter:**
```
1. Đo GPIO18 → GND
2. Khi "Circulation pump ON":
   Low-trigger: Phải đo được 0-0.3V
   High-trigger: Phải đo được 3.0-3.3V
```

**Hoặc dùng LED test:**
```
GPIO18 → [LED] → [220Ω] → GND

Khi bơm ON:
- Low-trigger: LED TẮT (vì GPIO18 = 0V)
- High-trigger: LED SÁNG (vì GPIO18 = 3.3V)
```

### Bước 3: Kiểm Tra Relay Module

**Visual check:**
```
1. LED trên module IN1 sáng khi pump ON?
   - Có: Relay module nhận tín hiệu ✅
   - Không: Dây IN1 chưa nối đúng ❌

2. Nghe relay có click không?
   - Có: Relay hoạt động ✅
   - Không: Relay hỏng hoặc sai trigger ❌
```

### Bước 4: Kiểm Tra Đấu Nối Bơm

**Kết nối bơm:**
```
Nguồn 12V → Relay COM (Common)
Relay NO (Normally Open) → Bơm (+)
Bơm (-) → GND nguồn 12V
```

**Lưu ý:**
- Dùng NO (Normally Open), không phải NC
- Khi relay BẬT → NO nối với COM → Bơm có điện

**Test:**
```
1. Dùng Multimeter đo điện áp:
   COM → NO khi relay BẬT
   
2. Phải đo được 12V (hoặc điện áp nguồn bơm)
```

---

## 📊 BẢNG TROUBLESHOOTING

| Hiện Tượng | Nguyên Nhân | Fix |
|------------|-------------|-----|
| LED module IN1 không sáng | Dây IN1 chưa nối | Kiểm tra dây GPIO18 → IN1 |
| LED sáng, relay không click | Relay hỏng hoặc sai trigger | Đổi trigger trong code |
| Relay click nhưng bơm không chạy | Đấu nối bơm sai | Kiểm tra COM-NO, nguồn 12V |
| Manual chạy, Auto không chạy | Logic code (ít gặp) | Kiểm tra Serial log |
| Tất cả relay không chạy | Relay module chưa có VCC | Cấp 5V cho module |

---

## ✅ GIẢI PHÁP NHANH

### Fix 1: Đổi Trigger (Nếu Cần)

**Thử đổi từ Low → High trigger:**

```cpp
// File: src/main.cpp line 458
void setRelay(uint8_t pin, bool on) {
  digitalWrite(pin, on ? HIGH : LOW);  // Đổi từ LOW:HIGH sang HIGH:LOW
}
```

Upload và test!

### Fix 2: Test Với Relay Khác

**Đổi sang relay A (IN2) tạm:**

```cpp
// Line 27
constexpr uint8_t PIN_RELAY_PUMP = 19;  // Đổi từ 18 sang 19
```

Nối bơm vào relay OUT2 và test.

### Fix 3: Force Manual Test

**Vào `/manual`:**
```
1. Chế Độ Thủ Công
2. Bật bơm Tuần hoàn
3. Nếu chạy → Vấn đề logic AUTO
4. Nếu không → Vấn đề phần cứng
```

---

## 🚀 CHECKLIST

- [ ] Serial log có "Circulation pump ON"?
- [ ] GPIO18 điện áp thay đổi khi ON/OFF?
- [ ] LED trên module IN1 sáng?
- [ ] Relay có click không?
- [ ] Bơm nối đúng COM-NO?
- [ ] Nguồn 12V có điện?
- [ ] Module relay có VCC 5V?
- [ ] Thử manual control?
- [ ] Thử đổi trigger Low↔High?
- [ ] Thử swap sang relay khác?

---

## 📞 NẾU VẪN KHÔNG CHẠY

**Gửi cho tôi:**

1. **Serial Monitor output:**
```
Circulation pump ON
```

2. **Đo điện áp GPIO18:**
```
Pump ON: ??? V
Pump OFF: ??? V
```

3. **LED module IN1:**
```
Pump ON: Sáng / Tắt ?
```

4. **Relay click:**
```
Có nghe click / Không ?
```

→ Tôi sẽ phân tích chính xác!

---

**Khả năng cao nhất: Relay module dùng High-trigger → Đổi code setRelay!** 🔧











