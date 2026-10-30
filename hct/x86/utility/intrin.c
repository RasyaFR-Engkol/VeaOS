/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : intrin.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Intrin Function
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 26-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file intrin.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
HctMemoryBarrierWithoutFence(VOID)
{
    __asm__ __volatile__ ("" ::: "memory");
}

UCHAR
VEAPI
READ_PORT_UCHAR(
  IN volatile UCHAR *Port
)
{
  UCHAR Value;
  __asm__ __volatile__(
      "inb %%dx, %%al\n\t"
      : "=a"(Value)
      : "d"((USHORT)(ULONG_PTR)Port)
      : "memory"
  );
  return Value;
}

USHORT
VEAPI
READ_PORT_USHORT(
  IN volatile USHORT *Port
)
{
  USHORT Value;
  __asm__ __volatile__(
      "inw %%dx, %%ax\n\t"
      : "=a"(Value)
      : "d"((USHORT)(ULONG_PTR)Port)
      : "memory"
  );
  return Value;
}

ULONG
VEAPI
READ_PORT_ULONG(
  IN volatile ULONG *Port
)
{
  ULONG Value;
  __asm__ __volatile__(
      "inl %%dx, %%eax\n\t"
      : "=a"(Value)
      : "d"((USHORT)(ULONG_PTR)Port)
      : "memory"
  );
  return Value;
}

VOID
VEAPI
WRITE_PORT_UCHAR(
  IN volatile UCHAR *Port,
  IN UCHAR Value
)
{
  __asm__ __volatile__(
      "outb %%al, %%dx\n\t"
      :
      : "a"(Value), "d"((USHORT)(ULONG_PTR)Port)
      : "memory"
  );
}

VOID
VEAPI
WRITE_PORT_USHORT(
  IN volatile USHORT *Port,
  IN USHORT Value
)
{
  __asm__ __volatile__(
      "outw %%ax, %%dx\n\t"
      :
      : "a"(Value), "d"((USHORT)(ULONG_PTR)Port)
      : "memory"
  );
}

VOID
VEAPI
WRITE_PORT_ULONG(
  IN volatile ULONG *Port,
  IN ULONG Value
)
{
  __asm__ __volatile__(
      "outl %%eax, %%dx\n\t"
      :
      : "a"(Value), "d"((USHORT)(ULONG_PTR)Port)
      : "memory"
  );
}

ULONG
VEAPI
READ_REGISTER_ULONG(
  IN volatile ULONG *Register
)
{
  __asm__ __volatile__ ("" ::: "memory");
  ULONG Value = *Register;
  __asm__ __volatile__ ("" ::: "memory");
  return Value;
}

USHORT
VEAPI
READ_REGISTER_USHORT(
  IN volatile USHORT *Register
)
{
  __asm__ __volatile__ ("" ::: "memory");
  USHORT Value = *Register;
  __asm__ __volatile__ ("" ::: "memory");
  return Value;
}

UCHAR
VEAPI
READ_REGISTER_UCHAR(
  IN volatile UCHAR *Register
)
{
  __asm__ __volatile__ ("" ::: "memory");
  UCHAR Value = *Register;
  __asm__ __volatile__ ("" ::: "memory");
  return Value;
}

VOID
VEAPI
WRITE_REGISTER_ULONG(
  IN volatile ULONG *Register,
  IN ULONG Value
)
{
  __asm__ __volatile__ ("" ::: "memory");
  *Register = Value;
  __asm__ __volatile__ ("" ::: "memory");
}

VOID
VEAPI
WRITE_REGISTER_USHORT(
  IN volatile USHORT *Register,
  IN USHORT Value
)
{
  __asm__ __volatile__ ("" ::: "memory");
  *Register = Value;
  __asm__ __volatile__ ("" ::: "memory");
}

VOID
VEAPI
WRITE_REGISTER_UCHAR(
  IN volatile UCHAR *Register,
  IN UCHAR Value
)
{
  __asm__ __volatile__ ("" ::: "memory");
  *Register = Value;
  __asm__ __volatile__ ("" ::: "memory");
}
