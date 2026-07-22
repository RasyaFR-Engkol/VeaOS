#pragma once

#include <stdint.h>
#pragma pack(push, 1)

typedef struct{
    unsigned char  jmp[3];
    unsigned char  oem[8];
    unsigned short bytes_per_sector;    // Biasanya 512
    unsigned char  sectors_per_cluster; // Penting untuk alokasi
    unsigned short reserved_sectors;    // PENTING: Untuk tahu di mana tabel FAT dimulai
    unsigned char  fat_count;           // Biasanya 2
    unsigned short dir_entries;         // (0 untuk FAT32)
    unsigned short total_sectors_16;    // (0 untuk FAT32)
    unsigned char  media_descriptor;
    unsigned short sectors_per_fat_16;  // (0 untuk FAT32)
    unsigned short sectors_per_track;
    unsigned short heads;
    unsigned long  hidden_sectors;      // Sama dengan lba_start biasanya
    unsigned long  total_sectors_32;    // Ukuran partisi

    // Extended FAT32 BPB
    unsigned long  sectors_per_fat_32;  // Ukuran tabel FAT
    unsigned short ext_flags;
    unsigned short fat_version;
    unsigned long  root_cluster;        // PENTING: Biasanya cluster 2
    unsigned short fs_info_sector;
    unsigned short backup_boot_sector;
    unsigned char  reserved[12];
    unsigned char  drive_number;
    unsigned char  reserved1;
    unsigned char  boot_signature;      // 0x29
    unsigned long  volume_id;
    unsigned char  volume_label[11];
    unsigned char  fs_type[8];          // "FAT32   "
} FAT32_BiosBlock;

typedef struct {
    unsigned char  name[11];            // Nama file 8.3 format
    unsigned char  attributes;          // 0x10 = Direktori, 0x20 = File Archive
    unsigned char  reserved;
    unsigned char  creation_time_tenths;
    unsigned short creation_time;
    unsigned short creation_date;
    unsigned short access_date;
    unsigned short cluster_high;        // 16 bit atas dari nomor cluster
    unsigned short modify_time;
    unsigned short modify_date;
    unsigned short cluster_low;         // 16 bit bawah dari nomor cluster
    unsigned long  size;                // Ukuran file dalam bytes
} FAT32_DirEntry;

typedef struct {
    unsigned char  sequence_number; // Urutan ke-berapa LFN ini (1, 2, 3... atau ditambah 0x40 kalau ini yang terakhir)
    unsigned char  name1[10];       // 5 karakter pertama (Unicode, masing-masing 2 byte)
    unsigned char  attribute;       // SELALU 0x0F
    unsigned char  type;            // SELALU 0x00
    unsigned char  checksum;        // Checksum dari entry DOS 8.3 di bawahnya
    unsigned char  name2[12];       // 6 karakter berikutnya (Unicode)
    unsigned short first_cluster;   // SELALU 0x0000
    unsigned char  name3[4];        // 2 karakter terakhir (Unicode)
} FAT32_LFN_Entry;

#pragma pack(pop)

extern FAT32_BiosBlock BiosBlock;
extern unsigned long fat_start_lba;
extern unsigned long data_start_lba;
extern unsigned long root_cluster;
int32_t fat32_init(void);

// Mengubah nomor Cluster menjadi alamat sektor absolut (LBA)
#define FAT32_CLUSTER_TO_LBA(cluster) \
    (data_start_lba + (((cluster) - 2) * BiosBlock.sectors_per_cluster))

// Mengubah alamat LBA kembali menjadi nomor Cluster
#define FAT32_LBA_TO_CLUSTER(lba) \
    ((((lba) - data_start_lba) / BiosBlock.sectors_per_cluster) + 2)

// Menggabungkan cluster
#define MERGE_CLUSTER_HI_N_LO(high, low) \
    (((unsigned long)(high) << 16) | (unsigned long)(low))

// FAT32 FLAG ATTRIBUTE
#define FAT32_DIR_RO 0x1
#define FAT32_DIR_HIDDEN 0x2
#define FAT32_DIR_SYSTEM 0x4
#define FAT32_DIR_VOLLABEL 0x8
#define FAT32_DIR_LFN 0xF
#define FAT32_DIR_SUBDIRECTORY 0x10
#define FAT32_DIR_ARCHIVE 0x20

// FAT32 MISC
#define FAT32_ENT_REMOVED 0xE5
#define FAT32_ENT_NOAVAIL 0x0

typedef int (*f32_iter_cb)(FAT32_DirEntry *entry, const char *name, void* user_data);

int fat32_construct_lfn(FAT32_DirEntry *DirEntry, char *lfn_final_buffer);
void fat32_construct_sfn_safe(FAT32_DirEntry *dir_entry, char *outbuffer);
unsigned long fat32_get_next_cluster(unsigned long current_cluster);
int fat32_iterate_directory(unsigned long start_cluster, f32_iter_cb callback, void *user_data);
int fat32_find_entry(unsigned long dir_cluster, const char *target_name, FAT32_DirEntry *out_entry);
unsigned long fat32_read_file(FAT32_DirEntry *file_entry, unsigned char *outbuffer);

struct FAT32_SearchData {
    const char *target_name;
    FAT32_DirEntry *out_entry;
};