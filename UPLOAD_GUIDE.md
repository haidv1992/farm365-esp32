# 🔧 Hướng Dẫn Upload Code ESP32

## Phương Pháp 1: Arduino IDE (Đơn Giản Nhất)

1. **Mở Arduino IDE**

2. **Mở file**: `File → Open` → Chọn:
   ```
   /Users/haidv/IdeaProjects/thuycanhesp32/main/main.ino
   ```

3. **Cấu hình Board**:
   - Tools → Board → **ESP32 Arduino → ESP32 Dev Module**
   - Tools → Port → **/dev/cu.usbserial-0001**
   - Tools → Upload Speed → **115200**

4. **Upload**: Click nút Upload (→)

5. **Xem IP**: Tools → Serial Monitor (115200) để xem IP address

## Phương Pháp 2: Terminal (Không Dùng VSCode)

**Bước 1:** Đóng HOÀN TOÀN VSCode

**Bước 2:** Mở Terminal mới (không phải terminal trong VSCode)

**Bước 3:** Chạy lệnh:

```bash
cd /Users/haidv/IdeaProjects/thuycanhesp32
killall -9 Python 2>/dev/null
pio run --target upload
```

## Phương Pháp 3: Dùng esptool Trực Tiếp

```bash
cd /Users/haidv/IdeaProjects/thuycanhesp32
killall -9 Python 2>/dev/null

# Upload firmware
~/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32 \
  --port /dev/cu.usbserial-0001 \
  --baud 921600 \
  write_flash -z \
  --flash_mode dio \
  --flash_freq 80m \
  --flash_size 4MB \
  0x1000 .pio/build/esp32dev/bootloader.bin \
  0x8000 .pio/build/esp32dev/partitions.bin \
  0xe000 .pio/build/esp32dev/boot_app0.bin \
  0x10000 .pio/build/esp32dev/firmware.bin

# Upload LittleFS (sau khi firmware upload xong)
~/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32 \
  --port /dev/cu.usbserial-0001 \
  --baud 921600 \
  write_flash \
  0x290000 .pio/build/esp32dev/littlefs.bin
```

## Kiểm Tra Kết Quả

Sau khi upload thành công, ESP32 sẽ:
- LED nháy 5 lần khi boot
- Kết nối WiFi (haiquynh)
- Hiển thị IP trên Serial Monitor
- Web UI tại: `http://IP-address`

---

**💡 Khuyến nghị:** Dùng Arduino IDE vì đơn giản và ổn định nhất!



