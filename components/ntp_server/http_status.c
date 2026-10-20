#include "http_status.h"

#include "dcf77.h"
#include "extern_server_fallback.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "http_status";

const char *html_part1 =
"<!DOCTYPE html>"
"<html><head>"
"<meta charset='UTF-8'>"
"<title>ESP32 Zeitstatus</title>"
"<style>"
"body{font-family:sans-serif;text-align:center;margin-top:3em;background:#111;color:#eee;}"
"h1{font-size:2.5em;margin-bottom:0.2em;}"
".source{font-size:1.3em;color:#6cf;margin-top:1em;}"
".info{color:#888;margin-top:2em;font-size:0.9em;}"
"</style>"
"</head><body>"
"<h1 id='time'>--:--:--</h1>"
"<div class='source'>Quelle: <span id='source'>-</span></div>"
"<div class='info'>NTP-Server: ";

const char *html_part2 =
"</div>"
"<script>"
"async function update(){"
"  try{"
"    const r=await fetch('/api/time',{cache:'no-store'});"
"    const j=await r.json();"
"    document.getElementById('time').textContent=j.time;"
"    document.getElementById('source').textContent=j.source;"
"  }catch(e){}"
"}"
"update();"
"setInterval(update,100);"
"</script>"
"</body></html>";

/* Prototypen */
void generate_html(char *dest, size_t max_len);
size_t calculate_html_length(void);

static void fill_time_and_source(char *timestr, size_t timestr_len, char *source, size_t source_len)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(timestr, timestr_len, "%Y-%m-%d %H:%M:%S", &timeinfo);

    if (dcf77_has_valid_time()) {
        snprintf(source, source_len, "DCF77");
    } else if (extern_server_fallback_is_running()) {
        snprintf(source, source_len, "PTB (SNTP Fallback)");
    } else {
        snprintf(source, source_len, "Unbekannt / noch nicht synchron");
    }
}

/* JSON-API: /api/time */
static esp_err_t api_time_get_handler(httpd_req_t *req)
{
    char timestr[64];
    char source[40];
    char json[160];

    fill_time_and_source(timestr, sizeof(timestr), source, sizeof(source));

    snprintf(json, sizeof(json),
             "{\"time\":\"%s\",\"source\":\"%s\"}",
             timestr, source);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* HTML-Seite: / */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    size_t len_html = calculate_html_length();
    char *html = malloc(len_html);
    if (html == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    generate_html(html, len_html);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    free(html);
    return ESP_OK;
}

void http_status_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 6;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP-Server konnte nicht gestartet werden");
        return;
    }

    httpd_uri_t root = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = root_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root);

    httpd_uri_t api = {
        .uri      = "/api/time",
        .method   = HTTP_GET,
        .handler  = api_time_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api);

    ESP_LOGI(TAG, "HTTP-Statusseite gestartet auf Port 80");
}

size_t calculate_html_length(void)
{
    size_t total = strlen(html_part1) + strlen(html_part2) + 1; // +1 für '\0'

    total += strlen("<ul>\n") + strlen("</ul>\n");

    const size_t li_overhead = strlen("  <li></li>\n");
    for (size_t i = 0; i < EXTERN_NTP_SERVERS_COUNT; i++) {
        total += li_overhead + strlen(EXTERN_NTP_SERVERS[i]);
    }

    return total;
}

void generate_html(char *dest, size_t max_len)
{
    if (!dest || max_len == 0) {
        return;
    }

    int written = snprintf(dest, max_len, "%s<ul>\n", html_part1);
    if (written < 0 || (size_t)written >= max_len) {
        return;
    }

    size_t offset = (size_t)written;

    for (size_t i = 0; i < EXTERN_NTP_SERVERS_COUNT; i++) {
        written = snprintf(dest + offset, max_len - offset,
                           "  <li>%s</li>\n", EXTERN_NTP_SERVERS[i]);
        if (written < 0 || (size_t)written >= max_len - offset) {
            return;
        }
        offset += (size_t)written;
    }

    snprintf(dest + offset, max_len - offset, "</ul>\n%s", html_part2);
}
