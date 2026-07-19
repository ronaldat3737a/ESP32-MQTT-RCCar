# ESP32 MQTT RC Car

Dự án xe ô tô tự hành đa chức năng sử dụng vi điều khiển ESP32, giao thức MQTT và giao diện Web điều khiển thông minh.

## 1. Tính năng chính

- **Điều khiển từ xa:** Điều hướng xe (Tiến, Lùi, Trái, Phải, Dừng) qua giao diện Web dựa trên giao thức MQTT.
- **Tự động tránh vật cản:** Sử dụng cảm biến siêu âm HC-SR04 tự động phanh khẩn cấp khi gặp vật cản (khoảng cách < 40 cm).
- **Giám sát hành trình (Odometry):** Đo tốc độ và quãng đường di chuyển thực tế bằng cảm biến Encoder hồng ngoại và hiển thị lộ trình trên bản đồ 2D (Canvas).
- **Tự hành bám vạch (Line Follower):** Sử dụng hệ thống 5 mắt cảm biến hồng ngoại TCRT5000 để tự động bám theo vạch đen.
- **Bảo mật (RFID):** Khóa/Mở xe bằng thẻ từ thông qua module RFID-RC522.

---

## 2. Sơ đồ đấu nối phần cứng (Pin Mapping)

Sử dụng ESP32 Dev Board với sơ đồ chân chi tiết:

| Module                 | Chân trên Module          | Chân ESP32         |
| :--------------------- | :------------------------ | :----------------- |
| **Động cơ (L298N)**    | IN1, IN2, IN3, IN4        | 22, 21, 19, 18     |
| **Động cơ (L298N)**    | enA, enB                  | 5, 23              |
| **Siêu âm (HC-SR04)**  | TRIG, ECHO                | 25, 26             |
| **Encoder (LM393)**    | D0                        | 27                 |
| **Dò line (TCRT5000)** | OUT1–OUT5                 | 34, 35, 32, 33, 14 |
| **RFID (RC522)**       | RST, SDA, SCK, MISO, MOSI | 2, 15, 4, 16, 13   |

---

## 3. Hướng dẫn cài đặt

### 3.1. Yêu cầu môi trường

1. Cài đặt **Arduino IDE**.
2. Thêm hỗ trợ board ESP32 trong **Board Manager**.

   Vào **File → Preferences**, thêm đường dẫn:

   ```text
   https://dl.espressif.com/dl/package_esp32_index.json
   ```

3. Cài đặt các thư viện:
   - `PubSubClient` (Nick O'Leary)
   - `MFRC522` (GithubCommunity)

---

### 3.2. Cấu hình

1. Mở file `MQTT_RCCar.ino`.

2. Thay đổi thông tin WiFi:

   ```cpp
   const char* ssid = "Tên_WiFi_Của_Bạn";
   const char* password = "Mật_Khẩu_WiFi";
   ```

3. (Nếu sử dụng MQTT Broker khác) cập nhật:

   ```cpp
   const char* mqtt_server = "broker_address";
   ```

4. Chọn đúng board ESP32 và cổng COM.

5. Nhấn **Upload** để nạp chương trình vào ESP32.

---

## 4. Cách vận hành

### Kết nối

- Mở **Serial Monitor** với tốc độ **115200 baud**.
- Kiểm tra ESP32 đã kết nối WiFi và MQTT thành công.

### Giao diện Web

- Mở file `index.html` bằng trình duyệt web.
- Hoặc triển khai bằng GitHub Pages để điều khiển từ xa.

### Mở khóa xe

Khi vừa cấp nguồn:

- Xe ở trạng thái **LOCKED**.
- Quẹt thẻ RFID đã đăng ký.
- Xe chuyển sang trạng thái **UNLOCKED** và sẵn sàng hoạt động.

### Điều khiển

Người dùng có thể:

- Điều khiển thủ công:
  - Tiến
  - Lùi
  - Rẽ trái
  - Rẽ phải
  - Dừng

- Hoặc chuyển sang chế độ:
  - 🤖 Line Follower (Bám vạch)

---

## 5. Đóng góp dự án

Dự án được thực hiện bởi nhóm sinh viên Trường Đại học Bách khoa Hà Nội.

| Thành viên       |   MSSV   | Phụ trách                          |
| :--------------- | :------: | :--------------------------------- |
| Nguyễn Công Đạt  | 20236023 | Cảnh báo vật cản & Bảo mật RFID    |
| Lê Thành Nguyên  | 20236049 | Điều khiển xe từ xa                |
| Nguyễn Công Đăng | 20236022 | Đo tốc độ & Quãng đường (Odometry) |
| Hoàng Duy Tân    | 20236053 | Tự động bám vạch (Line Follower)   |

---

## 6. Lưu ý

- Kiểm tra kỹ sơ đồ đấu nối trước khi cấp nguồn.
- Đảm bảo ESP32 kết nối đúng WiFi và MQTT Broker.
- Nếu RFID chưa nhận thẻ:
  - Kiểm tra dây SPI.
  - Mở Serial Monitor để đọc UID.
  - Thêm UID vào mảng `AUTHORIZED_UID` trong chương trình.

---

## 7. Cấu trúc dự án

```text
ESP32-MQTT-RCCar/
├── MQTT_RCCar.ino
├── index.html
├── MFRC522.cpp
├── MFRC522.h
├── PubSubClient.cpp
├── PubSubClient.h
├── deprecated.h
├── require_cpp11.h
└── README.md
```

---

## 8. Giấy phép

Dự án được phát triển phục vụ mục đích học tập, nghiên cứu và phi thương mại.
