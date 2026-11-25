# =============================================================
# VeaOS BOOTLOADER / OS LOADER EXECUTABLE (OSI386)
# =============================================================

set(SDK_DIR "${CMAKE_SOURCE_DIR}/sdk")
set(SDK_GCC_INCLUDE_DIR "${SDK_DIR}/lib/gcc")
set(SDK_INCLUDE_DIR "${SDK_DIR}/include")

file(GLOB_RECURSE LOADER_ASM_SOURCES CONFIGURE_DEPENDS "*.asm")
file(GLOB_RECURSE LOADER_C_SOURCES CONFIGURE_DEPENDS "*.c")
list(APPEND LOADER_C_SOURCES ${RTL_SOURCES})

# Keluarkan boot.asm dari list umum supaya bisa ditaruh di urutan paling awal
list(FILTER LOADER_ASM_SOURCES EXCLUDE REGEX ".*/boot\\.asm$")

# Target build bernama 'osi386', dengan boot.asm wajib di posisi pertama
add_executable(osi386
    ${CMAKE_CURRENT_SOURCE_DIR}/boot.asm
    ${LOADER_ASM_SOURCES}
    ${LOADER_C_SOURCES}
)

# Output akhir persis 'OSI386' tanpa ekstensi di folder ${OUT_DIR}
set_target_properties(osi386 PROPERTIES
    OUTPUT_NAME "OSI386"
    SUFFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}"
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld"
)

target_compile_definitions(osi386 PRIVATE BUILDING_VEA_LOADER)

target_include_directories(osi386 PRIVATE
    ${LOADER_INCLUDE_DIR}
    ${SDK_GCC_INCLUDE_DIR}
    ${SDK_INCLUDE_DIR}
    ${SDK_INCLUDE_DIR}/ndk
    ${SDK_INCLUDE_DIR}/ddk
    ${SDK_INCLUDE_DIR}/psdk
    ${CMAKE_SOURCE_DIR}/veakrnl/include
)

target_compile_options(osi386 PRIVATE
    $<$<COMPILE_LANGUAGE:C>:
        --target=i686-pc-none-elf
        -march=i686
        -m32
        -ffreestanding
        -nostdlib
        -fno-builtin
        -fms-extensions
    >
    $<$<COMPILE_LANGUAGE:ASM_NASM>:
        -I${ASM_INCLUDE_DIR}/
    >
)

target_link_options(osi386 PRIVATE
    --target=i686-pc-none-elf
    -fuse-ld=lld
    -nostdlib
    -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    -Wl,-m,elf_i386
    -Wl,-Map=${OUT_DIR}/OSI386.map
)