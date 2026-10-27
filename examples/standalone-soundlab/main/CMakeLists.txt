idf_component_register(SRCS "main.c" "../../plugins/soundlab/soundlab.c" "../../plugins/soundlab/upstream/nabo_voice_fixed.c" "../../../assets/fonts/zion_font_ui_14.c" "../../../assets/fonts/zion_font_14.c"
 INCLUDE_DIRS "." "../../plugins/soundlab" "../../plugins/soundlab/upstream" REQUIRES bsp zion_sdk nvs_flash esp_timer esp_driver_usb_serial_jtag)
target_compile_options(${COMPONENT_LIB} PRIVATE -Werror -Wno-misleading-indentation)
