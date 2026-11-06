# Implementation Plan: Migración a ESP32-S3 N8R2 y Modo Arcoíris ARGB

Adaptar el firmware del host offline de PS5 para la placa ESP32-S3 (8MB Flash / 2MB PSRAM) conectada en `/dev/ttyACM0`, aprovechando la memoria PSRAM, la nueva tabla de particiones de 8MB y agregando control del LED ARGB (WS2812) en modo arcoíris continuo.

## Proposed Changes

### 1. Sistema de Particiones: `partitions_8MB.csv`
- Crear tabla de particiones para 8 MB Flash (0x800000):
  - `nvs`: 0x9000 (20 KB)
  - `otadata`: 0xe000 (8 KB)
  - `app0`: 0x10000 (2.5 MB)
  - `spiffs`: 0x290000 (5.43 MB / 0x570000)

### 2. Configuración del Proyecto: `platformio.ini`
- Configurar entorno `env:esp32s3_n8r2`:
  - Placa: `esp32-s3-devkitc-1`
  - Memoria: `board_build.arduino.memory_type = qio_qspi` (2MB PSRAM Quad-SPI)
  - Flash: `board_build.flash_mode = qio`, `board_upload.flash_size = 8MB`
  - Particiones: `board_build.partitions = partitions_8MB.csv`
  - Filesystem: `board_build.filesystem = littlefs`
  - Flags de compilación: `-D BOARD_HAS_PSRAM`, `-D ARDUINO_USB_CDC_ON_BOOT=0`, `-D ARDUINO_USB_MODE=0`
  - Puerto de carga y monitor: `/dev/ttyACM0` a 115200 baudios.

### 3. Configuración de Hardware: `include/config.h`
- Definir pin ARGB: `RGB_LED_PIN` (por defecto 48 con fallback a `RGB_BUILTIN`).
- Constantes de brillo y velocidad para el efecto arcoíris.

### 4. Servidor HTTPS: `include/HttpsHandler.h`
- Aumentar `secure.httpd.max_open_sockets` a 4 (estándar para ESP32-S3 con PSRAM).
- Consolidar buffer de transmisión en 8192 bytes.

### 5. Control y Diagnóstico: `src/main.cpp`
- Diagnóstico de PSRAM en `setup()`: verificar `ESP.getPsramSize()` y `ESP.getFreePsram()`.
- Tarea o función no bloqueante para animación de arcoíris usando `rgbLedWrite()` nativo del core ESP32.

---

## Verification Plan

### Automated / Build Verification
- Compilar el proyecto con PlatformIO:
  ```bash
  /home/jiodev/.platformio/penv/bin/pio run -e esp32s3_n8r2
  ```
- Generar la imagen del sistema de archivos LittleFS:
  ```bash
  /home/jiodev/.platformio/penv/bin/pio run -e esp32s3_n8r2 -t buildfs
  ```
- Confirmar que no hay errores de sintaxis, falta de librerías ni desbordamiento de memoria.

