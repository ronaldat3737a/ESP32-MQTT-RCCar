#include <WiFi.h>
#include <PubSubClient.h>

// --- ĐOẠN THÊM MỚI RFID SỐ 1: Thư viện và chân RC522 ---
#include <SPI.h>
#include <MFRC522.h>
#define RST_PIN 2
#define SS_PIN 15
MFRC522 mfrc522(SS_PIN, RST_PIN);
// ------------------------------------------------------

const char* ssid = "Tt";
const char* password = "abcdefgh";

const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "hust/iot/rccar/control_v1";

WiFiClient espClient;
PubSubClient client(espClient);

// ===== QUẢN LÝ 4 CẤP TỐC ĐỘ =====
int Speed = 140;       // tốc độ mặc định

int speedLevel = 2;    // mức hiện tại

int speedTable[5] = {
  0,
  90,    // Level 1
  140,   // Level 2
  190,   // Level 3
  230    // Level 4
};
// =================================
int enA = 5;
int enB = 23;
int IN1 = 22;
int IN2 = 21;
int IN3 = 19;
int IN4 = 18;

String currentDirection = "STOP";

// --- ĐOẠN THÊM MỚI SỐ 1: Khai báo cảm biến siêu âm ---
const int TRIG_PIN = 25;
const int ECHO_PIN = 26;

const char* mqtt_topic_warn = "hust/iot/rccar/warning_v1"; // Topic gửi cảnh báo

bool isBlocked = false;            // Cờ đánh dấu xe đang bị chặn
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
// -----------------------------------------------------------------

// --- ĐOẠN THÊM MỚI RFID SỐ 2: Hệ thống Chìa Khóa ---
bool isLocked = true; // Mặc định xe BỊ KHÓA khi mới bật nguồn
String AUTHORIZED_UID = "1B 30 4D 06"; // Lát nữa bạn sẽ thay mã thẻ thật vào chữ CHUA BIET này
// --------------------------------------------------

// Điều khiển động cơ
void forward() {
  currentDirection="FORWARD";

  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void backward() {
  currentDirection="BACKWARD";

  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void left() {
  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void right() {
  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void stopCar() {

  Serial.println("==== STOP CAR ====");

  currentDirection = "STOP";

  ledcWrite(enA,0);
  ledcWrite(enB,0);

  digitalWrite(IN1,LOW);
  digitalWrite(IN2,LOW);

  digitalWrite(IN3,LOW);
  digitalWrite(IN4,LOW);
}

// ===== SET SPEED =====
void setSpeedLevel(int level)
{
  if(level < 1) level = 1;
  if(level > 4) level = 4;

  speedLevel = level;
  Speed = speedTable[level]; // ti mở ra
  Serial.print("Speed Level = ");
  Serial.print(level);
  Serial.print(" PWM=");
  Serial.println(Speed);
}
// ====================

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
  pulseCount++;
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

  // ===== ĐIỀU KHIỂN 4 CẤP TỐC =====
  if(messageTemp == "SPEED1")
  {
    setSpeedLevel(1);
    return;
  }

  else if(messageTemp == "SPEED2")
  {
    setSpeedLevel(2);
    return;
  }

  else if(messageTemp == "SPEED3")
  {
    setSpeedLevel(3);
    return;
  }

  else if(messageTemp == "SPEED4")
  {
    setSpeedLevel(4);
    return;
  }

  // ================================

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
    Speed = speedTable[1];// Chạy chậm để bám vạch chính xác
    Serial.println("Chế độ: TỰ ĐỘNG BÁM VẠCH");
  } 
  else if (messageTemp == "MANUAL") {
    isLineFollowerMode = false;
    setSpeedLevel(speedLevel);// Trả lại tốc độ tối đa

    Serial.println("Chế độ: ĐIỀU KHIỂN BẰNG TAY");
  }
  else if (messageTemp == "F") {
      isLineFollowerMode = false;
      // Đi tiến không bị giới hạn
      forward();
  }
  else if (messageTemp == "B") {
      isLineFollowerMode = false;
      // Chỉ chặn khi lùi
      if(!isBlocked)
      {
          backward();
      }
      else
      {
          stopCar();
          Serial.println("Từ chối lùi: Có vật cản phía sau!");
      }
  }
  else if (messageTemp == "L") {
      isLineFollowerMode = false;
      left();
  }
  else if (messageTemp == "R") {
      isLineFollowerMode = false;
      right();
  }
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

  // Chỉ kiểm tra vật cản khi xe đang LÙI
  if (currentDirection == "BACKWARD" 
      && distance > 0 
      && distance <= 40.0) {
      if (!isBlocked) {
          stopCar();
          isBlocked = true;
          String warnMsg = 
          "CẢNH BÁO: Không thể lùi! Vật cản cách "
          + String(distance) 
          + " cm";
          client.publish(
              mqtt_topic_warn,
              warnMsg.c_str()
          );
          Serial.print("Đã dừng khi lùi. Khoảng cách: ");
          Serial.print(distance);
          Serial.println(" cm");
      }
  }
  else {
      if (isBlocked) {
          isBlocked = false;
          String safeMsg =
          "AN TOÀN: Có thể lùi lại. Khoảng cách: "
          + String(distance)
          + " cm";
          client.publish(
              mqtt_topic_warn,
              safeMsg.c_str()
          );
          Serial.println(
          "Hết vật cản, cho phép lùi."
          );
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
  if (isLineFollowerMode && !isBlocked) {
    int s1 = digitalRead(IR_1); 
    int s2 = digitalRead(IR_2); 
    int s3 = digitalRead(IR_3); 
    int s4 = digitalRead(IR_4); 
    int s5 = digitalRead(IR_5); 

    // Logic bẻ lái: 1 là vạch đen, 0 là nền trắng
    if (s3 == 1) {
      Speed = speedTable[1]; forward(); // Xe ở giữa -> Đi thẳng
    } else if (s2 == 1) {
      Speed = speedTable[1]; left();    // Lệch phải -> Vạch ở bên trái -> Rẽ trái
    } else if (s4 == 1) {
      Speed = speedTable[1]; right();   // Lệch trái -> Vạch ở bên phải -> Rẽ phải
    } else if (s1 == 1) {
      Speed = speedTable[2]; left();    // Lệch cực phải -> Rẽ gắt trái
    } else if (s5 == 1) {
      Speed = speedTable[2]; right();   // Lệch cực trái -> Rẽ gắt phải
    } else if (s1 == 0 && s2 == 0 && s3 == 0 && s4 == 0 && s5 == 0) {
      stopCar();              // Trắng tinh -> Mất vạch -> Phanh
    }
  } else if (isLineFollowerMode && isBlocked) {
    stopCar(); // Đang bám vạch mà gặp vật cản thì phanh lại
  }
  // ------------------------------------------------------------------
}
