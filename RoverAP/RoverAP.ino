/*
 * RESCUE ROVER HUB (Arduino Uno R4 WiFi)
 * Features: 
 * - WiFi Access Point (WiFiS3 Library)
 * - 5V Logic (Ideal for L298N/L293D)
 * - 3x VL53L0X ToF Sensors (Obstacle Avoidance)
 * - Flame Sensing & Pump Control
 * - LED Matrix Status (A = Auto, M = Manual)
 */

#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "Adafruit_VL53L0X.h"
#include "Arduino_LED_Matrix.h"

// --- NETWORK ---
const char ssid[] = "RescueRover_HUB";
const char pass[] = "password123";
int status = WL_IDLE_STATUS;
WiFiUDP Udp;
unsigned int localPort = 4210;

// --- PIN DEFINITIONS (Uno R4 WiFi) ---
const int ENA = 3;  const int IN1 = 2;  const int IN2 = 4;  // Left Track (PWM on 3)
const int ENB = 5;  const int IN3 = 7;  const int IN4 = 8;  // Right Track (PWM on 5)
const int PUMP_PIN = 9; 
const int FLAME_PIN = 10;

// ToF XSHUT pins (for address assignment)
const int SHT_L = 11; const int SHT_C = 12; const int SHT_R = 13;

// --- SENSOR & MATRIX OBJECTS ---
Adafruit_VL53L0X loxL = Adafruit_VL53L0X();
Adafruit_VL53L0X loxC = Adafruit_VL53L0X();
Adafruit_VL53L0X loxR = Adafruit_VL53L0X();
ArduinoLEDMatrix matrix; // Corrected class name (removed underscore)

// Matrix Frames
const uint32_t manual_icon[] = { 0x6666667e, 0x66666666, 0x66000000 }; // 'M'
const uint32_t auto_icon[]   = { 0x183c6666, 0x7e666666, 0x66000000 }; // 'A'

// --- STATE ---
bool autonomousMode = false;
int manualL = 0, manualR = 0;
bool manualPump = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  matrix.begin();

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FLAME_PIN, INPUT);

  // Initialize ToF Sensors (Sequential XSHUT Logic)
  pinMode(SHT_L, OUTPUT); pinMode(SHT_C, OUTPUT); pinMode(SHT_R, OUTPUT);
  digitalWrite(SHT_L, LOW); digitalWrite(SHT_C, LOW); digitalWrite(SHT_R, LOW);
  delay(10);
  
  digitalWrite(SHT_L, HIGH); delay(10); loxL.begin(0x30);
  digitalWrite(SHT_C, HIGH); delay(10); loxC.begin(0x31);
  digitalWrite(SHT_R, HIGH); delay(10); loxR.begin(0x32);

  // Start Access Point
  Serial.print("Creating AP: "); Serial.println(ssid);
  status = WiFi.beginAP(ssid, pass);
  if (status != WL_AP_LISTENING) {
    Serial.println("AP Failed");
    while (true);
  }
  
  Udp.begin(localPort);
  matrix.loadFrame(manual_icon);
  Serial.println("Uno R4 WiFi HUB Online");
}

void loop() {
  receiveUDP();

  if (autonomousMode) {
    runAutonomousLogic();
  } else {
    driveMotors(manualL, manualR);
    digitalWrite(PUMP_PIN, manualPump ? HIGH : LOW);
  }
}

void receiveUDP() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    uint8_t buffer[10];
    Udp.read(buffer, 10);
    char cmd = (char)buffer[0];

    if (cmd == 'A' && !autonomousMode) {
      autonomousMode = true;
      matrix.loadFrame(auto_icon);
    }
    if (cmd == 'M' && autonomousMode) {
      autonomousMode = false;
      matrix.loadFrame(manual_icon);
    }

    if (!autonomousMode) {
      int l = (buffer[1] << 8) | buffer[2];
      int r = (buffer[3] << 8) | buffer[4];
      manualL = l - 255; 
      manualR = r - 255;
      manualPump = (buffer[5] == 1);
    }
  }
}

void runAutonomousLogic() {
  VL53L0X_RangingMeasurementData_t measureL, measureC, measureR;
  loxL.rangingTest(&measureL, false);
  loxC.rangingTest(&measureC, false);
  loxR.rangingTest(&measureR, false);

  if (digitalRead(FLAME_PIN) == LOW) { // Flame Detected
    stopMotors();
    digitalWrite(PUMP_PIN, HIGH);
    delay(2000); 
    digitalWrite(PUMP_PIN, LOW);
  } else {
    // Obstacle Avoidance
    if (measureC.RangeMilliMeter < 200) {
      if (measureL.RangeMilliMeter > measureR.RangeMilliMeter) driveMotors(-150, 150);
      else driveMotors(150, -150);
    } else {
      driveMotors(180, 180);
    }
  }
}

void driveMotors(int left, int right) {
  digitalWrite(IN1, left > 0); digitalWrite(IN2, left < 0);
  analogWrite(ENA, constrain(abs(left), 0, 255));
  digitalWrite(IN3, right > 0); digitalWrite(IN4, right < 0);
  analogWrite(ENB, constrain(abs(right), 0, 255));
}

void stopMotors() { driveMotors(0, 0); }