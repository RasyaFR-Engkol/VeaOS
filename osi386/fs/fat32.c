#include <osi386.h>
#include <fat32intrnl.h>

static char lfn_final_cat[256];
static unsigned char fat_buffer[512], buffer_lba[512]; 

#define POINTER void*

int fat32_construct_lfn(FAT32_DirEntry *DirEntry, char *lfn_final_buffer)
{
    FAT32_LFN_Entry *lfn = (FAT32_LFN_Entry*)DirEntry;
    int seq_num = lfn->sequence_number & 0x1F;
    int offset, char_idx, k;
    int status = 0;

    if(lfn->sequence_number & 0x40)
    {
        cmemset(lfn_final_buffer, 0, 256); // array of the char selalu 256
        status = 1;
    }

    offset = (seq_num - 1) * 13;
    char_idx = 0;

    for(k = 0; k < 10; k += 2)
    {
        if(lfn->name1[k] != 0xFF && lfn->name1[k] != 0x00)
            lfn_final_buffer[offset + char_idx++] = lfn->name1[k];
    }

    for(k = 0; k < 12; k += 2)
    {
        if(lfn->name2[k] != 0xFF && lfn->name2[k] != 0x00)
            lfn_final_buffer[offset + char_idx++] = lfn->name2[k];
    }

    for(k = 0; k < 4; k += 2)
    {
        if(lfn->name3[k] != 0xFF && lfn->name3[k] != 0x00)
            lfn_final_buffer[offset + char_idx++] = lfn->name3[k];
    }

    status = 1;
    return status;
}

void fat32_construct_sfn_safe(FAT32_DirEntry *dir_entry, char *outbuffer)
{
    int sfn_idx = 0, i;
    
    // Pecah flag NTRes jadi dua
    int lower_base = (dir_entry->reserved & 0x08) ? 1 : 0;
    int lower_ext  = (dir_entry->reserved & 0x10) ? 1 : 0;

    // 1. BACA NAMA UTAMA
    for(i = 0; i < 8; i++)
    {
        char c;
        if (dir_entry->name[i] == ' ' || dir_entry->name[i] == '\0') break;
        
        c = dir_entry->name[i];
        
        // Cek flag DAN pastikan dia beneran huruf Kapital (A-Z)
        if(lower_base && (c >= 'A' && c <= 'Z')) 
        {
            c += 32;
        }
        
        outbuffer[sfn_idx++] = c;
    }

    // 2. BACA EKSTENSI
    if(dir_entry->name[8] != ' ' && dir_entry->name[8] != '\0')
    {
        outbuffer[sfn_idx++] = '.';
        
        for(i = 8; i < 11; i++)
        {
            char c;
            if (dir_entry->name[i] == ' ' || dir_entry->name[i] == '\0') break;
            
            c = dir_entry->name[i];
            
            // Cek flag ekstensi DAN pastikan dia beneran huruf Kapital
            if(lower_ext && (c >= 'A' && c <= 'Z')) 
            {
                c += 32;
            }
            
            outbuffer[sfn_idx++] = c;
        }
    }

    // Tutup string
    outbuffer[sfn_idx] = '\0';
}

unsigned long fat32_get_next_cluster(unsigned long current_cluster)
{
    unsigned long fat_offset;
    unsigned long fat_sector;
    unsigned long entry_offset;
    unsigned long next_cluster;

    fat_offset = current_cluster * 4;
    fat_sector = fat_start_lba + (fat_offset / 512);
    entry_offset = fat_offset % 512;

    cmemset(fat_buffer, 0, 512);
    LBA_READ_TO_BUFFER(fat_sector, fat_buffer, 512);

    next_cluster = *((unsigned long*)&fat_buffer[entry_offset]);
    next_cluster = next_cluster & 0x0FFFFFFF;

    return next_cluster;
}

int fat32_iterate_directory(unsigned long start_cluster, f32_iter_cb callback, void *user_data)
{
    unsigned long cluster_to_read = start_cluster;
    unsigned long lba_to_read = 0;
    FAT32_DirEntry *dir_entry;
    char clean_sfn[13];
    int lfn_active = 0;
    int i, s;

    while(cluster_to_read < 0x0FFFFFF8 && cluster_to_read != 0)
    {
        unsigned long base_lba = FAT32_CLUSTER_TO_LBA(cluster_to_read);

        for(s = 0; s < BiosBlock.sectors_per_cluster; s++)
        {
            lba_to_read = base_lba + s;
            cmemset(buffer_lba, 0, 512);
            LBA_READ_TO_BUFFER(lba_to_read, buffer_lba, 512);

            dir_entry = (FAT32_DirEntry*)buffer_lba;

            for(i = 0; i < 16; i++)
            {
                const char *final_name;
                
                if(dir_entry[i].name[0] == 0x00) return 0; // Akhir dari direktori
                if(dir_entry[i].name[0] == FAT32_ENT_REMOVED) continue;

                if(dir_entry[i].attributes == FAT32_DIR_LFN)
                {
                    lfn_active = fat32_construct_lfn(&dir_entry[i], lfn_final_cat);
                    continue;
                }

                // Tentukan nama final (LFN atau SFN)
                if(lfn_active)
                {
                    final_name = lfn_final_cat;
                }
                else
                {
                    fat32_construct_sfn_safe(&dir_entry[i], clean_sfn);
                    final_name = clean_sfn;
                }

                // Tembak Callback! 
                // Kalau callback me-return 1, artinya iterasi disuruh berhenti (misal pas searching)
                if (callback(&dir_entry[i], final_name, user_data) == 1) 
                {
                    return 1; // Selesai / ketemu
                }

                lfn_active = 0; // Reset untuk entry berikutnya
            }
        }
        cluster_to_read = fat32_get_next_cluster(cluster_to_read);
    }
    return 0; // Selesai sampai akhir file
}

int internal_search_cb(FAT32_DirEntry *entry, const char *name, void *user_data)
{
    struct FAT32_SearchData *sd = (struct FAT32_SearchData *)user_data;
    
    if (cstrcmp(name, sd->target_name) == 0) 
    {
        // Ketemu! Copy data entry-nya buat dibawa pulang
        // (Pastikan lu punya fungsi cmemcpy ya)
        cmemcpy((unsigned char*)sd->out_entry, (unsigned char*)entry, sizeof(FAT32_DirEntry));
        return 1; // Stop iterasi!
    }
    return 0; // Lanjut cari!
}

int fat32_find_entry(unsigned long dir_cluster, const char *target_name, FAT32_DirEntry *out_entry)
{
    static struct FAT32_SearchData sd;
    sd.target_name = target_name;
    sd.out_entry = out_entry;

    // Kalau iterasi me-return 1, berarti file/folder ketemu
    if (fat32_iterate_directory(dir_cluster, internal_search_cb, (POINTER)&sd) == 1) 
    {
        return 0; // Berhasil
    }
    
    return -1; // Gagal / Not Found
}

unsigned long fat32_read_file(FAT32_DirEntry *file_entry, unsigned char *outbuffer)
{
    unsigned long current_cluster;
    unsigned long bytes_read = 0;
    unsigned long file_size = file_entry->size;
    unsigned long lba_to_read;
    int s;
    unsigned char sector_buffer[512];

    current_cluster = (unsigned long)MERGE_CLUSTER_HI_N_LO(file_entry->cluster_high, file_entry->cluster_low);

    if (file_size == 0 || current_cluster == 0) 
    {
        return 0;
    }

    while(current_cluster < 0x0FFFFFF8 && current_cluster != 0)
    {
        unsigned long base_lba = FAT32_CLUSTER_TO_LBA(current_cluster);

        for(s = 0; s < BiosBlock.sectors_per_cluster; s++)
        {
            unsigned long bytes_to_copy = 512;

            if(bytes_read >= file_size)
            {
                break;
            }

            lba_to_read = base_lba + s;
            cmemset(sector_buffer, 0, 512);
            LBA_READ_TO_BUFFER(lba_to_read, sector_buffer, 512);

            if ((file_size - bytes_read) < 512) 
            {
                bytes_to_copy = file_size - bytes_read;
            }

            cmemcpy(outbuffer + bytes_read, sector_buffer, bytes_to_copy);
            bytes_read += bytes_to_copy;
        }

        if (bytes_read >= file_size) 
        {
            break;
        }

        current_cluster = fat32_get_next_cluster(current_cluster);
    }

    return bytes_read;
}