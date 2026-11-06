#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "esp32-hal-rgb-led.h"
#include "config.h"
#include "dns.h"
#include "HttpsHandler.h"

static uint8_t lastStationCount = 0;

// Conversor HSV a RGB para animación suave de arcoíris en LED ARGB (WS2812)
static void updateRainbow(uint8_t hue, uint8_t brightness) {
    uint8_t r = 0, g = 0, b = 0;
    uint8_t region = hue / 43; // 6 regiones (0..5)
    uint8_t remainder = (hue - (region * 43)) * 6;

    uint8_t q = (brightness * (255 - remainder)) >> 8;
    uint8_t t = (brightness * remainder) >> 8;

    switch (region) {
        case 0: r = brightness; g = t;          b = 0;          break;
        case 1: r = q;          g = brightness; b = 0;          break;
        case 2: r = 0;          g = brightness; b = t;          break;
        case 3: r = 0;          g = q;          b = brightness; break;
        case 4: r = t;          g = 0;          b = brightness; break;
        default: r = brightness; g = 0;         b = q;          break;
    }
    rgbLedWrite(RGB_LED_PIN, r, g, b);
}

// Tarea independiente en segundo plano para el efecto arcoíris
static void rainbow_task(void *unused) {
    (void)unused;
    uint8_t hue = 0;
    for (;;) {
        updateRainbow(hue++, ARGB_BRIGHTNESS);
        vTaskDelay(pdMS_TO_TICKS(RGB_RAINBOW_MS));
    }
}

void setup() {
    // 1. Inicializar Serial por UART de hardware
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==========================================");
    Serial.println(" ESP32-S3 N8R2 - PS5 Offline Host         ");
    Serial.println("==========================================");

    // 2. Iniciar y reportar diagnóstico de memoria PSRAM
    if (psramInit()) {
        Serial.printf("[PSRAM] Detectada y activa: %u bytes (%u MB) totales | %u bytes libres\n",
                      (unsigned int)ESP.getPsramSize(),
                      (unsigned int)(ESP.getPsramSize() / (1024 * 1024)),
                      (unsigned int)ESP.getFreePsram());
    } else {
        Serial.println("[PSRAM] Aviso: No se detectó PSRAM externa.");
    }
    Serial.printf("[MEM]   Heap interno libre: %u bytes\n", (unsigned int)ESP.getFreeHeap());

    // 3. Iniciar tarea del LED ARGB en modo arcoíris continuo
    if (xTaskCreate(rainbow_task, "rainbow", 2048, NULL, 1, NULL) == pdPASS) {
        Serial.printf("[ARGB]  LED arcoiris iniciado en GPIO %d\n", RGB_LED_PIN);
    } else {
        Serial.println("[ARGB]  Error al crear la tarea del LED arcoiris.");
    }

    // 4. Montar sistema de archivos LittleFS sin formatear si falla
    if (!LittleFS.begin(false)) {
        Serial.println("[FATAL] No se pudo montar la particion LittleFS.");
        return;
    }
    Serial.printf("[FS]    Sistema de archivos web: %u / %u bytes\n",
                  (unsigned int)LittleFS.usedBytes(),
                  (unsigned int)LittleFS.totalBytes());

    // 5. Registrar eventos de red y DHCP
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
            Serial.printf("[WIFI]  Cliente conectado. MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
        } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
            Serial.printf("[WIFI]  Cliente desconectado. MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
                info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
                info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);
        } else if (event == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) {
            Serial.printf("[DHCP]  IP asignada: %s\n",
                IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
        }
    });

    // 6. Iniciar Punto de Acceso (SoftAP) y servidor DHCP en 192.168.4.1
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    IPAddress dhcpLeaseStart(AP_IP[0], AP_IP[1], AP_IP[2], 2);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET, dhcpLeaseStart, AP_IP);

    const char* pass = (strlen(WIFI_PASS) >= 8) ? WIFI_PASS : NULL;
    if (WiFi.softAP(WIFI_SSID, pass, WIFI_CHANNEL, 0, WIFI_MAX_CONN)) {
        Serial.printf("[WIFI]  Wi-Fi '%s': DHCP, gateway y DNS en %s (offline)\n",
                      WIFI_SSID, AP_IP.toString().c_str());
    } else {
        Serial.println("[FATAL] Error al iniciar el Punto de Acceso Wi-Fi.");
        return;
    }

    // 7. Iniciar tarea DNS selectiva en UDP 53
    if (xTaskCreate(dns_task, "dns", 4096, NULL, 5, NULL) != pdPASS) {
        Serial.println("[FATAL] No se pudo crear la tarea DNS.");
        return;
    }

    // 8. Iniciar servidores HTTP (:80) y HTTPS (:443) unificados
    Serial.printf("[MEM]   Heap libre antes de servidores: %u bytes\n", (unsigned int)ESP.getFreeHeap());
    start_servers();
    Serial.printf("[MEM]   Heap libre despues de servidores: %u bytes\n", (unsigned int)ESP.getFreeHeap());

    Serial.println("[HOST]  Listo. Conecta la PS5 con IP y DNS automaticos y abre la Guia del usuario.\n");
}

void loop() {
    uint8_t stationCount = WiFi.softAPgetStationNum();
    if (stationCount != lastStationCount) {
        lastStationCount = stationCount;
        Serial.printf("[WIFI]  Clientes conectados: %u\n", stationCount);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
}
