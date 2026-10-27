cmake_minimum_required(VERSION 3.16)
if(WIN32)
 set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON CACHE BOOL "Compiler response files" FORCE)
endif()
file(READ "${CMAKE_CURRENT_LIST_DIR}/zion-product.json" ZION_PRODUCT_JSON)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/zion-product.json")
string(JSON PROJECT_VER GET "${ZION_PRODUCT_JSON}" version)
string(JSON ZION_APP_PROJECT_NAME GET "${ZION_PRODUCT_JSON}" app_project_name)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(FoloToy-AI-Passport)
# Xiao Liu Ren links the LVGL QR widget, exposing a second global Nayuki copy
# in esp_emote_gfx. Keep LVGL's superset provider for both consumers; never use
# --allow-multiple-definition. Header ABI is checked at configure, and actual
# generator conformance is recorded by tools/verify-qrcode-provider.py.
if(CONFIG_LV_USE_QRCODE AND TARGET __idf_lvgl__lvgl AND TARGET __idf_espressif2022__esp_emote_gfx)
    idf_component_get_property(zion_lvgl_dir lvgl__lvgl COMPONENT_DIR)
    idf_component_get_property(zion_gfx_dir espressif2022__esp_emote_gfx COMPONENT_DIR)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${PROJECT_DIR}/tools/verify-qrcode-provider.py"
        "${zion_lvgl_dir}/src/libs/qrcode/qrcodegen.h"
        "${zion_gfx_dir}/src/lib/qrcode/qrcodegen.h")
    execute_process(COMMAND ${PYTHON} "${PROJECT_DIR}/tools/verify-qrcode-provider.py"
        --headers-only --lvgl-root "${zion_lvgl_dir}" --gfx-root "${zion_gfx_dir}"
        RESULT_VARIABLE zion_qr_abi_result)
    if(NOT zion_qr_abi_result EQUAL 0)
        message(FATAL_ERROR "QR provider headers are not ABI compatible")
    endif()
    idf_component_get_property(zion_gfx_lib espressif2022__esp_emote_gfx COMPONENT_LIB)
    get_target_property(zion_gfx_sources ${zion_gfx_lib} SOURCES)
    set(zion_qr_removed 0)
    foreach(zion_source ${zion_gfx_sources})
        if(zion_source MATCHES "[/\\\\]qrcodegen\\.c$")
            list(REMOVE_ITEM zion_gfx_sources "${zion_source}")
            math(EXPR zion_qr_removed "${zion_qr_removed}+1")
        endif()
    endforeach()
    if(NOT zion_qr_removed EQUAL 1)
        message(FATAL_ERROR "Expected exactly one duplicate graphics QR provider")
    endif()
    set_property(TARGET ${zion_gfx_lib} PROPERTY SOURCES "${zion_gfx_sources}")
endif()
if(ZION_RUNTIME_SIM OR ZION_RUNTIME_DEVICE_TEST)
    idf_build_set_property(COMPILE_DEFINITIONS "ZION_RUNTIME_TEST_KEYS=1" APPEND)
else()
    idf_build_set_property(COMPILE_DEFINITIONS "ZION_RUNTIME_TEST_KEYS=0" APPEND)
endif()
if(ZION_RUNTIME_SIM)
    idf_build_set_property(COMPILE_DEFINITIONS "ZION_RUNTIME_SIM=1" APPEND)
else()
    idf_build_set_property(COMPILE_DEFINITIONS "ZION_RUNTIME_SIM=0" APPEND)
endif()

# Keep portable artifact paths while branding ESP-IDF's NUL-terminated 32-byte
# descriptor. The exact 32-byte display name is stored separately in MNAM.
idf_component_get_property(ZION_APP_FORMAT_DIR esp_app_format COMPONENT_DIR)
get_source_file_property(ZION_DESC_DEFINITIONS
    "${ZION_APP_FORMAT_DIR}/esp_app_desc.c" DIRECTORY "${ZION_APP_FORMAT_DIR}" COMPILE_DEFINITIONS)
list(FILTER ZION_DESC_DEFINITIONS EXCLUDE REGEX "^[ \t]*PROJECT_NAME=")
file(WRITE "${CMAKE_BINARY_DIR}/zion_app_name.h"
    "#define ZION_APP_PROJECT_NAME_UTF8 \"${ZION_APP_PROJECT_NAME}\"\n")
list(APPEND ZION_DESC_DEFINITIONS "PROJECT_NAME=ZION_APP_PROJECT_NAME_UTF8")
set_source_files_properties("${ZION_APP_FORMAT_DIR}/esp_app_desc.c"
    DIRECTORY "${ZION_APP_FORMAT_DIR}" PROPERTIES COMPILE_DEFINITIONS "${ZION_DESC_DEFINITIONS}")
set_property(SOURCE "${ZION_APP_FORMAT_DIR}/esp_app_desc.c" DIRECTORY "${ZION_APP_FORMAT_DIR}"
    APPEND PROPERTY COMPILE_OPTIONS "-include" "${CMAKE_BINARY_DIR}/zion_app_name.h")
