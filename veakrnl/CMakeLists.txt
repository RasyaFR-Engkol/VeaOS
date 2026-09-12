add_compile_definitions(BUILDING_VEAKRNL)

# Path ke Root SDK Include
set(SDK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/sdk/include")
set(KERNEL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# =====================================================================
# DDK HEADER GENERATOR (veaddk.h from export.lst)
# =====================================================================
set(EXPORT_LST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/export.lst")
set(DDK_INCLUDE_DIR "${SDK_INCLUDE_DIR}/ddk")
set(DDK_HEADER_FILE "${DDK_INCLUDE_DIR}/veaddk.h")
set(GEN_DDK_SCRIPT "${CMAKE_SOURCE_DIR}/tool/gen_ddk.py")

if(EXISTS ${EXPORT_LST_FILE})
    execute_process(
        COMMAND python3 ${GEN_DDK_SCRIPT} 
                ${EXPORT_LST_FILE} 
                ${KERNEL_INCLUDE_DIR} 
                ${DDK_INCLUDE_DIR}
        RESULT_VARIABLE GEN_DDK_RESULT
    )
    if(NOT GEN_DDK_RESULT EQUAL 0)
        message(WARNING "[VeaDDK] Failed to generate veaddk.h!")
    endif()
else()
    message(WARNING "[VeaDDK] export.lst not found in ${CMAKE_CURRENT_SOURCE_DIR}!")
endif()

# =============================================================
# VeaOS COMPILE
# =============================================================

# File perantara .o masuk ke build/veakrnl/
set(OBJ_DIR ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB_RECURSE KERNEL_ASM_SOURCES "*.asm")
file(GLOB_RECURSE KERNEL_C_SOURCES "*.c")

file(GLOB_RECURSE RTL_SOURCES "${CMAKE_SOURCE_DIR}/sdk/lib/rtl/*.c")
list(APPEND KERNEL_C_SOURCES ${RTL_SOURCES})

set(KERNEL_OBJS "")

foreach(ASM_FILE ${KERNEL_ASM_SOURCES})
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${ASM_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${OBJ_DIR}/krnl_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        # Tambahkan -I${CMAKE_SOURCE_DIR}/ di sini
        COMMAND nasm -f elf32 ${ASM_FILE} -I${CMAKE_SOURCE_DIR}/ -I${ASM_INCLUDE_DIR}/ -o ${OBJ_FILE}
        DEPENDS ${ASM_FILE}
        COMMENT "Compiling Kernel ASM: ${SAFE_NAME}"
    )
    list(APPEND KERNEL_OBJS ${OBJ_FILE})
endforeach()

foreach(C_FILE ${KERNEL_C_SOURCES})
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${C_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${OBJ_DIR}/krnl_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        # PASTIKAN -DBUILDING_VEAKRNL ADA DI SINI
        COMMAND clang --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin -DBUILDING_VEAKRNL ${VEA_INCLUDES} -c ${C_FILE} -o ${OBJ_FILE} -std=gnu23 -fshort-wchar -fms-extensions
        DEPENDS ${C_FILE}
        COMMENT "Compiling C: ${SAFE_NAME}"
    )
    
    list(APPEND KERNEL_OBJS ${OBJ_FILE})
endforeach()

# Target Clangd
add_library(clangd_kernel_dummy OBJECT EXCLUDE_FROM_ALL ${KERNEL_C_SOURCES} ${HCT_C_SOURCES})
target_compile_options(clangd_kernel_dummy PRIVATE 
    --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin -ggdb -std=gnu23 -fshort-wchar -fms-extensions
)
target_include_directories(clangd_kernel_dummy PRIVATE 
    ${KERNEL_INCLUDE_DIR}
    ${SDK_INCLUDE_DIR}
)

# LINKING: Sekarang OUTPUT dan COMMAND sama-sama menunjuk ke OUT_DIR
add_custom_command(
    OUTPUT ${OUT_DIR}/veakrnl.elf # <--- DIBETULKAN (Pakai OUT_DIR)
    COMMAND ld.lld -T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -m elf_i386 ${KERNEL_OBJS} ${HCT_OBJS} -o ${OUT_DIR}/veakrnl.elf
    DEPENDS ${KERNEL_OBJS} ${HCT_OBJS} ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    COMMENT "Linking veakrnl.elf (with HCT included) using Clang/LLD"
)

add_custom_target(build_kernel DEPENDS ${OUT_DIR}/veakrnl.elf)