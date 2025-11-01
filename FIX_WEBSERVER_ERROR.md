# 🔧 Sửa Lỗi WebServer::send() với LittleFS

## Lỗi

```
error: no matching function for call to 'WebServer::send(fs::LittleFSFS&, String&, String&)'
```

## Nguyên Nhân

ESP32 Arduino v3.x đã thay đổi API của WebServer. Không còn hỗ trợ:

```cpp
server.send(LittleFS, path, mimetype); // ❌ Không hoạt động
```

## ✅ Giải Pháp

Dùng `server.streamFile()` thay thế:

```cpp
void handleStaticFile(String path) {
  String mimetype = "text/plain";
  if (path.endsWith(".html")) mimetype = "text/html";
  else if (path.endsWith(".css")) mimetype = "text/css";
  else if (path.endsWith(".js")) mimetype = "application/javascript";
  
  // Read file from LittleFS and send content
  File f = LittleFS.open(path, "r");
  if (!f) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  
  server.streamFile(f, mimetype);  // ✅ Đúng cách
  f.close();
}
```

## Đã Sửa Trong Code

File `main/main.ino` đã được cập nhật với cách gọi đúng.

## Compile Lại

Giờ bạn chỉ cần:
1. Mở `main/main.ino`
2. `Sketch → Upload`
3. ✅ Done!

---

**💡 Tip:** Đảm bảo đã cài OneWire và DallasTemperature libraries trước khi compile!




