# =============================================================
# VeaOS HARDWARE CONTROL LAYER (HCT x86) OBJECT LIBRARY
# =============================================================

file(GLOB_RECURSE HCT_ASM_SOURCES CONFIGURE_DEPENDS "*.asm")
file(GLOB_RECURSE HCT_C_SOURCES CONFIGURE_DEPENDS "*.c")

add_library(hct OBJECT
    ${HCT_ASM_SOURCES}
    ${HCT_C_SOURCES}
)

target_include_directories(hct PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/veakrnl/include
    ${CMAKE_SOURCE_DIR}/sdk/include
    ${CMAKE_SOURCE_DIR}/sdk/include/ndk
    ${CMAKE_SOURCE_DIR}/sdk/include/ddk
    ${CMAKE_SOURCE_DIR}/sdk/include/psdk
)

target_compile_options(hct PRIVATE
    $<$<COMPILE_LANGUAGE:C>:
        --target=i686-pc-none-elf
        -march=i686
        -m32
        -ffreestanding
        -nostdlib
        -fno-builtin
        -fno-pic
        -fno-pie
        -g
        -fms-extensions
    >
    $<$<COMPILE_LANGUAGE:ASM_NASM>:
        -I${ASM_INCLUDE_DIR}/
    >
)