#!/usr/bin/env python3
"""
ESP32-CAM DriveGuard - Servidor de Prueba
Fase 1 - Testing Tool

Este script simula el servidor Flutter/Flask que recibe imágenes del ESP32-CAM.
Úsalo para verificar que el ESP32-CAM está enviando frames correctamente.

Uso:
    python test_server.py

Luego configura el ESP32-CAM para enviar a la IP de esta máquina.
"""

import base64
import json
import os
from datetime import datetime
from flask import Flask, request, jsonify
from PIL import Image
import io

app = Flask(__name__)

# Configuración
SAVE_IMAGES = True  # Guardar imágenes recibidas en disco
OUTPUT_DIR = "received_frames"
VERBOSE = True  # Mostrar logs detallados

# Estadísticas
stats = {
    "total_received": 0,
    "total_errors": 0,
    "last_timestamp": 0,
    "start_time": datetime.now()
}

# Crear directorio de salida
if SAVE_IMAGES and not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)
    print(f"✓ Directorio creado: {OUTPUT_DIR}")

def print_header():
    """Imprimir header del servidor"""
    print("\n" + "="*60)
    print("  ESP32-CAM DRIVEGUARD - SERVIDOR DE PRUEBA")
    print("  Fase 1 - Testing Tool")
    print("="*60)
    print(f"  Servidor iniciado: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"  Guardar imágenes: {'SI' if SAVE_IMAGES else 'NO'}")
    print(f"  Directorio: {OUTPUT_DIR if SAVE_IMAGES else 'N/A'}")
    print("="*60 + "\n")

def print_stats():
    """Imprimir estadísticas actuales"""
    uptime = (datetime.now() - stats['start_time']).total_seconds()
    fps = stats['total_received'] / uptime if uptime > 0 else 0
    success_rate = (stats['total_received'] / (stats['total_received'] + stats['total_errors']) * 100) if (stats['total_received'] + stats['total_errors']) > 0 else 0

    print("\n" + "-"*60)
    print("  ESTADÍSTICAS DEL SERVIDOR")
    print("-"*60)
    print(f"  Frames recibidos: {stats['total_received']}")
    print(f"  Errores: {stats['total_errors']}")
    print(f"  Tasa de éxito: {success_rate:.2f}%")
    print(f"  FPS promedio: {fps:.2f}")
    print(f"  Uptime: {int(uptime)} segundos")
    print(f"  Último timestamp ESP32: {stats['last_timestamp']} ms")
    print("-"*60 + "\n")

@app.route('/upload', methods=['POST'])
def upload_image():
    """
    Endpoint para recibir imágenes del ESP32-CAM

    Espera JSON con formato:
    {
        "image": "<base64_encoded_jpeg>",
        "timestamp": 123456789
    }
    """
    try:
        # Obtener datos JSON
        data = request.get_json()

        if not data:
            stats['total_errors'] += 1
            return jsonify({"status": "error", "message": "No JSON data received"}), 400

        # Validar campos requeridos
        if 'image' not in data or 'timestamp' not in data:
            stats['total_errors'] += 1
            return jsonify({"status": "error", "message": "Missing required fields"}), 400

        base64_image = data['image']
        timestamp = data['timestamp']
        stats['last_timestamp'] = timestamp

        # Decodificar imagen Base64
        try:
            image_data = base64.b64decode(base64_image)
        except Exception as e:
            stats['total_errors'] += 1
            print(f"✗ Error decodificando Base64: {e}")
            return jsonify({"status": "error", "message": "Invalid Base64 data"}), 400

        # Incrementar contador
        stats['total_received'] += 1
        frame_number = stats['total_received']

        # Obtener información de la imagen
        image_size = len(image_data)
        base64_size = len(base64_image)

        # Logs
        if VERBOSE:
            print(f"[{datetime.now().strftime('%H:%M:%S')}] Frame #{frame_number}")
            print(f"  ├─ Timestamp ESP32: {timestamp} ms")
            print(f"  ├─ Tamaño JPEG: {image_size} bytes ({image_size/1024:.2f} KB)")
            print(f"  ├─ Tamaño Base64: {base64_size} bytes ({base64_size/1024:.2f} KB)")

        # Guardar imagen si está habilitado
        if SAVE_IMAGES:
            try:
                # Abrir imagen con PIL para verificar
                img = Image.open(io.BytesIO(image_data))
                width, height = img.size

                if VERBOSE:
                    print(f"  ├─ Resolución: {width}x{height} px")
                    print(f"  ├─ Formato: {img.format}")

                # Guardar con nombre único
                filename = f"frame_{frame_number:05d}_t{timestamp}.jpg"
                filepath = os.path.join(OUTPUT_DIR, filename)

                with open(filepath, 'wb') as f:
                    f.write(image_data)

                if VERBOSE:
                    print(f"  └─ ✓ Guardado: {filename}")

            except Exception as e:
                print(f"  └─ ✗ Error guardando imagen: {e}")
                stats['total_errors'] += 1
                return jsonify({"status": "error", "message": f"Error saving image: {str(e)}"}), 500

        # Mostrar estadísticas cada 10 frames
        if frame_number % 10 == 0:
            print_stats()

        # Respuesta exitosa
        return jsonify({
            "status": "ok",
            "frame_number": frame_number,
            "timestamp_received": datetime.now().isoformat(),
            "image_size": image_size,
            "resolution": f"{width}x{height}" if SAVE_IMAGES else "N/A"
        }), 200

    except Exception as e:
        stats['total_errors'] += 1
        print(f"\n✗ ERROR: {e}\n")
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/stats', methods=['GET'])
def get_stats():
    """Endpoint para obtener estadísticas del servidor"""
    uptime = (datetime.now() - stats['start_time']).total_seconds()
    fps = stats['total_received'] / uptime if uptime > 0 else 0

    return jsonify({
        "total_received": stats['total_received'],
        "total_errors": stats['total_errors'],
        "fps_average": round(fps, 2),
        "uptime_seconds": int(uptime),
        "last_timestamp": stats['last_timestamp']
    }), 200

@app.route('/health', methods=['GET'])
def health_check():
    """Endpoint de health check"""
    return jsonify({"status": "healthy", "service": "ESP32-CAM Test Server"}), 200

@app.route('/', methods=['GET'])
def index():
    """Página de inicio"""
    return """
    <html>
    <head><title>ESP32-CAM Test Server</title></head>
    <body>
        <h1>ESP32-CAM DriveGuard - Servidor de Prueba</h1>
        <h2>Endpoints disponibles:</h2>
        <ul>
            <li><b>POST /upload</b> - Recibir imágenes del ESP32-CAM</li>
            <li><b>GET /stats</b> - Ver estadísticas</li>
            <li><b>GET /health</b> - Health check</li>
        </ul>
        <h2>Estado actual:</h2>
        <p>Frames recibidos: """ + str(stats['total_received']) + """</p>
        <p>Errores: """ + str(stats['total_errors']) + """</p>
        <p><a href="/stats">Ver estadísticas completas (JSON)</a></p>
    </body>
    </html>
    """

def get_local_ip():
    """Obtener la IP local de la máquina"""
    import socket
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except:
        return "localhost"

if __name__ == '__main__':
    print_header()

    local_ip = get_local_ip()
    port = 8080

    print("┌─────────────────────────────────────────────────────┐")
    print(f"│  CONFIGURACIÓN DEL ESP32-CAM:                      │")
    print(f"│                                                     │")
    print(f"│  const char* FLUTTER_APP_IP = \"{local_ip}\";")
    print(f"│                                                     │")
    print("└─────────────────────────────────────────────────────┘\n")

    print(f"Servidor escuchando en: http://{local_ip}:{port}")
    print(f"Endpoint de upload: http://{local_ip}:{port}/upload")
    print(f"\nPresiona Ctrl+C para detener el servidor\n")

    try:
        app.run(host='0.0.0.0', port=port, debug=False)
    except KeyboardInterrupt:
        print("\n\n" + "="*60)
        print("  SERVIDOR DETENIDO")
        print("="*60)
        print_stats()
        print("\n✓ Servidor cerrado correctamente\n")
