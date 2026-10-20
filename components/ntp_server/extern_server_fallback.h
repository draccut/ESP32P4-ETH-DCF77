#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
    #endif

    /** NTP-Fallback-Server (Reihenfolge = Priorität) */
    static const char *const EXTERN_NTP_SERVERS[] = {
        "ptbtime1.ptb.de",
        "ptbtime2.ptb.de",
    };

    static const size_t EXTERN_NTP_SERVERS_COUNT = sizeof(EXTERN_NTP_SERVERS) / sizeof(EXTERN_NTP_SERVERS[0]);

    /**
     * Max. Zufallsdelay in ms (nur ab dem 2. Start)
     * Damit man nicht über das Intervall erkannt werden kann.
     */
    #ifndef EXTERN_NTP_DELAY_MAX_MS
    #define ONE_SEC_IN_MILLISECONDS 1000
    #define EXTERN_NTP_DELAY_MAX_MS (ONE_SEC_IN_MILLISECONDS * 60 * 10)
    #endif

    /**
     * @brief Initialisiert und startet den SNTP-Fallback (PTB-Server).
     *        Kann mehrfach aufgerufen werden – startet nur, wenn noch nicht aktiv.
     */
    void extern_server_fallback_init(void);

    /**
     * @brief Stoppt den SNTP-Client (falls er läuft).
     */
    void extern_server_fallback_stop(void);

    /**
     * @brief Gibt zurück, ob der SNTP-Client gerade aktiv ist.
     */
    bool extern_server_fallback_is_running(void);

    /**
     * @brief Startet eine Watchdog-Task, die automatisch zwischen
     *        DCF77 und den PTB-Servern umschaltet.
     *        - Hat DCF77 aktuelle Zeit → SNTP wird gestoppt
     *        - Kein aktuelles DCF77 → SNTP wird gestartet
     *
     * @param check_interval_ms  Prüfintervall in Millisekunden (z. B. 30000)
     */
    void extern_server_fallback_start_watchdog(uint32_t check_interval_ms);

    #ifdef __cplusplus
}
#endif
