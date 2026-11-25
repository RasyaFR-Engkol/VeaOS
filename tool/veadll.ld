SECTIONS
{
    .rsrc : ALIGN(16)
    {
        PROVIDE(VeaResourceStart = .);
        KEEP(*(.rsrc))
        . = ALIGN(16);
        PROVIDE(VeaResourceEnd = .);
    }
}
INSERT AFTER .rodata;