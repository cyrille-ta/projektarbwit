#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "Freenove_4WD_Car_For_ESP32.h"

#define CAMERA_MODEL_WROVER_KIT
#include "camera_pins.h"

// ================= SETTINGS =================
const char* ssid = "ESP32_CARr";
const char* password = "12345678";
WiFiServer server(80);

// ================= VARIABLES =================
unsigned long lastBeepTime = 0;
bool buzzerState = false;
int servo1_pos = 90;
int servo2_pos = 90;

#define TRIG_PIN 12
#define ECHO_PIN 15

// ================= CAMERA STREAM HANDLER =================
// (Camera functions stream_handler and startCameraServer remain the same...)

static esp_err_t stream_handler(httpd_req_t *req) {

camera_fb_t *fb;

esp_err_t res = ESP_OK;

char buf[64];

httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");

while (true) {

fb = esp_camera_fb_get();

if (!fb) return ESP_FAIL;

snprintf(buf, 64, "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);

httpd_resp_send_chunk(req, buf, strlen(buf));

httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);

httpd_resp_send_chunk(req, "\r\n", 2);

esp_camera_fb_return(fb);

}

}



void startCameraServer() {

httpd_config_t config = HTTPD_DEFAULT_CONFIG();

config.server_port = 81;

httpd_handle_t httpd = NULL;

httpd_uri_t uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };

if (httpd_start(&httpd, &config) == ESP_OK) { httpd_register_uri_handler(httpd, &uri); }

}

// ================= SONAR FUNCTION =================
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  float distance = duration * 0.034 / 2;
  return (distance == 0) ? 400 : distance;
}

// ================= SINGLE SETUP =================
void setup() {
  Serial.begin(115200);
  
  PCA9685_Setup();
  Buzzer_Setup(); 
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Motor_Move(0, 0, 0, 0);
  Servo_1_Angle(servo1_pos);
  Servo_2_Angle(servo2_pos);

  // Camera config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM; config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera Init Failed");
  }

  WiFi.softAP(ssid, password);
  startCameraServer();
  server.begin();
  Serial.println("System Online!");
}

// ================= LOOP =================
void loop() {
  float distance = getDistance();

  // Buzzer Beep Logic
  if (distance > 0 && distance < 50) {
    int beepInterval = map(distance, 2, 50, 80, 600);
    if (millis() - lastBeepTime >= beepInterval) {
      lastBeepTime = millis();
      buzzerState = !buzzerState;
      if (buzzerState) ledcWriteTone(PIN_BUZZER, 2000); 
      else ledcWriteTone(PIN_BUZZER, 0);
    }
  } else {
    ledcWriteTone(PIN_BUZZER, 0); 
  }

  // WiFi Command Logic
  WiFiClient client = server.available();
  if (client) {
    String req = client.readStringUntil('\r');
    if      (req.indexOf("/FORWARD")  != -1) Motor_Move(1500, 1500, 1500, 1500);
    else if (req.indexOf("/BACKWARD") != -1) Motor_Move(-1500, -1500, -1500, -1500);
    else if (req.indexOf("/LEFT")     != -1) Motor_Move(-1200, -1200, 1200, 1200);
    else if (req.indexOf("/RIGHT")    != -1) Motor_Move(1200, 1200, -1200, -1200);
    else if (req.indexOf("/STOP")     != -1) Motor_Move(0, 0, 0, 0);
    else if (req.indexOf("/SERVO1_RIGHT") != -1) { servo1_pos = min(servo1_pos + 10, 180); Servo_1_Angle(servo1_pos); }
    else if (req.indexOf("/SERVO1_LEFT")  != -1) { servo1_pos = max(servo1_pos - 10, 0);   Servo_1_Angle(servo1_pos); }
    else if (req.indexOf("/SERVO2_UP")    != -1) { servo2_pos = min(servo2_pos + 10, 180); Servo_2_Angle(servo2_pos); }
    else if (req.indexOf("/SERVO2_DOWN")  != -1) { servo2_pos = max(servo2_pos - 10, 0);   Servo_2_Angle(servo2_pos); }
    else if (req.indexOf("/BUZZER")     != -1) { 
        ledcWriteTone(PIN_BUZZER, 2000); 
        delay(200); 
        ledcWriteTone(PIN_BUZZER, 0);}

    client.println("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nOK");
    client.stop();
  }
}