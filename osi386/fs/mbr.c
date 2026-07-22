#include <osi386.h>

MBR_PartitionEntry mbr_part_entry[4];
int selected_partition = 1;
int partition_count = 0;

void mbr_detect(void)
{
    static unsigned char buffer_lba[512];

    LBA_READ_TO_BUFFER(0, buffer_lba, 512);

    if(buffer_lba[510] == 0x55 && buffer_lba[511] == 0xAA)
    {
        int i;
        MBR_PartitionEntry* mboot_table = (MBR_PartitionEntry*)&buffer_lba[446];

        for(i = 0; i < 4; i++)
        {
            mbr_part_entry[i] = mboot_table[i];

            if(mbr_part_entry[i].type)
            {
                partition_count++;
            }
        }

        if(mbr_part_entry[0].type == 0x0C /* FAT32 with LBA */)
        {
            selected_partition = 1;
        }
    }
}