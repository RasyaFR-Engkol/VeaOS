cmake_minimum_required(VERSION 3.16)

# =====================================================================
# CLANGD & BARE-METAL SETUP (DITARUH SEBELUM PROJECT!)
# =====================================================================
# 1. Paksa CMake biar nggak usah nge-test Linker C (karena kita OS dev / nostdlib)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 2. Paksa pakai clang
set(CMAKE_C_COMPILER "clang")

# 3. Aktifkan fitur export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 4. Aktifkan bahasa C (sebelumnya NONE, sekarang wajib C buat dummy target)
project(VeaOS LANGUAGES C)

# =====================================================================
# DIRECTORY SETUP
# =====================================================================
set(TOOL_DIR ${CMAKE_SOURCE_DIR}/tool)
set(STAGE0_DIR ${CMAKE_SOURCE_DIR}/entrystage0)
set(STAGE1_DIR ${CMAKE_SOURCE_DIR}/entryfat32)
set(LOADER_DIR ${CMAKE_SOURCE_DIR}/osi386)
set(LOADER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/sdk/include/ldr)
set(BIN_DIR ${CMAKE_BINARY_DIR}/out)

file(MAKE_DIRECTORY ${BIN_DIR})

# SDK Directories untuk VeaOS Kernel & HCT
set(NDK_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/sdk/include/ndk)
set(DDK_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/sdk/include/ddk)
set(PSDK_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/sdk/include/psdk)
set(ASM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/sdk/include/asm)

# Kernel & HCT Directory
set(KERNEL_DIR ${CMAKE_SOURCE_DIR}/veakrnl)
set(KERNEL_INCLUDE_DIR ${KERNEL_DIR}/include)
set(HCT_DIR ${CMAKE_SOURCE_DIR}/hct)

# =====================================================================
# STAGE 0 & 1: BOOTLOADER (MBR, VBR, EBR)
# =====================================================================
add_custom_command(
    OUTPUT ${BIN_DIR}/boot.bin
    COMMAND nasm -f bin ${STAGE0_DIR}/entry.asm -o ${BIN_DIR}/boot.bin
    DEPENDS ${STAGE0_DIR}/entry.asm
    COMMENT "Compiling Stage 0 MBR (boot.bin)"
)

add_custom_command(
    OUTPUT ${BIN_DIR}/vbr_sec0.bin
    COMMAND nasm -f bin ${STAGE1_DIR}/entryfat32.asm -o ${BIN_DIR}/vbr_sec0.bin
    DEPENDS ${STAGE1_DIR}/entryfat32.asm
    COMMENT "Compiling VBR Header (vbr_sec0.bin)"
)

add_custom_command(
    OUTPUT ${BIN_DIR}/ebr_31sec.bin
    COMMAND nasm -f bin ${STAGE1_DIR}/ebrfat32.asm -o ${BIN_DIR}/ebr_31sec.bin
    DEPENDS ${STAGE1_DIR}/ebrfat32.asm
    COMMENT "Compiling Extended Boot Record (ebr_31sec.bin)"
)

add_custom_command(
    OUTPUT ${BIN_DIR}/stage1.bin
    COMMAND cat ${BIN_DIR}/vbr_sec0.bin ${BIN_DIR}/ebr_31sec.bin > ${BIN_DIR}/stage1.bin
    DEPENDS ${BIN_DIR}/vbr_sec0.bin ${BIN_DIR}/ebr_31sec.bin
    COMMENT "Merging VBR and EBR components into stage1.bin"
)

# =====================================================================
# STAGE 2: OSI386 LOADER (C & ASM HYBRID)
# =====================================================================
file(GLOB_RECURSE LOADER_ASM_SOURCES "${LOADER_DIR}/*.asm")
file(GLOB_RECURSE LOADER_C_SOURCES "${LOADER_DIR}/*.c")

set(LOADER_OBJS "")

set(BOOT_OBJ ${BIN_DIR}/boot.o)
add_custom_command(
    OUTPUT ${BOOT_OBJ}
    COMMAND nasm -f elf32 ${LOADER_DIR}/boot.asm -I${ASM_INCLUDE_DIR}/ -o ${BOOT_OBJ}
    DEPENDS ${LOADER_DIR}/boot.asm
    COMMENT "Compiling Loader Bootstrap ASM (boot.o)"
)

set(LOADER_ASM_SOURCES_EXCLUDED ${LOADER_ASM_SOURCES})
list(FILTER LOADER_ASM_SOURCES_EXCLUDED EXCLUDE REGEX ".*/boot\\.asm$")

foreach(ASM_FILE ${LOADER_ASM_SOURCES_EXCLUDED})
    file(RELATIVE_PATH REL_PATH ${LOADER_DIR} ${ASM_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${BIN_DIR}/ldr_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND nasm -f elf32 ${ASM_FILE} -I${ASM_INCLUDE_DIR}/ -o ${OBJ_FILE}
        DEPENDS ${ASM_FILE}
        COMMENT "Compiling Loader ASM: ${SAFE_NAME}"
    )
    list(APPEND LOADER_OBJS ${OBJ_FILE})
endforeach()

foreach(C_FILE ${LOADER_C_SOURCES})
    file(RELATIVE_PATH REL_PATH ${LOADER_DIR} ${C_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${BIN_DIR}/ldr_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND clang --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin -I${LOADER_INCLUDE_DIR} -c ${C_FILE} -o ${OBJ_FILE}
        DEPENDS ${C_FILE}
        COMMENT "Compiling Loader C: ${SAFE_NAME}"
    )
    list(APPEND LOADER_OBJS ${OBJ_FILE})
endforeach()

add_library(clangd_loader_dummy OBJECT EXCLUDE_FROM_ALL ${LOADER_C_SOURCES})
target_compile_options(clangd_loader_dummy PRIVATE 
    --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin
)
target_include_directories(clangd_loader_dummy PRIVATE ${LOADER_INCLUDE_DIR})

list(REMOVE_ITEM LOADER_OBJS ${BOOT_OBJ})

add_custom_command(
    OUTPUT ${BIN_DIR}/OSI386
    COMMAND ld.lld -T ${LOADER_DIR}/linker.ld -m elf_i386 ${BOOT_OBJ} ${LOADER_OBJS} -o ${BIN_DIR}/OSI386
    DEPENDS ${BOOT_OBJ} ${LOADER_OBJS} ${LOADER_DIR}/linker.ld
    COMMENT "Linking OSI386 using Clang/LLD (boot.o placed first)"
)

add_custom_target(build_osi386 DEPENDS ${BIN_DIR}/OSI386)

# =====================================================================
# HCT (Hardware Communication Tool) - Compile & Shared Library
# =====================================================================
file(GLOB_RECURSE HCT_ASM_SOURCES "${HCT_DIR}/*.asm")
file(GLOB_RECURSE HCT_C_SOURCES "${HCT_DIR}/*.c")

set(HCT_OBJS "")

foreach(ASM_FILE ${HCT_ASM_SOURCES})
    file(RELATIVE_PATH REL_PATH ${HCT_DIR} ${ASM_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${BIN_DIR}/hct_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND nasm -f elf32 ${ASM_FILE} -I${ASM_INCLUDE_DIR}/ -o ${OBJ_FILE}
        DEPENDS ${ASM_FILE}
        COMMENT "Compiling HCT ASM: ${SAFE_NAME}"
    )
    list(APPEND HCT_OBJS ${OBJ_FILE})
endforeach()

foreach(C_FILE ${HCT_C_SOURCES})
    file(RELATIVE_PATH REL_PATH ${HCT_DIR} ${C_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${BIN_DIR}/hct_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        # Tambahan -fPIC biar aman dibikin .so nantinya
        COMMAND clang --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin -fPIC -I${KERNEL_INCLUDE_DIR} -I${NDK_INCLUDE_DIR} -I${DDK_INCLUDE_DIR} -I${PSDK_INCLUDE_DIR} -c ${C_FILE} -o ${OBJ_FILE}
        DEPENDS ${C_FILE}
        COMMENT "Compiling HCT C: ${SAFE_NAME}"
    )
    list(APPEND HCT_OBJS ${OBJ_FILE})
endforeach()

# Bikin hct.so (mirip hal.dll)
add_custom_command(
    OUTPUT ${BIN_DIR}/hct.so
    COMMAND ld.lld -shared -m elf_i386 ${HCT_OBJS} -o ${BIN_DIR}/hct.so
    DEPENDS ${HCT_OBJS}
    COMMENT "Linking HCT as shared library (hct.so)"
)

add_custom_target(build_hct DEPENDS ${BIN_DIR}/hct.so)

# =====================================================================
# STAGE 3: VEAOS KERNEL (veakrnl.elf)
# =====================================================================
file(GLOB_RECURSE KERNEL_ASM_SOURCES "${KERNEL_DIR}/*.asm")
file(GLOB_RECURSE KERNEL_C_SOURCES "${KERNEL_DIR}/*.c")

set(KERNEL_OBJS "")

foreach(ASM_FILE ${KERNEL_ASM_SOURCES})
    file(RELATIVE_PATH REL_PATH ${KERNEL_DIR} ${ASM_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${BIN_DIR}/krnl_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND nasm -f elf32 ${ASM_FILE} -I${ASM_INCLUDE_DIR}/ -o ${OBJ_FILE}
        DEPENDS ${ASM_FILE}
        COMMENT "Compiling Kernel ASM: ${SAFE_NAME}"
    )
    list(APPEND KERNEL_OBJS ${OBJ_FILE})
endforeach()

foreach(C_FILE ${KERNEL_C_SOURCES})
    file(RELATIVE_PATH REL_PATH ${KERNEL_DIR} ${C_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${BIN_DIR}/krnl_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND clang --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin -ggdb -I${KERNEL_INCLUDE_DIR} -I${NDK_INCLUDE_DIR} -I${DDK_INCLUDE_DIR} -I${PSDK_INCLUDE_DIR} -c ${C_FILE} -o ${OBJ_FILE} -std=gnu23 -fshort-wchar
        DEPENDS ${C_FILE}
        COMMENT "Compiling Kernel C: ${SAFE_NAME}"
    )
    list(APPEND KERNEL_OBJS ${OBJ_FILE})
endforeach()

# Target Dummy untuk autocomplete Clangd (Kernel + HCT digabung biar HCT juga kedeteksi)
add_library(clangd_kernel_dummy OBJECT EXCLUDE_FROM_ALL ${KERNEL_C_SOURCES} ${HCT_C_SOURCES})
target_compile_options(clangd_kernel_dummy PRIVATE 
    --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin -ggdb -std=gnu23 -fshort-wchar
)
target_include_directories(clangd_kernel_dummy PRIVATE 
    ${KERNEL_INCLUDE_DIR}
    ${NDK_INCLUDE_DIR} 
    ${DDK_INCLUDE_DIR} 
    ${PSDK_INCLUDE_DIR}
)

add_custom_command(
    OUTPUT ${BIN_DIR}/veakrnl.elf
    # Masukin HCT_OBJS ke dalam linker bareng KERNEL_OBJS
    COMMAND ld.lld -T ${KERNEL_DIR}/linker.ld -m elf_i386 ${KERNEL_OBJS} ${HCT_OBJS} -o ${BIN_DIR}/veakrnl.elf
    DEPENDS ${KERNEL_OBJS} ${HCT_OBJS} ${KERNEL_DIR}/linker.ld
    COMMENT "Linking veakrnl.elf (with HCT included) using Clang/LLD"
)

add_custom_target(build_kernel DEPENDS ${BIN_DIR}/veakrnl.elf)

# =====================================================================
# FINAL STAGE: DISK IMAGE GENERATION
# =====================================================================
add_custom_command(
    OUTPUT ${BIN_DIR}/VeaOS.img
    # Ingat buat update script Python lu kalau lu mau masukin hct.so ke dalam file system-nya juga!
    COMMAND python3 ${TOOL_DIR}/make_fat32.py ${BIN_DIR}/boot.bin ${BIN_DIR}/stage1.bin ${BIN_DIR}/VeaOS.img
    DEPENDS ${BIN_DIR}/boot.bin ${BIN_DIR}/stage1.bin ${BIN_DIR}/OSI386 ${BIN_DIR}/veakrnl.elf ${BIN_DIR}/hct.so ${TOOL_DIR}/make_fat32.py
    COMMENT "Generating 256MB FAT32 Disk Image (VeaOS.img)"
)

add_custom_target(build_disk ALL DEPENDS ${BIN_DIR}/VeaOS.img)