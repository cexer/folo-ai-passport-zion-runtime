idf_component_register(SRCS "../firmware/reader_redirect.c" REQUIRES esp_http_client lwip)
if(NOT ZION_TEST_ENDPOINT_DIR)
    message(FATAL_ERROR "Reader test requires the local fixture certificate directory")
endif()
target_include_directories(${COMPONENT_LIB} PRIVATE "${ZION_TEST_ENDPOINT_DIR}")
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=esp_http_client_init" "-Wl,--wrap=esp_sntp_init" "-Wl,--wrap=getaddrinfo")
