/*
 * RESCUE ROVER PRO CONTROLLER (Arduino Nano 33 IoT)
 * Features:
 * - WiFiNINA Library for Hub Communication
 * - Green LED: WiFi Status Heartbeat
 * - Red LED: Autonomous Mode Indicator
 * - Dual Joystick Y-Axis (Tank Drive)
 */

#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

// --- NETWORK SETTINGS ---
const char ssid[] = "RescueRover_HUB";
const char pass[] = "password123";
IPAddress hubIP(192, 168, 4, 1);
unsigned int localPort = 4210;
WiFiUDP Udp;

// --- PIN DEFINITIONS ---
const int PIN_LED_GREEN = 4;   // WiFi Heartbeat
const int PIN_LED_RED   = 5;   // Auto Mode LED
const int PIN_JOY_L_Y   = A0;  // Left Joystick
const int PIN_JOY_R_Y   = A1;  // Right Joystick
const int PIN_SW_AUTO   = 2;   // Mode Toggle Switch
const int PIN_BTN_PUMP  = 3;   // Water Button

unsigned long lastBlink = 0;
bool ledState = false;

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_SW_AUTO, INPUT_PULLUP);
  pinMode(PIN_BTN_PUMP, INPUT_PULLUP);

  // Check for the WiFi module
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  Serial.println("Nano 33 IoT: Searching for Rover Hub...");
}

void loop() {
  handleConnectivityLED();
  
  bool isAuto = (digitalRead(PIN_SW_AUTO) == LOW);
  bool pumpBtn = (digitalRead(PIN_BTN_PUMP) == LOW);
  digitalWrite(PIN_LED_RED, isAuto ? HIGH : LOW);

  // Read Joysticks (Nano 33 IoT ADC is 10-bit 0-1023)
  // Map to 0-510 for our -255 to 255 math on the Hub
  int valL = map(analogRead(PIN_JOY_L_Y), 0, 1023, 0, 510);
  int valR = map(analogRead(PIN_JOY_R_Y), 0, 1023, 0, 510);

  if (WiFi.status() == WL_CONNECTED) {
    sendPacket(isAuto, valL, valR, pumpBtn);
  } else {
    // Attempt to reconnect if lost
    WiFi.begin(ssid, pass);
  }

  delay(30); 
}

void handleConnectivityLED() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(PIN_LED_GREEN, HIGH);
  } else {
    if (millis() - lastBlink > 500) {
      ledState = !ledState;
      digitalWrite(PIN_LED_GREEN, ledState ? HIGH : LOW);
      lastBlink = millis();
    }
  }
}

void sendPacket(bool autoMode, int l, int r, bool pump) {
  Udp.beginPacket(hubIP, localPort);
  
  uint8_t packet[6];
  packet[0] = autoMode ? 'A' : 'M';
  packet[1] = (l >> 8) & 0xFF;
  packet[2] = l & 0xFF;
  packet[3] = (r >> 8) & 0xFF;
  packet[4] = r & 0xFF;
  packet[5] = pump ? 1 : 0;

  Udp.write(packet, 6);
  Udp.endPacket();
}