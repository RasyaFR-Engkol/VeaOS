#pragma once

#define IOCTL_SCSI_BASE                 0x00000004

#define IOCTL_ATA_PASS_THROUGH          CTL_CODE(IOCTL_SCSI_BASE, 0x040B, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

// IOCTL_ATA_PASS_THROUGH_DIRECT (0x0004C030)
#define IOCTL_ATA_PASS_THROUGH_DIRECT   CTL_CODE(IOCTL_SCSI_BASE, 0x040C, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define ATA_FLAGS_DRDY_REQUIRED         0x01
#define ATA_FLAGS_DATA_IN               0x02  // Device -> Host (Read)
#define ATA_FLAGS_DATA_OUT              0x04  // Host -> Device (Write)
#define ATA_FLAGS_48BIT_COMMAND         0x08  // Memakai LBA48 Extended Registers
#define ATA_FLAGS_USE_DMA               0x10  // Gunakan DMA alih-alih PIO

/* EOF */