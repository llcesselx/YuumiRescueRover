/* * DEEP DIAGNOSTICS VISION FIRMWARE (ESP32-CAM)
 * Logic: Detailed Error Reporting to pinpoint Power vs. Connection issues.
 */

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- I2C LCD SETTINGS ---
#define I2C_SDA 14
#define I2C_SCL 15
LiquidCrystal_I2C lcd(0x27, 16, 2); 

const char* ssid = "RescueRover_HUB"; 
const char* password = "password123";

// AI-THINKER PINOUT
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define V_SYNC_GPIO_NUM   25
#define H_REF_GPIO_NUM    23
#define PCLK_GPIO_NUM     22

// FLASH LED PIN (Built-in high brightness LED on GPIO 4)
#define FLASH_GPIO_NUM     4

httpd_handle_t stream_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    char * part_buf[64];
    static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
    static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
    static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    while(true) {
        fb = esp_camera_fb_get();
        if (!fb) { res = ESP_FAIL; break; }
        size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        if(res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if(res == ESP_OK) res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        esp_camera_fb_return(fb);
        if(res != ESP_OK) break;
    }
    return res;
}

void setup() {
  // 1. STABILIZE POWER & RESET HARDWARE
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  // Explicitly toggle PWDN (Pin 32) to reset the camera hardware state
  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH); 
  delay(500);
  digitalWrite(PWDN_GPIO_NUM, LOW);
  delay(1000); // Wait for the camera internal logic to boot

  // Initialize Flash LED
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);

  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("1. Network Sync");

  WiFi.begin(ssid, password);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 15) {
    delay(500);
    timeout++;
  }

  lcd.setCursor(0, 1);
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("IP: "); lcd.print(WiFi.localIP().toString());
    
    // Test the Flash: Blink once to signal connection success
    digitalWrite(FLASH_GPIO_NUM, HIGH);
    delay(200);
    digitalWrite(FLASH_GPIO_NUM, LOW);
    
  } else {
    lcd.print("WiFi Failed!");
  }

  delay(2000); 
  lcd.clear();
  lcd.print("2. Sensor Probe");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM; config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM; config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = V_SYNC_GPIO_NUM;
  config.pin_href = H_REF_GPIO_NUM; config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM; config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  
  // INCREASED CLOCK (10MHz)
  // 1MHz was likely too slow for the sensor ID phase. 10MHz is a stable mid-point.
  config.xclk_freq_hz = 10000000; 
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA; 
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // Final check: Give it one last pause before init
  delay(500);
  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    lcd.setCursor(0, 1);
    if (err == 0xFFFFFFFF) {
        lcd.print("SIO/Power Error"); 
    } else if (err == 0x106) {
        lcd.print("ID Mismatch/Noise"); // Specific handling for 0x106
    } else if (err == ESP_ERR_CAMERA_NOT_DETECTED) {
        lcd.print("Lens Unplugged");
    } else {
        lcd.print("Code: 0x"); lcd.print(err, HEX);
    }
    Serial.printf("Camera Init Failed: 0x%x\n", err);
  } else {
    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) s->set_hmirror(s, 1);
    
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };
    if (httpd_start(&stream_httpd, &server_config) == ESP_OK) {
      httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
    lcd.clear();
    lcd.print("SYSTEM READY");
    lcd.setCursor(0, 1);
    lcd.print("/stream active");
  }
}

void loop() { delay(10000); }