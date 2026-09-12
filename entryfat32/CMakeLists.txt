add_custom_command(
    OUTPUT ${OUT_DIR}/vbr_sec0.bin
    COMMAND nasm -f bin ${CMAKE_CURRENT_SOURCE_DIR}/entryfat32.asm -o ${OUT_DIR}/vbr_sec0.bin
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/entryfat32.asm
    COMMENT "Compiling VBR Header (vbr_sec0.bin)"
)

add_custom_command(
    OUTPUT ${OUT_DIR}/ebr_31sec.bin
    COMMAND nasm -f bin ${CMAKE_CURRENT_SOURCE_DIR}/ebrfat32.asm -o ${OUT_DIR}/ebr_31sec.bin
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ebrfat32.asm
    COMMENT "Compiling Extended Boot Record (ebr_31sec.bin)"
)

add_custom_command(
    OUTPUT ${OUT_DIR}/stage1.bin
    COMMAND cat ${OUT_DIR}/vbr_sec0.bin ${OUT_DIR}/ebr_31sec.bin > ${OUT_DIR}/stage1.bin
    DEPENDS ${OUT_DIR}/vbr_sec0.bin ${OUT_DIR}/ebr_31sec.bin
    COMMENT "Merging VBR and EBR components into stage1.bin"
)

add_custom_target(build_stage1 DEPENDS ${OUT_DIR}/stage1.bin)