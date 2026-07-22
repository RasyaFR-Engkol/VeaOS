#pragma once

#include "procbind.h"

static inline VOID outb(USHORT port, UCHAR data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

static inline UCHAR inb(USHORT port) {
    UCHAR ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}