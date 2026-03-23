/*
 * RESCUE ROVER HUB (RoverAP) - Arduino Uno R4 WiFi
 * Features: 
 * - WiFi Access Point: "RescueRover_HUB"
 * - UDP Receiver: Port 4210
 * - Motor Control: 4WD Wheeled Skid Steer
 * - Sensors: 1x Flame Sensor (Pin 10), 3x ToF (VL53L0X)
 * - MOSFET Pump Control: Pin 9 (Logic Level)
 * - FIX: Flattened Sensor Init to prevent "Boot Hangs"
 */

#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "Adafruit_VL53L0X.h"
#include "Arduino_LED_Matrix.h"

// --- NETWORK SETTINGS ---
const char ssid[] = "RescueRover_HUB";
const char pass[] = "password123";
WiFiUDP Udp;
unsigned int localPort = 4210;

// --- PIN DEFINITIONS ---
const int ENA = 3;  const int IN1 = 2;  const int IN2 = 4; // Left Side
const int ENB = 5;  const int IN3 = 7;  const int IN4 = 8; // Right Side
const int PUMP_PIN = 9;  
const int FLAME_PIN = 10; 
const int SHT_L = 11; const int SHT_C = 12; const int SHT_R = 13; 

// --- TORQUE TUNING ---
const int MIN_PWM = 130; 

// --- OBJECTS ---
Adafruit_VL53L0X loxL = Adafruit_VL53L0X();
Adafruit_VL53L0X loxC = Adafruit_VL53L0X();
Adafruit_VL53L0X loxR = Adafruit_VL53L0X();
ArduinoLEDMatrix matrix;


// --- 12x8 LED MATRIX ICONS (M and A) ---
const uint32_t manual_icon[] = {
    0x6666667e,
    0x66666666,
    0x66000000
}; // 'M'

const uint32_t auto_icon[] = {
    0x183c6666,
    0x7e666666,
    0x66000000
}; // 'A'

// --- GLOBAL STATE ---
bool autonomousMode = false;
int manualL = 0, manualR = 0;
bool manualPump = false;
bool sensorsDetected = false;
unsigned long lastHeartbeat = 0;

void setup() {
  Serial.begin(115200);
  
  // Wait for Serial to open for debugging
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000); 

  Serial.println("\n[BOOT] Rescue Rover Hub Initializing...");
  
  // 1. Initialize Visuals and Motors immediately
  matrix.begin();
  matrix.loadFrame(manual_icon);

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pinMode(FLAME_PIN, INPUT);

  // 2. START WIFI (High Priority)
  Serial.print("[WiFi] Starting Access Point: "); Serial.println(ssid);
  if (WiFi.beginAP(ssid, pass) != WL_AP_LISTENING) {
    Serial.println("[ERROR] WiFi Module failed. Check power.");
  } else {
    Serial.print("[WiFi] Online. Connect to: "); Serial.println(ssid);
    Serial.print("[WiFi] Hub IP: "); Serial.println(WiFi.localIP());
  }
  Udp.begin(localPort);

  // 3. ATTEMPT SENSOR INIT (Robust / Non-Blocking)
  Serial.println("[SENSORS] Initializing ToF Array...");
  Wire.begin();
  pinMode(SHT_L, OUTPUT); pinMode(SHT_C, OUTPUT); pinMode(SHT_R, OUTPUT);
  digitalWrite(SHT_L, LOW); digitalWrite(SHT_C, LOW); digitalWrite(SHT_R, LOW);
  delay(50);
  
  // Try Left
  digitalWrite(SHT_L, HIGH); delay(10);
  if(loxL.begin(0x30)) {
    Serial.println("  > Left ToF: OK");
    sensorsDetected = true; 
  } else {
    Serial.println("  > Left ToF: NOT FOUND");
  }

  // Try Center
  digitalWrite(SHT_C, HIGH); delay(10);
  if(loxC.begin(0x31)) {
    Serial.println("  > Center ToF: OK");
    sensorsDetected = true;
  } else {
    Serial.println("  > Center ToF: NOT FOUND");
  }

  // Try Right
  digitalWrite(SHT_R, HIGH); delay(10);
  if(loxR.begin(0x32)) {
    Serial.println("  > Right ToF: OK");
    sensorsDetected = true;
  } else {
    Serial.println("  > Right ToF: NOT FOUND");
  }

  Serial.println("[SYSTEM] Boot Complete. Awaiting Controller Packets...");
}

void loop() {
  receiveUDP();

  // Heartbeat: Blink TX LED by printing to serial every 2 seconds
  if (millis() - lastHeartbeat > 2000) {
    Serial.print("[STATUS] Hub Listening... WiFi: ");
    Serial.println(WiFi.status() == WL_AP_LISTENING ? "AP OK" : "AP ERROR");
    lastHeartbeat = millis();
  }

  if (autonomousMode && sensorsDetected) {
    runAutonomousLogic();
  } else {
    // If user flips to Auto but sensors are missing, force back to manual
    if(autonomousMode && !sensorsDetected) {
      autonomousMode = false;
      matrix.loadFrame(manual_icon);
      Serial.println("[SAFETY] Auto Mode Denied: No sensors found during boot.");
    }
    driveMotors(manualL, manualR);
    digitalWrite(PUMP_PIN, manualPump ? HIGH : LOW);
  }
}

void receiveUDP() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    uint8_t buffer[10];
    int len = Udp.read(buffer, 10);
    
    char cmd = (char)buffer[0];

    // Debug print to trigger TX LED on receipt
    Serial.print("[UDP] Packet from: "); Serial.print(Udp.remoteIP());
    Serial.print(" Cmd: "); Serial.println(cmd);

    if (cmd == 'A' && !autonomousMode) {
      autonomousMode = true;
      matrix.loadFrame(auto_icon);
    } else if (cmd == 'M' && autonomousMode) {
      autonomousMode = false;
      matrix.loadFrame(manual_icon);
    }

    if (!autonomousMode) {
      int rawL = (buffer[1] << 8) | buffer[2];
      int rawR = (buffer[3] << 8) | buffer[4];
      manualPump = (buffer[5] == 1);
      manualL = rawL - 255; 
      manualR = rawR - 255;
    }
  }
}

void runAutonomousLogic() {
  if (digitalRead(FLAME_PIN) == LOW) { 
    stopMotors();
    digitalWrite(PUMP_PIN, HIGH);
    delay(2000); 
    digitalWrite(PUMP_PIN, LOW);
    return;
  }

  VL53L0X_RangingMeasurementData_t measureL, measureC, measureR;
  loxL.rangingTest(&measureL, false);
  loxC.rangingTest(&measureC, false);
  loxR.rangingTest(&measureR, false);

  int distC = (measureC.RangeStatus != 4) ? measureC.RangeMilliMeter : 1000;
  int distL = (measureL.RangeStatus != 4) ? measureL.RangeMilliMeter : 1000;
  int distR = (measureR.RangeStatus != 4) ? measureR.RangeMilliMeter : 1000;

  if (distC < 250) { 
    if (distL > distR) driveMotors(-160, 160); 
    else driveMotors(160, -160);               
  } else {
    driveMotors(180, 180); 
  }
}

void driveMotors(int left, int right) {
  int pwrL = 0, pwrR = 0;
  
  if (abs(left) > 15) {
    pwrL = map(abs(left), 0, 255, MIN_PWM, 255);
    if (left < 0) pwrL *= -1;
  }
  
  if (abs(right) > 15) {
    pwrR = map(abs(right), 0, 255, MIN_PWM, 255);
    if (right < 0) pwrR *= -1;
  }
  
  digitalWrite(IN1, pwrL > 0);
  digitalWrite(IN2, pwrL < 0);
  analogWrite(ENA, abs(pwrL));
  
  digitalWrite(IN3, pwrR > 0);
  digitalWrite(IN4, pwrR < 0);
  analogWrite(ENB, abs(pwrR));
}

void stopMotors() { driveMotors(0, 0); }