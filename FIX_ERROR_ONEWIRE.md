# 🔧 Sửa Lỗi "OneWire.h: No such file or directory"

## ✅ Giải Pháp Nhanh (3 Phút)

### Cách 1: Arduino IDE - Library Manager (Đơn Giản Nhất)

1. **Mở Arduino IDE**
2. Vào menu: `Tools → Manage Libraries...`
3. Gõ "OneWire" vào ô tìm kiếm
4. Tìm thư viện "OneWire" **bởi Paul Stoffregen**
5. Nhấn nút **Install** (bên phải)
6. Đợi vài giây → Done!

**Làm tương tự cho:**
- **DallasTemperature** (bởi Paul Stoffregen)
- **BH1750** (bởi Christopher Laws) - Nếu dùng cảm biến ánh sáng

### Cách 2: Upload Sketches

1. Khởi động lại Arduino IDE (quan trọng!)
2. Mở `main/main.ino`
3. Upload lại

## 🚀 Xong! Bây Giờ Compile Lại

Nếu vẫn lỗi, kiểm tra:

- ✅ ESP32 Board đã cài chưa? (Tools → Board → ESP32 Arduino → ESP32 Dev Module)
- ✅ Thư viện đã install chưa? (Tools → Manage Libraries)
- ✅ Khởi động lại Arduino IDE

## 📋 Checklist

```
[ ] Cài OneWire library
[ ] Cài DallasTemperature library
[ ] (Optional) Cài BH1750 library
[ ] Khởi động lại Arduino IDE
[ ] Compile lại main.ino
```

## 🆘 Nếu Vẫn Lỗi

Xem chi tiết tại:
- [INSTALL_LIBRARIES.md](INSTALL_LIBRARIES.md)
- [ARDUINO_SETUP.md](ARDUINO_SETUP.md)

---

**💡 Tip:** Sau khi cài xong, compile lại sẽ thành công! ✅




