/*
 * ROVER "HUB" (Arduino Nano ESP32)
 * This chip is the Brain. It creates the Wi-Fi network.
 */
#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "RescueRover_HUB";
const char* password = "password123";

WiFiUDP udp;
unsigned int localPort = 4210;

void setup() {
  Serial.begin(115200);
  delay(2000); 

  // --- START THE HUB ---
  Serial.println("\n--- Initializing Rover Hub ---");
  WiFi.softAP(ssid, password);
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("Hub SSID: "); Serial.println(ssid);
  Serial.print("Hub IP Address: "); Serial.println(myIP);
  
  udp.begin(localPort);
  Serial.println("System Ready. Waiting for Camera to join...");
}

void loop() {
  // We will add motor and sensor logic here in the next step!
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char packetBuffer[255];
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;
    Serial.print("Received Command: "); Serial.println(packetBuffer);
  }
}