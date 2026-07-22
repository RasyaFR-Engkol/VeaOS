#pragma once

#include "procbind.h"

static inline unsigned long long HctReadMsr(unsigned int msr) {
    unsigned int lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static inline void HctWriteMsr(unsigned int msr, unsigned long long value) {
    unsigned int lo = (unsigned int)(value & 0xFFFFFFFF);
    unsigned int hi = (unsigned int)(value >> 32);
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}