#include <fat32intrnl.h>
#include <osi386.h>
#include <stdint.h>

FAT32_BiosBlock BiosBlock;
unsigned long fat_start_lba;
unsigned long data_start_lba;
unsigned long root_cluster;

int32_t fat32_init(void)
{
    unsigned long fat32_lba_start = mbr_part_entry[0].lba_start;
    int i;
    static unsigned char lba_fat32[512] = {0};

    LBA_READ_TO_BUFFER(fat32_lba_start, lba_fat32, 512);

    if(lba_fat32[0] == 0) return -1;

    cmemcpy(&BiosBlock, lba_fat32, sizeof(FAT32_BiosBlock));

    fat_start_lba = fat32_lba_start + BiosBlock.reserved_sectors;

    data_start_lba = fat_start_lba;
    for(i = 0; i < BiosBlock.fat_count; i++)
    {
        data_start_lba += BiosBlock.sectors_per_fat_32;
    }

    root_cluster = BiosBlock.root_cluster;

    return 0;
}