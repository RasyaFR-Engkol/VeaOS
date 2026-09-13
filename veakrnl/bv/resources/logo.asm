[BITS 32]
section .rodata

global BvLogoBitmap
global BvLogoBitmapEnd

BvLogoBitmap:
    incbin "veakrnl/bv/resources/logoboot.bmp"
BvLogoBitmapEnd: