/* SO Linker Module */

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)

SECTIONS
{
    . = 0x00000000 + SIZEOF_HEADERS;

    /* Tabel dynamic linking & relokasi */
    .hash           : { *(.hash) }
    .gnu.hash       : { *(.gnu.hash) }
    .dynsym         : { *(.dynsym) }
    .dynstr         : { *(.dynstr) }
    .rel.dyn        : { *(.rel.dyn) }
    .rel.plt        : { *(.rel.plt) }

    /* Kode executable (RX) */
    . = ALIGN(0x1000);
    .text           :
    {
        *(.text .text.*)
        *(.plt)
    }

    /* Data konstan & Resource tersemat (R) */
    . = ALIGN(0x1000);
    .rodata         :
    {
        *(.rodata .rodata.*)
    }

    . = ALIGN(16);
    .rsrc           :
    {
        VeaResourceStart = .;
        KEEP(*(.rsrc.header))
        KEEP(*(.rsrc.entries))
        KEEP(*(.rsrc.data))
        KEEP(*(.rsrc .rsrc.*))
        . = ALIGN(16);
        VeaResourceEnd = .;
    }

    /* Tabel dinamik & GOT/Data writable (RW) */
    . = ALIGN(0x1000);
    .dynamic        : { *(.dynamic) }
    .got            : { *(.got) }
    .got.plt        : { *(.got.plt) }

    .data           :
    {
        *(.data .data.*)
    }

    .bss            :
    {
        *(.bss .bss.*)
        *(COMMON)
    }

    /DISCARD/       :
    {
        *(.comment)
        *(.note.*)
    }
}

/* EOF */