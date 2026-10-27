if(CONFIG_ZION_592_PROBE OR CONFIG_ZION_LAUNCHER)
    idf_component_register(SRCS "zion_service.c" "zion592_wire.c" "zion_ui.c" "zion_install_view.c"
        "ui_pixel_math.c" "../assets/fonts/zion_font_14.c"
        INCLUDE_DIRS "."
        REQUIRES esp_mm zion_sdk xiaozhi bsp bt esp_hid esp_driver_usb_serial_jtag esp_event esp_netif esp_wifi esp_coex nvs_flash esp_timer
                 esp_http_client esp_http_server app_update esp_partition spi_flash mbedtls lwip fatfs spiffs esp_ringbuf espressif__mqtt espressif__esp_audio_codec espressif__esp_websocket_client)
    if(CONFIG_ZION_LAUNCHER)
        set(compat_dir "${CMAKE_CURRENT_BINARY_DIR}/idf-compat")
        file(GLOB_RECURSE compat_headers CONFIGURE_DEPENDS
            "${PROJECT_DIR}/components/bsp/include/*.h"
            "${PROJECT_DIR}/components/bsp/src/*.h"
            "${PROJECT_DIR}/components/bsp/src/*.c"
            "${PROJECT_DIR}/managed_components/espressif__esp_websocket_client/include/*.h"
            "${PROJECT_DIR}/managed_components/lvgl__lvgl/*.h"
            "$ENV{IDF_PATH}/components/freertos/*.h"
            "$ENV{IDF_PATH}/components/esp_common/include/*.h"
            "$ENV{IDF_PATH}/components/nvs_flash/include/*.h"
            "$ENV{IDF_PATH}/components/esp_timer/include/*.h"
            "$ENV{IDF_PATH}/components/log/include/*.h"
            "$ENV{IDF_PATH}/components/mbedtls/*.h"
            "$ENV{IDF_PATH}/components/esp_system/include/*.h")
        file(GLOB_RECURSE compat_wifi_headers CONFIGURE_DEPENDS
            "$ENV{IDF_PATH}/components/esp_wifi/include/*.h"
            "$ENV{IDF_PATH}/components/wpa_supplicant/esp_supplicant/include/*.h")
        list(APPEND compat_headers ${compat_wifi_headers})
        add_custom_command(OUTPUT "${compat_dir}/zion_idf_contract.h" "${compat_dir}/zion-idf-compat.json"
            COMMAND ${PYTHON} "${PROJECT_DIR}/tools/idf-compat-contract.py" --build "${CMAKE_BINARY_DIR}" --output "${compat_dir}"
            DEPENDS "${PROJECT_DIR}/tools/idf-compat-contract.py" "${PROJECT_DIR}/main/zion_idf_exports.inc" "${PROJECT_DIR}/main/zion_idf_auto_imports.inc"
                "${PROJECT_DIR}/main/zion_idf_dns.c" "${PROJECT_DIR}/main/zion_idf_dns.h"
                "${PROJECT_DIR}/main/zion_idf_ota.c" "${PROJECT_DIR}/main/zion_idf_ota.h"
                "${PROJECT_DIR}/main/zion_button_events.h"
                "${PROJECT_DIR}/main/zion_idf_link_hooks.c" "${PROJECT_DIR}/main/zion_idf_link_hooks.h"
                "${PROJECT_DIR}/main/zion_idf_compat.c" "${PROJECT_DIR}/main/zion_idf_board.c" "${PROJECT_DIR}/main/zion_idf_storage.c" "${PROJECT_DIR}/main/zion_elf.c" "${PROJECT_DIR}/main/zion_elf.h" "${PROJECT_DIR}/main/zion_elf_host.c" "${PROJECT_DIR}/main/zion_package.h" "${CMAKE_BINARY_DIR}/config/sdkconfig.h"
                "${CMAKE_BINARY_DIR}/toolchain/cflags" "${CMAKE_BINARY_DIR}/specs/picolibc.specs" ${compat_headers}
            VERBATIM)
        target_sources(${COMPONENT_LIB} PRIVATE "zion_idf_compat.c" "zion_idf_board.c" "zion_idf_storage.c" "zion_idf_dns.c" "zion_idf_ota.c" "zion_idf_link_hooks.c" "${compat_dir}/zion_idf_contract.h")
        target_include_directories(${COMPONENT_LIB} PRIVATE "${compat_dir}")
        target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_partition_find_first")
        target_link_options(${COMPONENT_LIB} INTERFACE
            "-Wl,--wrap=esp_mbedtls_mem_calloc" "-Wl,--wrap=esp_mbedtls_mem_free"
            "-Wl,--wrap=opus_encoder_create" "-Wl,--wrap=opus_encoder_destroy")
        if(ZION_RUNTIME_DEVICE_TEST OR ZION_RUNTIME_SIM)
            if(ZION_RUNTIME_DEVICE_TEST)
                target_sources(${COMPONENT_LIB} PRIVATE "../tests/firmware/audio_silence.c" "../tests/firmware/sleep_guard.c")
                target_link_options(${COMPONENT_LIB} INTERFACE
                    "-Wl,-u,__wrap_esp_deep_sleep_start" "-Wl,--wrap=esp_deep_sleep_start"
                    "-Wl,--wrap=esp_codec_dev_open" "-Wl,--wrap=esp_codec_dev_write"
                    "-Wl,--wrap=esp_codec_dev_set_out_vol" "-Wl,--wrap=esp_codec_dev_set_out_mute"
                    "-Wl,--wrap=i2s_channel_write")
            endif()
            target_sources(${COMPONENT_LIB} PRIVATE "zion_test_capture.c" "zion_test_runtime.c" "../tests/firmware/usb_line_control.c" "../tests/firmware/stdio_control.c" "../tests/firmware/adc_key_control.c" "../tests/firmware/button_edge_control.c")
            target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_lcd_panel_draw_bitmap")
            if(ZION_TEST_ENDPOINT_DIR AND NOT ZION_TEST_CW)
                target_sources(${COMPONENT_LIB} PRIVATE "../tests/firmware/mqtt_redirect.c")
                target_include_directories(${COMPONENT_LIB} PRIVATE "${ZION_TEST_ENDPOINT_DIR}")
                target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_mqtt_client_init")
            endif()
            if(ZION_TEST_READER)
                if(NOT ZION_TEST_ENDPOINT_DIR)
                    message(FATAL_ERROR "Reader test requires the local fixture certificate directory")
                endif()
                target_sources(${COMPONENT_LIB} PRIVATE "../tests/firmware/reader_redirect.c")
                target_include_directories(${COMPONENT_LIB} PRIVATE "${ZION_TEST_ENDPOINT_DIR}")
                target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_http_client_init"
                    "-Wl,--wrap=esp_sntp_init" "-Wl,--wrap=getaddrinfo")
            elseif(ZION_TEST_RADIO)
                target_sources(${COMPONENT_LIB} PRIVATE "../tests/firmware/radio_redirect.c")
                target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_http_client_init")
            endif()
        endif()
        target_sources(${COMPONENT_LIB} PRIVATE "zion_package.c" "zion_package_format.c" "zion_elf.c" "zion_elf_host.c" "zion_runtime.c" "zion_runtime_model.c" "zion_runtime_view.cc" "zion_launcher.c" "zion_menu.c" "zion_manage.c" "zion_prefs.c" "zion_policy.c" "zion_wifi.c" "zion_form.c" "../assets/images/folo_mini_code_208.c")
        target_sources(${COMPONENT_LIB} PRIVATE "zion_portal.c" "zion_portal_net.c")

        if(CONFIG_META_E2E_TEST_CONTROL)
            target_sources(${COMPONENT_LIB} PRIVATE "../tests/firmware/zion_font_selftest.c")
        endif()
        target_add_binary_data(${COMPONENT_LIB} "../assets/images/folo-mini-code-208.png" BINARY RENAME_TO zion_mini_png)
    endif()
    if(CONFIG_ZION_592_INSTALL)
        target_sources(${COMPONENT_LIB} PRIVATE "zion592_install.c" "zion592_image.c"
            "zion_title.c" "meta_store_json.c"
            "meta_carve.c" "meta_carve_store.c" "meta_carve_boot.c" "meta_carve_flash.c"
            "meta_md5.c" "meta_name.c")
    endif()
    if(ZION_RUNTIME_SIM)
        target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-function -Wno-unused-variable)
    endif()
    target_compile_options(${COMPONENT_LIB} PRIVATE -Werror)
    return()
endif()

idf_component_register(
    SRCS "main.c"
         "zion_policy.c"
         "zion_prefs.c"
         "zion_ui.c"
         "zion_text.c"
         "../assets/fonts/zion_font_14.c"
         "ui_pixel_math.c"
         "meta_image.c"
         "meta_name.c"
         "meta_slots.c"
         "meta_seq.c"
         "meta_store.c"
         "meta_sign.c"
         "meta_store_json.c"
         "meta_store_prov.c"
         "meta_store_net.c"
         "meta_store_install.c"
         "meta_install_model.c"
         # dynslot 纯逻辑(分配器/表物化/记录/裁决;bootloader hook 与 host
         # 测试链接同一份源,见 bootloader_components/meta_boot_hooks/CMakeLists.txt)
         "meta_carve.c"
         "meta_carve_store.c"
         "meta_carve_boot.c"
         # 启动器侧胶水(ensure/提交/物化/凭据搬移;裸 esp_flash_* 不依赖分区
         # 表缓存,app_main 第一动作就是 ensure,见 meta_carve_flash.h)
         "meta_carve_flash.c"
         "meta_backup.c"
         # 分区表 MD5 marker(meta_pt_encode/decode;host 测试与 bootloader 各
         # 链接同一份,见 bootloader_components/meta_boot_hooks/CMakeLists.txt)
         "meta_md5.c"
    INCLUDE_DIRS "."
    # Research declares bt during early dependency expansion so the optional
    # probe receives NimBLE headers. The normal launcher still disables BLE.
    # 保留 esp_http_server(配网页 +
    # 本地安装服务)、app_update/esp_partition(槽位与回滚)、mbedtls(install.c
    # 引 mbedtls/sha256.h,配网页也用其 RNG);WAN 下载通道已删,不再需要
    # esp_http_client / esp-tls / esp_crt_bundle(证书包随通道一并移除)。
    REQUIRES esp_mm fatfs espressif__mqtt espressif__esp_audio_codec zion_sdk xiaozhi bsp bt esp_hid esp_driver_usb_serial_jtag esp_coex esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
             esp_http_server esp_http_client app_update esp_partition esp_app_format mbedtls
             lwip      # install httpd 的 socket 层(自 v5.1 起需显式声明)
             spi_flash # 裸 flash 凭证备份(esp_flash_read/write/erase)
)

# BUG-14 教训:UB 警告(越界写)在构建日志里躺了三个版本没人看 —— 本组件
# 警告一律升级为错误,固件构建不再可能带着未处置的警告出门。
if(CONFIG_META_E2E_TEST_CONTROL)
    target_sources(${COMPONENT_LIB} PRIVATE "meta_e2e_control.c")
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE -Werror)
