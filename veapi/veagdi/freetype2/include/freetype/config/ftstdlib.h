#ifndef FTSTDLIB_H_
#define FTSTDLIB_H_

#include <veapi.h>
#include <stddef.h>
#include <limits.h>

/* 1. Integer Limits */
#define FT_CHAR_BIT    CHAR_BIT
#define FT_USHORT_MAX  USHRT_MAX
#define FT_INT_MAX     INT_MAX
#define FT_INT_MIN     INT_MIN
#define FT_UINT_MAX    UINT_MAX
#define FT_LONG_MIN    LONG_MIN
#define FT_LONG_MAX    LONG_MAX
#define FT_ULONG_MAX   ULONG_MAX

#define ft_ptrdiff_t   ptrdiff_t

/* 2. Memory Operations -> VeaOS Rtl* */
#define ft_memcpy(dst, src, len)   RtlCopyMemory((dst), (src), (len))
#define ft_memmove(dst, src, len)  RtlMoveMemory((dst), (src), (len))
#define ft_memset(dst, val, len)   RtlFillMemory((dst), (len), (UCHAR)(val))

static inline int ft_memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n--)
    {
        if (*p1 != *p2) return (int)*p1 - (int)*p2;
        p1++;
        p2++;
    }
    return 0;
}

static inline void *ft_memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;
    unsigned char uc = (unsigned char)c;
    while (n--)
    {
        if (*p == uc) return (void *)p;
        p++;
    }
    return NULL;
}

/* 3. String Operations */
static inline size_t ft_strlen(const char *s)
{
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

static inline int ft_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

static inline int ft_strncmp(const char *s1, const char *s2, size_t n)
{
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

static inline char *ft_strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++) != '\0') {}
    return ret;
}

static inline char *ft_strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

static inline char *ft_strcat(char *dst, const char *src)
{
    char *ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++) != '\0') {}
    return ret;
}

static inline char *ft_strrchr(const char *s, int c)
{
    const char *last = NULL;
    char ch = (char)c;
    do {
        if (*s == ch) last = s;
    } while (*s++);
    return (char *)last;
}

static inline char *ft_strstr(const char *haystack, const char *needle)
{
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++)
    {
        const char *h = haystack, *n = needle;
        while (*h && *n && (*h == *n)) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

/* 4. Number Parsing & Sorting */
static inline long ft_strtol(const char *nptr, char **endptr, int base)
{
    const char *s = nptr;
    long acc = 0;
    int neg = 0, c;

    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        s += 2;
        base = 16;
    }
    if (base == 0) base = (*s == '0') ? 8 : 10;

    while ((c = (unsigned char)*s) != '\0')
    {
        if (c >= '0' && c <= '9') c -= '0';
        else if (c >= 'a' && c <= 'z') c = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') c = c - 'A' + 10;
        else break;

        if (c >= base) break;
        acc = acc * base + c;
        s++;
    }

    if (endptr) *endptr = (char *)(acc || s != nptr ? s : nptr);
    return neg ? -acc : acc;
}

#define ft_atol(s)  ft_strtol((s), NULL, 10)

static inline void ft_qsort(void *base, size_t nmemb, size_t size,
                            int (*compar)(const void *, const void *))
{
    unsigned char *arr = (unsigned char *)base;
    for (size_t gap = nmemb / 2; gap > 0; gap /= 2)
    {
        for (size_t i = gap; i < nmemb; i++)
        {
            for (size_t j = i; j >= gap; j -= gap)
            {
                unsigned char *a = arr + (j - gap) * size;
                unsigned char *b = arr + j * size;
                if (compar(a, b) <= 0) break;
                for (size_t k = 0; k < size; k++)
                {
                    unsigned char tmp = a[k];
                    a[k] = b[k];
                    b[k] = tmp;
                }
            }
        }
    }
}

/* 5. setjmp / longjmp x86 32-bit (Wajib buat smooth rasterizer ftgrays.c) */
typedef unsigned long ft_jmp_buf[6]; /* ebx, esi, edi, ebp, esp, eip */

__attribute__((returns_twice))
static inline int ft_setjmp(ft_jmp_buf env)
{
    int ret;
    __asm__ __volatile__(
        "movl %%ebx, 0(%1)\n\t"
        "movl %%esi, 4(%1)\n\t"
        "movl %%edi, 8(%1)\n\t"
        "movl %%ebp, 12(%1)\n\t"
        "leal 4(%%esp), %%ecx\n\t"
        "movl %%ecx, 16(%1)\n\t"
        "movl $1f, 20(%1)\n\t"
        "xorl %0, %0\n\t"
        "1:\n\t"
        : "=&a"(ret)
        : "d"(env)
        : "ecx", "memory"
    );
    return ret;
}

__attribute__((noreturn))
static inline void ft_longjmp(ft_jmp_buf env, int val)
{
    if (val == 0) val = 1;
    __asm__ __volatile__(
        "movl 0(%0), %%ebx\n\t"
        "movl 4(%0), %%esi\n\t"
        "movl 8(%0), %%edi\n\t"
        "movl 12(%0), %%ebp\n\t"
        "movl 16(%0), %%esp\n\t"
        "jmp *20(%0)\n\t"
        :
        : "d"(env), "a"(val)
        : "memory"
    );
    __builtin_unreachable();
}

/* 6. Stub sisa macro standar */
#define ft_scalloc(nmemb, size)  NULL
#define ft_sfree(ptr)            ((void)(ptr))
#define ft_smalloc(size)         NULL
#define ft_srealloc(ptr, size)   NULL
#define ft_sprintf               /* tidak dipakai di modul inti */
#define ft_exit(status)          for (;;) {}
#define ft_getenv(x)             ((const char *)NULL)

#endif /* FTSTDLIB_H_ */