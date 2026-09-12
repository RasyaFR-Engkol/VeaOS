file(GLOB_RECURSE HCT_ASM_SOURCES "*.asm")
file(GLOB_RECURSE HCT_C_SOURCES "*.c")

# Folder untuk file objek (.o) sementara -> build/hct/x86/
set(OBJ_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(HCT_OBJS "")

# =============================================================
# COMPILE ASM
# =============================================================
foreach(ASM_FILE ${HCT_ASM_SOURCES})
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${ASM_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${OBJ_DIR}/hct_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND nasm -f elf32 ${ASM_FILE} -I${ASM_INCLUDE_DIR}/ -o ${OBJ_FILE}
        DEPENDS ${ASM_FILE}
        COMMENT "Compiling HCT ASM: ${SAFE_NAME}"
    )
    list(APPEND HCT_OBJS ${OBJ_FILE})
endforeach()

# =============================================================
# COMPILE C
# =============================================================
foreach(C_FILE ${HCT_C_SOURCES})
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${C_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${OBJ_DIR}/hct_${SAFE_NAME}.o)
    
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND clang --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin -fPIC 
                -I${CMAKE_CURRENT_SOURCE_DIR}/include 
                ${VEA_INCLUDES} 
                -c ${C_FILE} -o ${OBJ_FILE} -fms-extensions
        DEPENDS ${C_FILE}
        COMMENT "Compiling HCT C: ${SAFE_NAME}"
    )
    list(APPEND HCT_OBJS ${OBJ_FILE})
endforeach()

# =============================================================
# LINKING (Output ke build/out/hct.so)
# =============================================================
add_custom_command(
    OUTPUT ${OUT_DIR}/hct.so
    COMMAND ld.lld -shared -Bsymbolic -m elf_i386 ${HCT_OBJS} -o ${OUT_DIR}/hct.so
    DEPENDS ${HCT_OBJS}
    COMMENT "Linking HCT as shared library (${OUT_DIR}/hct.so)"
)

add_custom_target(build_hct DEPENDS ${OUT_DIR}/hct.so)

# Export variabel ke parent scope biar bisa dipakai veakrnl
set(HCT_OBJS ${HCT_OBJS} PARENT_SCOPE)
set(HCT_C_SOURCES ${HCT_C_SOURCES} PARENT_SCOPE)