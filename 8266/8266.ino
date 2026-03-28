#define FIREBASE_DISABLE_SD_FAT 
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

// --- 1. ข้อมูลเชื่อมต่อ ---
#define WIFI_SSID "Noey"
#define WIFI_PASSWORD "0611237530"
#define FIREBASE_HOST "smart-parking-iot-fd3c3-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "s42dh85GEuvMT6orG3IBwtQgoAWt7zirnr6tPkKQ"

// --- 2. ตั้งค่า Pin สำหรับ 2 ช่องจอด ---
const int numSlots = 2;
String slotNames[] = {"slot_01", "slot_02"};

// ลำดับขา: {Slot 01, Slot 02}
int trigPins[]  = {D5, D0};
int echoPins[]  = {D6, D4};
int greenPins[] = {D1, D7};
int redPins[]   = {D3, D8};

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastBlinkTime = 0;
bool blinkState = LOW;

void setup() {
  Serial.begin(115200);
  
  for (int i = 0; i < numSlots; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
    pinMode(greenPins[i], OUTPUT);
    pinMode(redPins[i], OUTPUT);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Serial.println("\nWiFi Connected & Firebase Ready");
}

long getDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 30000); // timeout 30ms
  if (duration == 0) return 999; 
  return (duration / 2) / 29.1;
}

void setLEDColor(int idx, String color) {
  if (color == "RED") {
    digitalWrite(greenPins[idx], LOW);
    digitalWrite(redPins[idx], HIGH);
  } else if (color == "GREEN") {
    digitalWrite(greenPins[idx], HIGH);
    digitalWrite(redPins[idx], LOW);
  } else if (color == "YELLOW") {
    digitalWrite(greenPins[idx], HIGH);
    digitalWrite(redPins[idx], HIGH);
  } else {
    digitalWrite(greenPins[idx], LOW);
    digitalWrite(redPins[idx], LOW);
  }
}

void loop() {
  // จัดการจังหวะกระพริบ
  if (millis() - lastBlinkTime > 500) {
    lastBlinkTime = millis();
    blinkState = !blinkState;
  }

  for (int i = 0; i < numSlots; i++) {
    long distance = getDistance(trigPins[i], echoPins[i]);
    
    String path = "/parking_slots/" + slotNames[i];
    String status = "";
    bool blinkCmd = false;

    // อ่านค่าจาก Firebase
    if (Firebase.getString(fbdo, path + "/status")) status = fbdo.stringData();
    if (Firebase.getBool(fbdo, path + "/blink")) blinkCmd = fbdo.boolData();

    // Logic ควบคุม
    if (distance > 0 && distance < 10) { 
      // มีรถจอด (แดง)
      setLEDColor(i, "RED");
      if (status != "occupied") {
        Firebase.setString(fbdo, path + "/status", "occupied");
        Firebase.setBool(fbdo, path + "/blink", false);
      }
    } 
    else if (blinkCmd) {
      // เช็คอินแล้ว (เหลืองกระพริบ)
      if (blinkState) setLEDColor(i, "YELLOW");
      else setLEDColor(i, "OFF");
    } 
    else if (status == "booked") {
      // จองแล้ว (เหลืองค้าง)
      setLEDColor(i, "YELLOW");
    } 
    else {
      // ว่าง (เขียว)
      setLEDColor(i, "GREEN");
      if (status == "occupied") {
        Firebase.setString(fbdo, path + "/status", "available");
      }
    }
    yield(); // ให้เวลา WiFi ทำงาน
  }
  delay(100); 
}