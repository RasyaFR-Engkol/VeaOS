#include <osi386.h>

void cmemset(void *dst, char value, unsigned int n)
{
    char *p = (char*)dst;
    if(n == 0) return;

    while(n > 0)
    {
        *p = value;
        p++;
        n--;
    }
}

void* cmemcpy(void *dst, const void *src, int n)
{
    char *pdst = (char*)dst;
    const char *psrc = (const char*)src;

    if(n == 0) return dst;

    while(n > 0)
    {
        *pdst = *psrc;
        pdst++;
        psrc++;
        n--;
    }

    return dst;
}

int cmemcmp(const void *s1, const void *s2, unsigned int n)
{
    // FIX: Wajib cast ke unsigned char biar perbandingan byte 0x00-0xFF akurat
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    while (n > 0)
    {
        // FIX: Cek perbedaan di dalam loop saat pointer sedang pas menunjuk datanya
        if (*p1 != *p2)
        {
            return (int)(*p1 - *p2);
        }
        p1++;
        p2++;
        n--;
    }

    return 0; // FIX: Jika loop selesai dan tidak ada perbedaan, berarti cocok (return 0)
}

void* cmemmove(void *dst, const void *src, unsigned int n)
{
    char *pdst = (char *)dst;
    const char *psrc = (const char *)src;

    if (pdst == psrc || n == 0) return dst;

    // Jika alamat tujuan berada SEBELUM sumber, aman pakai copy maju
    if (pdst < psrc) {
        while (n > 0) {
            *pdst++ = *psrc++;
            n--;
        }
    } 
    // Jika alamat tujuan berada DI SETELAH sumber, wajib copy MUNDUR
    else {
        pdst += n;
        psrc += n;
        while (n > 0) {
            pdst--;
            psrc--;
            *pdst = *psrc;
            n--;
        }
    }

    return dst;
}

char* cstrcpy(char *dst, const char *src)
{
    char *p = dst;
    while (*src != '\0') {
        *p = *src;
        p++;
        src++;
    }
    *p = '\0'; // Jangan lupa tutup string-nya
    return dst;
}

char* cstrncpy(char *dst, const char *src, unsigned int n)
{
    char *p = dst;
    
    // Copy selama n belum habis dan src belum null
    while (n > 0 && *src != '\0') {
        *p++ = *src++;
        n--;
    }
    
    // Standar POSIX: Isi sisa buffer dengan null-terminator (padding)
    while (n > 0) {
        *p++ = '\0';
        n--;
    }
    
    return dst;
}

int cstrcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    // Return selisih karakter terakhir yang berbeda
    return (int)(*(const unsigned char *)s1 - *(const unsigned char *)s2);
}

int cstrncmp(const char *s1, const char *s2, unsigned int n)
{
    if (n == 0) return 0;

    // Loop selama n > 1 dan karakter masih sama serta belum null
    while (n > 1 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    
    return (int)(*(const unsigned char *)s1 - *(const unsigned char *)s2);
}

char* cstrcat(char *dst, const char *src)
{
    char *p = dst;

    // 1. Cari ujung string tujuan
    while (*p != '\0') {
        p++;
    }

    // 2. Tempelkan string sumber ke ujung string tujuan
    while (*src != '\0') {
        *p = *src;
        p++;
        src++;
    }

    *p = '\0'; // Tutup string hasil gabungan
    return dst;
}

char *cstrfwas(char* string)
{
    char *p = string;
    if(string == NULL) return NULL;

    while(*p != '\0')
    {
        if(*p == ' ')
        {
            p++;
            return p;
        }

        p++;
    }

    return NULL; // gada apa apa 
}

int cstrequal(const char *str1, const char *str2)
{
    while(*str1 && *str2)
    {
        if(*str1 != *str2) return 0;
        str1++; str2++;
    }
    return (*str1 == *str2);
}

const char* next_token(const char *src, char *dest_buf) {
    int i = 0;
    while (*src == ' ' || *src == ',' || *src == '\t') {
        src++; // Lewati spasi/koma di depan
    }
    if (*src == '\0' || *src == '\n' || *src == '\r') {
        dest_buf[0] = '\0';
        return src;
    }
    while (*src && *src != ' ' && *src != ',' && *src != '\t' && *src != '\n' && *src != '\r') {
        dest_buf[i++] = *src++;
    }
    dest_buf[i] = '\0';
    return src; // Kembalikan posisi pointer terakhir
}

int parse_decimal(const char *str) {
    int result = 0;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

int cstrlen(const char *string)
{
    const char *p = string;
    int count = 0;

    while(*p != '\0')
    {
        count++;
        p++;
    }

    return count;
}