#include <ft2build.h>
#include FT_CONFIG_CONFIG_H
#include FT_INTERNAL_DEBUG_H
#include FT_INTERNAL_STREAM_H
#include FT_SYSTEM_H
#include FT_ERRORS_H
#include FT_TYPES_H

/* Sesuaikan 2 macro ini dengan fungsi allocator pool di veakrnl/veapi kamu */
#define VEA_KERNEL_ALLOC(Size)   UlAllocatePoolWithTag(NonPagedPool, (Size), 'ft22')
#define VEA_KERNEL_FREE(Ptr)     UlFreePool((Ptr))

typedef struct _VEA_FT_ALLOC_HEADER {
    SIZE_T Size;
    SIZE_T Padding[3]; /* Jaga alignment 16-byte */
} VEA_FT_ALLOC_HEADER, *PVEA_FT_ALLOC_HEADER;

FT_CALLBACK_DEF(void *)
ft_alloc(FT_Memory memory, long size)
{
    FT_UNUSED(memory);
    if (size <= 0) return NULL;

    PVEA_FT_ALLOC_HEADER Hdr = (PVEA_FT_ALLOC_HEADER)VEA_KERNEL_ALLOC(
        sizeof(VEA_FT_ALLOC_HEADER) + (SIZE_T)size
    );
    if (!Hdr) return NULL;

    Hdr->Size = (SIZE_T)size;
    return (void *)(Hdr + 1);
}

FT_CALLBACK_DEF(void)
ft_free(FT_Memory memory, void *block)
{
    FT_UNUSED(memory);
    if (!block) return;

    PVEA_FT_ALLOC_HEADER Hdr = ((PVEA_FT_ALLOC_HEADER)block) - 1;
    VEA_KERNEL_FREE(Hdr);
}

FT_CALLBACK_DEF(void *)
ft_realloc(FT_Memory memory, long cur_size, long new_size, void *block)
{
    FT_UNUSED(cur_size);

    if (!block) return ft_alloc(memory, new_size);
    if (new_size <= 0)
    {
        ft_free(memory, block);
        return NULL;
    }

    PVEA_FT_ALLOC_HEADER OldHdr = ((PVEA_FT_ALLOC_HEADER)block) - 1;
    void *NewBlock = ft_alloc(memory, new_size);
    if (!NewBlock) return NULL;

    SIZE_T CopyBytes = (OldHdr->Size < (SIZE_T)new_size) ? OldHdr->Size : (SIZE_T)new_size;
    RtlCopyMemory(NewBlock, block, CopyBytes);
    ft_free(memory, block);
    return NewBlock;
}

/* Stream file tidak dipakai karena kita pakai FT_New_Memory_Face */
FT_BASE_DEF(FT_Error)
FT_Stream_Open(FT_Stream stream, const char *filepathname)
{
    FT_UNUSED(stream);
    FT_UNUSED(filepathname);
    return FT_THROW(Unimplemented_Feature);
}

FT_BASE_DEF(FT_Memory)
FT_New_Memory(void)
{
    FT_Memory memory = (FT_Memory)VEA_KERNEL_ALLOC(sizeof(*memory));
    if (memory)
    {
        memory->user    = NULL;
        memory->alloc   = ft_alloc;
        memory->realloc = ft_realloc;
        memory->free    = ft_free;
    }
    return memory;
}

FT_BASE_DEF(void)
FT_Done_Memory(FT_Memory memory)
{
    if (memory)
    {
        VEA_KERNEL_FREE(memory);
    }
}