#include <osi386.h>
#include <fat32intrnl.h>

extern unsigned long root_cluster;

int FsLookupFileInformation(
    const char* FullPath,
    PVFS_FILE_INFO OutFileInfo
)
{
    unsigned long current_cluster = root_cluster;
    FAT32_DirEntry current_entry;
    int status;
    const char *p = FullPath;
    char node_name[256];
    int i;

    // Save FULL PATH of Lookup for logging
    cstrcpy(LdrLastCheckedFile, FullPath);

    while (*p == '/' || *p == '\\') p++;

    while(*p != '\0')
    {
        i = 0;
        while (*p != '\0' && *p != '/' && *p != '\\')
        {
            if (i < 255) { // Cegah buffer overflow
                node_name[i++] = *p;
            }
            p++;
        }
        node_name[i] = '\0';

        while (*p == '/' || *p == '\\') p++;

        status = fat32_find_entry(current_cluster, node_name, &current_entry);

        if (status != 0) {
            return -1; // File atau folder di path ini nggak ketemu
        }

        if (*p != '\0') 
        {
            // Kalau masih ada sisa, berarti node_name HARUS sebuah direktori
            if (!(current_entry.attributes & FAT32_DIR_SUBDIRECTORY)) {
                return -1; // Error: Path mengharapkan folder, tapi nemunya file
            }

            // Update current_cluster buat iterasi berikutnya
            current_cluster = MERGE_CLUSTER_HI_N_LO(current_entry.cluster_high, current_entry.cluster_low);
            
            // QUIRK FAT32: Cluster 0 di subdirectory kadang merujuk ke Root Cluster
            if (current_cluster == 0) {
                current_cluster = root_cluster;
            }
        }
    }

    OutFileInfo->FileSize = current_entry.size;
    OutFileInfo->Attributes = 0;

    if (current_entry.attributes & FAT32_DIR_RO) OutFileInfo->Attributes |= FS_ATTR_READONLY;
    if (current_entry.attributes & FAT32_DIR_SUBDIRECTORY) OutFileInfo->Attributes |= FS_ATTR_DIRECTORY;

    cmemcpy(OutFileInfo->FsInternal, (unsigned char*)&current_entry, sizeof(FAT32_DirEntry));
    
    return 0;
}

unsigned long FsReadFile(PVFS_FILE_INFO FileInfo, void *Buffer)
{
    if (!FileInfo || !Buffer) return 0;

    // 1. Ekstrak kembali FAT32_DirEntry dari FsInternal
    FAT32_DirEntry *raw_entry = (FAT32_DirEntry *)FileInfo->FsInternal;

    // 2. Lempar ke driver FAT32
    return fat32_read_file(raw_entry, (unsigned char *)Buffer);
}

VOID
FsInitialize(VOID)
{
    mbr_detect();

    if(fat32_init() < 0)
    {
        LdrError(STATUS_FILE_SYSTEM_CORRUPT);
        while(1) asm("hlt");
    }
}