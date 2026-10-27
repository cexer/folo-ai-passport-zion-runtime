idf_component_register(SRCS "main.c" "../../plugins/nescpu/nescpu.c" "../../plugins/nescpu/upstream/nes6502.c"
 "../../../assets/fonts/zion_font_ui_14.c" "../../../assets/fonts/zion_font_14.c"
 INCLUDE_DIRS "." "../../plugins/nescpu" "../../plugins/nescpu/upstream" REQUIRES bsp zion_sdk nvs_flash esp_timer esp_driver_usb_serial_jtag)
target_compile_options(${COMPONENT_LIB} PRIVATE -Werror -Wno-misleading-indentation)
set_source_files_properties("../../plugins/nescpu/upstream/nes6502.c" PROPERTIES COMPILE_OPTIONS "-Wno-discarded-qualifiers;-O2")
