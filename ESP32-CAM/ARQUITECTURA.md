# 🏗️ ARQUITECTURA DEL SISTEMA - FASE 1
## ESP32-CAM DriveGuard

---

## 📐 DIAGRAMA DE FLUJO GENERAL

```
┌─────────────────────────────────────────────────────────────────┐
│                         ESP32-CAM                               │
│                                                                 │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐ │
│  │    SETUP     │─────▶│     LOOP     │─────▶│  FreeRTOS    │ │
│  │              │      │              │      │  CaptureTask │ │
│  └──────────────┘      └──────────────┘      └──────────────┘ │
│         │                     │                      │         │
│         │                     │                      │         │
│         ▼                     ▼                      ▼         │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐ │
│  │  Init WiFi   │      │ Check WiFi   │      │ Capture Frame│ │
│  │  Init Camera │      │ Handle Cmds  │      │ Convert Base64│ │
│  │  Create Task │      │ Auto Logs    │      │ HTTP POST    │ │
│  └──────────────┘      └──────────────┘      └──────────────┘ │
│                                                      │         │
└──────────────────────────────────────────────────────┼─────────┘
                                                       │
                                                       │ HTTP POST
                                                       │ JSON + Base64
                                                       ▼
                                          ┌────────────────────────┐
                                          │   Flutter App / Server │
                                          │   Port 8080            │
                                          │   Endpoint: /upload    │
                                          └────────────────────────┘
```

---

## 🔄 FLUJO DE INICIALIZACIÓN (setup)

```
START
  │
  ├─▶ Serial.begin(115200)
  │     │
  │     ▼
  ├─▶ printDebugHeader()
  │     │
  │     ▼
  ├─▶ initCamera()
  │     │
  │     ├─ Configurar pines del sensor OV2640
  │     ├─ Resolución: QVGA (320x240)
  │     ├─ Calidad JPEG: 12
  │     ├─ Frame buffers: 2
  │     └─ Optimizar configuraciones del sensor
  │     │
  │     ▼
  │     ¿Éxito?
  │       │
  │       NO ──▶ LED parpadeo error ──▶ HALT (while 1)
  │       │
  │       SÍ
  │       │
  │       ▼
  ├─▶ connectToWiFi()
  │     │
  │     ├─ WiFi.begin(SSID, PASSWORD)
  │     ├─ LED parpadeo rápido (conectando)
  │     └─ Reintentar hasta 10 veces
  │     │
  │     ▼
  │     ¿Conectado?
  │       │
  │       NO ──▶ LED parpadeo error ──▶ HALT (while 1)
  │       │
  │       SÍ ──▶ LED fijo (conectado)
  │       │
  │       ▼
  ├─▶ obtainAndDisplayIP()
  │     │
  │     └─ Guardar IP en variable global
  │     │
  │     ▼
  ├─▶ sendIPBroadcast()
  │     │
  │     └─ UDP broadcast en puerto 8888
  │     │
  │     ▼
  ├─▶ LED parpadeo de éxito (x3)
  │     │
  │     ▼
  ├─▶ xTaskCreatePinnedToCore(captureTask, ...)
  │     │
  │     ├─ Crear tarea en Core 0
  │     ├─ Stack: 8KB
  │     ├─ Prioridad: 1
  │     └─ Handle: captureTaskHandle
  │     │
  │     ▼
  ├─▶ printDebugInfo()
  │     │
  │     ▼
  └─▶ "SISTEMA LISTO Y OPERATIVO"
       │
       ▼
     LOOP
```

---

## 🔁 FLUJO DEL LOOP PRINCIPAL

```
loop() - Ejecuta cada 1 segundo
  │
  ├─▶ handleSerialCommands()
  │     │
  │     ├─ Leer Serial
  │     ├─ Parsear comando
  │     └─ Ejecutar acción (help, info, stats, test, etc.)
  │
  ├─▶ Cada 30 segundos:
  │     │
  │     └─▶ Verificar WiFi conectado
  │           │
  │           NO ──▶ reconnectWiFi()
  │           │
  │           SÍ ──▶ Continuar
  │
  ├─▶ Cada 60 segundos:
  │     │
  │     └─▶ printDebugInfo()
  │
  └─▶ Cada 5 minutos:
        │
        └─▶ sendIPBroadcast()
```

---

## ⚡ FLUJO DE LA TAREA FREERTOS (captureTask)

```
captureTask() - Core 0 - Ejecuta cada 500ms
  │
  │ LOOP INFINITO
  │   │
  │   ├─▶ ¿WiFi conectado?
  │   │     │
  │   │     NO ──▶ reconnectWiFi() ──▶ Esperar 5s ──┐
  │   │     │                                        │
  │   │     SÍ                                       │
  │   │     │                                        │
  │   │     ▼                                        │
  │   ├─▶ Timestamp inicio = millis()                │
  │   │     │                                        │
  │   │     ▼                                        │
  │   ├─▶ captureAndSendFrame()                      │
  │   │     │                                        │
  │   │     ├─▶ esp_camera_fb_get()                  │
  │   │     │     │                                  │
  │   │     │     ▼                                  │
  │   │     │     ¿Frame OK?                         │
  │   │     │       │                                │
  │   │     │       NO ──▶ LOG error ──▶ Return false
  │   │     │       │                                │
  │   │     │       SÍ                               │
  │   │     │       │                                │
  │   │     │       ▼                                │
  │   │     ├─▶ frameToBase64(fb)                    │
  │   │     │     │                                  │
  │   │     │     └─▶ base64::encode()               │
  │   │     │           │                            │
  │   │     │           ▼                            │
  │   │     ├─▶ esp_camera_fb_return(fb)             │
  │   │     │     │                                  │
  │   │     │     ▼                                  │
  │   │     └─▶ sendImageToFlutter(base64)           │
  │   │           │                                  │
  │   │           ├─▶ http.begin(url)                │
  │   │           ├─▶ http.setTimeout(2000)          │
  │   │           ├─▶ JSON = {"image": base64, ...}  │
  │   │           ├─▶ http.POST(JSON)                │
  │   │           │     │                            │
  │   │           │     ▼                            │
  │   │           │     ¿Código 200?                 │
  │   │           │       │                          │
  │   │           │       NO ──▶ LOG error ──▶ false │
  │   │           │       │                          │
  │   │           │       SÍ ──▶ LOG success ──▶ true│
  │   │           │                                  │
  │   │           └─▶ http.end()                     │
  │   │                 │                            │
  │   │                 ▼                            │
  │   ├─▶ Timestamp fin = millis()                   │
  │   │     │                                        │
  │   │     ▼                                        │
  │   ├─▶ Calcular tiempo = fin - inicio             │
  │   │     │                                        │
  │   │     ▼                                        │
  │   ├─▶ ¿Éxito?                                    │
  │   │     │                                        │
  │   │     SÍ ──▶ frame_count_success++             │
  │   │     │      LOG: "Frame #X enviado (Yms)"     │
  │   │     │                                        │
  │   │     NO ──▶ frame_count_error++               │
  │   │            LOG: "Error enviando frame"       │
  │   │                                              │
  │   └─▶ vTaskDelay(500ms) ─────────────────────────┘
```

---

## 📡 FLUJO DE COMUNICACIÓN HTTP

```
ESP32-CAM                                    Flutter/Server
    │                                              │
    │  1. Capturar frame                          │
    │     ├─ esp_camera_fb_get()                  │
    │     └─ Frame JPEG (~12 KB)                  │
    │                                              │
    │  2. Convertir a Base64                      │
    │     ├─ base64::encode()                     │
    │     └─ String Base64 (~16 KB)               │
    │                                              │
    │  3. Crear JSON                              │
    │     {                                        │
    │       "image": "<base64>",                   │
    │       "timestamp": 123456                    │
    │     }                                        │
    │                                              │
    │  4. HTTP POST ─────────────────────────────▶│
    │     URL: http://IP:8080/upload              │
    │     Content-Type: application/json          │
    │     Body: JSON (~16 KB)                     │
    │                                              │
    │                                              │  5. Procesar
    │                                              │     ├─ Validar JSON
    │                                              │     ├─ Decodificar Base64
    │                                              │     ├─ Guardar imagen
    │                                              │     └─ Crear respuesta
    │                                              │
    │  6. Respuesta HTTP ◀────────────────────────┤
    │     Status: 200 OK                          │
    │     {                                        │
    │       "status": "ok",                        │
    │       "frame_number": 123                    │
    │     }                                        │
    │                                              │
    │  7. Procesar respuesta                      │
    │     ├─ Verificar código 200                 │
    │     ├─ Incrementar contador éxitos          │
    │     └─ LOG éxito                            │
    │                                              │
    ▼                                              ▼
```

---

## 🧠 ARQUITECTURA DE MEMORIA

```
┌─────────────────────────────────────────────────────┐
│                    ESP32 MEMORY                     │
├─────────────────────────────────────────────────────┤
│                                                     │
│  HEAP MEMORY (~300 KB total)                        │
│  ┌─────────────────────────────────────────────┐   │
│  │  Variables globales          (~5 KB)        │   │
│  │  HTTPClient buffer           (~16 KB)       │   │
│  │  Base64 encoding buffer      (~20 KB)       │   │
│  │  WiFi stack                  (~30 KB)       │   │
│  │  FreeRTOS overhead           (~10 KB)       │   │
│  │  Free heap                   (~100+ KB)     │   │
│  └─────────────────────────────────────────────┘   │
│                                                     │
│  PSRAM (~4 MB en ESP32-CAM)                         │
│  ┌─────────────────────────────────────────────┐   │
│  │  Frame buffer 1              (~14 KB)       │   │
│  │  Frame buffer 2              (~14 KB)       │   │
│  │  Camera driver buffers       (~30 KB)       │   │
│  │  Free PSRAM                  (~3.9 MB)      │   │
│  └─────────────────────────────────────────────┘   │
│                                                     │
│  STACK (FreeRTOS Tasks)                             │
│  ┌─────────────────────────────────────────────┐   │
│  │  CaptureTask Stack           (8 KB)         │   │
│  │  Arduino Loop Stack          (8 KB)         │   │
│  │  WiFi Task Stack             (4 KB)         │   │
│  └─────────────────────────────────────────────┘   │
│                                                     │
└─────────────────────────────────────────────────────┘
```

---

## 🎛️ SISTEMA DE COMANDOS DEBUGGING

```
Serial Monitor Input
        │
        ▼
  handleSerialCommands()
        │
        ├─ Serial.available() ?
        │     │
        │     NO ──▶ Return
        │     │
        │     SÍ
        │     │
        │     ▼
        ├─ Leer línea completa
        │     │
        │     ▼
        ├─ Trim y toLowerCase()
        │     │
        │     ▼
        └─ Switch (command)
              │
              ├─ "help" ──▶ Mostrar lista de comandos
              │
              ├─ "info" ──▶ printDebugInfo()
              │               ├─ WiFi status
              │               ├─ IP address
              │               ├─ Camera ready
              │               ├─ Estadísticas
              │               ├─ Memoria
              │               └─ Uptime
              │
              ├─ "stats" ──▶ Mostrar estadísticas de frames
              │                ├─ Frames exitosos
              │                ├─ Frames error
              │                ├─ Tasa de éxito
              │                └─ Último frame
              │
              ├─ "tasks" ──▶ printTaskStatus()
              │                ├─ Status de CaptureTask
              │                └─ Stack usage
              │
              ├─ "wifi" ──▶ Estado WiFi detallado
              │               ├─ Estado conexión
              │               ├─ SSID
              │               ├─ RSSI
              │               └─ Canal
              │
              ├─ "ip" ──▶ obtainAndDisplayIP()
              │            └─ sendIPBroadcast()
              │
              ├─ "test" ──▶ captureAndSendFrame()
              │               └─ Mostrar resultado
              │
              ├─ "reset" ──▶ Reiniciar contadores
              │                ├─ frame_count_success = 0
              │                └─ frame_count_error = 0
              │
              ├─ "reboot" ──▶ ESP.restart()
              │
              └─ Otro ──▶ "Comando desconocido"
```

---

## 🔌 DIAGRAMA DE PINES (AI-Thinker ESP32-CAM)

```
┌────────────────────────────────────────────────┐
│          ESP32-CAM (AI-Thinker)                │
├────────────────────────────────────────────────┤
│                                                │
│  Camera Module (OV2640)                        │
│  ├─ PWDN_GPIO_NUM    = 32                      │
│  ├─ RESET_GPIO_NUM   = -1  (No usado)          │
│  ├─ XCLK_GPIO_NUM    = 0                       │
│  ├─ SIOD_GPIO_NUM    = 26  (I2C Data)          │
│  ├─ SIOC_GPIO_NUM    = 27  (I2C Clock)         │
│  ├─ Y9_GPIO_NUM      = 35  (Data bit 9)        │
│  ├─ Y8_GPIO_NUM      = 34  (Data bit 8)        │
│  ├─ Y7_GPIO_NUM      = 39  (Data bit 7)        │
│  ├─ Y6_GPIO_NUM      = 36  (Data bit 6)        │
│  ├─ Y5_GPIO_NUM      = 21  (Data bit 5)        │
│  ├─ Y4_GPIO_NUM      = 19  (Data bit 4)        │
│  ├─ Y3_GPIO_NUM      = 18  (Data bit 3)        │
│  ├─ Y2_GPIO_NUM      = 5   (Data bit 2)        │
│  ├─ VSYNC_GPIO_NUM   = 25  (Vertical Sync)     │
│  ├─ HREF_GPIO_NUM    = 23  (Horizontal Ref)    │
│  └─ PCLK_GPIO_NUM    = 22  (Pixel Clock)       │
│                                                │
│  LEDs                                          │
│  ├─ LED_BUILTIN      = 33  (LED rojo)          │
│  └─ LED_FLASH        = 4   (LED blanco)        │
│                                                │
│  Serial (FTDI)                                 │
│  ├─ RX = GPIO 3                                │
│  └─ TX = GPIO 1                                │
│                                                │
│  Boot Mode                                     │
│  └─ GPIO 0 = GND → Modo programación           │
│     GPIO 0 = Float → Modo ejecución            │
│                                                │
└────────────────────────────────────────────────┘
```

---

## 🕐 TIMELINE DE EJECUCIÓN

```
Tiempo     Evento
──────────────────────────────────────────────────
0ms        │ ESP32 boot
           │
500ms      ├─▶ setup() inicia
           │   ├─ Serial.begin(115200)
           │   └─ printDebugHeader()
           │
1000ms     ├─▶ initCamera()
           │   └─ Configurar OV2640
           │
1500ms     ├─▶ Camera ready
           │
2000ms     ├─▶ connectToWiFi()
           │   └─ LED parpadeo rápido
           │
3000-5000ms├─▶ WiFi conectado
           │   └─ LED fijo
           │
5500ms     ├─▶ obtainAndDisplayIP()
           │   └─ Mostrar IP
           │
6000ms     ├─▶ sendIPBroadcast()
           │   └─ UDP broadcast
           │
6500ms     ├─▶ xTaskCreatePinnedToCore()
           │   └─ Crear CaptureTask
           │
7000ms     ├─▶ "SISTEMA LISTO"
           │
7500ms     ├─▶ CaptureTask: Frame #1
           │   ├─ Capturar (50ms)
           │   ├─ Base64 (100ms)
           │   └─ HTTP POST (200ms)
           │   Total: ~350ms
           │
8000ms     ├─▶ CaptureTask: Frame #2
           │   └─ (cada 500ms)
           │
8500ms     ├─▶ CaptureTask: Frame #3
           │
...        │   (loop continúa cada 500ms)
           │
60000ms    ├─▶ loop(): printDebugInfo()
           │   (auto cada 60s)
           │
300000ms   ├─▶ loop(): sendIPBroadcast()
           │   (auto cada 5 min)
           │
∞          └─▶ Sistema corriendo...
```

---

## 🔒 ESTADOS DEL SISTEMA

```
┌─────────────────────────────────────────────────┐
│              DIAGRAMA DE ESTADOS                │
└─────────────────────────────────────────────────┘

     [BOOT]
        │
        ▼
  [INIT_CAMERA] ─── Error ──▶ [HALT_CAM_ERROR]
        │
        │ OK
        ▼
   [INIT_WIFI] ─── Error ──▶ [HALT_WIFI_ERROR]
        │
        │ OK
        ▼
  [GET_IP_ADDRESS]
        │
        ▼
  [CREATE_TASK]
        │
        ▼
    [RUNNING] ◀─────────────────┐
        │                       │
        ├─ CaptureTask activa   │
        ├─ Loop monitoreando    │
        ├─ Comandos disponibles │
        │                       │
        ├─ WiFi OK? ────────────┘
        │      │
        │      NO
        │      │
        │      ▼
        │  [RECONNECTING]
        │      │
        │      │ Reintentar
        │      └────────▶ [RUNNING]
        │
        │ Comando: reboot
        │
        ▼
    [REBOOT] ──▶ [BOOT]
```

---

## 📊 MÉTRICAS Y MONITOREO

```
┌──────────────────────────────────────────────────┐
│           MÉTRICAS DEL SISTEMA                   │
├──────────────────────────────────────────────────┤
│                                                  │
│  Captura de Frames                               │
│  ┌────────────────────────────────────────────┐ │
│  │ Frecuencia: 500ms (2 FPS)                  │ │
│  │ Resolución: 320x240 px                     │ │
│  │ Formato: JPEG                              │ │
│  │ Calidad: 12                                │ │
│  │ Tamaño promedio: 12 KB                     │ │
│  └────────────────────────────────────────────┘ │
│                                                  │
│  Transmisión HTTP                                │
│  ┌────────────────────────────────────────────┐ │
│  │ Protocolo: HTTP POST                       │ │
│  │ Formato: JSON + Base64                     │ │
│  │ Tamaño payload: ~16 KB                     │ │
│  │ Timeout: 2 segundos                        │ │
│  │ Tiempo promedio: 150-400 ms                │ │
│  └────────────────────────────────────────────┘ │
│                                                  │
│  Rendimiento                                     │
│  ┌────────────────────────────────────────────┐ │
│  │ Success Rate esperada: > 95%               │ │
│  │ Free Heap: > 100 KB                        │ │
│  │ Free PSRAM: > 3 MB                         │ │
│  │ Stack usage: < 6 KB                        │ │
│  └────────────────────────────────────────────┘ │
│                                                  │
└──────────────────────────────────────────────────┘
```

---

## 🔧 COMPONENTES PRINCIPALES

### 1. WiFi Manager
- Archivo: `main.cpp` líneas 102-157
- Funciones: `connectToWiFi()`, `reconnectWiFi()`
- Responsabilidad: Mantener conexión WiFi estable

### 2. Camera Controller
- Archivo: `main.cpp` líneas 189-262
- Funciones: `initCamera()`
- Responsabilidad: Inicializar y configurar OV2640

### 3. HTTP Client
- Archivo: `main.cpp` líneas 264-351
- Funciones: `sendImageToFlutter()`, `frameToBase64()`
- Responsabilidad: Transmisión de imágenes

### 4. FreeRTOS Task
- Archivo: `main.cpp` líneas 353-390
- Funciones: `captureTask()`
- Responsabilidad: Loop de captura continua

### 5. Debug System
- Archivo: `main.cpp` líneas 64-100, 392-471
- Funciones: `handleSerialCommands()`, `printDebugInfo()`
- Responsabilidad: Monitoreo y debugging

---

## 🎯 PUNTOS CRÍTICOS DE RENDIMIENTO

### Optimizaciones Implementadas

1. **Double Buffering**
   - 2 frame buffers
   - Captura mientras procesa frame anterior
   - Reduce latencia

2. **FreeRTOS Core Pinning**
   - CaptureTask en Core 0
   - WiFi/Loop en Core 1
   - Mejor utilización de CPU

3. **PSRAM para Buffers**
   - Frame buffers en PSRAM
   - Libera heap RAM
   - Mejor estabilidad

4. **HTTP Timeout**
   - Timeout de 2s
   - Evita bloqueos indefinidos
   - Permite reconexión rápida

5. **Base64 In-Memory**
   - Encoding directo en RAM
   - Sin archivos temporales
   - Más rápido

---

## 📝 LOGS Y DEBUGGING

### Niveles de Log

```
[Setup]    → Inicialización
[Camera]   → Operaciones de cámara
[WiFi]     → Conectividad WiFi
[Network]  → Información de red
[UDP]      → Broadcast UDP
[Capture]  → Captura de frames
[Base64]   → Conversión Base64
[HTTP]     → Peticiones HTTP
[Task]     → FreeRTOS CaptureTask
[Loop]     → Loop principal
[CMD]      → Comandos de usuario
[TEST]     → Pruebas manuales
```

### Símbolos de Estado

```
✓  → Éxito
✗  → Error
│  → Separador/Continuación
├─ → Rama
└─ → Final de rama
```

---

**Diagrama completo de la arquitectura del sistema ESP32-CAM DriveGuard - Fase 1**

Para más información, consulta:
- `README_FASE1.md` - Documentación técnica
- `GUIA_DEBUGGING.md` - Guía de debugging
- `RESUMEN_IMPLEMENTACION.md` - Resumen ejecutivo
