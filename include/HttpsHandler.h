#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_http_server.h>
#include <esp_https_server.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "cert.h"

static httpd_handle_t http_server = NULL;
static httpd_handle_t https_server = NULL;

// Réplica exacta de content_type() en host.c de ps5-esp32host-main
static const char *content_type(const char *path)
{
    const char *extension = strrchr(path, '.');
    if (!extension) return "application/octet-stream";
    if (!strcmp(extension, ".html")) return "text/html; charset=utf-8";
    if (!strcmp(extension, ".js")) return "application/javascript";
    if (!strcmp(extension, ".css")) return "text/css";
    if (!strcmp(extension, ".json")) return "application/json";
    return "application/octet-stream";
}

// Réplica de serve_file() en host.c adaptada a LittleFS + soporte .gz para ESP32-S2 Mini
static esp_err_t serve_file(httpd_req_t *request)
{
    char uri[192];
    size_t length = strcspn(request->uri, "?#");
    if (length == 0 || length >= sizeof(uri)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid path");
    }
    memcpy(uri, request->uri, length);
    uri[length] = 0;

    httpd_resp_set_hdr(request, "Cache-Control", "no-store");

    // Redirección de rutas de la Guía de Usuario (/document y /document/*) a /
    if (!strcmp(uri, "/document") || !strncmp(uri, "/document/", 10)) {
        httpd_resp_set_status(request, "302 Found");
        httpd_resp_set_hdr(request, "Location", "/");
        return httpd_resp_send(request, NULL, 0);
    }

    // Endpoint de estado idéntico a host.c
    if (!strcmp(uri, "/status")) {
        httpd_resp_set_type(request, "application/json");
        char json[96];
        snprintf(json, sizeof(json), "{\"host\":\"PS5-Host\",\"ip\":\"%s\",\"dhcp\":true,\"offline\":true}", AP_IP.toString().c_str());
        return httpd_resp_sendstr(request, json);
    }

    // Validación de seguridad de ruta idéntica a host.c
    if (strstr(uri, "..") || strchr(uri, '%') || strchr(uri, '\\')) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid path");
    }

    const char *relative = !strcmp(uri, "/") ? "/index.html" : uri;
    char mapped_path[220];
    char gz_path[230];
    bool use_gz = false;
    File file;

    // 1. Probar ruta directa (.gz primero para compresión óptima, luego normal)
    snprintf(gz_path, sizeof(gz_path), "%s.gz", relative);
    file = LittleFS.open(gz_path, "r");
    if (file) {
        use_gz = true;
    } else {
        file = LittleFS.open(relative, "r");
    }

    // 2. Si no se encontró y no tiene prefijo conocido, resolver en /src/
    if (!file && strcmp(relative, "/index.html") && strncmp(relative, "/src/", 5) &&
        strncmp(relative, "/offsets/", 9) && strncmp(relative, "/payloads/", 10)) {
        snprintf(mapped_path, sizeof(mapped_path), "/src%s", relative);
        snprintf(gz_path, sizeof(gz_path), "%s.gz", mapped_path);
        file = LittleFS.open(gz_path, "r");
        if (file) {
            use_gz = true;
            relative = mapped_path;
        } else {
            file = LittleFS.open(mapped_path, "r");
            if (file) {
                relative = mapped_path;
            }
        }
    }

    if (!file) {
        Serial.printf("[404] %s no encontrado\n", uri);
        return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "File not found");
    }

    httpd_resp_set_type(request, content_type(relative));
    if (use_gz) {
        httpd_resp_set_hdr(request, "Content-Encoding", "gzip");
    }

    Serial.printf("[GET] %s%s\n", uri, use_gz ? " (gz)" : "");

    size_t buf_size = 8192;
    char *buffer = nullptr;
    if (psramFound()) {
        buffer = (char *)ps_malloc(buf_size);
    }
    if (!buffer) {
        buffer = (char *)malloc(buf_size);
    }
    if (!buffer) {
        buf_size = 2048;
        buffer = (char *)malloc(buf_size);
    }
    if (!buffer) {
        file.close();
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    esp_err_t result = ESP_OK;
    while (file.available()) {
        size_t got = file.read((uint8_t *)buffer, buf_size);
        if (got == 0) break;
        result = httpd_resp_send_chunk(request, buffer, got);
        if (result != ESP_OK) break;
    }

    free(buffer);
    file.close();

    if (result != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(request, NULL, 0);
}

// Réplica de start_servers() en host.c de ps5-esp32host-main
static bool start_servers(void)
{
    httpd_config_t plain = HTTPD_DEFAULT_CONFIG();
    plain.server_port = WEB_PORT;
    plain.ctrl_port = 32768;
    plain.max_open_sockets = 3;
    plain.lru_purge_enable = true;
    plain.uri_match_fn = httpd_uri_match_wildcard;
    plain.stack_size = 6144;
    plain.send_wait_timeout = 20;

    httpd_uri_t route = {
        .uri      = "/*",
        .method   = HTTP_GET,
        .handler  = serve_file,
        .user_ctx = NULL
    };

    esp_err_t ret_http = httpd_start(&http_server, &plain);
    if (ret_http != ESP_OK) {
        Serial.printf("[HTTP] Error al iniciar servidor HTTP: 0x%x\n", ret_http);
        return false;
    }
    esp_err_t reg_http = httpd_register_uri_handler(http_server, &route);
    if (reg_http != ESP_OK) {
        Serial.printf("[HTTP] Error al registrar ruta: 0x%x\n", reg_http);
        return false;
    }

    httpd_ssl_config_t secure = HTTPD_SSL_CONFIG_DEFAULT();
    secure.httpd.ctrl_port = 32769;
    secure.httpd.max_open_sockets = 4;
    secure.httpd.lru_purge_enable = true;
    secure.httpd.uri_match_fn = httpd_uri_match_wildcard;
    secure.httpd.stack_size = 8192;
    secure.httpd.send_wait_timeout = 20;
    secure.servercert = (const uint8_t *)server_cert_pem;
    secure.servercert_len = sizeof(server_cert_pem);
    secure.prvtkey_pem = (const uint8_t *)server_key_pem;
    secure.prvtkey_len = sizeof(server_key_pem);
    secure.port_secure = 443;

    esp_err_t ret_https = httpd_ssl_start(&https_server, &secure);
    if (ret_https != ESP_OK) {
        Serial.printf("[HTTPS] Error al iniciar servidor HTTPS: 0x%x\n", ret_https);
        return false;
    }
    esp_err_t reg_https = httpd_register_uri_handler(https_server, &route);
    if (reg_https != ESP_OK) {
        Serial.printf("[HTTPS] Error al registrar ruta: 0x%x\n", reg_https);
        return false;
    }

    Serial.println("[WEB] HTTP :80 y HTTPS :443 listos (ps5-esp32host-main)");
    return true;
}
