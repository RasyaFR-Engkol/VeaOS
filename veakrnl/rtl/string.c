#include <veakrnl.h>
#include <stdarg.h>

static
VOID
VEAPI
RtlIntegerToString(
    ULONG Value,
    INT Base,
    BOOLEAN IsUpper,
    PCHAR OutBuf
)
{
    CHAR Temp[36];
    ULONG i = 0;
    const PCHAR Chars = IsUpper ? "0123456789ABCDEF" : "0123456789abcdef";

    if(Value == 0)
    {
        OutBuf[0] = '0';
        OutBuf[1] = '\0';
        return;
    }

    while(Value > 0)
    {
        Temp[i++] = Chars[Value % Base];
        Value /= Base;
    }

    ULONG j = 0;
    while(i > 0)
    {
        OutBuf[j++] = Temp[--i];
    }
    OutBuf[j] = '\0';
}

ULONG
RtlStringCbPrintfAImpl(
    char *Dest, unsigned int cbDest, const char* Format, va_list Args
)
{
    ULONG Written = 0;
    const char* p = Format;

    while(*p != '\0' && Written < cbDest - 1)
    {
        if (*p != '%') 
        {
            Dest[Written++] = *p++;
            continue;
        }

        p++;

        BOOLEAN PadZero = 0;
        BOOLEAN LeftAlign = 0;

        while (*p == '0' || *p == '-') {
            if (*p == '0') PadZero = 1;
            if (*p == '-') LeftAlign = 1;
            p++;
        }

        if (LeftAlign) PadZero = 0;

        INT Width = 0;
        while (*p >= '0' && *p <= '9') {
            Width = Width * 10 + (*p - '0');
            p++;
        }

        BOOLEAN IsLong = 0;
        BOOLEAN IsWide = 0;

        if (*p == 'l') {
            IsLong = 1;
            p++;
        } else if (*p == 'w') {
            IsWide = 1;
            p++;
        }

        switch(*p)
        {
            case 'c':
            {
                char c = (char)va_arg(Args, int);
                int pad = Width - 1;

                // Pad spasi di kiri (Right Align)
                if (!LeftAlign) {
                    while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = ' ';
                }
                
                if (Written < cbDest - 1) Dest[Written++] = c;

                // Pad spasi di kanan (Left Align)
                if (LeftAlign) {
                    while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = ' ';
                }
                break;
            }

            case 's': 
            {
                char *s = va_arg(Args, char *);
                if (!s) s = "(null)";

                // Hitung panjang string
                int len = 0;
                char *tmp = s;
                while (*tmp++) len++;

                int pad = Width - len;

                // Pad spasi di kiri (Right Align)
                if (!LeftAlign) {
                    while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = ' ';
                }

                // Tulis isi string
                while (*s && Written < cbDest - 1) {
                    Dest[Written++] = *s++;
                }

                // Pad spasi di kanan (Left Align)
                if (LeftAlign) {
                    while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = ' ';
                }
                break;
            }

            case 'd':
            case 'u':
            case 'x':
            case 'X':
            case 'p':
            {
                char num_buf[36];
                char prefix[3];
                RtlZeroMemory(prefix, 3);
                int prefix_len = 0;

                if (*p == 'd') {
                    int val = va_arg(Args, int);
                    unsigned long uval;
                    if (val < 0) {
                        prefix[0] = '-';
                        prefix_len = 1;
                        uval = (unsigned long)(0 - val); 
                    } else {
                        uval = (unsigned long)val;
                    }
                    RtlIntegerToString(uval, 10, 0, num_buf);
                } 
                else if (*p == 'u') {
                    unsigned int val = va_arg(Args, unsigned int);
                    RtlIntegerToString((unsigned long)val, 10, 0, num_buf);
                } 
                else { // 'x', 'X', 'p'
                    unsigned long val = va_arg(Args, unsigned long);
                    if (*p == 'p') {
                        prefix[0] = '0';
                        prefix[1] = 'x';
                        prefix_len = 2;
                    }
                    int is_upper = (*p == 'X' || *p == 'p') ? 1 : 0;
                    RtlIntegerToString(val, 16, is_upper, num_buf);
                }

                // Hitung panjang digit
                int digit_len = 0;
                char *tmp = num_buf;
                while (*tmp++) digit_len++;

                int total_len = prefix_len + digit_len;
                int pad = Width - total_len;

                // A. Spasi sebelum prefix (jika Rata Kanan & Bukan Zero-Padding)
                if (!LeftAlign && !PadZero) {
                    while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = ' ';
                }

                // B. Cetak Prefix ('-' atau "0x")
                for (int i = 0; i < prefix_len && Written < cbDest - 1; i++) {
                    Dest[Written++] = prefix[i];
                }

                // C. Nol sebelum digit (jika Rata Kanan & Ada Zero-Padding %04d)
                if (!LeftAlign && PadZero) {
                    while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = '0';
                }

                // D. Cetak Digit Angka
                char *s = num_buf;
                while (*s && Written < cbDest - 1) {
                    Dest[Written++] = *s++;
                }

                // E. Spasi setelah digit (jika Rata Kiri %-10d)
                if (LeftAlign) {
                    while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = ' ';
                }
                break;
            }

            case 'Z':
            {
                unsigned int chars = 0;

                if (IsWide) {
                    PUNICODE_STRING us = va_arg(Args, PUNICODE_STRING);
                    if (us && us->Buffer) {
                        chars = us->Length / sizeof(unsigned short);
                        int pad = Width - (int)chars;

                        if (!LeftAlign) {
                            while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = ' ';
                        }
                        for (unsigned int i = 0; i < chars && Written < cbDest - 1; i++) {
                            Dest[Written++] = (char)(us->Buffer[i] & 0xFF); 
                        }
                        if (LeftAlign) {
                            while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = ' ';
                        }
                    }
                } else {
                    PANSI_STRING as = va_arg(Args, PANSI_STRING);
                    if (as && as->Buffer) {
                        chars = as->Length;
                        int pad = Width - (int)chars;

                        if (!LeftAlign) {
                            while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = ' ';
                        }
                        for (unsigned int i = 0; i < chars && Written < cbDest - 1; i++) {
                            Dest[Written++] = as->Buffer[i];
                        }
                        if (LeftAlign) {
                            while (pad-- > 0 && Written < cbDest - 1) Dest[Written++] = ' ';
                        }
                    }
                }
                break;
            }

            case '%': {
                Dest[Written++] = '%';
                break;
            }

            default: {
                Dest[Written++] = '%';
                if (Written < cbDest - 1) {
                    Dest[Written++] = *p;
                }
                break;
            }
        }
        p++;
    }

    Dest[Written] = '\0';

    return Written;
}

ULONG
RtlStringCbPrintfA(
    char *Dest, unsigned int cbDest, const char* Format, ...
)
{
    va_list args;
    va_start(args, Format);
    ULONG Written = RtlStringCbPrintfAImpl(Dest, cbDest, Format, args);
    va_end(args);

    return Written;
}

VOID 
VEAPI
RtlInitAnsiString(
    PANSI_STRING DestinationString,
    const PCHAR SourceString
)
{
    if(SourceString != NULL)
    {
        USHORT Length = 0;

        while(SourceString[Length] != '\0')
        {
            Length++;
        }

        DestinationString->Length = Length;
        DestinationString->MaximumLength = Length + 1;
        DestinationString->Buffer = (PCHAR)SourceString;
    }
    else
    {
        DestinationString->Length = 0;
        DestinationString->MaximumLength = 0;
        DestinationString->Buffer = NULL;
    }
}

VOID
VEAPI
RtlInitUnicodeString(
    PUNICODE_STRING DestinationString,
    const PWCHAR SourceString
)
{
    if (SourceString != NULL) 
    {
        unsigned short lengthInChars = 0;
        
        // Hitung jumlah karakter (mirip wcslen)
        while (SourceString[lengthInChars] != '\0') {
            lengthInChars++;
        }
        
        // Length dihitung dalam satuan BYTE (jumlah karakter * 2 byte)
        DestinationString->Length = lengthInChars * sizeof(unsigned short);
        
        // MaximumLength juga dalam BYTE, ditambah ukuran Null-terminator (2 byte)
        DestinationString->MaximumLength = (lengthInChars + 1) * sizeof(unsigned short);
        
        DestinationString->Buffer = (unsigned short *)SourceString;
    } 
    else 
    {
        DestinationString->Length = 0;
        DestinationString->MaximumLength = 0;
        DestinationString->Buffer = NULL;
    }
}

static 
CHAR 
VEAPI
RtlToUpperChar(CHAR c)
{
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

static 
WCHAR
VEAPI
RtlToUpperWChar(WCHAR c)
{
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

VOID
VEAPI 
RtlCopyString(PANSI_STRING DestinationString, 
              const ANSI_STRING *SourceString) 
{
    if (!DestinationString) return;
    
    if (!SourceString) {
        DestinationString->Length = 0;
        return;
    }

    unsigned short copyLength = SourceString->Length;
    
    // Cegah overflow: potong jika source lebih besar dari kapasitas destinasi
    if (copyLength > DestinationString->MaximumLength) {
        copyLength = DestinationString->MaximumLength;
    }

    for (unsigned short i = 0; i < copyLength; i++) {
        DestinationString->Buffer[i] = SourceString->Buffer[i];
    }

    DestinationString->Length = copyLength;

    // Pasang Null-terminator jika masih ada sisa ruang di buffer
    if (copyLength < DestinationString->MaximumLength) {
        DestinationString->Buffer[copyLength] = '\0';
    }
}

VOID
VEAPI
RtlCopyUnicodeString(PUNICODE_STRING DestinationString, 
                     const UNICODE_STRING *SourceString) 
{
    if (!DestinationString) return;
    
    if (!SourceString) {
        DestinationString->Length = 0;
        return;
    }

    unsigned short copyBytes = SourceString->Length;
    
    // Cegah overflow
    if (copyBytes > DestinationString->MaximumLength) {
        copyBytes = DestinationString->MaximumLength;
        // Pastikan kita gak motong setengah karakter (harus genap/kelipatan 2)
        copyBytes &= ~1; 
    }

    unsigned short charsToCopy = copyBytes / sizeof(unsigned short);
    
    for (unsigned short i = 0; i < charsToCopy; i++) {
        DestinationString->Buffer[i] = SourceString->Buffer[i];
    }

    DestinationString->Length = copyBytes;

    // Pasang Null-terminator (2 byte) jika masih ada sisa ruang
    if (copyBytes + sizeof(unsigned short) <= DestinationString->MaximumLength) {
        DestinationString->Buffer[charsToCopy] = 0;
    }
}

BOOLEAN 
VEAPI
RtlEqualString(const ANSI_STRING *String1, 
               const ANSI_STRING *String2, 
               BOOLEAN CaseInSensitive) 
{
    if (String1->Length != String2->Length) {
        return FALSE;
    }

    for (unsigned short i = 0; i < String1->Length; i++) {
        char c1 = String1->Buffer[i];
        char c2 = String2->Buffer[i];

        if (CaseInSensitive) {
            c1 = RtlToUpperChar(c1);
            c2 = RtlToUpperChar(c2);
        }

        if (c1 != c2) {
            return FALSE;
        }
    }

    return TRUE;
}

BOOLEAN 
VEAPI
RtlEqualUnicodeString(const UNICODE_STRING *String1, 
                      const UNICODE_STRING *String2, 
                      BOOLEAN CaseInSensitive) 
{
    if (String1->Length != String2->Length) {
        return FALSE;
    }

    unsigned short charsToCompare = String1->Length / sizeof(unsigned short);

    for (unsigned short i = 0; i < charsToCompare; i++) {
        unsigned short c1 = String1->Buffer[i];
        unsigned short c2 = String2->Buffer[i];

        if (CaseInSensitive) {
            c1 = RtlToUpperWChar(c1);
            c2 = RtlToUpperWChar(c2);
        }

        if (c1 != c2) {
            return FALSE;
        }
    }

    return TRUE;
}

PCHAR RtlFindSubstringAscii(const ANSI_STRING *String, 
                            const ANSI_STRING *SubString, 
                            BOOLEAN CaseInSensitive)
{
    if (SubString->Length == 0) {
        return String->Buffer;
    }

    if (SubString->Length > String->Length) {
        return NULL;
    }

    unsigned short searchLimit = String->Length - SubString->Length;

    for (unsigned short i = 0; i <= searchLimit; i++) {
        BOOLEAN match = TRUE;
        
        for (unsigned short j = 0; j < SubString->Length; j++) {
            char c1 = String->Buffer[i + j];
            char c2 = SubString->Buffer[j];

            if (CaseInSensitive) {
                c1 = RtlToUpperChar(c1);
                c2 = RtlToUpperChar(c2);
            }

            if (c1 != c2) {
                match = FALSE;
                break;
            }
        }

        if (match) {
            return &String->Buffer[i]; // Berhasil ketemu! Kembalikan alamat pointer-nya
        }
    }

    return NULL; // Gak ketemu sama sekali
}

BOOLEAN
VEAPI
RtlAnsiStringToUnicodeString(
    PUNICODE_STRING DestinationString,
    const ANSI_STRING *SourceString,
    BOOLEAN AllocateDestinationString
) {
    if (!DestinationString || !SourceString) return FALSE;

    // Hitung kebutuhan memori: 1 karakter ANSI = 2 byte Unicode
    unsigned short reqBytes = SourceString->Length * sizeof(unsigned short);
    
    // MaximumLength harus ditambah ukuran Null-terminator (2 byte)
    unsigned short maxBytes = reqBytes + sizeof(unsigned short);

    if (AllocateDestinationString) {
        // Panggil allocator lu (Asumsi: PoolType 0 adalah NonPagedPool)
        DestinationString->Buffer = (unsigned short *)UlAllocatePoolWithTag(0, maxBytes, TAG_RTL_STRING);
        
        if (!DestinationString->Buffer) {
            return FALSE; // Gagal alokasi (Out of Memory)
        }
        DestinationString->MaximumLength = maxBytes;
    } else {
        // Kalau pakai buffer sendiri, pastikan ukurannya muat!
        if (DestinationString->MaximumLength < maxBytes) {
            return FALSE; 
        }
    }

    DestinationString->Length = reqBytes;
    unsigned short charCount = SourceString->Length;

    for (unsigned short i = 0; i < charCount; i++) {
        // Casting dari char ke unsigned short
        DestinationString->Buffer[i] = (unsigned short)(unsigned char)SourceString->Buffer[i];
    }

    // Pasang Null-terminator Unicode
    DestinationString->Buffer[charCount] = 0;

    return TRUE;
}

BOOLEAN
VEAPI
RtlUnicodeStringToAnsiString(
    PANSI_STRING DestinationString,
    const UNICODE_STRING *SourceString,
    BOOLEAN AllocateDestinationString
)
{
    if (!DestinationString || !SourceString) return FALSE;

    // Hitung kebutuhan: 1 karakter Unicode dipangkas jadi 1 byte ANSI
    unsigned short reqBytes = SourceString->Length / sizeof(unsigned short);
    
    // MaximumLength ditambah 1 byte buat Null-terminator
    unsigned short maxBytes = reqBytes + 1;

    if (AllocateDestinationString) {
        DestinationString->Buffer = (char *)UlAllocatePoolWithTag(0, maxBytes, TAG_RTL_STRING);
        
        if (!DestinationString->Buffer) {
            return FALSE; // Out of Memory
        }
        DestinationString->MaximumLength = maxBytes;
    } else {
        if (DestinationString->MaximumLength < maxBytes) {
            return FALSE;
        }
    }

    DestinationString->Length = reqBytes;

    for (unsigned short i = 0; i < reqBytes; i++) {
        // Down-casting secara naif (membuang byte tinggi pada UTF-16)
        DestinationString->Buffer[i] = (char)(SourceString->Buffer[i] & 0xFF);
    }

    // Pasang Null-terminator ANSI
    DestinationString->Buffer[reqBytes] = '\0';

    return TRUE;
}

VOID
VEAPI
RtlFreeUnicodeString(PUNICODE_STRING UnicodeString) 
{
    if (UnicodeString && UnicodeString->Buffer) {
        // Pastikan nama fungsi dealokasi lu bener ya, ganti kalau beda
        UlFreePoolWithTag(UnicodeString->Buffer, TAG_RTL_STRING);
        
        UnicodeString->Buffer = NULL;
        UnicodeString->Length = 0;
        UnicodeString->MaximumLength = 0;
    }
}

VOID
VEAPI 
RtlFreeAnsiString(PANSI_STRING AnsiString) 
{
    if (AnsiString && AnsiString->Buffer) {
        UlFreePoolWithTag(AnsiString->Buffer, TAG_RTL_STRING);
        
        AnsiString->Buffer = NULL;
        AnsiString->Length = 0;
        AnsiString->MaximumLength = 0;
    }
}

LONG RtlRawStringLength(const char *string)
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

PCHAR RtlCopyRawString(char *dst, const char *src)
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