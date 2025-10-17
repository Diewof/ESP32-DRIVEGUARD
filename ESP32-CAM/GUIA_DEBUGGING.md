# 🔧 GUÍA RÁPIDA DE DEBUGGING - ESP32-CAM DriveGuard

## 📋 TABLA DE CONTENIDOS

1. [Inicio Rápido](#inicio-rápido)
2. [Comandos Esenciales](#comandos-esenciales)
3. [Interpretación de Logs](#interpretación-de-logs)
4. [Troubleshooting Común](#troubleshooting-común)
5. [Servidor de Prueba](#servidor-de-prueba)
6. [Checklist de Verificación](#checklist-de-verificación)

---

## 🚀 INICIO RÁPIDO

### Paso 1: Conectar Serial Monitor

```bash
# Abrir monitor serial en PlatformIO
pio device monitor

# O usar cualquier terminal serial a 115200 baud
```

### Paso 2: Ver Comandos Disponibles

```
help
```

### Paso 3: Verificar Sistema

```
info
```

**Debes ver**:
- ✓ WiFi Conectado
- ✓ Camera Ready: SI
- IP Address visible
- Frames Enviados incrementando

---

## 📝 COMANDOS ESENCIALES

### `help` - Lista de Comandos
Muestra todos los comandos disponibles.

**Uso**: Escribe `help` y presiona Enter

**Output esperado**:
```
╔═══════════════════════════════════════╗
║        COMANDOS DISPONIBLES          ║
╠═══════════════════════════════════════╣
║ help      - Mostrar esta ayuda       ║
║ info      - Información del sistema  ║
...
```

---

### `info` - Dashboard Completo

**Cuándo usar**: Cada vez que quieras ver el estado general del sistema

**Output esperado**:
```
┌─────────── DEBUG INFO ───────────┐
│ WiFi Status    : ✓ Conectado
│ IP Address     : 192.168.1.50
│ Signal (RSSI)  : -45 dBm
│ Camera Ready   : ✓ SI
│ Frames Enviados: 150
│ Frames Error   : 3
│ Success Rate   : 98.04%
│ Free Heap      : 145236 bytes
│ Free PSRAM     : 4023456 bytes
│ Uptime         : 78 seg
└──────────────────────────────────┘
```

**Qué verificar**:
- ✅ WiFi Status = ✓ Conectado
- ✅ Camera Ready = ✓ SI
- ✅ Success Rate > 95%
- ✅ Free Heap > 100,000 bytes
- ✅ Frames Enviados incrementando

---

### `stats` - Estadísticas de Frames

**Cuándo usar**: Para ver rendimiento detallado de captura/envío

**Output esperado**:
```
┌───────── ESTADÍSTICAS ──────────┐
│ Frames exitosos: 150
│ Frames con error: 3
│ Total intentos: 153
│ Tasa de éxito: 98.04%
│ Último frame: hace 485 ms
└─────────────────────────────────┘
```

**Qué verificar**:
- ✅ Tasa de éxito > 95%
- ✅ Último frame < 1000 ms (debe estar capturando activamente)
- ⚠️ Si "Frames con error" crece rápido → revisar conexión

---

### `test` - Captura Manual

**Cuándo usar**: Para probar una captura y envío individual

**Output esperado**:
```
[TEST] Capturando frame de prueba...
[Capture] Frame capturado: 12543 bytes, formato: 4
[Base64] Imagen codificada: 12543 bytes -> 16724 caracteres
[HTTP] Enviando a: http://192.168.1.100:8080/upload
[HTTP] Payload size: 16780 bytes
[HTTP] ✓ Respuesta: 200
[HTTP] Server response: {"status":"ok"}
[TEST] Resultado: ✓ ÉXITO
```

**Si falla**:
```
[TEST] Resultado: ✗ ERROR
```
→ Revisa los logs anteriores para ver dónde falló (Capture, Base64, o HTTP)

---

### `wifi` - Estado de Red

**Cuándo usar**: Cuando hay problemas de conectividad

**Output esperado**:
```
┌──────── ESTADO WiFi ────────┐
│ Estado: ✓ Conectado
│ SSID: DriveGuard
│ IP: 192.168.1.50
│ RSSI: -45 dBm
│ Canal: 6
└─────────────────────────────┘
```

**Interpretación de RSSI**:
- -30 a -50 dBm: Excelente ✅
- -50 a -60 dBm: Buena ✅
- -60 a -70 dBm: Aceptable ⚠️
- -70 a -80 dBm: Débil ⚠️
- < -80 dBm: Muy débil ❌

---

### `tasks` - Estado FreeRTOS

**Cuándo usar**: Para verificar que la tarea de captura está corriendo

**Output esperado**:
```
┌───── FREERTOS TASKS STATUS ──────┐
│ Task: CaptureTask
│ Status: ✓ Running
│ Stack High Water Mark: 2456 bytes
└──────────────────────────────────┘
```

**Qué verificar**:
- ✅ Status = ✓ Running
- ✅ Stack High Water Mark > 1000 bytes (buffer saludable)
- ❌ Si Stack < 500 bytes → riesgo de stack overflow

---

### `ip` - Mostrar y Broadcast IP

**Cuándo usar**: Para obtener la IP del ESP32 y hacer broadcast

**Output esperado**:
```
╔═══════════════════════════════════╗
║  IP ASIGNADA: 192.168.1.50       ║
╚═══════════════════════════════════╝

[Network] MAC Address: AA:BB:CC:DD:EE:FF
[Network] Gateway: 192.168.1.1
[Network] Subnet: 255.255.255.0
[Network] DNS: 192.168.1.1
[Network] Signal Strength: -45 dBm
[UDP] Broadcast enviado: ESP32CAM_IP:192.168.1.50
```

**Uso práctico**:
Copia la IP y úsala en el código de Flutter o en el servidor de prueba.

---

### `reset` - Reiniciar Estadísticas

**Cuándo usar**: Después de hacer cambios o para empezar mediciones limpias

**Output esperado**:
```
[CMD] ✓ Estadísticas reiniciadas
```

Los contadores de frames vuelven a 0.

---

### `reboot` - Reiniciar ESP32

**Cuándo usar**: Como último recurso si el sistema se vuelve inestable

**Output esperado**:
```
[CMD] Reiniciando ESP32 en 3 segundos...
```

El ESP32 se reiniciará completamente y volverá a ejecutar setup().

---

## 📊 INTERPRETACIÓN DE LOGS

### Logs de Inicio

```
╔════════════════════════════════════════════╗
║   ESP32-CAM DRIVEGUARD - DEBUG MONITOR    ║
║            FASE 1 - v1.0                  ║
╚════════════════════════════════════════════╝

[Setup] Iniciando configuración ESP32-CAM...

[Camera] Inicializando cámara...
[Camera] ✓ Cámara inicializada correctamente
[Camera] Resolución: QVGA (320x240)
[Camera] Calidad JPEG: 12
[Camera] Frame buffers: 2

[WiFi] Iniciando conexión...
[WiFi] SSID: DriveGuard
..........
[WiFi] ✓ Conexión exitosa!

╔═══════════════════════════════════╗
║  IP ASIGNADA: 192.168.1.50       ║
╚═══════════════════════════════════╝

[Setup] ✓ Tarea de captura iniciada en Core 0

╔════════════════════════════════════════╗
║    SISTEMA LISTO Y OPERATIVO          ║
╚════════════════════════════════════════╝

[Task] CaptureTask iniciada
```

**✅ TODO OK**: Si ves este flujo completo

---

### Logs de Captura Exitosa

```
[Capture] Frame capturado: 12543 bytes, formato: 4
[Base64] Imagen codificada: 12543 bytes -> 16724 caracteres
[HTTP] Enviando a: http://192.168.1.100:8080/upload
[HTTP] Payload size: 16780 bytes
[HTTP] ✓ Respuesta: 200
[HTTP] Server response: {"status":"ok"}
[Task] ✓ Frame #151 enviado exitosamente (tiempo: 234 ms)
```

**Valores normales**:
- Frame size: 10,000 - 15,000 bytes
- Base64 size: ~1.33x el tamaño del frame
- Tiempo de envío: 150 - 400 ms

---

### Logs de Error HTTP

```
[Capture] Frame capturado: 12543 bytes, formato: 4
[Base64] Imagen codificada: 12543 bytes -> 16724 caracteres
[HTTP] Enviando a: http://192.168.1.100:8080/upload
[HTTP] Payload size: 16780 bytes
[HTTP] ✗ Error: connection timeout
[Task] ✗ Error enviando frame #152 (total errores: 4)
```

**Causas comunes**:
1. Servidor no está corriendo
2. IP incorrecta en el código
3. Firewall bloqueando conexión
4. Red inestable

---

### Logs de Error de Cámara

```
[Capture] ✗ Error al capturar frame
[Task] ✗ Error enviando frame #153 (total errores: 5)
```

**Causas comunes**:
1. Cámara desconectada
2. Problema de hardware
3. Memoria insuficiente

---

## 🔍 TROUBLESHOOTING COMÚN

### Problema 1: "WiFi no conecta"

**Síntomas**:
```
[WiFi] Iniciando conexión...
[WiFi] SSID: DriveGuard
..........
[WiFi] ✗ Error: No se pudo conectar
```

**Solución paso a paso**:

1. **Verificar credenciales** en `main.cpp` líneas 16-17:
   ```cpp
   const char* WIFI_SSID = "DriveGuard";
   const char* WIFI_PASSWORD = "driveguard123";
   ```

2. **Verificar router**: ¿Está encendido? ¿Está en alcance?

3. **Verificar señal**: Acerca el ESP32 al router

4. **Reintentar**: Escribe `reboot` en el Serial Monitor

5. **Verificar modo WiFi**: El ESP32 debe estar en modo Station (WIFI_STA)

---

### Problema 2: "Frames no se envían - Error HTTP"

**Síntomas**:
```
[HTTP] ✗ Error: connection timeout
[Task] ✗ Error enviando frame
```

**Solución paso a paso**:

1. **Verificar IP del servidor** en `main.cpp` línea 32:
   ```cpp
   const char* FLUTTER_APP_IP = "192.168.1.100";  // <-- CAMBIAR AQUÍ
   ```

2. **Verificar que el servidor esté corriendo**:
   - Si usas el servidor de prueba: `python test_server.py`
   - Debe mostrar: `Servidor escuchando en: http://...`

3. **Hacer ping desde tu PC**:
   ```bash
   # Ping al ESP32
   ping 192.168.1.50

   # Ping al servidor
   ping 192.168.1.100
   ```

4. **Verificar firewall**:
   - Windows: Permitir puerto 8080 en Firewall
   - Temporalmente desactiva firewall para probar

5. **Captura manual**:
   ```
   test
   ```
   → Revisa logs detallados del error

---

### Problema 3: "Cámara no inicializa"

**Síntomas**:
```
[Camera] ✗ Error de inicialización: 0x20001
[Setup] ✗ FATAL: No se pudo inicializar la cámara
```

**Solución paso a paso**:

1. **Verificar conexión física**:
   - Desconecta y vuelve a conectar el cable de la cámara
   - Asegúrate que el seguro azul esté trabado

2. **Verificar pines**: Los pines están hardcodeados para AI-Thinker
   - Si tienes otro modelo de ESP32-CAM, ajusta los pines

3. **Reboot**:
   ```
   reboot
   ```

4. **Verificar PSRAM**:
   ```
   info
   ```
   → Free PSRAM debe ser > 3 MB

5. **Último recurso**: Desconecta alimentación, espera 10 seg, reconecta

---

### Problema 4: "Sistema lento o inestable"

**Síntomas**:
- FPS < 1
- Timeouts frecuentes
- Free Heap disminuyendo constantemente

**Solución paso a paso**:

1. **Verificar memoria**:
   ```
   info
   ```
   → Free Heap debe ser > 100,000 bytes

2. **Verificar stack**:
   ```
   tasks
   ```
   → Stack High Water Mark > 1000 bytes

3. **Reducir frecuencia**: En `main.cpp` línea 359
   ```cpp
   const TickType_t delay500ms = pdMS_TO_TICKS(1000);  // Cambiar de 500 a 1000
   ```

4. **Reiniciar estadísticas**:
   ```
   reset
   ```

5. **Reboot completo**:
   ```
   reboot
   ```

---

### Problema 5: "Success Rate < 95%"

**Síntomas**:
```
│ Success Rate   : 78.23%
```

**Solución paso a paso**:

1. **Verificar señal WiFi**:
   ```
   wifi
   ```
   → RSSI debe ser > -70 dBm

2. **Verificar servidor**: ¿Está respondiendo rápido?
   - Revisa logs del servidor
   - Revisa carga del servidor

3. **Aumentar timeout**: En `main.cpp` línea 34
   ```cpp
   const int HTTP_TIMEOUT_MS = 5000;  // Cambiar de 2000 a 5000
   ```

4. **Reducir calidad**: En `main.cpp` línea 220
   ```cpp
   config.jpeg_quality = 15;  // Cambiar de 12 a 15 (menor calidad = menor tamaño)
   ```

---

## 🖥️ SERVIDOR DE PRUEBA

### Instalación

```bash
# Instalar dependencias
pip install -r requirements.txt

# Ejecutar servidor
python test_server.py
```

### Output Esperado

```
============================================================
  ESP32-CAM DRIVEGUARD - SERVIDOR DE PRUEBA
  Fase 1 - Testing Tool
============================================================
  Servidor iniciado: 2024-01-15 14:30:00
  Guardar imágenes: SI
  Directorio: received_frames
============================================================

┌─────────────────────────────────────────────────────┐
│  CONFIGURACIÓN DEL ESP32-CAM:                      │
│                                                     │
│  const char* FLUTTER_APP_IP = "192.168.1.100";     │
│                                                     │
└─────────────────────────────────────────────────────┘

Servidor escuchando en: http://192.168.1.100:8080
Endpoint de upload: http://192.168.1.100:8080/upload

Presiona Ctrl+C para detener el servidor
```

### Logs por Frame Recibido

```
[14:30:15] Frame #1
  ├─ Timestamp ESP32: 5234 ms
  ├─ Tamaño JPEG: 12543 bytes (12.25 KB)
  ├─ Tamaño Base64: 16724 bytes (16.33 KB)
  ├─ Resolución: 320x240 px
  ├─ Formato: JPEG
  └─ ✓ Guardado: frame_00001_t5234.jpg
```

### Ver Imágenes Recibidas

Las imágenes se guardan en: `received_frames/`

```bash
# Windows
explorer received_frames

# Linux/Mac
open received_frames
```

Nombres de archivo: `frame_00001_t5234.jpg`
- `00001`: Número de frame
- `t5234`: Timestamp del ESP32

---

## ✅ CHECKLIST DE VERIFICACIÓN

### Antes de Compilar

- [ ] IP de Flutter App configurada (línea 32 de main.cpp)
- [ ] Puerto COM correcto (línea 17 de platformio.ini)
- [ ] Credenciales WiFi correctas (líneas 16-17 de main.cpp)

### Después de Subir al ESP32

- [ ] LED parpadea rápidamente (conectando WiFi)
- [ ] LED se queda fijo (WiFi conectado)
- [ ] Serial Monitor muestra header de inicio
- [ ] `info` muestra WiFi conectado
- [ ] `info` muestra Camera Ready: SI
- [ ] IP asignada visible

### Funcionamiento Normal

- [ ] `stats` muestra frames incrementando
- [ ] Success Rate > 95%
- [ ] `tasks` muestra CaptureTask Running
- [ ] `test` ejecuta captura exitosamente
- [ ] Servidor de prueba recibe frames
- [ ] Imágenes guardadas en `received_frames/`

### Rendimiento Óptimo

- [ ] FPS = 2 (1 frame cada 500ms)
- [ ] Tiempo de envío < 400ms
- [ ] Free Heap > 100,000 bytes
- [ ] Success Rate > 98%
- [ ] RSSI > -60 dBm

---

## 📞 COMANDOS RÁPIDOS DE EMERGENCIA

Si el sistema está completamente trabado:

```
reboot
```

Si quieres reiniciar mediciones:

```
reset
stats
```

Si quieres ver todo de un vistazo:

```
info
stats
tasks
wifi
```

---

## 🎯 WORKFLOW DE DEBUGGING RECOMENDADO

### Al Encender

1. `info` → Verificar todo esté OK
2. `wifi` → Verificar señal buena
3. `tasks` → Verificar task corriendo

### Cada 5 Minutos

1. `stats` → Revisar tasa de éxito

### Cuando Hay Problemas

1. `info` → Ver dashboard completo
2. `test` → Captura manual para ver logs detallados
3. `wifi` → Verificar conexión
4. `tasks` → Verificar task viva
5. Si persiste → `reboot`

---

## 📈 MONITOREO CONTINUO

El sistema automáticamente imprime:

- **Cada frame**: Status de envío
- **Cada 60 segundos**: Info completa del sistema
- **Cada 30 segundos**: Verificación WiFi
- **Cada 5 minutos**: Broadcast IP

No necesitas estar escribiendo comandos constantemente, solo observa los logs automáticos.

---

**¡Listo para debuggear! 🚀**

Si encuentras un problema no cubierto aquí, revisa los logs detallados del Serial Monitor.
