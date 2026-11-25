##
# Target Build: None : COMRC Definition
##
set(COMRC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/comrc.c)
set(COMRC_BIN ${CMAKE_CURRENT_BINARY_DIR}/comrc)

if(CMAKE_HOST_UNIX)
    set(HOST_CC cc)
else()
    set(HOST_CC gcc)
endif()

add_custom_command(
    OUTPUT ${COMRC_BIN}
    COMMAND ${HOST_CC} -O2 -Wall ${COMRC_SRC} -o ${COMRC_BIN}
    DEPENDS ${COMRC_SRC}
    COMMENT "Building host resource compiler (comrc)..."
)

add_custom_target(comrc_tool DEPENDS ${COMRC_BIN})

set(VEA_COMRC_EXECUTABLE ${COMRC_BIN} CACHE INTERNAL "Path to comrc host binary")
set(VEA_TOOL_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "Path to VeaOS tool directory")

##
# Target Build: generate_disk : Craete 256MB disk of FAT32
# FIXME: FAT32 still allocated at 128MB. the rest of 128MB are Unallocated
##

add_custom_command(
    OUTPUT ${OUT_DIR}/VeaOS.img
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/make_fat32.py ${OUT_DIR}/boot.bin ${OUT_DIR}/stage1.bin ${OUT_DIR}/VeaOS.img
    DEPENDS ${OUT_DIR}/boot.bin ${OUT_DIR}/stage1.bin ${CMAKE_CURRENT_SOURCE_DIR}/make_fat32.py
    COMMENT "Generating 256MB FAT32 Disk Image (VeaOS.img)..."
)

add_custom_command(
    OUTPUT ${OUT_DIR}/disk.vhd
    COMMAND VBoxManage convertfromraw ${OUT_DIR}/VeaOS.img ${OUT_DIR}/disk.vhd --format VHD --variant Fixed
    DEPENDS ${OUT_DIR}/VeaOS.img
    COMMENT "Converting VeaOS.img to Fixed VHD (disk.vhd)..."
)

add_custom_target(generate_disk DEPENDS ${OUT_DIR}/disk.vhd)