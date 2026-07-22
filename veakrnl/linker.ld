ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    /* VMA (Virtual Memory Address) ditaruh di 3GB + 1MB */
    . = 0xC0100000;

    .text ALIGN(4K) :
    {
        *(.text)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata)
    }

    .data ALIGN(4K) :
    {
        *(.data)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
        
        /* Label penanda akhir dari kernel lu di memori */
        _veaos_end = .; 
    }
}