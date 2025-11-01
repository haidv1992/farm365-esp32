# Sơ Đồ Đấu Nối - Farm365 ESP32

## ⚠️ QUAN TRỌNG: Nguồn và Phần Cứng

### 1. Nguồn Cung Cấp

```
┌─────────────────────────────────────────────────┐
│  12V BUS (Nguồn Chính)                          │
│  ╔═══════════════════════════════════════════╗   │
│  ║   Tụ lọc: 1000µF // 0.1µF                 ║   │
│  ╚═══════════════════════════════════════════╝   │
│          │                                        │
│          ├─→ Buck 5V (Relay coil)                │
│          │   Tụ: 470µF // 0.1µF                  │
│          │                                        │
│          └─→ Buck 3.3V (ESP32 + Sensors)         │
│              Tụ: 100µF // 0.1µF                  │
│              ⚠️ SÁT chân 3V3 ESP32              │
└─────────────────────────────────────────────────┘

⚠️ KHÔNG BAO GIỜ cắm USB 5V ĐỒNG THỜI cung cấp 3.3V ngoài
→ Nếu debug USB: Dùng cáp data-only (cắt VBUS)
```

### 2. GND kiểu Sao

```
                    GND CENTRAL
                        │
        ┌───────────────┼───────────────┐
        │               │               │
    LOGIC            FORCE          SENSORS
   (ESP32)      (Relay + Bơm)    (pH/TDS/Temp)
    3.3V            12V              3.3V
```

### 3. LED Trạng Thái

```
ESP32 GPIO2 ──┬── [R 220-330Ω] ──┬── LED + (chân dài)
              │                   │
              └── GND ────────────┘── LED - (chân ngắn)
```

**Pattern:**
- Booting: Nháy nhanh 5 lần
- OK: Chớp 1 lần / 2s
- WiFi Error: Chớp đôi
- Sensor Error: Nháy 3 lần nhanh lặp
- Pumping: Sáng liên tục

## 🔌 Pin Map Chi Tiết

### ESP32 Dev Module

```
                          ESP32-DevKitC
    ┌──────────────────────────────────────┐
    │  GND  GND  3V3  3V3   EN  ──  ──  ── │
    │  GP13 GP12 G14  G27  G26 G25 G33 G32 │
    │  GND  GND  G35  G34  VIN  ──  ──  ── │
    │  GND   D2  D4  G16 G17  G5  G18 G19  │   ═══ Đặc biệt:
    │  GND G21 G22  ──  ──  ──  ──  ──  ── │       D2 = GPIO2 (LED)
    │  GND GND  ──  ──  ──  ──  ──  ──  ── │       D4 = GPIO4 (DS18B20)
    └──────────────────────────────────────┘
```

### Kết Nối Relay (4 kênh, Low-trigger)

```
┌─────────────────────────────────────────────────────────┐
│  Relay Module 4-channel                                 │
│  ┌──────────┬──────────┬──────────┬──────────┐         │
│  │ CH1      │ CH2      │ CH3      │ CH4      │         │
│  │ Tuần hoàn│ Pump A   │ Pump B   │ Down-pH  │         │
│  ├──────────┼──────────┼──────────┼──────────┤         │
│  │ IN1→18   │ IN2→19   │ IN3→21   │ IN4→22   │         │
│  │ VCC→5V   │ VCC→5V   │ VCC→5V   │ VCC→5V   │         │
│  │ GND chung│ GND chung│ GND chung│ GND chung│         │
│  │ NC/NO→   │ NC/NO→   │ NC/NO→   │ NC/NO→   │         │
│  │ Bơm      │ Bơm A    │ Bơm B    │ Down-pH  │         │
│  └──────────┴──────────┴──────────┴──────────┘         │
└─────────────────────────────────────────────────────────┘
```

### Cảm Biến Nhiệt Độ DS18B20

```
                   DS18B20
    ┌─────────────────────────┐
    │  VDD → 3.3V             │
    │  Data → GPIO4 (D4) ──────┼───→ [10kΩ] → 3.3V
    │         ↑                │
    │         │                │
    │      [10kΩ]              │
    │         │                │
    │  GND → GND ESP32        │
    └─────────────────────────┘
    
    ⚠️ Pull-up 10kΩ giữa Data và 3.3V (hoặc 4.7kΩ)
```

### Cảm Biến pH (Phổ Biến / China Board)

```
                   pH Module
    ┌──────────────────────────────────┐
    │  VCC → 3.3V                      │
    │  GND → GND                       │
    │  OUT → GPIO33 (ADC1) ────→       │
    │         │                        │
    │         └── [RC Filter]          │
    │             100Ω nối tiếp        │
    │             0.1µF → GND         │
    │                                  │
    │  ⚠️ Tụ lọc 10µF // 0.1µF         │
    │     ngay module                  │
    └──────────────────────────────────┘
```

### Cảm Biến TDS

```
                   TDS Module
    ┌──────────────────────────────────┐
    │  VCC → 3.3V                      │
    │  GND → GND                       │
    │  OUT → GPIO32 (ADC1)             │
    │                                  │
    │  ⚠️ Tụ lọc 10µF // 0.1µF         │
    │     ngay module                  │
    └──────────────────────────────────┘
```

### BH1750 Ánh Sáng (I²C - Tùy chọn)

```
                   BH1750
    ┌─────────────────────────┐
    │  VCC → 3.3V             │
    │  SDA → GPIO25           │
    │  SCL → GPIO26           │
    │  GND → GND              │
    │                         │
    │  I²C pull-up: 4.7kΩ     │
    │  (nội bộ module thường)  │
    └─────────────────────────┘
```

### ZMCT103C AC Current Sensor (Tùy chọn)

```
                   ZMCT103C
    ┌──────────────────────────────────┐
    │  VCC → 3.3V                      │
    │  GND → GND                       │
    │  OUT → GPIO34 (ADC1) ────→       │
    │         │                        │
    │         └── [220nF] → GND       │
    │         (lọc nhiễu tần số cao)  │
    │                                  │
    │  ⚠️ Calib theo tải chuẩn:        │
    │     Scale factor = (V_out/V_in)  │
    └──────────────────────────────────┘
```

## 🔋 Diode Flyback (BẮT BUỘC cho Bơm DC)

Mỗi bơm DC cần 1 diode bảo vệ:

```
     ┌─── Bơm DC ───┐
     │              │
    [Motor]         │
     │              │
 12V+│ ────┐   ─────│── 12V+
     │     │   │
     └───┐ │   │
         │ │   │
    [1N4007] (song song)
         │ │   │
         └─┴───┘
```

→ Đặt diode GẦN tải (không xa hơn 10cm)

## 📊 Bảng Pin Đầy Đủ

| Chức Năng | ESP32 GPIO | ADC | Ghi Chú |
|-----------|------------|-----|---------|
| Relay Pump (IN1) | 18 | - | Tuần hoàn, Low-trigger |
| Relay A (IN2) | 19 | - | Bơm A, Low-trigger |
| Relay B (IN3) | 21 | - | Bơm B, Low-trigger |
| Relay Down-pH (IN4) | 22 | - | Down-pH, Low-trigger |
| LED Status | 2 | - | R 220-330Ω → GND |
| DS18B20 Data | 4 | - | Pull-up 10k → 3.3V |
| TDS OUT | 32 | ADC1 | 12-bit, 0-3.3V |
| pH OUT | 33 | ADC1 | 12-bit, 0-3.3V, RC filter |
| ZMCT103C OUT | 34 | ADC1 | 12-bit, AC current |
| BH1750 SDA | 25 | - | I²C |
| BH1750 SCL | 26 | - | I²C |

## 🔧 Checklist Bring-up

### Bước 1: Kiểm Tra Phần Cứng

- [ ] Nguồn 12V ổn định (≥5A)
- [ ] Tụ lọc đã gắn ở các điểm quan trọng
- [ ] GND sao: Logic tách Lực
- [ ] Diode flyback song song từng bơm DC
- [ ] Pull-up DS18B20 → 3.3V
- [ ] RC filter pH: 100Ω + 0.1µF

### Bước 2: Cấp Nguồn

- [ ] Cắm 3.3V vào chân 3V3 ESP32
- [ ] KHÔNG cắm USB 5V đồng thời!
- [ ] Kiểm tra LED boot pattern (5 lần nháy)

### Bước 3: Test Manual

- [ ] Mở Serial Monitor (115200 baud)
- [ ] Kiểm tra WiFi kết nối
- [ ] Vào Web UI `/manual`
- [ ] Test từng relay ON/OFF (1 giây)
- [ ] Xác nhận bơm đóng đúng

### Bước 4: Hiệu Chuẩn

- [ ] pH: Set pH7 → Set pH4
- [ ] TDS: Nhúng dung dịch chuẩn 1413 µS/cm → Set TDS
- [ ] Kiểm tra số đo trên dashboard

### Bước 5: Điều Khiển Tự Động

- [ ] Đặt mục tiêu pH: 6.0
- [ ] Đặt mục tiêu TDS: 800 ppm
- [ ] Kiểm tra lockout 90s hoạt động
- [ ] Kiểm tra fail-safe (giới hạn ngày)
- [ ] Chạy thử vòng tuần hoàn 15/45'

### Bước 6: Monitor

- [ ] Xem log `/log.txt` trong LittleFS
- [ ] Kiểm tra không có brownout/reset
- [ ] LED pattern luôn là "OK" (chớp 1/2s)

## 🐛 Troubleshooting

| Vấn Đề | Nguyên Nhân | Giải Pháp |
|--------|-------------|-----------|
| LED chớp đôi | WiFi lỗi | Kiểm tra SSID/password, xem Serial |
| LED nháy 3 lần | Sensor lỗi | Kiểm tra dây, nguồn 3.3V, GND |
| Brownout | Nguồn yếu | Tăng tụ lọc, kiểm tra 12V BUS |
| pH đọc sai | Chưa hiệu chuẩn | Hiệu chuẩn lại pH7/pH4 |
| TDS đọc sai | Chưa hiệu chuẩn | Hiệu chuẩn với dung dịch chuẩn |
| Bơm không chạy | Relay ngược trigger | Đổi logic LOW/HIGH |
| Reset liên tục | Xung động nguồn | Kiểm tra tụ, diode flyback |

---

**📌 Ghi Chú Quan Trọng:**

1. **Tụ lọc là BẮT BUỘC** - Không bỏ qua!
2. **Diode flyback** bảo vệ board khi tắt bơm
3. **GND sao** giảm nhiễu ADC
4. **Không cắm USB + nguồn ngoài đồng thời** (trừ khi cắt VBUS)
5. **Kiểm tra brownout** trong setup() → Thêm tụ nếu cần




