idf_component_register(SRCS "main.c" "../../plugins/counter.c" "../../../assets/fonts/zion_font_14.c"
    INCLUDE_DIRS "." REQUIRES bsp zion_sdk nvs_flash esp_timer esp_driver_usb_serial_jtag)
target_compile_options(${COMPONENT_LIB} PRIVATE -Werror)
