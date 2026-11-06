#pragma once
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "config.h"

static inline uint16_t ps5_dns_read16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

// Réplica exacta de dns.c en ps5-esp32host-main:
// Responde únicamente registros A para manuals.playstation.net y ps5.host con 192.168.4.1,
// devuelve NOERROR vacío para AAAA de esos dominios y NXDOMAIN para todo lo demás.
static inline size_t ps5_dns_reply(const uint8_t *q, size_t size, uint8_t *out,
                                   size_t capacity, const uint8_t ip[4])
{
    if (size < 17 || ps5_dns_read16(q + 4) != 1 || (q[2] & 0xf8) != 0) return 0;
    char name[254];
    size_t pos = 12, used = 0;
    while (pos < size && q[pos]) {
        unsigned label = q[pos++];
        // Rechazar compresión en preguntas DNS
        if (label > 63 || pos + label >= size || used + label + 1 >= sizeof(name)) return 0;
        if (used) name[used++] = '.';
        for (unsigned i = 0; i < label; i++) {
            uint8_t c = q[pos++];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
            name[used++] = (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
        }
    }
    if (pos >= size || pos + 5 > size) return 0;
    name[used] = 0;
    pos++;
    uint16_t type = ps5_dns_read16(q + pos), klass = ps5_dns_read16(q + pos + 2);
    pos += 4;
    if (klass != 1 || capacity < pos + 16) return 0;
    bool known = !strcmp(name, "manuals.playstation.net") || !strcmp(name, "ps5.host");
    bool answer = known && type == 1;
    memcpy(out, q, pos);
    out[2] = 0x84 | (q[2] & 1); // respuesta autoritativa, copiar RD
    out[3] = known ? 0 : 3;     // NXDOMAIN para otros hosts; sin internet aguas arriba
    memset(out + 6, 0, 6);      // contadores answer, authority, additional
    if (!answer) return pos;    // AAAA para un nombre conocido devuelve NOERROR sin datos
    out[7] = 1;
    const uint8_t record[] = {0xc0, 0x0c, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4};
    memcpy(out + pos, record, sizeof(record));
    memcpy(out + pos + sizeof(record), ip, 4);
    return pos + 16;
}

static void dns_task(void *unused)
{
    (void)unused;
    const uint8_t host_ip[4] = {AP_IP[0], AP_IP[1], AP_IP[2], AP_IP[3]};
    uint8_t query[512], reply[512];
    for (;;) {
        int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        struct sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_port = htons(DNS_PORT);
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        if (fd < 0 || bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
            Serial.printf("[DNS] Error en socket UDP %d: errno=%d\n", DNS_PORT, errno);
            if (fd >= 0) close(fd);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        Serial.printf("[DNS] Listo en UDP %d (manuals.playstation.net / ps5.host -> %d.%d.%d.%d)\n",
                      DNS_PORT, host_ip[0], host_ip[1], host_ip[2], host_ip[3]);
        for (;;) {
            struct sockaddr_in peer;
            socklen_t length = sizeof(peer);
            int got = recvfrom(fd, query, sizeof(query), 0, (struct sockaddr *)&peer, &length);
            if (got < 0) break;
            size_t count = ps5_dns_reply(query, (size_t)got, reply, sizeof(reply), host_ip);
            if (count) sendto(fd, reply, count, 0, (struct sockaddr *)&peer, length);
        }
        close(fd);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

