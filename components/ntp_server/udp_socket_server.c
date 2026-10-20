#include <string.h>
#include <sys/param.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/time.h>
#include <time.h>

#include "lwip/netdb.h"
#include "lwip/sockets.h"

static const char *TAG = "udp_server";
// NTP port and packet buffer
#define NTP_PORT 123
#define NTP_PACKET_SIZE 48
static bool overflow;
long offset = 0;
const long oneSecond_inMicroseconds_L = 1000000;  // one second in microseconds

struct tm getTimeStruct() {
    struct tm timeinfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);
    time_t tt = mktime(&timeinfo);

    if (overflow) {
        tt += 63071999;
    }
    if (offset > 0) {
        tt += (unsigned long)offset;
    } else {
        tt -= (unsigned long)(offset * -1);
    }
    struct tm *tn = localtime(&tt);
    if (overflow) {
        tn->tm_year += 64;
    }
    return *tn;
}

unsigned long getEpoch() {
    struct tm timeinfo = getTimeStruct();
    return mktime(&timeinfo);
}
unsigned long getMicros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec;
}

uint64_t getCurrentTimeInNTP64BitFormat(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t seconds = (uint64_t)tv.tv_sec + 2208988800ULL;
    uint64_t fraction = ((uint64_t)tv.tv_usec * 4294ULL) + ((uint64_t)tv.tv_usec * 967296ULL) / 1000000ULL;
    return (seconds << 32) | (fraction & 0xFFFFFFFFULL);
}

/**
 * Client-Timestamp wird sofort nach dem Empfang gerettet memset für sauberes Paket
 * Korrekter Header: 0x24
 * Bessere Precision
 * Kein Überschreiben mehr vor dem Kopieren
 */
void udp_server_task(void *pvParameters) {
    char ntp_packet[NTP_PACKET_SIZE];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(NTP_PORT);

    // print time

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        return;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", NTP_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);

        int len = recvfrom(sock, ntp_packet, NTP_PACKET_SIZE, 0,
                           (struct sockaddr *)&source_addr, &socklen);

        ESP_LOGI(TAG, "received udp request");

        if (len < 48) {
            ESP_LOGE(TAG, "Paket zu kurz oder Fehler: %d", len);
            continue;
        }

        // === 1. Client Transmit Timestamp SOFORT sichern (Bytes 40-47) ===
        uint8_t client_transmit_ts[8];
        memcpy(client_transmit_ts, &ntp_packet[40], 8);

        // === 2. Receive-Zeit nehmen ===
        uint64_t receiveTime = getCurrentTimeInNTP64BitFormat();

        // === 3. Paket neu aufbauen ===
        memset(ntp_packet, 0, NTP_PACKET_SIZE);

        // LI=0, VN=4, Mode=4 (Server)
        ntp_packet[0] = 0x24;

        // Stratum 1
        ntp_packet[1] = 1;

        // Poll Interval
        ntp_packet[2] = 4;

        // Precision (ca. 1 µs)
        ntp_packet[3] = 0xEC;

        // Root Delay + Root Dispersion auf 0 lassen (schon durch memset)

        // Reference ID "DCF\0"
        ntp_packet[12] = 'D';
        ntp_packet[13] = 'C';
        ntp_packet[14] = 'F';
        ntp_packet[15] = 0;

        // Reference Timestamp (Bytes 16-23) = Receive Time
        ntp_packet[16] = (receiveTime >> 56) & 0xFF;
        ntp_packet[17] = (receiveTime >> 48) & 0xFF;
        ntp_packet[18] = (receiveTime >> 40) & 0xFF;
        ntp_packet[19] = (receiveTime >> 32) & 0xFF;
        ntp_packet[20] = (receiveTime >> 24) & 0xFF;
        ntp_packet[21] = (receiveTime >> 16) & 0xFF;
        ntp_packet[22] = (receiveTime >>  8) & 0xFF;
        ntp_packet[23] = (receiveTime      ) & 0xFF;

        // Originate Timestamp (Bytes 24-31) = Client Transmit Timestamp (exakt!)
        memcpy(&ntp_packet[24], client_transmit_ts, 8);

        // Receive Timestamp (Bytes 32-39)
        ntp_packet[32] = (receiveTime >> 56) & 0xFF;
        ntp_packet[33] = (receiveTime >> 48) & 0xFF;
        ntp_packet[34] = (receiveTime >> 40) & 0xFF;
        ntp_packet[35] = (receiveTime >> 32) & 0xFF;
        ntp_packet[36] = (receiveTime >> 24) & 0xFF;
        ntp_packet[37] = (receiveTime >> 16) & 0xFF;
        ntp_packet[38] = (receiveTime >>  8) & 0xFF;
        ntp_packet[39] = (receiveTime      ) & 0xFF;

        // Transmit Timestamp (Bytes 40-47)
        uint64_t transmitTime = getCurrentTimeInNTP64BitFormat();
        ntp_packet[40] = (transmitTime >> 56) & 0xFF;
        ntp_packet[41] = (transmitTime >> 48) & 0xFF;
        ntp_packet[42] = (transmitTime >> 40) & 0xFF;
        ntp_packet[43] = (transmitTime >> 32) & 0xFF;
        ntp_packet[44] = (transmitTime >> 24) & 0xFF;
        ntp_packet[45] = (transmitTime >> 16) & 0xFF;
        ntp_packet[46] = (transmitTime >>  8) & 0xFF;
        ntp_packet[47] = (transmitTime      ) & 0xFF;

        // Absenden
        sendto(sock, ntp_packet, NTP_PACKET_SIZE, 0,
               (struct sockaddr *)&source_addr, sizeof(source_addr));
    }

    if (sock != -1) {
        close(sock);
    }
    vTaskDelete(NULL);
}
