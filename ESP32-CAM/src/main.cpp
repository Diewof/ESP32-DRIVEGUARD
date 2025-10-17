#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "base64.h"

// ============================================
// FASE 1: CONFIGURACIÓN INICIAL ESP32-CAM
// ============================================

// ==== TAREA 1.1: CONFIGURACIÓN WIFI ====
const char* WIFI_SSID = "DriveGuard";
const char* WIFI_PASSWORD = "driveguard123";
const int WIFI_MAX_RETRIES = 10;
const int WIFI_RETRY_DELAY_MS = 1000;

// LED integrado para indicador de estado (GPIO 33 en AI-Thinker)
#define LED_BUILTIN 33
#define LED_FLASH 4  // Flash LED en GPIO 4

// ==== TAREA 1.2: VARIABLES GLOBALES DE RED ====
String esp32_ip_address = "";
const int UDP_BROADCAST_PORT = 8888;
WiFiUDP udp;

// ==== TAREA 1.4: CONFIGURACIÓN HTTP CLIENT ====
// IMPORTANTE: Cambia esta IP por la IP de tu app Flutter
const char* FLUTTER_APP_IP = "192.168.1.100";  // <-- CAMBIAR AQUÍ
const int FLUTTER_APP_PORT = 8080;
const int HTTP_TIMEOUT_MS = 2000;
HTTPClient http;

// ==== TAREA 1.5: VARIABLES DE CONTROL ====
unsigned long frame_count_success = 0;
unsigned long frame_count_error = 0;
unsigned long last_frame_time = 0;
bool camera_ready = false;

// Task handles para FreeRTOS
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
// HERRAMIENTAS DE DEBUGGING
// ============================================

void printDebugHeader() {
  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║   ESP32-CAM DRIVEGUARD - DEBUG MONITOR    ║");
  Serial.println("║            FASE 1 - v1.0                  ║");
  Serial.println("╚════════════════════════════════════════════╝\n");
}

void printDebugInfo() {
  Serial.println("\n┌─────────── DEBUG INFO ───────────┐");
  Serial.printf("│ WiFi Status    : %s\n", WiFi.status() == WL_CONNECTED ? "✓ Conectado" : "✗ Desconectado");
  Serial.printf("│ IP Address     : %s\n", esp32_ip_address.c_str());
  Serial.printf("│ Signal (RSSI)  : %d dBm\n", WiFi.RSSI());
  Serial.printf("│ Camera Ready   : %s\n", camera_ready ? "✓ SI" : "✗ NO");
  Serial.printf("│ Frames Enviados: %lu\n", frame_count_success);
  Serial.printf("│ Frames Error   : %lu\n", frame_count_error);
  Serial.printf("│ Success Rate   : %.2f%%\n",
    frame_count_success + frame_count_error > 0
      ? (float)frame_count_success / (frame_count_success + frame_count_error) * 100
      : 0.0f);
  Serial.printf("│ Free Heap      : %d bytes\n", ESP.getFreeHeap());
  Serial.printf("│ Free PSRAM     : %d bytes\n", ESP.getFreePsram());
  Serial.printf("│ Uptime         : %lu seg\n", millis() / 1000);
  Serial.println("└──────────────────────────────────┘\n");
}

void printTaskStatus() {
  Serial.println("┌───── FREERTOS TASKS STATUS ──────┐");
  Serial.printf("│ Task: CaptureTask\n");
  Serial.printf("│ Status: %s\n", captureTaskHandle != NULL ? "✓ Running" : "✗ Stopped");
  Serial.printf("│ Stack High Water Mark: %d bytes\n",
    captureTaskHandle != NULL ? uxTaskGetStackHighWaterMark(captureTaskHandle) : 0);
  Serial.println("└──────────────────────────────────┘\n");
}

// ============================================
// TAREA 1.1: CONFIGURACIÓN WIFI
// ============================================

void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_BUILTIN, LOW);
    delay(delayMs);
  }
}

void fastBlinkLED() {
  // Parpadeo rápido: conectando
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

bool connectToWiFi() {
  Serial.println("\n[WiFi] Iniciando conexión...");
  Serial.printf("[WiFi] SSID: %s\n", WIFI_SSID);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED && retry_count < WIFI_MAX_RETRIES) {
    fastBlinkLED(); // Parpadeo rápido mientras conecta
    delay(WIFI_RETRY_DELAY_MS);
    Serial.print(".");
    retry_count++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH); // LED fijo: conectado
    Serial.println("\n[WiFi] ✓ Conexión exitosa!");
    return true;
  } else {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("\n[WiFi] ✗ Error: No se pudo conectar");
    return false;
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Reconectando...");
    digitalWrite(LED_BUILTIN, LOW);
    WiFi.disconnect();
    delay(1000);
    connectToWiFi();
  }
}

// ============================================
// TAREA 1.2: OBTENER Y BROADCAST IP
// ============================================

void obtainAndDisplayIP() {
  if (WiFi.status() == WL_CONNECTED) {
    esp32_ip_address = WiFi.localIP().toString();
    Serial.println("\n╔═══════════════════════════════════╗");
    Serial.printf("║  IP ASIGNADA: %-20s║\n", esp32_ip_address.c_str());
    Serial.println("╚═══════════════════════════════════╝\n");

    Serial.printf("[Network] MAC Address: %s\n", WiFi.macAddress().c_str());
    Serial.printf("[Network] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("[Network] Subnet: %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("[Network] DNS: %s\n", WiFi.dnsIP().toString().c_str());
    Serial.printf("[Network] Signal Strength: %d dBm\n", WiFi.RSSI());
  }
}

void sendIPBroadcast() {
  // Broadcast UDP opcional para facilitar descubrimiento
  if (WiFi.status() == WL_CONNECTED && esp32_ip_address.length() > 0) {
    udp.beginPacket("255.255.255.255", UDP_BROADCAST_PORT);
    String message = "ESP32CAM_IP:" + esp32_ip_address;
    udp.print(message);
    udp.endPacket();
    Serial.printf("[UDP] Broadcast enviado: %s\n", message.c_str());
  }
}

// ============================================
// TAREA 1.3: CONFIGURACIÓN DE CÁMARA
// ============================================

bool initCamera() {
  Serial.println("[Camera] Inicializando cámara...");

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

  // TAREA 1.3: QVGA (320x240) con calidad JPEG 12
  config.frame_size = FRAMESIZE_QVGA;  // 320x240
  config.jpeg_quality = 12;             // Balance calidad/tamaño
  config.fb_count = 2;                  // Double buffering

  // Inicializar cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[Camera] ✗ Error de inicialización: 0x%x\n", err);
    camera_ready = false;
    return false;
  }

  // Configuraciones adicionales del sensor
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 0);     // -2 a 2
    s->set_contrast(s, 0);       // -2 a 2
    s->set_saturation(s, 0);     // -2 a 2
    s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
    s->set_wb_mode(s, 0);        // 0 a 4
    s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
    s->set_aec2(s, 0);           // 0 = disable , 1 = enable
    s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
    s->set_agc_gain(s, 0);       // 0 a 30
    s->set_gainceiling(s, (gainceiling_t)0);  // 0 a 6
    s->set_bpc(s, 0);            // 0 = disable , 1 = enable
    s->set_wpc(s, 1);            // 0 = disable , 1 = enable
    s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
    s->set_lenc(s, 1);           // 0 = disable , 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
    s->set_vflip(s, 0);          // 0 = disable , 1 = enable
    s->set_dcw(s, 1);            // 0 = disable , 1 = enable
    s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
  }

  Serial.println("[Camera] ✓ Cámara inicializada correctamente");
  Serial.printf("[Camera] Resolución: QVGA (320x240)\n");
  Serial.printf("[Camera] Calidad JPEG: 12\n");
  Serial.printf("[Camera] Frame buffers: 2\n");

  camera_ready = true;
  return true;
}

// ============================================
// TAREA 1.4: CLIENTE HTTP Y ENVÍO BASE64
// ============================================

String frameToBase64(camera_fb_t *fb) {
  if (fb == NULL || fb->buf == NULL || fb->len == 0) {
    Serial.println("[Base64] Error: Frame buffer vacío");
    return "";
  }

  // Codificar a Base64
  String base64Image = base64::encode(fb->buf, fb->len);

  Serial.printf("[Base64] Imagen codificada: %d bytes -> %d caracteres\n",
                fb->len, base64Image.length());

  return base64Image;
}

bool sendImageToFlutter(String base64Image) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Error: WiFi desconectado");
    return false;
  }

  if (base64Image.length() == 0) {
    Serial.println("[HTTP] Error: Imagen Base64 vacía");
    return false;
  }

  String url = "http://" + String(FLUTTER_APP_IP) + ":" + String(FLUTTER_APP_PORT) + "/upload";

  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");

  // Crear JSON payload
  String jsonPayload = "{\"image\":\"" + base64Image + "\",\"timestamp\":" + String(millis()) + "}";

  Serial.printf("[HTTP] Enviando a: %s\n", url.c_str());
  Serial.printf("[HTTP] Payload size: %d bytes\n", jsonPayload.length());

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] ✓ Respuesta: %d\n", httpResponseCode);
    String response = http.getString();
    Serial.printf("[HTTP] Server response: %s\n", response.c_str());
    http.end();
    return true;
  } else {
    Serial.printf("[HTTP] ✗ Error: %s\n", http.errorToString(httpResponseCode).c_str());
    http.end();
    return false;
  }
}

bool captureAndSendFrame() {
  if (!camera_ready) {
    Serial.println("[Capture] Error: Cámara no está lista");
    return false;
  }

  // Capturar frame
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[Capture] ✗ Error al capturar frame");
    return false;
  }

  Serial.printf("[Capture] Frame capturado: %d bytes, formato: %d\n", fb->len, fb->format);

  // Convertir a Base64
  String base64Image = frameToBase64(fb);

  // Liberar buffer de la cámara
  esp_camera_fb_return(fb);

  if (base64Image.length() == 0) {
    Serial.println("[Capture] Error: Conversión a Base64 falló");
    return false;
  }

  // Enviar por HTTP
  bool success = sendImageToFlutter(base64Image);

  return success;
}

// ============================================
// TAREA 1.5: LOOP DE ENVÍO CONTINUO FREERTOS
// ============================================

void captureTask(void *parameter) {
  Serial.println("[Task] CaptureTask iniciada");
  const TickType_t delay500ms = pdMS_TO_TICKS(500);

  while (true) {
    // Verificar conexión WiFi
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[Task] WiFi desconectado, intentando reconectar...");
      reconnectWiFi();
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    // Capturar y enviar
    unsigned long start_time = millis();
    bool success = captureAndSendFrame();
    unsigned long elapsed = millis() - start_time;

    if (success) {
      frame_count_success++;
      Serial.printf("[Task] ✓ Frame #%lu enviado exitosamente (tiempo: %lu ms)\n",
                    frame_count_success, elapsed);
    } else {
      frame_count_error++;
      Serial.printf("[Task] ✗ Error enviando frame #%lu (total errores: %lu)\n",
                    frame_count_success + frame_count_error, frame_count_error);
    }

    last_frame_time = millis();

    // Esperar 500ms antes del siguiente envío
    vTaskDelay(delay500ms);
  }
}

// ============================================
// COMANDOS DE DEBUG POR SERIAL
// ============================================

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    Serial.printf("\n[CMD] Comando recibido: '%s'\n", command.c_str());

    if (command == "help" || command == "?") {
      Serial.println("\n╔═══════════════════════════════════════╗");
      Serial.println("║        COMANDOS DISPONIBLES          ║");
      Serial.println("╠═══════════════════════════════════════╣");
      Serial.println("║ help      - Mostrar esta ayuda       ║");
      Serial.println("║ info      - Información del sistema  ║");
      Serial.println("║ stats     - Estadísticas de frames   ║");
      Serial.println("║ tasks     - Estado de tareas FreeRTOS║");
      Serial.println("║ wifi      - Estado WiFi              ║");
      Serial.println("║ ip        - Mostrar IP y broadcast   ║");
      Serial.println("║ test      - Capturar y enviar 1 frame║");
      Serial.println("║ reset     - Reiniciar estadísticas   ║");
      Serial.println("║ reboot    - Reiniciar ESP32          ║");
      Serial.println("╚═══════════════════════════════════════╝\n");
    }
    else if (command == "info") {
      printDebugInfo();
    }
    else if (command == "stats") {
      Serial.println("\n┌───────── ESTADÍSTICAS ──────────┐");
      Serial.printf("│ Frames exitosos: %lu\n", frame_count_success);
      Serial.printf("│ Frames con error: %lu\n", frame_count_error);
      Serial.printf("│ Total intentos: %lu\n", frame_count_success + frame_count_error);
      Serial.printf("│ Tasa de éxito: %.2f%%\n",
        frame_count_success + frame_count_error > 0
          ? (float)frame_count_success / (frame_count_success + frame_count_error) * 100
          : 0.0f);
      Serial.printf("│ Último frame: hace %lu ms\n", millis() - last_frame_time);
      Serial.println("└─────────────────────────────────┘\n");
    }
    else if (command == "tasks") {
      printTaskStatus();
    }
    else if (command == "wifi") {
      Serial.println("\n┌──────── ESTADO WiFi ────────┐");
      Serial.printf("│ Estado: %s\n", WiFi.status() == WL_CONNECTED ? "✓ Conectado" : "✗ Desconectado");
      Serial.printf("│ SSID: %s\n", WIFI_SSID);
      Serial.printf("│ IP: %s\n", esp32_ip_address.c_str());
      Serial.printf("│ RSSI: %d dBm\n", WiFi.RSSI());
      Serial.printf("│ Canal: %d\n", WiFi.channel());
      Serial.println("└─────────────────────────────┘\n");
    }
    else if (command == "ip") {
      obtainAndDisplayIP();
      sendIPBroadcast();
    }
    else if (command == "test") {
      Serial.println("\n[TEST] Capturando frame de prueba...");
      bool result = captureAndSendFrame();
      Serial.printf("[TEST] Resultado: %s\n\n", result ? "✓ ÉXITO" : "✗ ERROR");
    }
    else if (command == "reset") {
      frame_count_success = 0;
      frame_count_error = 0;
      last_frame_time = millis();
      Serial.println("[CMD] ✓ Estadísticas reiniciadas\n");
    }
    else if (command == "reboot") {
      Serial.println("[CMD] Reiniciando ESP32 en 3 segundos...");
      delay(3000);
      ESP.restart();
    }
    else {
      Serial.printf("[CMD] ✗ Comando desconocido: '%s'\n", command.c_str());
      Serial.println("[CMD] Escribe 'help' para ver comandos disponibles\n");
    }
  }
}

// ============================================
// SETUP Y LOOP PRINCIPAL
// ============================================

void setup() {
  // Inicializar Serial
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(1000);

  printDebugHeader();

  // Flash LED para debug visual
  pinMode(LED_FLASH, OUTPUT);
  digitalWrite(LED_FLASH, LOW);

  Serial.println("[Setup] Iniciando configuración ESP32-CAM...\n");

  // TAREA 1.3: Inicializar cámara
  if (!initCamera()) {
    Serial.println("[Setup] ✗ FATAL: No se pudo inicializar la cámara");
    Serial.println("[Setup] Sistema detenido. Revisa las conexiones de la cámara.");
    blinkLED(10, 200); // Parpadeo de error
    while (1) { delay(1000); }
  }

  // TAREA 1.1: Conectar WiFi
  if (!connectToWiFi()) {
    Serial.println("[Setup] ✗ FATAL: No se pudo conectar a WiFi");
    Serial.println("[Setup] Verifica SSID y password en el código");
    blinkLED(10, 200); // Parpadeo de error
    while (1) { delay(1000); }
  }

  // TAREA 1.2: Obtener IP y broadcast
  obtainAndDisplayIP();
  sendIPBroadcast();

  // Parpadeo de éxito
  blinkLED(3, 100);

  // TAREA 1.5: Crear tarea FreeRTOS para captura continua
  xTaskCreatePinnedToCore(
    captureTask,           // Función de la tarea
    "CaptureTask",         // Nombre
    8192,                  // Stack size (8KB)
    NULL,                  // Parámetros
    1,                     // Prioridad
    &captureTaskHandle,    // Handle
    0                      // Core 0
  );

  if (captureTaskHandle != NULL) {
    Serial.println("[Setup] ✓ Tarea de captura iniciada en Core 0");
  } else {
    Serial.println("[Setup] ✗ Error al crear tarea de captura");
  }

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║    SISTEMA LISTO Y OPERATIVO          ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("\n[Info] Escribe 'help' para ver comandos de debug\n");

  printDebugInfo();
}

void loop() {
  // Monitoreo de comandos de debug
  handleSerialCommands();

  // Verificar conexión WiFi cada 30 segundos
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[Loop] Detectada desconexión WiFi, reconectando...");
      reconnectWiFi();
    }
    lastWiFiCheck = millis();
  }

  // Mostrar info de debug cada 60 segundos
  static unsigned long lastDebugPrint = 0;
  if (millis() - lastDebugPrint > 60000) {
    printDebugInfo();
    lastDebugPrint = millis();
  }

  // Broadcast IP cada 5 minutos
  static unsigned long lastBroadcast = 0;
  if (millis() - lastBroadcast > 300000) {
    sendIPBroadcast();
    lastBroadcast = millis();
  }

  delay(1000);
}
