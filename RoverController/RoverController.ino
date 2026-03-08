/*
 * RESCUE ROVER CONTROLLER - Arduino Nano 33 IoT
 * FIX: Added Udp.begin() and connection management
 */

#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

const char ssid[] = "RescueRover_HUB";
const char pass[] = "password123";
IPAddress hubIP(192, 168, 4, 1);
unsigned int localPort = 4210; // The port we send FROM
WiFiUDP Udp;

const int PIN_LED_GREEN = 4;   
const int PIN_LED_RED   = 5;   
const int PIN_JOY_L_Y   = A0;  
const int PIN_JOY_R_Y   = A1;  
const int PIN_SW_AUTO   = 2;   
const int PIN_BTN_PUMP  = 3;   

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_SW_AUTO, INPUT_PULLUP);
  pinMode(PIN_BTN_PUMP, INPUT_PULLUP);

  Serial.println("Attempting to connect to RescueRover_HUB...");
  
  // Attempt connection once at start
  WiFi.begin(ssid, pass);
  
  // Open the UDP socket! (Crucial for Nano 33 IoT)
  Udp.begin(localPort);
}

void loop() {
  int status = WiFi.status();

  if (status == WL_CONNECTED) {
    digitalWrite(PIN_LED_GREEN, HIGH);
    
    bool isAuto = (digitalRead(PIN_SW_AUTO) == LOW);
    bool pumpBtn = (digitalRead(PIN_BTN_PUMP) == LOW);
    digitalWrite(PIN_LED_RED, isAuto ? HIGH : LOW);

    int valL = map(analogRead(PIN_JOY_L_Y), 0, 1023, 510, 0);
    int valR = map(analogRead(PIN_JOY_R_Y), 0, 1023, 510, 0);

    // Send packet
    if (Udp.beginPacket(hubIP, 4210)) {
      uint8_t packet[6];
      packet[0] = isAuto ? 'A' : 'M';
      packet[1] = (valL >> 8) & 0xFF;
      packet[2] = valL & 0xFF;
      packet[3] = (valR >> 8) & 0xFF;
      packet[4] = valR & 0xFF;
      packet[5] = pumpBtn ? 1 : 0;
      
      Udp.write(packet, 6);
      Udp.endPacket();
      
      // Serial Debug
      static unsigned long lastPrint = 0;
      if (millis() - lastPrint > 500) {
        Serial.print("Packet Sent! L:"); Serial.print(valL-255);
        Serial.print(" R:"); Serial.println(valR-255);
        lastPrint = millis();
      }
    } else {
      Serial.println("Error: UDP beginPacket failed!");
    }
  } else {
    // If not connected, blink and try to reconnect slowly
    digitalWrite(PIN_LED_GREEN, LOW);
    Serial.println("WiFi Disconnected. Reconnecting...");
    WiFi.begin(ssid, pass);
    delay(2000); 
  }
  
  delay(30); 
}