# PS5 Relapse - ESP32 Offline Host

Host local autónomo y plug-and-play para el exploit **PS5 Relapse**, diseñado para ejecutarse en microcontroladores **ESP32-S3** (8MB Flash / 2MB PSRAM) y adaptable a placas de la familia **ESP32 / ESP32-S2**.

Permite ejecutar el exploit de forma 100% offline directamente desde la **Guía del usuario** de la consola, sin requerir conexión a internet ni ordenadores externos.

---

## Características Principales

* **Soporte de Firmware PS5**: Totalmente compatible con versiones desde **7.00 hasta 13.60**.
* **Punto de Acceso Wi-Fi Offline**: Crea su propia red segura (`SSID: relapse`), aislando la consola y previniendo cualquier intento de actualización de firmware.
* **Servidor DNS Selectivo (UDP :53)**:
  * Resuelve autoritativamente `manuals.playstation.net` y `ps5.host` apuntando a `192.168.4.1`.
  * Devuelve `NXDOMAIN` para cualquier otra consulta, bloqueando telemetría y descargas de Sony.
* **Servidores Web HTTP (:80) y HTTPS (:443 TLS)**:
  * Certificado SSL integrado para compatibilidad nativa con la Guía del usuario de PS5.
  * Streaming directo por bloques con asignación en **PSRAM** para máxima estabilidad de memoria SRAM.
* **Compresión GZIP Automática y LittleFS**:
  * Pipeline automatizado (`scripts/prepare_data.py`) que optimiza y comprime los assets desde `server/` hacia `data/`.
  * Reduce los payloads y scripts web de >3.1 MB a 1.5 MB, garantizando transferencias ultrarrápidas hacia el navegador de la consola.
* **Cadena Relapse Completa**:
  * Exploit WebKit con primitivas de lectura/escritura (ARW).
  * Exploit Kernel (carrera y escape de sandbox).
  * Servidor ELF Loader en puerto `9021`.
  * Inyección automática de Payload Manager (`pldmgr_v0.6.3.elf`) y soporte de recarga de payloads mediante el botón **R2**.
* **LED de Estado ARGB / WS2812**:
  * Tarea en segundo plano para animación suave de arcoíris (HSV) en el LED RGB integrado (GPIO 48 en DevKit S3).

---

## Estructura del Repositorio

```text
├── server/                 # Archivos fuente del exploit web (HTML, JS, offsets y payloads)
│   ├── offsets/            # Tablas de offsets y gadgets para FW 7.00 - 13.60
│   ├── payloads/           # Payloads ELF y binarios (elfldr, pldmgr, kexp)
│   ├── src/                # Lógica del exploit (WebKit, ROP, Kernel, etc.)
│   └── index.html          # Interfaz web del exploit
├── include/                # Cabeceras C++ del firmware ESP32
│   ├── config.h            # Configuración de Wi-Fi, puertos, IP y hardware
│   ├── cert.h              # Certificado TLS y llave privada para HTTPS
│   ├── dns.h               # Servidor DNS de baja latencia con filtrado
│   └── HttpsHandler.h      # Servidor HTTP/HTTPS con soporte LittleFS y streaming PSRAM
├── scripts/
│   └── prepare_data.py     # Script PlatformIO que sincroniza y comprime server/ -> data/
├── src/
│   └── main.cpp            # Inicialización de hardware, Wi-Fi AP, tareas y LED
├── partitions_8MB.csv      # Tabla de particiones (2.5MB App / 5.43MB LittleFS)
└── platformio.ini          # Configuración del entorno de compilación PlatformIO
```

---

## Requisitos de Hardware

* **Placa recomendada**: ESP32-S3 DevKitC-1 N8R2 (8MB Quad-Flash / 2MB Quad-PSRAM).
* Cable USB de datos para conexión al puerto serie `/dev/ttyACM0` (o COM correspondiente).
* Consola PS5 con firmware entre **7.00 y 13.60**.

---

## Compilación y Flasheo

Este proyecto utiliza [PlatformIO](https://platformio.org/). Puedes utilizar VS Code con la extensión PlatformIO o la CLI de PlatformIO:

### 1. Compilar y flashear el sistema de archivos (LittleFS)
Este paso sincroniza automáticamente la carpeta `server/`, comprime los archivos con gzip y escribe la partición LittleFS en la memoria Flash:

```bash
pio run -t buildfs
pio run -t uploadfs
```

### 2. Compilar y flashear el Firmware
Compila el binario C++ y lo transfiere a la placa:

```bash
pio run -t upload
```

### 3. Monitor Serial
Para verificar el arranque, diagnósticos de PSRAM, eventos de red y peticiones HTTP/HTTPS:

```bash
pio device monitor
```

---

## Configuración en la Consola PS5

1. Enciende la PS5 y dirígete a:
   **Ajustes** > **Red** > **Configurar conexión a Internet**.
2. Conéctate a la red Wi-Fi emitida por el microcontrolador:
   * **SSID**: `relapse`
   * **Contraseña**: *(Ninguna, red abierta por defecto)*
3. Mantén los ajustes de **Dirección IP** y **DNS** en **Automático** (el ESP32 asigna automáticamente la IP, máscara, gateway y DNS `192.168.4.1`).
4. Ve a **Ajustes** > **Guía del usuario, seguridad y otra información** > **Guía del usuario**.
5. Se cargará la interfaz de PS5 Relapse. Pulsa el botón **Iniciar** para comenzar la ejecución de la cadena.
6. Una vez completado, el ELF Loader quedará a la escucha en el puerto `9021`. Si necesitas recargar los payloads en cualquier momento, presiona el botón **R2** del mando.

---

## Personalización

Puedes modificar los parámetros de funcionamiento en [`include/config.h`](include/config.h):

* `WIFI_SSID`: Nombre de la red Wi-Fi (por defecto `"relapse"`).
* `WIFI_PASS`: Contraseña de red (déjalo vacío `""` para red abierta o define al menos 8 caracteres para WPA2).
* `RGB_LED_PIN`: Pin del LED RGB direccionable (por defecto GPIO `48`).
* `ARGB_BRIGHTNESS`: Nivel de brillo del LED (0 a 255).
* `AP_IP`: Dirección IP del host (por defecto `192.168.4.1`).

---

## Créditos

### Desarrollo del Exploit Relapse
* **Sonic_Iso**: Kernel Exploit.
* **Jordy**: WebKit Exploit y Kernel Bug.
* **ntfargo**: Desarrollo de Exploit.
* **ufm42**: Desarrollo de Exploit.
* **Dr. Yenyen**: Testing y verificación.

### Agradecimientos Especiales
* TheFlow, SlidyBat, Flatz, cow, nhk, bollarz, Sleirsgoevy, EchoStretch, EarthOnion.

---

## Aviso Legal / Disclaimer

Este proyecto ha sido desarrollado con fines **estrictamente educativos y de investigación en ciberseguridad**. No promueve la piratería ni el acceso no autorizado a dispositivos comerciales. Utilízalo únicamente en dispositivos de tu propiedad o en aquellos para los que dispongas de autorización expresa, cumpliendo siempre con las leyes y regulaciones aplicables.

El software se proporciona tal cual, sin garantías de ningún tipo. Los mantenedores no se hacen responsables de daños resultantes del uso o mal uso del software.
