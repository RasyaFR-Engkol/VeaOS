import sys
from pathlib import Path

def main():
    if len(sys.argv) < 4:
        print("Usage: python make_fat32.py <boot.bin> <stage1.bin> <output_disk.img>")
        sys.exit(1)

    boot_path = Path(sys.argv[1])
    stage1_path = Path(sys.argv[2])
    img_path = Path(sys.argv[3])

    boot_bytes = boot_path.read_bytes()
    stage1_bytes = stage1_path.read_bytes();

    disk_size = 256 * 1024 * 1024
    img = bytearray(disk_size);

    SECTOR_SIZE = 512
    START_LBA = 2048
    RESERVED_SECTORS = 32
    SECTORS_PER_FAT = 2000

    img[:len(boot_bytes)] = boot_bytes

    vbr_offset = START_LBA * SECTOR_SIZE
    img[vbr_offset:vbr_offset+len(stage1_bytes)] = stage1_bytes

    fsinfo_offset = (START_LBA + 1) * SECTOR_SIZE
    img[fsinfo_offset : fsinfo_offset + 4] = b'\x52\x52\x61\x41'  # LeadSig: 0x41615252 ("RRaA")
    img[fsinfo_offset + 484 : fsinfo_offset + 488] = b'\x72\x72\x41\x61'  # StrucSig: 0x61417272 ("rrAa")
    img[fsinfo_offset + 488 : fsinfo_offset + 492] = b'\xFF\xFF\xFF\xFF'  # Free Cluster Count (Unknown)
    img[fsinfo_offset + 492 : fsinfo_offset + 496] = b'\xFF\xFF\xFF\xFF'  # Next Free Cluster
    img[fsinfo_offset + 508 : fsinfo_offset + 512] = b'\x00\x00\x55\xAA'  # TrailSig: 0xAA550000

    backup_vbr_offset = (START_LBA + 6) * SECTOR_SIZE
    img[backup_vbr_offset : backup_vbr_offset + len(stage1_bytes)] = stage1_bytes

    backup_fsinfo_offset = (START_LBA + 7) * SECTOR_SIZE
    img[backup_fsinfo_offset : backup_fsinfo_offset + SECTOR_SIZE] = img[fsinfo_offset : fsinfo_offset + SECTOR_SIZE]

    fat1_sector = START_LBA + RESERVED_SECTORS
    fat2_sector = fat1_sector + SECTORS_PER_FAT

    fat1_offset = fat1_sector * SECTOR_SIZE
    fat2_offset = fat2_sector * SECTOR_SIZE

    # Setiap entri FAT32 berukuran 4 byte. 3 entri pertama wajib diisi:
    # Entri 0: Media Descriptor (0x0FFFFFF8) -> F8 FF FF 0F
    # Entri 1: Partition Status / EOC Marker (0x0FFFFFFF) -> FF FF FF 0F
    # Entri 2: Cluster 2 (Root Directory) End of Chain -> FF FF FF 0F
    fat_init_bytes = b'\xF8\xFF\xFF\x0F\xFF\xFF\xFF\x0F\xFF\xFF\xFF\x0F'

    img[fat1_offset : fat1_offset + len(fat_init_bytes)] = fat_init_bytes
    img[fat2_offset : fat2_offset + len(fat_init_bytes)] = fat_init_bytes

    # Tulis seluruh array byte menjadi file disk.img asli
    img_path.write_bytes(img)
    print(f"[make_fat32.py] Real FAT32 MBR Image generated successfully at: {img_path}")

if __name__ == "__main__":
    main()