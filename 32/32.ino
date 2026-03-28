#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Keypad.h>

// --- 1. ข้อมูลการเชื่อมต่อ ---
#define WIFI_SSID "Noey"
#define WIFI_PASSWORD "0611237530"
#define FIREBASE_HOST "smart-parking-iot-fd3c3-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "s42dh85GEuvMT6orG3IBwtQgoAWt7zirnr6tPkKQ"

// --- 2. ขาใช้งาน ---
#define GAS_SENSOR_PIN 34  
#define TRIG_PIN       21  
#define ECHO_PIN       22  
#define BUZZER_PIN      5  
#define SERVO_PIN      25  

#define GAS_THRESHOLD  300 
#define DISTANCE_OPEN  10  

// --- 3. อุปกรณ์ ---
Servo gateServo;
Adafruit_ILI9341 tft = Adafruit_ILI9341(15, 2, 4); 

const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
byte rowPins[ROWS] = {13, 12, 14, 27}; 
byte colPins[COLS] = {26, 33, 32, 35}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastCheckTime = 0;
const long checkInterval = 1000; 
String inputCode = "";
bool isEmergency = false;

void setup() {
  Serial.begin(115200);
  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  gateServo.attach(SERVO_PIN);
  gateServo.write(5); 

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  showIdleScreen();
  Serial.println("\n--- System Ready | ลองกดปุ่ม D แล้วดู Serial Monitor นะครับ ---");
}

void loop() {
  char key = keypad.getKey();
  if (key && !isEmergency) {
    Serial.print("Pressed Key: "); Serial.println(key); // เช็คว่ากดปุ่มไหน
    handleKeypad(key);
  }

  if (millis() - lastCheckTime >= checkInterval) {
    runSystemChecks();
    lastCheckTime = millis();
  }
}

void runSystemChecks() {
  int gasValue = analogRead(GAS_SENSOR_PIN);
  if (gasValue > GAS_THRESHOLD) {
    tone(BUZZER_PIN, 2000); 
    if (!isEmergency) {
      isEmergency = true;
      triggerEmergencyDisplay();
      gateServo.write(90); 
    }
  } else {
    noTone(BUZZER_PIN); 
    if (isEmergency) {
      isEmergency = false;
      gateServo.write(0);
      showIdleScreen();
    }
    
    long distance = getDistance();
    if (distance > 0 && distance <= DISTANCE_OPEN) {
      openGate(); 
    }
  }
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return 999;
  return (duration * 0.034) / 2;
}

void openGate() {
  tft.fillRect(0, 150, 320, 90, ILI9341_BLACK);
  tft.setCursor(40, 180); tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(3); tft.println("GATE OPENING");
  
  gateServo.attach(SERVO_PIN);
  gateServo.write(90);         
  delay(5000);                 
  gateServo.write(0);          
  showIdleScreen();
}

void handleKeypad(char key) {
  if (key >= '0' && key <= '9') {
    if (inputCode.length() < 6) {
      inputCode += key;
      updateDisplayCode();
    }
  } 
  else if (key == 'D') { // ปุ่มลบ
    if (inputCode.length() > 0) {
      inputCode.remove(inputCode.length() - 1); // ตัดตัวท้ายออก
      
    }
  }
  else if (key == '#') {
    verifyBookingCode();
  } 
  else if (key == '*') {
    inputCode = "";
    showIdleScreen();
  }
}

void verifyBookingCode() {
  tft.fillRect(0, 150, 320, 90, ILI9341_BLACK);
  tft.setCursor(60, 190); tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2); tft.println("VERIFYING...");

  bool found = false;
  for (int i = 1; i <= 4; i++) {
    String path = "/parking_slots/slot_0" + String(i) + "/booking_code";
    if (Firebase.getString(fbdo, path)) {
      String dbCode = fbdo.stringData();
      dbCode.trim();
      if (dbCode == inputCode && inputCode != "") {
        found = true;
        break;
      }
    }
  }

  if (found) {
    tone(BUZZER_PIN, 2500); delay(150); noTone(BUZZER_PIN);
    openGate(); 
  } else {
    tone(BUZZER_PIN, 500); delay(500); noTone(BUZZER_PIN); 
    tft.fillRect(0, 150, 320, 90, ILI9341_BLACK);
    tft.setCursor(60, 195); tft.setTextColor(ILI9341_RED);
    tft.println("INVALID CODE!");
    delay(1500);
    showIdleScreen();
  }
  inputCode = "";
}

void showIdleScreen() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(65, 40); tft.println("SMART PARKING");
  tft.drawFastHLine(20, 70, 280, ILI9341_WHITE);
  tft.setCursor(50, 100); tft.println("ENTER BOOKING CODE:");
  tft.drawRect(60, 130, 200, 45, ILI9341_WHITE);
}

void updateDisplayCode() {
  // ไม่ต้องล้าง Rect ตรงนี้แล้ว เพราะล้างใน handleKeypad ไปแล้วเพื่อความเร็ว
  tft.setCursor(100, 142); 
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(3); 
  tft.println(inputCode);
}

void triggerEmergencyDisplay() {
  tft.fillScreen(ILI9341_RED);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setCursor(45, 100); tft.println("!! GAS LEAK !!");
}