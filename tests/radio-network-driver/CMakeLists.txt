idf_component_register(SRCS "../firmware/radio_redirect.c" REQUIRES esp_http_client)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_http_client_init")
