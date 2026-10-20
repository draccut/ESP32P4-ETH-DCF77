#include "extern_server_fallback.h"

#include "dcf77.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>

static const char *TAG = "ext_fallback";
static bool first_start_done = 0;

static bool sntp_running = false;

static void sntp_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Zeit erfolgreich von PTB-Server übernommen");
}

void extern_server_fallback_init(void)
{
    if (sntp_running) {
        return;
    }

    if (first_start_done) {
        uint32_t delay_ms = esp_random() % EXTERN_NTP_DELAY_MAX_MS;
        ESP_LOGI(TAG, "Zufälliger Start-Delay: %lu ms", (unsigned long)delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    } else {
        ESP_LOGI(TAG, "Erster Start – kein Delay");
        first_start_done = true;
    }

    ESP_LOGI(TAG, "Starte SNTP-Fallback");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    for (size_t i = 0; i < EXTERN_NTP_SERVERS_COUNT; i++) {
        esp_sntp_setservername((uint8_t)i, EXTERN_NTP_SERVERS[i]);
        ESP_LOGI(TAG, "NTP-Server[%u]: %s", (unsigned)i, EXTERN_NTP_SERVERS[i]);
    }

    esp_sntp_set_time_sync_notification_cb(sntp_sync_notification_cb);
    esp_sntp_init();

    sntp_running = true;
}

void extern_server_fallback_stop(void)
{
    if (!sntp_running) {
        return;
    }

    ESP_LOGI(TAG, "Stoppe SNTP-Fallback");
    esp_sntp_stop();
    sntp_running = false;
}

bool extern_server_fallback_is_running(void)
{
    return sntp_running;
}

static void watchdog_task(void *pvParameters)
{
    uint32_t interval_ms = (uint32_t)(uintptr_t)pvParameters;
    if (interval_ms < 5000) {
        interval_ms = 30000;   // Default 30 s
    }

    const TickType_t delay = pdMS_TO_TICKS(interval_ms);

    ESP_LOGI(TAG, "Watchdog gestartet (Intervall %lu ms)", (unsigned long)interval_ms);

    while (1) {
        if (dcf77_has_valid_time()) {
            if (sntp_running) {
                ESP_LOGI(TAG, "DCF77 aktuell -> SNTP wird gestoppt");
                extern_server_fallback_stop();
            }
        } else {
            if (!sntp_running) {
                ESP_LOGW(TAG, "Kein aktuelles DCF77 -> starte PTB-Fallback");
                extern_server_fallback_init();
            }
        }

        vTaskDelay(delay);
    }
}

void extern_server_fallback_start_watchdog(uint32_t check_interval_ms)
{
    xTaskCreate(watchdog_task,
                "sntp_watchdog",
                3072,
                (void *)(uintptr_t)check_interval_ms,
                4,
                NULL);
}
