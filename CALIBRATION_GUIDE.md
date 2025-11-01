# Hướng Dẫn Hiệu Chuẩn Chi Tiết

## 📌 Lưu Ý Quan Trọng

**NVS (Non-Volatile Storage) KHÔNG bị reset khi nạp lại firmware!** 
- Tất cả cấu hình và hiệu chuẩn được lưu trong NVS
- Chỉ reset khi:
  - Xóa NVS bằng code: `prefs.clear()`
  - Flash ESP32 bằng esptool với option `--erase-all`

---

## 🧪 Hiệu Chuẩn pH (2 điểm)

### Cần có:
- Dung dịch chuẩn pH7 (buffer)
- Dung dịch chuẩn pH4 hoặc pH10 (buffer)
- Nước cất để rửa probe
- (Tuỳ chọn) Đồng hồ vạn năng để đo điện áp OUT

### Các bước:

1. **Rửa probe** bằng nước cất
2. **Nhúng probe vào dung dịch pH7**, đợi **30-60 giây** để ổn định
3. **Bấm "Set pH7"** → Hệ thống ghi lại điện áp tại pH7
4. **Rửa lại probe** bằng nước cất, lau khô nhẹ
5. **Nhúng probe vào dung dịch pH4** (hoặc pH10), đợi **30-60 giây**
6. **Bấm "Set pH4"** → Hệ thống ghi lại điện áp tại pH4
7. **Kiểm tra**: Xem giá trị pH trên Dashboard có đúng không

---

### ⚙️ Chiết Áp trên Module pH (Aideepen / LM358)

#### 🎯 Mục đích:
- Chỉnh **điểm giữa (offset)** để khi probe pH = 6.86 thì điện áp OUT ≈ **2.5V** (pH trung tính)
- Giúp tín hiệu nằm đúng dải ADC (1.0 - 3.3V cho ESP32)

#### 🔧 Cách vặn chiết áp:

| Hướng vặn | Hiệu ứng | Ghi chú khi hiệu chuẩn |
|-----------|----------|------------------------|
| **Theo chiều kim đồng hồ (CW)** | **Tăng điện áp OUT** | pH đo **giảm** (ví dụ: 7.0 → 6.5) |
| **Ngược chiều kim đồng hồ (CCW)** | **Giảm điện áp OUT** | pH đo **tăng** (ví dụ: 7.0 → 7.5) |

#### ✅ Ghi chú nên dán trên module:
```
pH Module - Hướng vặn & giá trị chuẩn:
• Vặn CW (phải) → giảm pH hiển thị
• Vặn CCW (trái) → tăng pH hiển thị
Mục tiêu:
• pH 6.86 → OUT ≈ 2.50V
• pH 4.00 → OUT ≈ 3.00V
(chênh 0.50V là lý tưởng)
```

#### 📝 Quy trình chỉnh chiết áp (nếu cần):

1. **Reset hiệu chuẩn phần mềm** về giá trị mặc định
2. **Nhúng probe vào pH 6.86**, đợi ổn định
3. **Dùng đồng hồ đo** chân OUT và GND
4. **Vặn chiết áp từ từ** (1/8 vòng mỗi lần) cho đến khi OUT ≈ 2.50V
5. **Nhúng vào pH 4.00**, kiểm tra OUT ≈ 3.00V (chênh 0.5V)
6. **Set lại pH7 và pH4** bằng phần mềm để tăng độ chính xác

#### ⚠️ Lưu ý:
- **KHÔNG** chỉnh chiết áp sau khi đã hiệu chuẩn phần mềm
- Vặn **rất chậm** (loại 3296W chỉ 20-25 vòng toàn hành trình)
- Sau khi chỉnh xong → dùng sơn cách điện hoặc keo dán để tránh rung lệch

---

## ⚖️ Hiệu Chuẩn TDS

### Cần có:
- Dung dịch chuẩn TDS (ví dụ: 1413 µS/cm hoặc 2764 µS/cm)

### Các bước:

1. **Rửa probe** bằng nước cất
2. **Nhúng probe vào dung dịch chuẩn**, đợi **30 giây** để ổn định
3. **Nhập EC chuẩn** của dung dịch (µS/cm) vào ô "EC Chuẩn"
4. **Bấm "Set TDS"** → Hệ thống tính hệ số `tds_k` và lưu
5. **Kiểm tra**: Xem giá trị TDS trên Dashboard có đúng không

### Lưu ý:
- TDS ≈ 0.5 × EC (µS/cm)
- Ví dụ: EC = 1413 µS/cm → TDS ≈ 706 ppm
- Module TDS thường có chiết áp, nhưng **KHÔNG cần chỉnh** nếu dùng hiệu chuẩn phần mềm

---

## ⚡ Hiệu Chuẩn ZMCT103C (Dòng Điện AC)

### Cần có:
- Tải AC đã biết dòng điện (ví dụ: đèn 100W, quạt 50W)
- Ampe kế clamp để đo dòng thực tế
- (Tuỳ chọn) Đồng hồ vạn năng để đo điện áp OUT

### Bước 1: Hiệu Chuẩn Offset (Không có tải)

1. **Tắt tất cả tải** (không có dòng điện chạy qua ZMCT)
2. **Bấm "Set Offset"** → Hệ thống ghi lại điện áp offset (~1.65V)
3. Offset này sẽ được trừ khi tính RMS

### Bước 2: Hiệu Chuẩn Sensitivity (Có tải)

1. **Bật tải đã biết dòng điện** (ví dụ: đèn 100W)
2. **Đo dòng điện thực tế** bằng ampe kế clamp → Ví dụ: 0.45A
3. **Nhập dòng điện** vào ô "Dòng Điện (A)" → 0.45
4. **Bấm "Set Sensitivity"** → Hệ thống tính hệ số A/V và lưu
5. **Kiểm tra**: Xem giá trị "Dòng Điện (A)" trên Dashboard có đúng không

### Công thức:
- **Dòng điện (A)** = (Voltage_RMS - Offset) × Sensitivity
- Sensitivity = KnownCurrent / (Voltage_RMS - Offset)

---

### ⚙️ Chiết Áp trên Module ZMCT103C

#### 🎯 Mục đích:
- Điều chỉnh **độ khuếch đại (gain)** sao cho tín hiệu analog nằm trong giới hạn ADC (không bão hòa 0-3.3V)
- Khi chưa có tải, OUT ≈ **2.5V (offset trung tâm)**

#### 🔧 Cách vặn chiết áp:

| Hướng vặn | Hiệu ứng | Ghi chú khi hiệu chuẩn |
|-----------|----------|------------------------|
| **Theo chiều kim đồng hồ (CW)** | **Tăng biên độ tín hiệu OUT** | Tín hiệu dao động rộng hơn, dễ bão hòa nếu quá mạnh |
| **Ngược chiều kim đồng hồ (CCW)** | **Giảm biên độ tín hiệu OUT** | Tín hiệu nhỏ, khó phân biệt dòng thấp |

#### ✅ Ghi chú nên dán trên module:
```
ZMCT Module - Hướng vặn & biên độ tín hiệu:
• Vặn CW (phải) → tăng độ nhạy (biên độ lớn)
• Vặn CCW (trái) → giảm độ nhạy (biên độ nhỏ)
Mục tiêu:
• Không tải → OUT ≈ 2.5V
• Tải 100W (0.45A) → OUT dao động ±0.3-0.4V
  quanh 2.5V (2.1-2.9V)
```

#### 📝 Quy trình chỉnh chiết áp (nếu cần):

1. **Tắt tất cả tải** (không có dòng điện)
2. **Dùng đồng hồ đo** chân OUT và GND
3. **Vặn chiết áp** cho đến khi OUT ≈ 2.5V
4. **Bật tải 100W** (0.45A)
5. **Kiểm tra tín hiệu dao động** ±0.3-0.4V quanh 2.5V
   - Nếu dao động quá lớn (> ±0.8V) → vặn CCW giảm nhạy
   - Nếu dao động quá nhỏ (< ±0.2V) → vặn CW tăng nhạy
6. **Set lại Offset và Sensitivity** bằng phần mềm

#### ⚠️ Lưu ý:
- **KHÔNG** chỉnh chiết áp sau khi đã hiệu chuẩn phần mềm
- Vặn **rất chậm** (1/8 vòng mỗi lần)
- Tránh bão hòa tín hiệu (OUT không được < 0.5V hoặc > 3.0V)
- Sau khi chỉnh xong → dùng sơn cách điện hoặc keo dán

---

## 📋 Bảng Tóm Tắt - Ghi Chú Kỹ Thuật

### Module pH Sensor (Aideepen / LM358)

```
┌────────────────────────────────────────────────────────┐
│ pH Module - Hướng vặn & giá trị chuẩn                 │
├────────────────────────────────────────────────────────┤
│ • Vặn CW (phải) → giảm pH hiển thị                    │
│ • Vặn CCW (trái) → tăng pH hiển thị                   │
│                                                        │
│ Mục tiêu hiệu chuẩn:                                  │
│ • pH 6.86 → OUT ≈ 2.50V                               │
│ • pH 4.00 → OUT ≈ 3.00V                               │
│   (chênh 0.50V là lý tưởng)                           │
│                                                        │
│ Lưu ý: Vặn từ từ 1/8 vòng mỗi lần                     │
│ Sau khi chỉnh → dùng keo dán cố định                  │
└────────────────────────────────────────────────────────┘
```

### Module ZMCT103C (Đo Dòng AC)

```
┌────────────────────────────────────────────────────────┐
│ ZMCT Module - Hướng vặn & biên độ tín hiệu            │
├────────────────────────────────────────────────────────┤
│ • Vặn CW (phải) → tăng độ nhạy (biên độ lớn)          │
│ • Vặn CCW (trái) → giảm độ nhạy (biên độ nhỏ)         │
│                                                        │
│ Mục tiêu:                                             │
│ • Không tải → OUT ≈ 2.5V                              │
│ • Tải 100W (0.45A) → OUT dao động ±0.3-0.4V           │
│   quanh 2.5V (2.1-2.9V)                               │
│                                                        │
│ Lưu ý: Tránh bão hòa (OUT: 0.5V - 3.0V)              │
│ Sau khi chỉnh → dùng keo dán cố định                  │
└────────────────────────────────────────────────────────┘
```

### Mẹo Thực Hành

1. **Gắn đồng hồ đo điện áp** trực tiếp vào chân OUT và GND trong lúc vặn
2. **Vặn rất chậm** (1/8 vòng mỗi lần) - loại 3296W chỉ 20-25 vòng toàn hành trình
3. **Ghi chú ngay**: Dán nhãn lên module với hướng vặn và giá trị mục tiêu
4. **Cố định sau khi chỉnh**: Dùng sơn cách điện hoặc keo dán nhẹ lên vít để tránh rung lệch
5. **Ưu tiên phần mềm**: Chỉnh chiết áp chỉ khi cần thiết, ưu tiên hiệu chuẩn phần mềm

---

## 🔄 Reset Tổng Điện Năng

Nếu muốn reset đồng hồ điện năng về 0:
1. **Bấm "Reset Tổng Điện Năng (kWh)"**
2. Giá trị sẽ về 0 ngay lập tức
3. Hệ thống tiếp tục tích lũy từ 0

---

## ❓ Câu Hỏi Thường Gặp

### Q: Nạp lại firmware có mất hiệu chuẩn không?
**A:** KHÔNG! Tất cả hiệu chuẩn được lưu trong NVS (non-volatile), chỉ reset khi xóa NVS.

### Q: Khi nào cần hiệu chuẩn lại?
**A:** 
- pH: Mỗi 2-4 tuần hoặc khi thay dung dịch
- TDS: Khi thay dung dịch hoặc probe bị bẩn
- ZMCT: Hiếm khi, chỉ khi thay module hoặc sai số lớn

### Q: Có thể chỉnh chiết áp thay vì hiệu chuẩn phần mềm không?
**A:** Có, nhưng không khuyến nghị vì:
- Chiết áp dễ bị thay đổi do rung động/nhiệt độ
- Hiệu chuẩn phần mềm chính xác và ổn định hơn

### Q: Hiệu chuẩn pH/TDS có cần làm ở nhiệt độ nhất định không?
**A:** Buffer pH thường ổn định ở nhiệt độ phòng (20-25°C). Nếu nhiệt độ khác nhiều, nên đợi probe ổn định lâu hơn (60-90 giây).

---

## 🔧 Troubleshooting

### pH đo sai:
1. Kiểm tra probe có bẩn không → Rửa bằng nước cất
2. Buffer có hết hạn không → Dùng buffer mới
3. Hiệu chuẩn lại pH7 và pH4

### TDS đo sai:
1. Kiểm tra probe có bẩn không → Rửa bằng nước cất
2. Dung dịch chuẩn có phải là EC (µS/cm) không → Đổi sang EC
3. Hiệu chuẩn lại với dung dịch chuẩn mới

### ZMCT đo = 0:
1. Kiểm tra offset đã hiệu chuẩn chưa
2. Kiểm tra sensitivity đã hiệu chuẩn chưa
3. Kiểm tra dây nối ZMCT → GPIO34
4. Kiểm tra có tải thực sự chạy qua ZMCT không

