# =====================================================================
# VeaOS KERNEL EXECUTABLE (veakrnl.elf)
# =====================================================================
file(GLOB_RECURSE KERNEL_ASM_SOURCES CONFIGURE_DEPENDS "*.asm")
file(GLOB_RECURSE KERNEL_C_SOURCES CONFIGURE_DEPENDS "*.c")

list(APPEND KERNEL_C_SOURCES ${RTL_SOURCES} ${AUTEXP_C_FILE})
list(APPEND KERNEL_ASM_SOURCES ${VZ_WRAPPERS_ASM_FILE})

set_source_files_properties(${AUTEXP_C_FILE} ${VZ_WRAPPERS_ASM_FILE} PROPERTIES GENERATED TRUE)

add_executable(veakrnl
    ${KERNEL_C_SOURCES}
    ${KERNEL_ASM_SOURCES}
)

add_dependencies(veakrnl veakrnl_codegen)
target_link_libraries(veakrnl PRIVATE hct)

set_target_properties(veakrnl PROPERTIES
    OUTPUT_NAME "veakrnl"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}"
    POSITION_INDEPENDENT_CODE OFF
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld"
)

target_compile_definitions(veakrnl PRIVATE BUILDING_VEAKRNL)

target_include_directories(veakrnl PRIVATE
    ${KERNEL_INCLUDE_DIR}
    ${SDK_INCLUDE_DIR}
    ${NDK_INCLUDE_DIR}
    ${DDK_INCLUDE_DIR}
    ${PSDK_INCLUDE_DIR}
)

target_compile_options(veakrnl PRIVATE
    $<$<COMPILE_LANGUAGE:C>:
        --target=i686-pc-none-elf
        -march=i686
        -m32
        -ffreestanding
        -nostdlib
        -fno-builtin
        -fno-builtin-memcpy
        -fno-pic
        -fno-pie
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
        -I${CMAKE_SOURCE_DIR}/
        -I${ASM_INCLUDE_DIR}/
    >
)

set(CMAKE_C_LINK_EXECUTABLE "ld.lld <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

target_link_options(veakrnl PRIVATE
    "SHELL:-T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld"
    "SHELL:-m elf_i386"
)