
#include <WiFi.h>
#include <PubSubClient.h>

// --- ĐOẠN THÊM MỚI RFID SỐ 1: Thư viện và chân RC522 ---
#include <SPI.h>
#include <MFRC522.h>
#define RST_PIN 2
#define SS_PIN 15
MFRC522 mfrc522(SS_PIN, RST_PIN);
// ------------------------------------------------------

const char* ssid = "TP-Link_DE3A";
const char* password = "87111873";

const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "hust/iot/rccar/control_v1";

WiFiClient espClient;
PubSubClient client(espClient);

int Speed = 255; // Tốc độ khởi tạo (đã tăng tối đa 255 để tránh tiếng rít)
int enA = 5;
int enB = 23;
int IN1 = 22;
int IN2 = 21;
int IN3 = 19;
int IN4 = 18;
String currentDirection = "STOP"; // Lưu hướng di chuyển hiện tại

// --- ĐOẠN THÊM MỚI SỐ 1: Khai báo cảm biến siêu âm ---
const int TRIG_PIN = 25;
const int ECHO_PIN = 26;

const char* mqtt_topic_warn = "hust/iot/rccar/warning_v1"; // Topic gửi cảnh báo

unsigned long lastMeasureTime = 0; // Bộ đếm thời gian đo khoảng cách
// -----------------------------------------------------

// --- ĐOẠN THÊM MỚI ODOMETRY: Biến Encoder ---
const int ENCODER_PIN = 27; // Chân D27 nối với D0 của LM393
const char* mqtt_topic_telemetry = "hust/iot/rccar/telemetry_v1"; // Topic gửi tốc độ

const float WHEEL_DIAMETER = 6.5; // Đường kính bánh xe (cm)
const int ENCODER_HOLES = 20;     // Số lỗ trên đĩa encoder
const float CM_PER_PULSE = (3.14159 * WHEEL_DIAMETER) / ENCODER_HOLES; // Quãng đường 1 xung

volatile unsigned long pulseCount = 0; // Biến đếm xung ngắt
float totalDistance = 0.0;             // Tổng quãng đường (cm)
unsigned long lastOdoTime = 0;         // Bộ đếm thời gian đo tốc độ
// --------------------------------------------

// --- ĐOẠN THÊM MỚI LINE FOLLOWER SỐ 1: Khai báo 5 mắt cảm biến ---
const int IR_1 = 34; // Mắt 1: Trái ngoài cùng
const int IR_2 = 35; // Mắt 2: Trái bên trong
const int IR_3 = 32; // Mắt 3: Chính giữa
const int IR_4 = 33; // Mắt 4: Phải bên trong
const int IR_5 = 14; // Mắt 5: Phải ngoài cùng

bool isLineFollowerMode = false; // Cờ theo dõi chế độ Tự động / Bằng tay
bool obstacleWarned = false;     // Cờ báo hiệu có vật cản phía trước
// -----------------------------------------------------------------

// --- ĐOẠN THÊM MỚI RFID SỐ 2: Hệ thống Chìa Khóa ---
bool isLocked = true; // Mặc định xe BỊ KHÓA khi mới bật nguồn
String AUTHORIZED_UID = "1B 30 4D 06"; // Lát nữa bạn sẽ thay mã thẻ thật vào chữ CHUA BIET này
// --------------------------------------------------

// Điều khiển động cơ
void forward() {
  currentDirection = "FORWARD";
  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void backward() {
  currentDirection = "BACKWARD";
  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void left() {
  currentDirection = "LEFT";
  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void right() {
  currentDirection = "RIGHT";
  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void stopCar() {
  currentDirection = "STOP";
  ledcWrite(enA, 0); ledcWrite(enB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

// --- ĐOẠN THÊM MỚI SỐ 2: Hàm tính khoảng cách ---
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
  if (duration == 0) return 999.0; // Không nhận được tín hiệu
  
  return duration * 0.034 / 2;     // Trả về khoảng cách (cm)
}
// -----------------------------------------------

// --- ĐOẠN THÊM MỚI ODOMETRY: Hàm ngắt đếm xung ---
void IRAM_ATTR countPulse() {
  static unsigned long last_micros = 0;
  unsigned long current_micros = micros();
  // Bỏ qua các xung nhiễu quá gần nhau (nhỏ hơn 2ms)
  if (current_micros - last_micros > 2000) { 
    pulseCount++;
    last_micros = current_micros;
  }
}
// -------------------------------------------------

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Đang kết nối vào WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("Đã kết nối WiFi thành công!");
  Serial.print("Địa chỉ IP của xe: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }

  Serial.print("Tín hiệu nhận được từ Web: ");
  Serial.println(messageTemp);

  // --- ĐOẠN THÊM MỚI RFID SỐ 3b: Chủ động khóa xe qua nút Web ---
  if (messageTemp == "LOCK") {
    isLocked = true;
    isLineFollowerMode = false;
    stopCar();
    Serial.println("Đã KHÓA xe qua lệnh từ Web.");
    client.publish(mqtt_topic_warn, "CẢNH BÁO: Xe đang bị khóa! Quẹt thẻ để mở.");
    return;
  }
  // ----------------------------------------------------------------

  // --- ĐOẠN THÊM MỚI RFID SỐ 3: Chặn lệnh nếu xe chưa mở khóa ---
  if (isLocked) {
    Serial.println("TỪ CHỐI: Xe đang bị khóa! Vui lòng quẹt thẻ từ.");
    client.publish(mqtt_topic_warn, "CẢNH BÁO: Xe đang bị khóa! Quẹt thẻ để mở.");
    return; // Lệnh return này sẽ thoát ngay lập tức, vô hiệu hóa các nút điều khiển phía dưới
  }
  // --------------------------------------------------------------

  // --- ĐOẠN SỬA ĐỔI LINE FOLLOWER SỐ 2: Tích hợp chọn chế độ ---
  if (messageTemp == "AUTO") {
    isLineFollowerMode = true;
    Speed = 150; // Tốc độ bám vạch (đã giảm xuống chậm hơn)
    Serial.println("Chế độ: TỰ ĐỘNG BÁM VẠCH");
  } 
  else if (messageTemp == "MANUAL") {
    isLineFollowerMode = false;
    Speed = 255; // Trả lại tốc độ mặc định tối đa
    stopCar();
    Serial.println("Chế độ: ĐIỀU KHIỂN BẰNG TAY");
  }
  else if (messageTemp == "F") { isLineFollowerMode = false; Speed = 255; forward(); }
  else if (messageTemp == "B") { 
    if (obstacleWarned) stopCar(); 
    else { isLineFollowerMode = false; Speed = 255; backward(); } 
  }
  else if (messageTemp == "L") { isLineFollowerMode = false; Speed = 255; left(); }
  else if (messageTemp == "R") { isLineFollowerMode = false; Speed = 255; right(); }
  else if (messageTemp == "S") { stopCar(); }
  // -----------------------------------------------------------
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Đang kết nối tới MQTT Broker... ");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {     
      Serial.println("Thành công!");
      client.subscribe(mqtt_topic);
    } else {     
      Serial.print("Thất bại, mã lỗi (state): ");
      Serial.print(client.state());
      Serial.println(" -> Thử lại sau 5 giây");
      delay(5000);
    }
  }
}

void setup() {
  // KHỞI TẠO SERIAL (Cực kỳ quan trọng để debug)
  Serial.begin(115200);
  delay(100);
  Serial.println("Bắt đầu khởi động hệ thống xe MQTT...");

  // --- ĐOẠN THÊM MỚI RFID SỐ 4: Khởi động SPI và RC522 ---
  SPI.begin(4, 16, 13, 15);

  // Khởi tạo RC522
  mfrc522.PCD_Init();
  Serial.print("GPIO2 = ");
  Serial.println(digitalRead(RST_PIN));
  // Kiểm tra module có phản hồi không
  byte version = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);

  Serial.print("RC522 Version = 0x");
  Serial.println(version, HEX);

  if (version == 0x91 || version == 0x92)
  {
      Serial.println("RC522 OK");
  }
  else
  {
      Serial.println("RC522 KHÔNG phản hồi!");
  }

  Serial.println("Đã khởi tạo RC522. Hãy quẹt thẻ.");
  // -------------------------------------------------------

  // --- ĐOẠN THÊM MỚI SỐ 4: Thiết lập chân cảm biến ---
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  // ---------------------------------------------------

  // --- ĐOẠN THÊM MỚI ODOMETRY: Cấu hình ngắt ---
  pinMode(ENCODER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), countPulse, FALLING);
  // ---------------------------------------------

  // --- ĐOẠN THÊM MỚI LINE FOLLOWER SỐ 3: Khởi tạo chân 5 cảm biến ---
  pinMode(IR_1, INPUT);
  pinMode(IR_2, INPUT);
  pinMode(IR_3, INPUT);
  pinMode(IR_4, INPUT);
  pinMode(IR_5, INPUT);
  // ------------------------------------------------------------------

  pinMode(enA, OUTPUT); pinMode(enB, OUTPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  ledcAttach(enA, 5000, 8);
  ledcAttach(enB, 5000, 8);

  stopCar();

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); 
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // --- ĐOẠN THÊM MỚI RFID SỐ 5: Quét thẻ liên tục ---
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      uidString += String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      uidString += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidString.trim();
    uidString.toUpperCase(); // In hoa chữ cái mã thẻ

    Serial.print("Đã quẹt thẻ! Mã UID là: ");
    Serial.println(uidString);

    if (uidString == AUTHORIZED_UID) {
      isLocked = false; // MỞ KHÓA
      Serial.println("-> MÃ HỢP LỆ! XE ĐÃ ĐƯỢC MỞ KHÓA.");
      client.publish(mqtt_topic_warn, "AN TOÀN: Đã mở khóa xe thành công!");
    } else {
      Serial.println("-> SAI THẺ! Truy cập bị từ chối.");
      // Gửi mã thẻ lên khung cảnh báo của Web để bạn copy
      String msg = "CẢNH BÁO: Thẻ sai! Mã thẻ của bạn là: " + uidString;
      client.publish(mqtt_topic_warn, msg.c_str());
    }
    mfrc522.PICC_HaltA(); // Tạm dừng đọc để không bị spam tin nhắn
  }
  // --------------------------------------------------

  // --- ĐOẠN THÊM MỚI SỐ 5: Quét vật cản tự động (chu kỳ 100ms) ---
  if (millis() - lastMeasureTime > 100) {
    lastMeasureTime = millis();
    float distance = getDistance();

    static int obstacleCount = 0; // Bộ lọc nhiễu sóng siêu âm

    // HC-SR04 hay bị nhiễu sóng trả về giá trị siêu nhỏ (<2cm) khi xe chạy rung xóc
    if (distance > 2.0 && distance <= 10.0) { 
      obstacleCount++;
    } else {
      obstacleCount = 0;
    }

    if (obstacleCount >= 2) { // Phải có 2 lần liên tiếp (<10cm) mới tính là vật cản thật
      // Chỉ phanh khi xe đang LÙI (vì cảm biến ở đằng sau)
      if (currentDirection == "BACKWARD") {
        stopCar(); 
      }
      
      if (!obstacleWarned) {
        obstacleWarned = true;
        String warnMsg = "CẢNH BÁO: Phát hiện vật cản (" + String(distance) + "cm). Đã phanh khẩn cấp!";
        client.publish(mqtt_topic_warn, warnMsg.c_str());
        Serial.println("Phát hiện vật cản! Đã phanh khẩn cấp.");
        // Không tắt chế độ bám vạch nữa - khi hết vật cản xe sẽ tự chạy tiếp
      }
    } else {
      if (obstacleWarned && obstacleCount == 0) {
        obstacleWarned = false;
        String safeMsg = "AN TOÀN: Đã hết vật cản (" + String(distance) + "cm).";
        client.publish(mqtt_topic_warn, safeMsg.c_str());
        Serial.println("Đã hết vật cản.");
      }
    }
  }
  // --------------------------------------------------------------
  // --- ĐOẠN THÊM MỚI ODOMETRY: Tính Tốc độ & Quãng đường (chu kỳ 1 giây) ---
  if (millis() - lastOdoTime > 1000) {
    float deltaTime = (millis() - lastOdoTime) / 1000.0; 
    lastOdoTime = millis();

    // Dừng ngắt tạm thời để lấy số xung an toàn
    noInterrupts();
    unsigned long currentPulses = pulseCount;
    pulseCount = 0; // Reset đếm xung cho chu kỳ tới
    interrupts();

    // Tính toán quãng đường và vận tốc
    float distanceTraveled = currentPulses * CM_PER_PULSE; 
    totalDistance += distanceTraveled;                     
    float speed = distanceTraveled / deltaTime;            

    // Ghép chuỗi gửi lên MQTT: "Vận_tốc|Quãng_đường"
    String teleMsg = String(speed, 1) + "|" + String(totalDistance, 1);
    client.publish(mqtt_topic_telemetry, teleMsg.c_str());
    
    // In ra Serial Monitor
    //Serial.print("ODOMETRY - Vận tốc: "); Serial.print(speed);
    //Serial.print(" cm/s | Quãng đường: "); Serial.print(totalDistance); Serial.println(" cm");
  }
  // -------------------------------------------------------------------------

  // --- ĐOẠN THÊM MỚI LINE FOLLOWER SỐ 4: Thuật toán dò vạch 5 mắt ---
  if (isLineFollowerMode) {
    int s1 = digitalRead(IR_1); // Trái ngoài cùng
    int s2 = digitalRead(IR_2); // Trái trong
    int s3 = digitalRead(IR_3); // Giữa
    int s4 = digitalRead(IR_4); // Phải trong
    int s5 = digitalRead(IR_5); // Phải ngoài cùng

    // Debug: In giá trị 5 mắt cảm biến lên Serial Monitor (mỗi 500ms)
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 500) {
      lastDebug = millis();
      Serial.print("IR: ");
      Serial.print(s1); Serial.print(" ");
      Serial.print(s2); Serial.print(" ");
      Serial.print(s3); Serial.print(" ");
      Serial.print(s4); Serial.print(" ");
      Serial.println(s5);
    }

    // Logic bẻ lái: 0 = vạch đen, 1 = nền trắng
    // Lưu hướng rẽ cuối cùng để xử lý khi mất vạch (11111)
    static String lastTurn = "LEFT";
    static unsigned long lostLineTime = 0;

    int blackCount = (s1 == 0) + (s2 == 0) + (s3 == 0) + (s4 == 0) + (s5 == 0);

    if (blackCount >= 1 && blackCount <= 3) {
      lostLineTime = 0; // Đang thấy vạch -> reset timer

      // === ĐI THẲNG: chỉ mắt giữa thấy vạch (11011) ===
      if (s3 == 0 && s2 == 1 && s4 == 1) {
        Speed = 200; forward();
      }
      // === RẼ PHẢI: vạch lệch sang phải ===
      else if (s3 == 0 && s4 == 0) {
        // 11001 hoặc 10001 -> lệch phải nhẹ
        Speed = 200; right();
        lastTurn = "RIGHT";
      }
      else if (s4 == 0 && s3 == 1) {
        // 11101 -> vạch ở bên phải
        Speed = 200; right();
        lastTurn = "RIGHT";
      }
      else if (s5 == 0 && s4 == 1) {
        // 11110 -> vạch lệch cực phải, rẽ gắt
        Speed = 240; right();
        lastTurn = "RIGHT";
      }
      // === RẼ TRÁI: vạch lệch sang trái (đối xứng) ===
      else if (s3 == 0 && s2 == 0) {
        // 10011 hoặc 10010 -> lệch trái nhẹ
        Speed = 200; left();
        lastTurn = "LEFT";
      }
      else if (s2 == 0 && s3 == 1) {
        // 10111 -> vạch ở bên trái
        Speed = 200; left();
        lastTurn = "LEFT";
      }
      else if (s1 == 0 && s2 == 1) {
        // 01111 -> vạch lệch cực trái, rẽ gắt
        Speed = 240; left();
        lastTurn = "LEFT";
      }
    }
    // === MẤT VẠCH (11111): Quay theo hướng rẽ cuối cùng để tìm lại ===
    else if (blackCount == 0) {
      if (lostLineTime == 0) lostLineTime = millis();
      if (millis() - lostLineTime > 2000) {
        // Mất vạch quá 2 giây -> dừng hẳn (tránh quay vô hạn)
        stopCar();
        Serial.println("LINE FOLLOWER: Mất vạch quá lâu -> DỪNG!");
      } else {
        // Quay theo hướng cuối cùng để tìm lại vạch
        Speed = 200;
        if (lastTurn == "RIGHT") right();
        else left();
      }
    }
    else {
      // blackCount >= 4: Đè quá nhiều vạch (ngã tư / lỗi) -> dừng
      if (lostLineTime == 0) lostLineTime = millis();
      if (millis() - lostLineTime > 1000) {
        stopCar();
      }
    }
  }
  // ------------------------------------------------------------------

}