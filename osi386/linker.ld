ENTRY(_start)
OUTPUT_FORMAT("binary")

SECTIONS
{
    /* Tentukan titik awal program (OSI386 ditaruh di sini oleh EBR) */
    . = 0x10000;

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
        *(.rodata*)
    }

    .bss : {
        *(.bss)
    }
}