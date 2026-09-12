# 1. Buat raw image sementara
add_custom_command(
    OUTPUT ${OUT_DIR}/VeaOS.img
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/make_fat32.py ${OUT_DIR}/boot.bin ${OUT_DIR}/stage1.bin ${OUT_DIR}/VeaOS.img
    DEPENDS ${OUT_DIR}/boot.bin ${OUT_DIR}/stage1.bin ${CMAKE_CURRENT_SOURCE_DIR}/make_fat32.py
    COMMENT "Generating 256MB FAT32 Disk Image (VeaOS.img)..."
)

# 2. Konversi ke Fixed VHD dengan nama disk.vhd
add_custom_command(
    OUTPUT ${OUT_DIR}/disk.vhd
    COMMAND VBoxManage convertfromraw ${OUT_DIR}/VeaOS.img ${OUT_DIR}/disk.vhd --format VHD --variant Fixed
    DEPENDS ${OUT_DIR}/VeaOS.img
    COMMENT "Converting VeaOS.img to Fixed VHD (disk.vhd)..."
)

# 3. Target utama
add_custom_target(generate_disk DEPENDS ${OUT_DIR}/disk.vhd)