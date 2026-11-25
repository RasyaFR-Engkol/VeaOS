# =============================================================
# VeaOS KERNEL EXECUTABLE (veakrnl.elf)
# =============================================================

file(GLOB_RECURSE KERNEL_ASM_SOURCES CONFIGURE_DEPENDS "*.asm")
file(GLOB_RECURSE KERNEL_C_SOURCES CONFIGURE_DEPENDS "*.c")
file(GLOB_RECURSE RTL_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/sdk/lib/rtl/*.c")

list(APPEND KERNEL_C_SOURCES ${RTL_SOURCES})

if(EXISTS ${AUTEXP_C_FILE})
    list(APPEND KERNEL_C_SOURCES ${AUTEXP_C_FILE})
endif()

if(EXISTS ${VZ_WRAPPERS_ASM_FILE})
    list(APPEND KERNEL_ASM_SOURCES ${VZ_WRAPPERS_ASM_FILE})
endif()

# Langsung jadi native EXECUTABLE target (otomatis masuk target 'all')
add_executable(veakrnl
    ${KERNEL_C_SOURCES}
    ${KERNEL_ASM_SOURCES}
    ${HCT_OBJS}
)

# Pastikan kalau HCT dibuild lewat custom target, veakrnl nunggu HCT kelar dulu
if(TARGET build_hct)
    add_dependencies(veakrnl build_hct)
endif()

# Output tetap ke ${OUT_DIR}/veakrnl.elf
set_target_properties(veakrnl PROPERTIES
    OUTPUT_NAME "veakrnl"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}"
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld"
)

target_compile_definitions(veakrnl PRIVATE BUILDING_VEAKRNL)

target_include_directories(veakrnl PRIVATE
    ${KERNEL_INCLUDE_DIR}
    ${SDK_INCLUDE_DIR}
    ${VEA_INCLUDES}
)

# Flags khusus C compiler (Clang i686)
target_compile_options(veakrnl PRIVATE
    $<$<COMPILE_LANGUAGE:C>:
        --target=i686-pc-none-elf
        -march=i686
        -m32
        -ffreestanding
        -nostdlib
        -fno-builtin
        -fno-builtin-memcpy
        -g
        -std=gnu23
        -fshort-wchar
        -fms-extensions
        -fstack-protector-all
        -Wthread-safety
        -Wthread-safety-analysis
        -Werror=thread-safety
        -fmacro-prefix-map=/home/xvrasya/VeaOS/=
        -fno-omit-frame-pointer
        -ferror-limit=0
    >
    $<$<COMPILE_LANGUAGE:ASM_NASM>:
        -f elf32
        -I${CMAKE_SOURCE_DIR}/
        -I${ASM_INCLUDE_DIR}/
    >
)

# Linking lewat Clang driver ke ld.lld dengan linker.ld
target_link_options(veakrnl PRIVATE
    --target=i686-pc-none-elf
    -fuse-ld=lld
    -nostdlib
    -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    -Wl,-m,elf_i386
)