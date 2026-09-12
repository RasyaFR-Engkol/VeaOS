file(GLOB_RECURSE LOADER_ASM_SOURCES "*.asm")
file(GLOB_RECURSE LOADER_C_SOURCES "*.c")
list(APPEND LOADER_C_SOURCES ${RTL_SOURCES})

# Folder untuk file objek (.o) sementara -> build/osi386/
set(OBJ_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(LOADER_OBJS "")

# 1. Compile Bootstrap ASM (boot.o)
set(BOOT_OBJ ${OBJ_DIR}/boot.o)
add_custom_command(
    OUTPUT ${BOOT_OBJ}
    COMMAND nasm -f elf32 ${CMAKE_CURRENT_SOURCE_DIR}/boot.asm -I${ASM_INCLUDE_DIR}/ -o ${BOOT_OBJ}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/boot.asm
    COMMENT "Compiling Loader Bootstrap ASM (boot.o)"
)

# Filter out boot.asm dari list regular ASM
set(LOADER_ASM_SOURCES_EXCLUDED ${LOADER_ASM_SOURCES})
list(FILTER LOADER_ASM_SOURCES_EXCLUDED EXCLUDE REGEX ".*/boot\\.asm$")

# 2. Compile sisa file ASM
foreach(ASM_FILE ${LOADER_ASM_SOURCES_EXCLUDED})
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${ASM_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${OBJ_DIR}/ldr_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND nasm -f elf32 ${ASM_FILE} -I${ASM_INCLUDE_DIR}/ -o ${OBJ_FILE}
        DEPENDS ${ASM_FILE}
        COMMENT "Compiling Loader ASM: ${SAFE_NAME}"
    )
    list(APPEND LOADER_OBJS ${OBJ_FILE})
endforeach()

# 3. Compile file C
foreach(C_FILE ${LOADER_C_SOURCES})
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${C_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${OBJ_DIR}/ldr_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        # TAMBAHKAN ${VEA_INCLUDES} DI SINI
        COMMAND clang --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin -DBUILDING_VEA_LOADER -I${LOADER_INCLUDE_DIR} ${VEA_INCLUDES} -c ${C_FILE} -o ${OBJ_FILE} -fms-extensions
        DEPENDS ${C_FILE}
        COMMENT "Compiling Loader C: ${SAFE_NAME}"
    )
    
    list(APPEND LOADER_OBJS ${OBJ_FILE})
endforeach()

# Target Dummy clangd
add_library(clangd_loader_dummy OBJECT EXCLUDE_FROM_ALL ${LOADER_C_SOURCES})
target_compile_options(clangd_loader_dummy PRIVATE 
    --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin -fms-extensions
)
target_include_directories(clangd_loader_dummy PRIVATE ${LOADER_INCLUDE_DIR})

list(REMOVE_ITEM LOADER_OBJS ${BOOT_OBJ})

# 4. LINKING (Output binary OSI386 ditaro ke build/out/OSI386)
add_custom_command(
    OUTPUT ${OUT_DIR}/OSI386
    COMMAND ld.lld -T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -m elf_i386 
            ${BOOT_OBJ} ${LOADER_OBJS} 
            -o ${OUT_DIR}/OSI386
            -Map=${OUT_DIR}/OSI386.map
    DEPENDS ${BOOT_OBJ} ${LOADER_OBJS} ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    COMMENT "Linking OSI386 using Clang/LLD (${OUT_DIR}/OSI386)"
)

add_custom_target(build_osi386 DEPENDS ${OUT_DIR}/OSI386)