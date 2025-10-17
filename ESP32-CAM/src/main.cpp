#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "base64.h"

// ============================================
// CONFIGURACIÓN ESP32-CAM DRIVEGUARD
// ============================================

// Configuración WiFi
const char* WIFI_SSID = "DriveGuard";
const char* WIFI_PASSWORD = "driveguard123";
const int WIFI_MAX_RETRIES = 10;
const int WIFI_RETRY_DELAY_MS = 1000;

// LED integrado (GPIO 33 en AI-Thinker)
#define LED_BUILTIN 33

// Configuración HTTP Client
const char* FLUTTER_APP_IP = "192.168.1.100";  // <-- CAMBIAR AQUÍ
const int FLUTTER_APP_PORT = 8080;
const int HTTP_TIMEOUT_MS = 5000;
HTTPClient http;

// Variables de control
bool camera_ready = false;
TaskHandle_t captureTaskHandle = NULL;

// ==== PINES DEL MÓDULO AI THINKER (ESP32-CAM) ====
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
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ============================================
// FUNCIONES WIFI
// ============================================

bool connectToWiFi() {
  Serial.println("[WiFi] Conectando...");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED && retry_count < WIFI_MAX_RETRIES) {
    delay(WIFI_RETRY_DELAY_MS);
    Serial.print(".");
    retry_count++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("\n[WiFi] Conectado");
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("\n[WiFi] Error de conexión");
    return false;
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW);
    WiFi.disconnect();
    delay(1000);
    connectToWiFi();
  }
}

// ============================================
// CONFIGURACIÓN DE CÁMARA
// ============================================

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;   // 640x480
  config.jpeg_quality = 10;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[Camera] Error: 0x%x\n", err);
    return false;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
  }

  Serial.println("[Camera] Iniciada - VGA 640x480");
  camera_ready = true;
  return true;
}

// ============================================
// CLIENTE HTTP Y ENVÍO
// ============================================

bool sendImageToFlutter(camera_fb_t *fb) {
  if (!fb || WiFi.status() != WL_CONNECTED) return false;

  String base64Image = base64::encode(fb->buf, fb->len);
  if (base64Image.length() == 0) return false;

  String url = "http://" + String(FLUTTER_APP_IP) + ":" + String(FLUTTER_APP_PORT) + "/upload";
  String jsonPayload = "{\"image\":\"" + base64Image + "\",\"timestamp\":" + String(millis()) + "}";

  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonPayload);
  http.end();

  return (httpResponseCode > 0);
}

// ============================================
// TAREA DE CAPTURA CONTINUA
// ============================================

void captureTask(void *parameter) {
  const TickType_t delay500ms = pdMS_TO_TICKS(500);

  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      reconnectWiFi();
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    if (camera_ready) {
      camera_fb_t *fb = esp_camera_fb_get();
      if (fb) {
        bool success = sendImageToFlutter(fb);
        esp_camera_fb_return(fb);

        if (success) {
          Serial.print(".");
        }
      }
    }

    vTaskDelay(delay500ms);
  }
}

// ============================================
// SETUP Y LOOP
// ============================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ESP32-CAM DriveGuard ===");

  if (!initCamera()) {
    Serial.println("[ERROR] Camara no inicializada");
    while (1) { delay(1000); }
  }

  if (!connectToWiFi()) {
    Serial.println("[ERROR] WiFi no conectado");
    while (1) { delay(1000); }
  }

  xTaskCreatePinnedToCore(
    captureTask,
    "CaptureTask",
    8192,
    NULL,
    1,
    &captureTaskHandle,
    0
  );

  Serial.println("[OK] Sistema iniciado");
}

void loop() {
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      reconnectWiFi();
    }
    lastWiFiCheck = millis();
  }

  delay(1000);
}
