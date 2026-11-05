ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    /* VMA is at HH + 1MB */
    . = 0xC0100000;

    /* Text = Function of the code */
    .text ALIGN(4K) :
    {
        *(.text)
    }

    /* Rodata = Read Only Data */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
    }

    /* veasymboltable = Kernel exported symbol */
    .veasymboltable ALIGN(4K) :
    {
        __veasymboltable_start = .;
        *(.veasymboltable)
        __veasymboltable_end = .;
    }

    /* Make 2 Reserved for incoming feature */
    .reserved1 ALIGN(4K) :
    {
        __reserved1_start = .;
        *(.reserved1)
        __reserved1_end = .;
    }

    .reserved2 ALIGN(4K) :
    {
        __reserved2_start = .;
        *(.reserved2)
        __reserved2_end = .;
    }

    /* Data = put data here */
    .data ALIGN(4K) :
    {
        *(.data)
    }

    /* bss = where all replaces as 0 or none */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
        
        /* where VeaOS ends */
        _veaos_end = .; 
    }
}