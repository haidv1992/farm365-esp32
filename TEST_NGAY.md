# 🚀 TEST NHANH BƠM TUẦN HOÀN

**Để test relay AUTO mode ngay (không đợi 45 phút):**

---

## ⚡ CÁCH 1: Đổi Chu Kỳ Thành 1 Phút

### Bước 1: Sửa Code

**File:** `src/main.cpp`  
**Line:** 52

**Đổi từ:**
```cpp
struct LoopCfg { 
  uint16_t on_min = 15;
  uint16_t off_min = 45;  // ← 45 phút
} loopCfg;
```

**Đổi thành:**
```cpp
struct LoopCfg { 
  uint16_t on_min = 15;
  uint16_t off_min = 1;   // ← 1 phút
} loopCfg;
```

### Bước 2: Upload

```bash
cd /Users/haidv/IdeaProjects/thuycanhesp32
pio run --target upload
```

### Bước 3: Đợi 1 Phút

**Mở Serial Monitor (115200 baud):**

```
WiFi connected: 192.168.0.102
Server started
...
Circulation pump ON  ← Sau 1 phút
```

**Kiểm tra:**
```bash
curl http://192.168.0.102/api/sensor
```

**Kết quả mong đợi:**
```json
{"pump": 1, "loopOn": 1}
```

**Nghe relay click + Bơm chạy = THÀNH CÔNG!** ✅

### Bước 4: Đổi Lại Sau Khi Test

**Sau khi xác nhận relay hoạt động:**

```cpp
uint16_t off_min = 45;  // ← Đổi lại 45 phút
```

→ Upload lại!

---

## 🔧 CÁCH 2: Force Bật Qua API (Đơn Giản Hơn)

### Không cần sửa code!

**Mở trình duyệt:**

```
http://192.168.0.102/manual
```

**Click:**
```
1. Chế Độ Tự Động
2. Bật bơm Tuần hoàn (nút Bật)
```

**Relay sẽ HÚT NGAY!**

**Nếu Manual hút mà AUTO không:**
→ Vấn đề logic AUTO (đã fix)

---

## 📊 KẾT QUẢ MONG ĐỢI

### Cách 1 (Đợi 1 phút):

```
0:00 - {"pump": 0, "loopOn": 0}  ← Boot
1:00 - {"pump": 1, "loopOn": 1}  ← AUTO BẬT ✅
1:01 - Relay hút
1:02 - Bơm chạy
```

### Cách 2 (Manual):

```
Click Manual → Bật → Relay hút ngay ✅
```

---

## ✅ KIỂM TRA SAU KHI TEST

**Nếu relay AUTO hoạt động:**
→ Fix thành công! Đổi lại `off_min = 45`

**Nếu relay vẫn không hút:**
→ Đọc [DEBUG_RELAY_PUMP.md](DEBUG_RELAY_PUMP.md) để kiểm tra phần cứng

---

**Chọn cách 1 hoặc 2 để test ngay!** 🚀











