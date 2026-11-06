# Diseño: Replicación del Servidor Web `ps5-esp32host-main` en ESP32-S2 Mini

## Intención y Por Qué
Replicar la arquitectura probada de servicio web y DNS de `ps5-esp32host-main` (`host.c` y `dns.c`) en la placa WEMOS LOLIN ESP32-S2 Mini para servir de forma estable y privada la cadena alojada en `cadena/`.

## Objetivos y No-Objetivos
- **Objetivos**:
  - Unificar HTTP (`:80`, `ctrl_port = 32768`) y HTTPS (`:443`, `ctrl_port = 32769`) mediante `esp_http_server` y `esp_https_server` compartiendo el mismo manejador `serve_file`.
  - Replicar el comportamiento de `serve_file` de `host.c`: limpieza de query/fragmentos (`?#`), `Cache-Control: no-store`, redirección `302 Found` de `/document` y `/document/*` a `/`, endpoint `/status`, validación de rutas (`/index.html`, `/src/`, `/offsets/`, `/payloads/`) y streaming por bloques desde `LittleFS` (con soporte transparente para archivos `.gz`).
  - Replicar `ps5_dns_reply` y `dns_task` (UDP 53 en FreeRTOS) para resolver únicamente registros `A` de `manuals.playstation.net` y `ps5.host` hacia `192.168.4.1`, respondiendo `NXDOMAIN` al resto de dominios.
  - Actualizar `scripts/prepare_data.py` para sincronizar y comprimir desde `cadena/` hacia `data/`.
- **No-Objetivos**:
  - No modificar el contenido funcional de los payloads ni scripts dentro de `cadena/`.

## Restricciones
- **Hardware ESP32-S2 Mini**: 4 MB de Flash y ~320 KB de SRAM sin PSRAM. Se mantiene `LittleFS` con compresión `.gz` y `max_open_sockets` ajustado a la memoria interna (3 en HTTP, 2-3 en HTTPS con `lru_purge_enable = true` y `send_wait_timeout = 20`).

## Enfoques Considerados
1. **Opción 1 (Elegida)**: Usar las APIs nativas de ESP-IDF (`esp_http_server`, `esp_https_server`, sockets UDP FreeRTOS) sobre el entorno actual PlatformIO (`framework = arduino`) con `LittleFS`. Conserva el soporte USB CDC nativo del S2 Mini y la partición LittleFS mientras ejecuta exactamente el mismo motor de red que `host.c`/`dns.c`.
2. **Opción 2**: Migrar todo el proyecto a `framework = espidf` puro. Descartada por complejidad innecesaria en USB CDC y LittleFS sin beneficio en el servidor web.

## Decisiones de Implementación
- **`include/dns.h`**: Port directo de `ps5_dns_reply` y tarea FreeRTOS `dns_task` en puerto UDP 53.
- **`include/HttpsHandler.h` (Servidor unificado HTTP/HTTPS)**: Implementa `serve_file` y `start_servers()` replicando `host.c` con soporte `.gz` sobre `LittleFS`.
- **`src/main.cpp`**: Inicializa `LittleFS`, SoftAP con DHCP ofreciendo DNS `192.168.4.1`, lanza `dns_task` y `start_servers()`, eliminando `WebServer` y `DNSServer` de Arduino.
- **`scripts/prepare_data.py`**: Origen actualizado de `exploit/` a `cadena/`.

