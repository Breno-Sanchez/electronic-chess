if("$ENV{XADREZ_MAP_MODE}" STREQUAL "1")
    idf_component_register(
        SRCS
            "led_map_mode.c"
        INCLUDE_DIRS "."
        PRIV_REQUIRES
            led_strip
            esp_driver_usb_serial_jtag
    )
else()
    idf_component_register(
        SRCS
            "main.c"
            "led.c"
            "sensor.c"
            "server.c"
            "Jogo.cpp"
            "Fen_PGN.cpp"
            "valida_jogada.cpp"
        INCLUDE_DIRS "."
        PRIV_REQUIRES
            led_strip
            esp_driver_gpio
            esp_http_server
            esp_wifi
            esp_event
            esp_netif
            nvs_flash
            wpa_supplicant
    )
endif()
