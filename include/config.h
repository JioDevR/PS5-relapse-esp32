#pragma once
#include <Arduino.h>
#include <IPAddress.h>

// ==========================================
// Configuración de Red Wi-Fi (SoftAP)
// ==========================================
#define WIFI_SSID       "relapse"
#define WIFI_PASS       ""              // Red abierta (o contraseña >= 8 caracteres para WPA2)
#define WIFI_CHANNEL    1
#define WIFI_MAX_CONN   4               // Máximo 4 clientes como en ps5-esp32host-main

// Configuración de Red y Servidor DHCP
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// ==========================================
// Configuración de Servidores
// ==========================================
#define DNS_PORT        53
#define WEB_PORT        80

// ==========================================
// Hardware - ESP32-S3 LED ARGB (WS2812)
// ==========================================
#ifndef RGB_LED_PIN
#ifdef RGB_BUILTIN
#define RGB_LED_PIN     RGB_BUILTIN
#else
#define RGB_LED_PIN     48              // GPIO 48 es el estándar en ESP32-S3 DevKit
#endif
#endif

#define ARGB_BRIGHTNESS 40              // Brillo del LED ARGB (0-255)
#define RGB_RAINBOW_MS  25              // Velocidad de rotación del arcoíris en ms
