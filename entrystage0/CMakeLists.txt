add_custom_command(
    OUTPUT ${OUT_DIR}/boot.bin
    COMMAND nasm -f bin ${CMAKE_CURRENT_SOURCE_DIR}/entry.asm -o ${OUT_DIR}/boot.bin
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/entry.asm
    COMMENT "Compiling Stage 0 MBR (boot.bin)"
)

add_custom_target(build_stage0 DEPENDS ${OUT_DIR}/boot.bin)