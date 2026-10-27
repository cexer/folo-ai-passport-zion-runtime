set(zion_test_bsp_dependency "")
if(ZION_STANDALONE_BSP_HEADER_DIR)
    set(zion_test_bsp_dependency bsp)
endif()
idf_component_register(SRCS "driver.c" "../../main/zion_test_capture.c"
    REQUIRES esp_driver_usb_serial_jtag esp_lcd espressif__button espressif__esp_lvgl_port lvgl__lvgl esp_timer espressif__esp_codec_dev esp_driver_i2s ${zion_test_bsp_dependency})
target_sources(${COMPONENT_LIB} PRIVATE "../firmware/audio_silence.c" "../firmware/button_edge_control.c" "../firmware/sleep_guard.c")
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,-u,__wrap_esp_deep_sleep_start" "-Wl,--wrap=esp_deep_sleep_start"
    "-Wl,-u,__wrap_i2s_channel_write"
    "-Wl,--wrap=esp_codec_dev_open" "-Wl,--wrap=esp_codec_dev_write"
    "-Wl,--wrap=esp_codec_dev_set_out_vol" "-Wl,--wrap=esp_codec_dev_set_out_mute"
    "-Wl,--wrap=i2s_channel_write")
target_compile_definitions(${COMPONENT_LIB} PRIVATE ZION_STANDALONE_TEST=1)
if(ZION_STANDALONE_LVGL_PROBE)
    target_sources(${COMPONENT_LIB} PRIVATE "../firmware/lvgl_allocation_probe.c")
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,-u,__wrap_lv_malloc_core" "-Wl,-u,__wrap_lv_realloc_core"
        "-Wl,--wrap=lv_malloc_core" "-Wl,--wrap=lv_realloc_core")
endif()
if(ZION_STANDALONE_EXTRA_SOURCES)
    target_sources(${COMPONENT_LIB} PRIVATE ${ZION_STANDALONE_EXTRA_SOURCES})
    target_include_directories(${COMPONENT_LIB} PRIVATE ${ZION_STANDALONE_EXTRA_INCLUDE_DIRS})
endif()
foreach(zion_wrap_symbol ${ZION_STANDALONE_LINK_WRAP})
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=${zion_wrap_symbol}")
endforeach()
if(ZION_STANDALONE_BSP_HEADER_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${ZION_STANDALONE_BSP_HEADER_DIR}")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ZION_STANDALONE_BSP=1)
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=bsp_button_init")
    file(READ "${ZION_STANDALONE_BSP_HEADER_DIR}/bsp_button.h" zion_button_header)
    if(zion_button_header MATCHES "[\r\n][ \t]*BSP_BTN_RELEASE[ \t]*[,=]")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE ZION_STANDALONE_KEY_EDGES=1)
    endif()
endif()

if(ZION_STANDALONE_ADC_POLL)
    target_sources(${COMPONENT_LIB} PRIVATE "../firmware/adc_key_control.c")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ZION_STANDALONE_ADC_POLL=1)
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=bsp_button_read_mv")
endif()
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=app_main"
    "-Wl,--wrap=iot_button_new_adc_device" "-Wl,--wrap=iot_button_register_cb"
    "-Wl,--wrap=esp_lcd_panel_draw_bitmap")

if(ZION_STANDALONE_USB)
    target_sources(${COMPONENT_LIB} PRIVATE "../firmware/usb_line_control.c")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ZION_STANDALONE_USB=1)
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=usb_serial_jtag_read_bytes")
endif()
if(ZION_STANDALONE_STDIO)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ZION_STANDALONE_STDIO=1)
    target_sources(${COMPONENT_LIB} PRIVATE "../firmware/native_stdio_control.c")
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=fgetc")
endif()
