idf_component_register(
    SRCS
        "udp_socket_server.c"
        "extern_server_fallback.c"
        "http_status.c"
    INCLUDE_DIRS "."
    REQUIRES
        dcf77
        lwip
        esp_netif
        esp_http_server
)
