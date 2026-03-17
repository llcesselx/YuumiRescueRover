/*
 * RESCUE ROVER HUB (RoverAP) - Arduino Uno R4 WiFi
 * Features: 
 * - WiFi Access Point: "RescueRover_HUB"
 * - UDP Receiver: Port 4210
 * - Motor Control: Tank Drive (L298N/L293D)
 * - LED Matrix: Status Display (M = Manual, A = Autonomous)
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
const int ENA = 3;  const int IN1 = 2;  const int IN2 = 4;
const int ENB = 5;  const int IN3 = 7;  const int IN4 = 8;
const int PUMP_PIN = 9; 

// --- OBJECTS ---
ArduinoLEDMatrix matrix;
const uint32_t manual_icon[] = { 0x6666667e, 0x66666666, 0x66000000 };
const uint32_t auto_icon[]   = { 0x183c6666, 0x7e666666, 0x66000000 };

// --- GLOBAL STATE ---
bool autonomousMode = false;
int manualL = 0, manualR = 0;
bool manualPump = false;
unsigned long packetCount = 0;

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000); 

  Serial.println("\n[SYSTEM] Hub Booting...");
  matrix.begin();
  matrix.loadFrame(manual_icon);

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  Serial.print("[WiFi] Starting AP: "); Serial.println(ssid);
  if (WiFi.beginAP(ssid, pass) != WL_AP_LISTENING) {
    Serial.println("[CRITICAL] WiFi Module Failure.");
    while (true);
  }
  
  Udp.begin(localPort);
  Serial.print("[WiFi] AP Online. IP: "); Serial.println(WiFi.localIP());
  Serial.println("[SYSTEM] Listening for UDP on Port 4210...");
}

void loop() {
  receiveUDP();

  if (!autonomousMode) {
    driveMotors(manualL, manualR);
    digitalWrite(PUMP_PIN, manualPump ? HIGH : LOW);
  } else {
    stopMotors(); 
  }
}

void receiveUDP() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    packetCount++;
    Serial.print("[UDP #"); Serial.print(packetCount);
    Serial.print("] From: "); Serial.print(Udp.remoteIP());
    Serial.print(" | Size: "); Serial.println(packetSize);

    uint8_t buffer[10];
    Udp.read(buffer, 10);
    char cmd = (char)buffer[0];

    if (cmd == 'A' && !autonomousMode) {
      autonomousMode = true;
      matrix.loadFrame(auto_icon);
      Serial.println("  >> MODE: AUTO");
    } else if (cmd == 'M' && autonomousMode) {
      autonomousMode = false;
      matrix.loadFrame(manual_icon);
      Serial.println("  >> MODE: MANUAL");
    }

    if (!autonomousMode) {
      int rawL = (buffer[1] << 8) | buffer[2];
      int rawR = (buffer[3] << 8) | buffer[4];
      manualPump = (buffer[5] == 1);
      manualL = rawL - 255; 
      manualR = rawR - 255;
      
      // Throttle logging to avoid Serial overflow
      if (packetCount % 20 == 0) {
        Serial.print("  >> Speeds: L="); Serial.print(manualL);
        Serial.print(" R="); Serial.print(manualR);
        Serial.print(" Pump="); Serial.println(manualPump ? "ON" : "OFF");
      }
    }
  }
}

void driveMotors(int left, int right) {
  digitalWrite(IN1, left > 10);
  digitalWrite(IN2, left < -10);
  analogWrite(ENA, constrain(abs(left), 0, 255));
  digitalWrite(IN3, right > 10);
  digitalWrite(IN4, right < -10);
  analogWrite(ENB, constrain(abs(right), 0, 255));
}

void stopMotors() { driveMotors(0, 0); }