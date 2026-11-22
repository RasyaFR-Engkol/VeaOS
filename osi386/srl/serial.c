#include <osi386.h>

#define DEFAULT_COM 0x3F8
#define SRL_BUFFER_SIZE 512

VOID
VEAPI
SrlInit(VOID)
{
    __Out8(DEFAULT_COM + 1, 0x00);
    __Out8(DEFAULT_COM + 3, 0x80);
    __Out8(DEFAULT_COM + 0, 0x03);
    __Out8(DEFAULT_COM + 1, 0x00);
    __Out8(DEFAULT_COM + 3, 0x03);
    __Out8(DEFAULT_COM + 2, 0xC7);
    __Out8(DEFAULT_COM + 4, 0x0B);

    __Out8(DEFAULT_COM + 4, 0x1E);
    __Out8(DEFAULT_COM + 0, 0xAE);
    if (__In8(DEFAULT_COM + 0) != 0xAE)
    {
        return;
    }

    __Out8(DEFAULT_COM + 4, 0x0F);
    return;
}

static
VOID
SrlpPutChar(IN CHAR Character)
{
    while ((__In8(DEFAULT_COM + 5) & 0x20) == 0)
    {
        __YieldProcessor();
    }

    __Out8(DEFAULT_COM + 0, (UCHAR)Character);
}

ULONG
SrlSendMsg(IN PCHAR Format, ...)
{
    CHAR Buffer[SRL_BUFFER_SIZE];
    va_list Args;
    ULONG Index;

    if (Format == NULL)
    {
        return 0;
    }

    va_start(Args, Format);
    RtlStringCbVPrintfA(Buffer, sizeof(Buffer), Format, Args);
    va_end(Args);

    for (Index = 0; Buffer[Index] != '\0'; Index++)
    {
        if (Buffer[Index] == '\n')
        {
            SrlpPutChar('\r');
        }

        SrlpPutChar(Buffer[Index]);
    }

    return Index;
}

/* EOF */