#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "TP-Link_6475";
const char* password = "34954928";

const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "hust/iot/rccar/control_v1";

WiFiClient espClient;
PubSubClient client(espClient);

int Speed = 255;
int enA = 5;
int enB = 23;
int IN1 = 22;
int IN2 = 21;
int IN3 = 19;
int IN4 = 18;

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

// Điều khiển động cơ
void forward() {
  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void backward() {
  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void left() {
  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void right() {
  ledcWrite(enA, Speed); ledcWrite(enB, Speed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void stopCar() {
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

  // Báo ra màn hình để biết xe đã nhận được lệnh
  Serial.print("Tín hiệu nhận được từ Web: ");
  Serial.println(messageTemp);

  // --- ĐOẠN THÊM MỚI SỐ 3: Kiểm tra điều kiện an toàn ---
  if (messageTemp == "F") {
    if (!isBlocked) {
      forward();
    } else {
      Serial.println("Từ chối lệnh TIẾN: Phía trước có vật cản!");
    }
  }
  // ------------------------------------------------------
  else if (messageTemp == "B") backward();
  else if (messageTemp == "L") left();
  else if (messageTemp == "R") right();
  else if (messageTemp == "S") stopCar();
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

  // --- ĐOẠN THÊM MỚI SỐ 4: Thiết lập chân cảm biến ---
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  // ---------------------------------------------------

  // --- ĐOẠN THÊM MỚI ODOMETRY: Cấu hình ngắt ---
  pinMode(ENCODER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), countPulse, FALLING);
  // ---------------------------------------------

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
  // --- ĐOẠN THÊM MỚI SỐ 5: Quét vật cản tự động (chu kỳ 100ms) ---
  if (millis() - lastMeasureTime > 100) {
    lastMeasureTime = millis();
    float distance = getDistance();

    if (distance > 0 && distance <= 40.0) { // Khoảng cách nhỏ hơn 40cm
      if (!isBlocked) {
        stopCar(); // Tự động phanh lập tức
        isBlocked = true;
        String warnMsg = "CẢNH BÁO: Phát hiện vật cản! Khoảng cách: " + String(distance) + " cm";
        client.publish(mqtt_topic_warn, warnMsg.c_str());
        Serial.print("CẢNH BÁO: Đã phanh xe! Khoảng cách đo được: ");
        Serial.print(distance);
        Serial.println(" cm");
      }
    } else {
      if (isBlocked) { // Đã hết vật cản
        isBlocked = false;
        String safeMsg = "AN TOÀN: Hết vật cản. Khoảng cách: " + String(distance) + " cm";
        client.publish(mqtt_topic_warn, safeMsg.c_str());
        Serial.println("AN TOÀN: Hết vật cản, mở khóa điều khiển.");
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
    Serial.print("ODOMETRY - Vận tốc: "); Serial.print(speed);
    Serial.print(" cm/s | Quãng đường: "); Serial.print(totalDistance); Serial.println(" cm");
  }
  // -------------------------------------------------------------------------

}