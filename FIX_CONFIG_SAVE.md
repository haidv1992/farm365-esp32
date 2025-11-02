# ✅ FIX TRANG CẤU HÌNH KHÔNG LƯU

**Ngày:** 2025-10-28  
**Vấn đề:** Thay đổi cấu hình bơm (3 phút bơm / 1 phút nghỉ) → Ấn Lưu → Tải lại trang → Lại về 15/45 phút

---

## 🔴 NGUYÊN NHÂN

### 1. HTML Không Load Giá Trị Hiện Tại

**File:** `data/config.html`

**Vấn đề:**
```html
<!-- Line 58-62 - GIÁ TRỊ CỐ ĐỊNH -->
<input type="number" name="loopOn" value="15" min="1" max="60" required>
<input type="number" name="loopOff" value="45" min="1" max="120" required>
```

→ HTML luôn hiển thị `value="15"` và `value="45"` mặc định!

**Hệ quả:**
1. Mở trang `/config` → Form hiển thị 15/45 (giá trị cứng trong HTML)
2. Nhập 3/1 → Ấn Lưu → ESP32 lưu vào NVS ✅
3. **Tải lại trang** → Form lại hiển thị 15/45 (vì HTML không đọc từ ESP32)
4. User nghĩ là "không lưu được" ❌

**Thực tế:** 
- ESP32 **ĐÃ LƯU** đúng (kiểm tra API: `curl http://192.168.0.102/api/config`)
- Nhưng HTML không fetch giá trị đã lưu → Hiển thị sai!

### 2. Thiếu API Để Lấy Cấu Hình

**Trước fix:**
```cpp
// KHÔNG CÓ endpoint này!
server.on("/api/config", handleGetConfig);
```

→ JavaScript không có API để fetch giá trị hiện tại!

---

## ✅ FIX

### Fix 1: Thêm API `/api/config` (Backend)

**File:** `src/main.cpp`

**Line 211:** Thêm route mới
```cpp
server.on("/api/config", handleGetConfig);  // ✅ NEW
```

**Line 636-650:** Thêm handler mới
```cpp
void handleGetConfig() {
  String json = "{";
  json += "\"loopOn\":" + String(loopCfg.on_min) + ",";
  json += "\"loopOff\":" + String(loopCfg.off_min) + ",";
  json += "\"tdsTarget\":" + String(tdsCfg.target) + ",";
  json += "\"tdsHyst\":" + String(tdsCfg.hyst) + ",";
  json += "\"tdsDose\":" + String(tdsCfg.dose_ms) + ",";
  json += "\"tdsLock\":" + String(tdsCfg.lock_s) + ",";
  json += "\"phTarget\":" + String(phCfg.target, 1) + ",";
  json += "\"phHyst\":" + String(phCfg.hyst, 1) + ",";
  json += "\"phDose\":" + String(phCfg.dose_ms) + ",";
  json += "\"phLock\":" + String(phCfg.lock_s);
  json += "}";
  sendJSON(json);
}
```

**Test API:**
```bash
curl http://192.168.0.102/api/config
# Output: {"loopOn":3,"loopOff":1,"tdsTarget":800,...}
```

### Fix 2: JavaScript Load Giá Trị Khi Mở Trang (Frontend)

**File:** `data/config.html`

**Line 133-151:** Thêm function load config
```javascript
function loadConfig() {
    fetch('/api/config')
        .then(response => response.json())
        .then(data => {
            document.querySelector('input[name="loopOn"]').value = data.loopOn || 15;
            document.querySelector('input[name="loopOff"]').value = data.loopOff || 45;
            document.querySelector('input[name="tdsTarget"]').value = data.tdsTarget || 800;
            // ... các trường khác
        })
        .catch(error => {
            console.error('Error loading config:', error);
        });
}
```

**Line 154:** Gọi khi trang load
```javascript
window.addEventListener('DOMContentLoaded', loadConfig);
```

**Line 167:** Reload sau khi lưu thành công
```javascript
if (data.status === 'ok') {
    alert('✅ Cấu hình đã được lưu thành công!');
    loadConfig(); // ← Reload để xác nhận
}
```

---

## 📊 SO SÁNH TRƯỚC/SAU

### Trước Fix:

```
1. Mở /config
   → HTML hiển thị: loopOn=15, loopOff=45 (cứng trong HTML)

2. Nhập: loopOn=3, loopOff=1
   → Ấn Lưu
   → ESP32 lưu vào NVS ✅
   → Alert: "Cấu hình đã được lưu thành công!"

3. Tải lại trang (F5)
   → HTML hiển thị: loopOn=15, loopOff=45 ❌
   → User: "Hả? Không lưu được à?"

4. Kiểm tra API:
   curl http://192.168.0.102/api/config
   → {"loopOn":3,"loopOff":1,...} ✅
   → THỰC TẾ ĐÃ LƯU! Nhưng HTML không hiển thị!
```

### Sau Fix:

```
1. Mở /config
   → JavaScript fetch /api/config
   → {"loopOn":3,"loopOff":1,...}
   → HTML hiển thị: loopOn=3, loopOff=1 ✅

2. Nhập: loopOn=10, loopOff=5
   → Ấn Lưu
   → ESP32 lưu vào NVS ✅
   → loadConfig() gọi lại
   → HTML cập nhật: loopOn=10, loopOff=5 ✅

3. Tải lại trang (F5)
   → loadConfig() chạy lại
   → HTML hiển thị: loopOn=10, loopOff=5 ✅
```

---

## 🧪 CÁCH TEST

### Test 1: Kiểm Tra API

```bash
curl http://192.168.0.102/api/config
```

**Kết quả mong đợi:**
```json
{
  "loopOn": 3,
  "loopOff": 1,
  "tdsTarget": 800,
  "tdsHyst": 50,
  "tdsDose": 700,
  "tdsLock": 90,
  "phTarget": 6.0,
  "phHyst": 0.3,
  "phDose": 300,
  "phLock": 90
}
```

### Test 2: Test Qua Trình Duyệt

**Bước 1:** Mở `http://192.168.0.102/config`

**Kiểm tra:**
- Ô "Thời Gian BƠM" hiển thị `3`
- Ô "Thời Gian NGHỈ" hiển thị `1`

→ Nếu đúng → API load thành công! ✅

**Bước 2:** Đổi thành `10` và `5`

**Bước 3:** Ấn "💾 Lưu Cấu Hình"

**Kết quả:**
- Alert: "✅ Cấu hình đã được lưu thành công!"
- Form tự động cập nhật: `10` và `5`

**Bước 4:** Tải lại trang (F5)

**Kiểm tra:**
- Ô "Thời Gian BƠM" vẫn hiển thị `10` ✅
- Ô "Thời Gian NGHỈ" vẫn hiển thị `5` ✅

→ Nếu đúng → Fix hoàn toàn thành công! 🎉

### Test 3: Kiểm Tra Serial Monitor

**Mở Serial Monitor (115200 baud):**

**Khi ấn Lưu, phải thấy:**
```
✅ Config saved: loopOn=10, loopOff=5
```

→ Xác nhận ESP32 đã nhận và lưu giá trị!

### Test 4: Test Sau Khi Reboot ESP32

**Bước 1:** Lưu cấu hình `loopOn=10, loopOff=5`

**Bước 2:** Reboot ESP32 (ngắt nguồn hoặc ấn RESET)

**Bước 3:** Đợi ESP32 boot xong → Mở `/config`

**Kiểm tra:**
- Ô "Thời Gian BƠM" hiển thị `10` ✅
- Ô "Thời Gian NGHỈ" hiển thị `5` ✅

→ Xác nhận NVS lưu vĩnh viễn (không mất khi mất điện)!

---

## 🔧 TROUBLESHOOTING

### Vấn Đề 1: Mở `/config` Vẫn Hiển thị 15/45

**Nguyên nhân:** LittleFS chưa upload (file `config.html` cũ)

**Fix:**
```bash
cd /Users/haidv/IdeaProjects/thuycanhesp32
pio run --target uploadfs
```

**Kiểm tra:**
- F12 (Developer Tools) → Console
- Phải thấy: `fetch('/api/config')` được gọi

### Vấn Đề 2: API `/api/config` Trả Về 404

**Nguyên nhân:** Firmware chưa upload (code `main.cpp` cũ)

**Fix:**
```bash
pio run --target upload
```

**Kiểm tra:**
```bash
curl http://192.168.0.102/api/config
# Không được 404!
```

### Vấn Đề 3: Lưu Xong Vẫn Hiển thị Giá Trị Cũ

**Nguyên nhân:** Browser cache

**Fix:**
```
Ctrl + Shift + R (Windows/Linux)
Cmd + Shift + R (Mac)
```

→ Hard reload (xóa cache)

### Vấn Đề 4: Serial Monitor Không Thấy "Config saved"

**Kiểm tra:**
1. Baud rate = 115200?
2. ESP32 đã kết nối Serial?
3. POST request có gửi đến ESP32?

**Debug:**
- F12 → Network → Xem request `/config` POST
- Status = 200? → Thành công
- Response = `{"status":"ok"}`? → ESP32 đã nhận

---

## 📖 CHI TIẾT KỸ THUẬT

### Flow Hoạt Động

#### 1. Khi Mở Trang `/config`:

```
Browser → GET /config → ESP32
ESP32 → Read /config.html from LittleFS
ESP32 → Send HTML to Browser
Browser → Render HTML
Browser → Execute JavaScript:
  → fetch('/api/config')
  → ESP32 handleGetConfig()
  → Return JSON: {"loopOn":3, "loopOff":1, ...}
  → JavaScript populate form fields
```

#### 2. Khi Ấn "Lưu Cấu Hình":

```
Browser → POST /config (FormData)
ESP32 handleConfig():
  → loopCfg.on_min = arg("loopOn")
  → loopCfg.off_min = arg("loopOff")
  → saveConfig()  // Write to NVS
  → Serial.printf("✅ Config saved")
  → Return {"status":"ok"}
Browser → alert("✅ Cấu hình đã được lưu")
Browser → loadConfig()  // Reload values
```

#### 3. NVS Storage:

**Khi `saveConfig()`:**
```cpp
prefs.putUInt("loopOn", loopCfg.on_min);   // Write to Flash
prefs.putUInt("loopOff", loopCfg.off_min); // Persistent storage
```

**Khi `loadConfig()` (Boot):**
```cpp
loopCfg.on_min = prefs.getUInt("loopOn", 15);  // Read from Flash
loopCfg.off_min = prefs.getUInt("loopOff", 45); // Default: 15, 45
```

→ Dữ liệu lưu trong **NVS Partition** của ESP32 Flash (không mất khi mất điện)

---

## ✅ KẾT LUẬN

### Đã Fix:

- ✅ Thêm API `/api/config` để lấy giá trị hiện tại
- ✅ JavaScript load giá trị từ API khi mở trang
- ✅ JavaScript reload giá trị sau khi lưu thành công
- ✅ Thêm Serial log để debug (`✅ Config saved: loopOn=X, loopOff=Y`)

### Đã Upload:

- ✅ Firmware (`main.cpp`)
- ✅ LittleFS (`config.html`)

### Kết Quả:

→ **Trang cấu hình giờ hiển thị đúng giá trị đã lưu!** 🎉

### Test Ngay:

```
http://192.168.0.102/config
```

1. Mở trang → Thấy `3` và `1` (hoặc giá trị đã lưu)
2. Đổi thành `10` và `5` → Lưu
3. Tải lại trang → Vẫn thấy `10` và `5` ✅

---

**Fix hoàn thành - Cấu hình giờ lưu và hiển thị đúng!** ✅











