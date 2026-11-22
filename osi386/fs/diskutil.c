#include <osi386.h>
#include <stdint.h>

uint8_t LBA_READ_TO_BUFFER(uint32_t lba, void* buffer, uint32_t size)
{
    static DiskAddressPacket dap;
    uint32_t linear_addr = (uint32_t)buffer;

    // BIOS Real Mode cuma bisa akses < 1 MB (0x00000 - 0xFFFFF)
    if (linear_addr >= 0x100000) {
        return 0xFF; 
    }

    dap.size = 16;
    dap.reserved = 0;
    dap.sectors = (uint16_t)((size + 511) / 512);
    dap.buffer_offset = (uint16_t)(linear_addr & 0x000F);
    dap.buffer_segment = (uint16_t)((linear_addr >> 4) & 0xFFFF);
    dap.lba_lower = lba;
    dap.lba_upper = 0;

    return _LBA_READ(0x80, &dap); // 0 = sukses, selain 0 = kode error BIOS AH
}

VOID
VEAPI
LdrExtractDiskInformation(IN OUT HPBLOCK_BOOT_2 BlockBoot2)
{
    static UCHAR SectorBuffer[512];
    extern INT selected_partition;

    if (!BlockBoot2)
    {
        return;
    }

    /* Read to buffer */
    cmemset(SectorBuffer, 0, sizeof(SectorBuffer));
    LBA_READ_TO_BUFFER(0, SectorBuffer, sizeof(SectorBuffer));

    /* This should not happen. WTF is this disk*/
    if (SectorBuffer[510] != 0x55 || SectorBuffer[511] != 0xAA)
    {
        BlockBoot2->BootPartitionSignature.Type = BootDeviceTypeUnknown;
        return;
    }

    /* Should check this is GPT or not */
    MBR_PartitionEntry *PartTable = (MBR_PartitionEntry *)&SectorBuffer[446];

    if(PartTable[0].type == 0xEE)
    {
        /* GPT Scheme*/
        BlockBoot2->BootPartitionSignature.Type = BootDeviceTypeGpt;

        /* TODO: Parse GPT GUID Disk */
        cmemset(&BlockBoot2->BootPartitionSignature.u.Gpt.PartitionGuid, 0, sizeof(GUID));
    }
    else
    {
        BlockBoot2->BootPartitionSignature.Type = BootDeviceTypeMbr;

        ULONG DiskSignature = *(ULONG *)&SectorBuffer[440];

        BlockBoot2->BootPartitionSignature.u.Mbr.DiskSignature = DiskSignature;
        BlockBoot2->BootPartitionSignature.u.Mbr.PartitionNumber = (ULONG)selected_partition;
    }
}