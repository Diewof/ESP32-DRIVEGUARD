# ESP32-CAM DriveGuard - FASE 1
## Configuración Inicial y Sistema de Captura

---

## RESUMEN DE IMPLEMENTACIÓN

Todas las tareas de la Fase 1 han sido implementadas exitosamente:

### ✅ Tarea 1.1: Configuración WiFi
- Credenciales configuradas: `DriveGuard` / `driveguard123`
- Sistema de reintentos automáticos (máximo 10 intentos)
- Indicador LED visual:
  - **Parpadeo rápido**: Conectando a WiFi
  - **LED fijo encendido**: Conectado exitosamente
  - **LED apagado**: Desconectado o error
- Función de reconexión automática en caso de pérdida de señal

### ✅ Tarea 1.2: Obtención y Broadcast de IP
- La IP se obtiene automáticamente al conectarse
- Se muestra en el Serial Monitor con formato destacado
- Implementado broadcast UDP en puerto 8888 para descubrimiento automático
- Variables globales disponibles para uso posterior

### ✅ Tarea 1.3: Configuración de Cámara
- Resolución: **QVGA (320x240 píxeles)**
- Calidad JPEG: **12** (balance óptimo calidad/tamaño)
- Double buffering habilitado para mejor rendimiento
- Configuración optimizada del sensor OV2640

### ✅ Tarea 1.4: Cliente HTTP y Envío Base64
- Conversión automática de frames a Base64
- Cliente HTTP con timeout de 2 segundos
- Envío de imágenes a Flutter App vía POST request
- Formato JSON: `{"image": "<base64>", "timestamp": <millis>}`
- Manejo robusto de errores con logs detallados

### ✅ Tarea 1.5: Loop de Envío Continuo
- Tarea FreeRTOS dedicada en Core 0
- Frecuencia de captura: **500ms** (2 frames por segundo)
- Contador de frames exitosos y errores
- Reconexión automática en caso de fallo
- Stack size: 8KB

---

## CONFIGURACIÓN PREVIA

### 1. Ajustar IP de la App Flutter

Abre el archivo `src/main.cpp` y modifica la línea 32:

```cpp
const char* FLUTTER_APP_IP = "192.168.1.100";  // <-- CAMBIAR AQUÍ
```

Reemplaza `192.168.1.100` con la IP real donde esté corriendo tu aplicación Flutter.

### 2. Verificar Puerto COM

En `platformio.ini`, línea 17:

```ini
upload_port = COM5  ; <-- CAMBIA esto por tu puerto FTDI
```

### 3. Conectar Hardware

1. Conecta el programador FTDI al ESP32-CAM:
   - VCC → 5V
   - GND → GND
   - TX → RX (GPIO 3)
   - RX → TX (GPIO 1)

2. Para programar, conecta GPIO 0 a GND antes de dar alimentación

3. Después de programar, desconecta GPIO 0 de GND y reinicia

---

## COMPILACIÓN Y CARGA

```bash
# Compilar el proyecto
pio run

# Subir al ESP32-CAM
pio run --target upload

# Abrir monitor serial
pio device monitor
```

**IMPORTANTE**: Como solicitaste, el código NO intenta compilar ni subir automáticamente. Debes hacerlo manualmente.

---

## HERRAMIENTAS DE DEBUGGING

### 1. Monitor Serial Interactivo

Al iniciar, verás un header visual:

```
╔════════════════════════════════════════════╗
║   ESP32-CAM DRIVEGUARD - DEBUG MONITOR    ║
║            FASE 1 - v1.0                  ║
╚════════════════════════════════════════════╝
```

### 2. Comandos Disponibles

Escribe estos comandos en el Serial Monitor (115200 baud):

| Comando | Descripción |
|---------|-------------|
| `help` o `?` | Muestra lista de todos los comandos |
| `info` | Información completa del sistema (WiFi, memoria, estadísticas) |
| `stats` | Estadísticas detalladas de frames (éxitos, errores, tasa) |
| `tasks` | Estado de las tareas FreeRTOS y stack usage |
| `wifi` | Estado detallado de la conexión WiFi |
| `ip` | Mostrar IP y enviar broadcast UDP |
| `test` | Capturar y enviar un frame de prueba manual |
| `reset` | Reiniciar contadores de estadísticas |
| `reboot` | Reiniciar el ESP32 completamente |

### 3. Logs Automáticos

El sistema imprime automáticamente:

- **Cada 60 segundos**: Información completa del sistema
- **Cada frame**: Status de envío (éxito/error) con tiempo de ejecución
- **Cada 30 segundos**: Verificación de conexión WiFi
- **Cada 5 minutos**: Broadcast UDP de la IP

### 4. Ejemplo de Salida de `info`

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

### 5. Ejemplo de Salida de `stats`

```
┌───────── ESTADÍSTICAS ──────────┐
│ Frames exitosos: 150
│ Frames con error: 3
│ Total intentos: 153
│ Tasa de éxito: 98.04%
│ Último frame: hace 485 ms
└─────────────────────────────────┘
```

### 6. Logs de Captura y Envío

Cada vez que se captura y envía un frame, verás:

```
[Capture] Frame capturado: 12543 bytes, formato: 4
[Base64] Imagen codificada: 12543 bytes -> 16724 caracteres
[HTTP] Enviando a: http://192.168.1.100:8080/upload
[HTTP] Payload size: 16780 bytes
[HTTP] ✓ Respuesta: 200
[HTTP] Server response: {"status":"ok"}
[Task] ✓ Frame #151 enviado exitosamente (tiempo: 234 ms)
```

---

## VERIFICACIÓN DE FUNCIONAMIENTO

### Checklist de Pruebas

1. **Conexión WiFi**
   ```
   > wifi
   ```
   Debe mostrar: `✓ Conectado` y la IP asignada

2. **Cámara Inicializada**
   ```
   > info
   ```
   Debe mostrar: `Camera Ready: ✓ SI`

3. **Captura Manual**
   ```
   > test
   ```
   Debe capturar y enviar un frame, mostrando todos los logs del proceso

4. **Estadísticas en Tiempo Real**
   ```
   > stats
   ```
   Verifica que `Frames exitosos` esté incrementando

5. **FreeRTOS Task**
   ```
   > tasks
   ```
   Debe mostrar: `Status: ✓ Running` y el Stack High Water Mark

6. **Tasa de Éxito**
   Espera 1 minuto y ejecuta:
   ```
   > stats
   ```
   La `Tasa de éxito` debe ser > 95%

---

## SOLUCIÓN DE PROBLEMAS

### Problema: WiFi no conecta

**Síntomas**: LED parpadea continuamente, log muestra `✗ Error: No se pudo conectar`

**Soluciones**:
1. Verifica que el SSID sea exactamente "DriveGuard"
2. Verifica que el password sea "driveguard123"
3. Asegúrate de que el router esté encendido y en alcance
4. Verifica señal con: `> wifi` (RSSI debe ser > -80 dBm)

### Problema: Cámara no inicializa

**Síntomas**: `Camera Ready: ✗ NO`

**Soluciones**:
1. Revisa las conexiones físicas del módulo de cámara
2. Verifica que la cámara esté firmemente conectada al PCB
3. Reinicia el ESP32: `> reboot`
4. Verifica que el modelo sea AI-Thinker ESP32-CAM

### Problema: Frames no se envían

**Síntomas**: `Frames Error` incrementa continuamente

**Soluciones**:
1. Verifica que la IP de Flutter App esté correcta en el código (línea 32)
2. Verifica que el servidor Flask esté corriendo en el puerto 8080
3. Haz ping a la IP del servidor desde tu PC
4. Revisa firewall del servidor
5. Prueba manualmente: `> test`

### Problema: Timeout HTTP

**Síntomas**: `[HTTP] ✗ Error: connection timeout`

**Soluciones**:
1. Aumenta `HTTP_TIMEOUT_MS` en línea 34 (de 2000 a 5000)
2. Verifica latencia de red
3. Reduce tamaño de imagen cambiando calidad JPEG a 15

### Problema: Free Heap bajo

**Síntomas**: `Free Heap` < 50000 bytes, sistema inestable

**Soluciones**:
1. Reduce frecuencia de captura de 500ms a 1000ms (línea 359)
2. Verifica memory leaks con: `> info` (el heap debe mantenerse estable)
3. Reinicia estadísticas: `> reset`

---

## INDICADORES LED

### LED Integrado (GPIO 33)

- **Parpadeo rápido (cada 1 segundo)**: Intentando conectar a WiFi
- **Encendido fijo**: WiFi conectado correctamente
- **Apagado**: WiFi desconectado o error de conexión
- **Parpadeos rápidos x3**: Inicialización exitosa completa
- **Parpadeos rápidos x10**: Error fatal (cámara o WiFi)

### LED Flash (GPIO 4)

- Permanece **apagado** durante operación normal (para ahorrar energía)
- Se puede activar manualmente para debugging

---

## ESTRUCTURA DEL CÓDIGO

```
src/main.cpp
├── [Líneas 15-44]   Variables de configuración
├── [Líneas 68-100]  Funciones de debugging
├── [Líneas 102-157] TAREA 1.1: WiFi
├── [Líneas 159-187] TAREA 1.2: IP y Broadcast
├── [Líneas 189-262] TAREA 1.3: Configuración Cámara
├── [Líneas 264-351] TAREA 1.4: HTTP Client y Base64
├── [Líneas 353-390] TAREA 1.5: FreeRTOS Task
├── [Líneas 392-471] Sistema de comandos debug
└── [Líneas 473-568] Setup y Loop principal
```

---

## MÉTRICAS DE RENDIMIENTO ESPERADAS

| Métrica | Valor Esperado | Cómo Verificar |
|---------|----------------|----------------|
| Frames por segundo | 2 FPS (1 cada 500ms) | `> stats` → último frame < 600ms |
| Tasa de éxito | > 95% | `> stats` → Success Rate |
| Tiempo de envío | 150-400 ms | Logs de `[Task]` |
| Tamaño de frame | 10-15 KB | Logs de `[Capture]` |
| Tamaño Base64 | 13-20 KB | Logs de `[Base64]` |
| Free Heap | > 100 KB | `> info` |
| Free PSRAM | > 3 MB | `> info` |
| Stack usage CaptureTask | < 6 KB | `> tasks` |

---

## FORMATO DE DATOS ENVIADOS

### Estructura JSON

```json
{
  "image": "<base64_encoded_jpeg_string>",
  "timestamp": 123456789
}
```

- `image`: String Base64 del frame JPEG capturado
- `timestamp`: Milisegundos desde el boot del ESP32

### Endpoint

```
POST http://<FLUTTER_APP_IP>:8080/upload
Content-Type: application/json
```

---

## PRÓXIMOS PASOS (Fases siguientes)

Esta implementación de Fase 1 proporciona la base para:

- **Fase 2**: Detección de objetos con TensorFlow Lite
- **Fase 3**: Integración con sensores MPU6050
- **Fase 4**: Sistema de alertas y almacenamiento
- **Fase 5**: Interfaz Flutter completa

---

## NOTAS TÉCNICAS

### Consumo de Memoria

- **PSRAM**: Utilizado para buffers de cámara (esencial)
- **Heap**: ~8KB por tarea FreeRTOS
- **Stack de CaptureTask**: 8KB asignados

### Optimizaciones Implementadas

1. **Double buffering**: Permite captura mientras se procesa frame anterior
2. **Base64 encoding**: Eficiente para transmisión HTTP
3. **Timeout HTTP**: Evita bloqueos indefinidos
4. **FreeRTOS separado**: Core 0 para captura, Core 1 para WiFi
5. **Reconexión automática**: Sistema robusto ante fallos de red

### Seguridad

- No hay autenticación implementada (esto se puede agregar en fases posteriores)
- Broadcast UDP puede ser deshabilitado comentando líneas 509, 563

---

## CONTACTO Y SOPORTE

Para debugging avanzado:

1. Conecta Serial Monitor a 115200 baud
2. Escribe `help` para ver todos los comandos
3. Ejecuta `info` cada minuto para monitorear sistema
4. Guarda logs para análisis posterior

---

## CHANGELOG

**v1.0 - Fase 1 Inicial**
- Implementación completa de todas las tareas 1.1 a 1.5
- Sistema de debugging interactivo
- Logs detallados para troubleshooting
- Comandos seriales para control y monitoreo

---

**¡Sistema listo para compilar y probar!**

Recuerda cambiar la IP de Flutter App en la línea 32 de `main.cpp` antes de compilar.
