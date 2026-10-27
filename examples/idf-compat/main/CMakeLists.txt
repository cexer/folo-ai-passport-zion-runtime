idf_component_register(SRCS "main.c" "../../plugins/nescpu/upstream/nes6502.c"
    INCLUDE_DIRS "../../plugins/nescpu" "../../plugins/nescpu/upstream"
    REQUIRES bsp nvs_flash)
target_compile_options(${COMPONENT_LIB} PRIVATE -Werror -Wno-discarded-qualifiers)
