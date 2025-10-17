@echo off
REM Script para iniciar el servidor de prueba ESP32-CAM
REM Windows Batch File

echo ============================================================
echo   ESP32-CAM DRIVEGUARD - INICIADOR DE SERVIDOR DE PRUEBA
echo ============================================================
echo.

REM Verificar si Python está instalado
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python no esta instalado o no esta en el PATH
    echo.
    echo Por favor instala Python desde: https://www.python.org/downloads/
    echo.
    pause
    exit /b 1
)

echo Python detectado:
python --version
echo.

REM Verificar si existe el archivo requirements.txt
if not exist requirements.txt (
    echo ERROR: No se encuentra requirements.txt
    echo Asegurate de estar en el directorio correcto del proyecto
    echo.
    pause
    exit /b 1
)

REM Preguntar si instalar dependencias
echo ¿Deseas instalar/actualizar las dependencias? (S/N)
set /p INSTALL_DEPS="> "

if /i "%INSTALL_DEPS%"=="S" (
    echo.
    echo Instalando dependencias...
    python -m pip install -r requirements.txt
    if errorlevel 1 (
        echo.
        echo ERROR: Fallo la instalacion de dependencias
        pause
        exit /b 1
    )
    echo.
    echo Dependencias instaladas exitosamente
    echo.
)

REM Verificar si existe test_server.py
if not exist test_server.py (
    echo ERROR: No se encuentra test_server.py
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo   INICIANDO SERVIDOR...
echo ============================================================
echo.
echo Presiona Ctrl+C para detener el servidor
echo.

REM Ejecutar el servidor
python test_server.py

echo.
echo Servidor detenido
pause
