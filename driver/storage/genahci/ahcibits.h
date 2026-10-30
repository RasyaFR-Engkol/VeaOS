#pragma once

// Setting BIT
#define AHCI_SET_BIT(reg, bit)          ((reg) |= (bit))
#define AHCI_CLEAR_BIT(reg, bit)        ((reg) &= ~(bit))
#define AHCI_TEST_BIT(reg, bit)         (((reg) & (bit)) != 0)

// GHC Abar Offset 
#define AHCI_REG_CAP                    0x00    // Host Capabilities
#define AHCI_REG_GHC                    0x04    // Global Host Control
#define AHCI_REG_IS                     0x08    // Interrupt Status
#define AHCI_REG_PI                     0x0C    // Ports Implemented
#define AHCI_REG_VS                     0x10    // Version
#define AHCI_REG_CCC_CTL                0x14    // Command Completion Coalescing Control
#define AHCI_REG_CCC_PORTS              0x18    // Command Completion Coalescing Ports
#define AHCI_REG_EM_LOC                 0x1C    // Enclosure Management Location
#define AHCI_REG_EM_CTL                 0x20    // Enclosure Management Control
#define AHCI_REG_CAP2                   0x24    // Host Capabilities Extended
#define AHCI_REG_BOHC                   0x28    // BIOS/OS Handoff Control and Status

// CAP Resgiter Bits
#define AHCI_CAP_NP_MASK                0x0000001F  // Number of Ports (NP + 1)
#define AHCI_CAP_SXS                    (1 << 5)    // Supports External SATA
#define AHCI_CAP_EMS                    (1 << 6)    // Enclosure Management Supported
#define AHCI_CAP_CCCS                   (1 << 7)    // Command Completion Coalescing Supported
#define AHCI_CAP_NCS_MASK               0x00001F00  // Number of Command Slots (NCS + 1)
#define AHCI_CAP_PSC                    (1 << 13)   // Partial State Capable
#define AHCI_CAP_SSC                    (1 << 14)   // Slumber State Capable
#define AHCI_CAP_PMD                    (1 << 15)   // PIO Multiple DRQ Block
#define AHCI_CAP_FBSS                   (1 << 16)   // FIS-based Switching Supported
#define AHCI_CAP_SPM                    (1 << 17)   // Supports Port Multiplier
#define AHCI_CAP_SAM                    (1 << 18)   // Supports AHCI Mode Only
#define AHCI_CAP_ISS_MASK               0x000F0000  // Interface Speed Support
#define AHCI_CAP_SCLO                   (1 << 24)   // Supports Command List Override
#define AHCI_CAP_SAL                    (1 << 25)   // Supports Activity LED
#define AHCI_CAP_SALP                   (1 << 26)   // Supports Aggressive Link Power Management
#define AHCI_CAP_SSS                    (1 << 27)   // Supports Staggered Spin-up
#define AHCI_CAP_SMPS                   (1 << 28)   // Supports Mechanical Presence Switch
#define AHCI_CAP_SSNTF                  (1 << 29)   // Supports SNotification Register
#define AHCI_CAP_SNCQ                   (1 << 30)   // Supports Native Command Queuing (NCQ)
#define AHCI_CAP_S64A                   (1 << 31)   // Supports 64-bit Addressing

// GHC Resgiter Bits
#define AHCI_GHC_HR                     (1 << 0)    // HBA Reset (1 = Reset, Self-Clearing)
#define AHCI_GHC_IE                     (1 << 1)    // Interrupt Enable (Global)
#define AHCI_GHC_MRSM                   (1 << 2)    // MSI Revert to Single Message
#define AHCI_GHC_AE                     (1 << 31)   // AHCI Enable (1 = AHCI, 0 = Legacy IDE)

// CAP2 Register Bits
#define AHCI_CAP2_BOHC                  (1 << 0)    // BIOS/OS Handoff Supported
#define AHCI_CAP2_NVMP                  (1 << 1)    // NVM HBA Premium Supported
#define AHCI_CAP2_APST                  (1 << 2)    // Automatic Partial to Slumber Transitions
#define AHCI_CAP2_SDS                   (1 << 3)    // Supports Device Sleep
#define AHCI_CAP2_SADM                  (1 << 4)    // Supports Aggressive Device Sleep Management
#define AHCI_CAP2_DESO                  (1 << 5)    // Device Sleep Entrance From Slumber Only

// BOHC Register Bits
#define AHCI_BOHC_SMS                   (1 << 0)    // SMI on OS Ownership Change Enable
#define AHCI_BOHC_BOS                   (1 << 0)    // BIOS Ownership Semaphore
#define AHCI_BOHC_OOS                   (1 << 1)    // OS Ownership Semaphore
#define AHCI_BOHC_SOOE                  (1 << 2)    // SMI on OS Ownership Change Enable
#define AHCI_BOHC_OOB                   (1 << 3)    // OS Ownership Change Bit
#define AHCI_BOHC_BB                    (1 << 4)    // BIOS Busy

// Port Registers
#define AHCI_PORT_OFFSET(port)          (0x100 + ((port) * 0x80))

#define AHCI_PxCLB                      0x00    // Command List Base Address (Low 32)
#define AHCI_PxCLBU                     0x04    // Command List Base Address (Upper 32)
#define AHCI_PxFB                       0x08    // FIS Base Address (Low 32)
#define AHCI_PxFBU                      0x0C    // FIS Base Address (Upper 32)
#define AHCI_PxIS                       0x10    // Interrupt Status
#define AHCI_PxIE                       0x14    // Interrupt Enable
#define AHCI_PxCMD                      0x18    // Command and Status
#define AHCI_PxTFD                      0x20    // Task File Data
#define AHCI_PxSIG                      0x24    // Signature
#define AHCI_PxSSTS                     0x28    // SATA Status (SCR0)
#define AHCI_PxSCTL                     0x2C    // SATA Control (SCR2)
#define AHCI_PxSERR                     0x30    // SATA Error (SCR1)
#define AHCI_PxSACT                     0x34    // SATA Active (SActive)
#define AHCI_PxCI                       0x38    // Command Issue
#define AHCI_PxSNTF                     0x3C    // SATA Notification (SNotification)
#define AHCI_PxFBS                      0x40    // FIS-based Switching Control

// PxCMD Register Bits
#define AHCI_PxCMD_ST                   (1 << 0)    // Start Command Engine (1 = Run, 0 = Stop)
#define AHCI_PxCMD_SUD                  (1 << 1)    // Spin-Up Device
#define AHCI_PxCMD_POD                  (1 << 2)    // Power On Device
#define AHCI_PxCMD_CLO                  (1 << 3)    // Command List Override
#define AHCI_PxCMD_FRE                  (1 << 4)    // FIS Receive Enable
#define AHCI_PxCMD_FR                   (1 << 14)   // FIS Receive Running (Read Only)
#define AHCI_PxCMD_CR                   (1 << 15)   // Command List Running (Read Only)
#define AHCI_PxCMD_CPS                  (1 << 16)   // Cold Presence State
#define AHCI_PxCMD_PMA                  (1 << 17)   // Port Multiplier Attached
#define AHCI_PxCMD_HPCP                 (1 << 18)   // Hot Plug Capable Port
#define AHCI_PxCMD_MPSP                 (1 << 19)   // Mechanical Presence Switch Attached
#define AHCI_PxCMD_CPAP                 (1 << 20)   // Cold Presence Allowed
#define AHCI_PxCMD_ESP                  (1 << 21)   // External SATA Port
#define AHCI_PxCMD_FBSCP               (1 << 22)   // FIS-based Switching Capable Port
#define AHCI_PxCMD_APSTE               (1 << 23)   // Automatic Partial to Slumber Transitions Enable
#define AHCI_PxCMD_ATAPI               (1 << 24)   // Device is ATAPI
#define AHCI_PxCMD_DLA                 (1 << 25)   // Drive LED Active
#define AHCI_PxCMD_ALPE                (1 << 26)   // Aggressive Link Power Management Enable
#define AHCI_PxCMD_ASP                  (1 << 27)   // Aggressive Slumber / Partial
#define AHCI_PxCMD_ICC_MASK             0xF0000000  // Interface Communication Control
#define AHCI_PxCMD_ICC_ACTIVE           (1 << 28)
#define AHCI_PxCMD_ICC_PARTIAL          (2 << 28)
#define AHCI_PxCMD_ICC_SLUMBER          (6 << 28)

// PxIS / PxIE Register Bits
#define AHCI_PxIS_DHRS                  (1 << 0)    // Device to Host FIS Interrupt
#define AHCI_PxIS_PSS                   (1 << 1)    // PIO Setup FIS Interrupt
#define AHCI_PxIS_SDBS                  (1 << 2)    // Set Device Bits Interrupt
#define AHCI_PxIS_UFS                   (1 << 3)    // Unknown FIS Interrupt
#define AHCI_PxIS_DPS                   (1 << 4)    // Descriptor Processed Interrupt
#define AHCI_PxIS_PCS                   (1 << 5)    // Port Connect Change Interrupt
#define AHCI_PxIS_DMPS                  (1 << 6)    // Device Mechanical Presence Status
#define AHCI_PxIS_PRCS                  (1 << 22)   // PhyRdy Change Interrupt
#define AHCI_PxIS_IPMS                  (1 << 23)   // Incorrect Port Multiplier Status
#define AHCI_PxIS_OFS                   (1 << 24)   // Overflow Status
#define AHCI_PxIS_INFS                  (1 << 26)   // Interface Non-fatal Error Status
#define AHCI_PxIS_IFS                   (1 << 27)   // Interface Fatal Error Status
#define AHCI_PxIS_HBDS                  (1 << 28)   // Host Bus Data Error Status
#define AHCI_PxIS_HBFS                  (1 << 29)   // Host Bus Fatal Error Status
#define AHCI_PxIS_TFES                  (1 << 30)   // Task File Error Status
#define AHCI_PxIS_CPDS                  (1 << 31)   // Cold Port Detect Status

#define AHCI_PxIS_ERROR_MASK            (AHCI_PxIS_UFS  | AHCI_PxIS_IPMS | \
                                         AHCI_PxIS_OFS  | AHCI_PxIS_INFS | \
                                         AHCI_PxIS_IFS  | AHCI_PxIS_HBDS | \
                                         AHCI_PxIS_HBFS | AHCI_PxIS_TFES)

// PxSSTS Register
#define AHCI_PxSSTS_DET_MASK            0x0000000F  // Device Detection Status
#define AHCI_PxSSTS_DET_NONE            0           // No device detected
#define AHCI_PxSSTS_DET_PRESENT_NOPHY   1           // Device detected, no Phy communication
#define AHCI_PxSSTS_DET_PRESENT_PHY     3           // Device detected AND Phy communication established
#define AHCI_PxSSTS_DET_OFFLINE         4           // Device offline

#define AHCI_PxSSTS_SPD_MASK            0x000000F0  // Current Interface Speed
#define AHCI_PxSSTS_SPD_GEN1            (1 << 4)    // 1.5 Gbps (SATA I)
#define AHCI_PxSSTS_SPD_GEN2            (2 << 4)    // 3.0 Gbps (SATA II)
#define AHCI_PxSSTS_SPD_GEN3            (3 << 4)    // 6.0 Gbps (SATA III)

#define AHCI_PxSSTS_IPM_MASK            0x00000F00  // Interface Power Management
#define AHCI_PxSSTS_IPM_NOT_PRESENT     (0 << 8)
#define AHCI_PxSSTS_IPM_ACTIVE          (1 << 8)
#define AHCI_PxSSTS_IPM_PARTIAL         (2 << 8)
#define AHCI_PxSSTS_IPM_SLUMBER         (6 << 8)

// PxSCTL Registers
#define AHCI_PxSCTL_DET_MASK            0x0000000F
#define AHCI_PxSCTL_DET_NOINIT          0           // No initialization request
#define AHCI_PxSCTL_DET_INIT            1           // Perform COMRESET
#define AHCI_PxSCTL_DET_DISABLE         4           // Disable SATA interface

// PxTFD Regsters
#define AHCI_PxTFD_STS_ERR              (1 << 0)    // Error Flag
#define AHCI_PxTFD_STS_DRQ              (1 << 3)    // Data Transfer Requested
#define AHCI_PxTFD_STS_BSY              (1 << 7)    // Busy Status

// Device Signature
#define AHCI_DEV_SIG_SATA               0x00000101  // SATA Disk (Direct access)
#define AHCI_DEV_SIG_SATAPI             0xEB140101  // SATAPI Optical Drive (CD/DVD)
#define AHCI_DEV_SIG_SEMB               0xC33C0101  // Enclosure Management Bridge
#define AHCI_DEV_SIG_PM                 0x96690101  // Port Multiplier

// FIS
#define FIS_TYPE_REG_H2D                0x27    // Register FIS - Host to Device
#define FIS_TYPE_REG_D2H                0x34    // Register FIS - Device to Host
#define FIS_TYPE_DMA_ACT                0x39    // DMA Activate FIS - Device to Host
#define FIS_TYPE_DMA_SETUP              0x41    // DMA Setup FIS - Bidirectional
#define FIS_TYPE_DATA                   0x46    // Data FIS - Bidirectional
#define FIS_TYPE_BIST                   0x58    // BIST Activate FIS - Bidirectional
#define FIS_TYPE_PIO_SETUP              0x5F    // PIO Setup FIS - Device to Host
#define FIS_TYPE_DEV_BITS               0xA1    // Set Device Bits FIS - Device to Host

// ATAPI Command
#define ATA_CMD_READ_DMA                0xC8
#define ATA_CMD_READ_DMA_EX             0x25    // LBA48 Read DMA
#define ATA_CMD_WRITE_DMA               (0xCA)
#define ATA_CMD_WRITE_DMA_EX            0x35    // LBA48 Write DMA
#define ATA_CMD_READ_FPDMA_QUEUED       0x60    // NCQ Read
#define ATA_CMD_WRITE_FPDMA_QUEUED      0x61    // NCQ Write
#define ATA_CMD_IDENTIFY                0xEC    // Identify SATA Device
#define ATA_CMD_IDENTIFY_PACKET         0xA1    // Identify ATAPI Device
#define ATA_CMD_FLUSH_CACHE             0xE7
#define ATA_CMD_FLUSH_CACHE_EX          0xEA    // LBA48 Flush Cache
#define ATA_CMD_DATA_SET_MANAGEMENT     0x06

/* EOF */