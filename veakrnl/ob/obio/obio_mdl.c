/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_mdl.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Obio MDL
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

#define OBIO_MDL_TAG ' LDV'

static BOOLEAN
ObioGetPageFrame(
    IN ULONG VirtualAddress,
    OUT PFN_NUMBER *PageFrame
)
{
    PAGE_ENTRY *Pde = MM_GET_PDE(VirtualAddress);

    if (!Pde->bits.present) {
        return FALSE;
    }

    if (Pde->bits.ps_or_pat) {
        *PageFrame = ((*(ULONG *)Pde & 0xFFC00000) |
                      (VirtualAddress & 0x003FF000)) >> PAGE_SHIFT;
        return TRUE;
    }

    PAGE_ENTRY *Pte = MM_GET_PTE(VirtualAddress);
    if (!Pte->bits.present) {
        return FALSE;
    }

    *PageFrame = Pte->bits.frame;
    return TRUE;
}

PMDL
VEAPI
ObioAllocateMdl(
    IN PVOID VirtualAddress,
    IN ULONG Length,
    IN BOOLEAN HaveSecondaryBuffer,
    IN BOOLEAN ChargeQuote,
    IN POIP Oip
)
{
    ULONG_PTR Address = (ULONG_PTR)VirtualAddress;
    ULONG PageCount;
    ULONG MdlSize;
    PMDL Mdl;

    (VOID)HaveSecondaryBuffer;
    (VOID)ChargeQuote;

    if (Length == 0 || VirtualAddress == NULL ||
        Address > 0xFFFFFFFFUL - ((ULONGLONG)Length - 1)) {
        return NULL;
    }

    if (Oip != NULL && Oip->MdlAddress != NULL) {
        return NULL;
    }

    PageCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, Length);
    if (PageCount == 0 ||
        PageCount > (0xFFFF - sizeof(MDL)) / sizeof(PFN_NUMBER)) {
        return NULL;
    }

    MdlSize = sizeof(MDL) + PageCount * sizeof(PFN_NUMBER);
    Mdl = (PMDL)UlAllocatePoolWithTag(NonPagedPool, MdlSize, OBIO_MDL_TAG);
    if (Mdl == NULL) {
        return NULL;
    }

    RtlZeroMemory(Mdl, MdlSize);
    Mdl->Size = (USHORT)MdlSize;
    Mdl->ByteOffset = (ULONG)(Address & (PAGE_SIZE - 1));
    Mdl->ByteCount = Length;
    Mdl->StartVa = PAGE_ALIGN(VirtualAddress);

    /* VeaOS currently has no page-locking primitive; record resident PFNs only. */
    ULONG PageAddress = (ULONG)(ULONG_PTR)Mdl->StartVa;
    for (ULONG Index = 0; Index < PageCount; Index++) {
        if (!ObioGetPageFrame(PageAddress, &Mdl->PfnArray[Index])) {
            UlFreePoolWithTag(Mdl, OBIO_MDL_TAG);
            return NULL;
        }

        PageAddress += PAGE_SIZE;
    }

    if (Oip != NULL) {
        Oip->MdlAddress = Mdl;
    }

    return Mdl;
}

VEASTATUS
VEAPI
ObioBuildPartialMdl(
    IN PMDL SourceMdl,
    IN PMDL TargetMdl,
    IN PVOID VirtualAddress,
    IN ULONG Length
)
{
    ULONGLONG SourceStart;
    ULONGLONG SourceEnd;
    ULONGLONG TargetStart = (ULONG_PTR)VirtualAddress;
    ULONGLONG TargetEnd;
    ULONG TargetPageCount;
    ULONG SourcePageCount;
    ULONG SourcePageIndex;
    ULONG RequiredSize;

    if (SourceMdl == NULL || TargetMdl == NULL || SourceMdl == TargetMdl ||
        VirtualAddress == NULL || Length == 0 ||
        TargetStart > 0xFFFFFFFFUL - ((ULONGLONG)Length - 1)) {
        return STATUS_INVALID_PARAMETER;
    }

    if (SourceMdl->Size < sizeof(MDL) ||
        SourceMdl->ByteOffset >= PAGE_SIZE ||
        SourceMdl->ByteCount == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    SourceStart = (ULONGLONG)(ULONG_PTR)SourceMdl->StartVa +
                  SourceMdl->ByteOffset;
    SourceEnd = SourceStart + SourceMdl->ByteCount;
    TargetEnd = TargetStart + Length;

    if (SourceEnd > 0x100000000ULL ||
        TargetEnd > 0x100000000ULL ||
        TargetStart < SourceStart || TargetEnd > SourceEnd) {
        return STATUS_INVALID_PARAMETER;
    }

    TargetPageCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, Length);
    RequiredSize = sizeof(MDL) + TargetPageCount * sizeof(PFN_NUMBER);
    if (TargetPageCount == 0 || RequiredSize > TargetMdl->Size) {
        return STATUS_INVALID_PARAMETER;
    }

    SourcePageCount = (SourceMdl->Size - sizeof(MDL)) / sizeof(PFN_NUMBER);
    SourcePageIndex = ((ULONG_PTR)PAGE_ALIGN(VirtualAddress) -
                       (ULONG_PTR)SourceMdl->StartVa) >> PAGE_SHIFT;
    if (SourcePageIndex > SourcePageCount ||
        TargetPageCount > SourcePageCount - SourcePageIndex) {
        return STATUS_INVALID_PARAMETER;
    }

    TargetMdl->Next = NULL;
    TargetMdl->MdlFlags = MDL_PARTIAL |
                          (SourceMdl->MdlFlags & MDL_SOURCE_IS_NONPAGED_POOL);
    TargetMdl->Process = SourceMdl->Process;
    TargetMdl->MappedSystemVa = NULL;
    TargetMdl->StartVa = PAGE_ALIGN(VirtualAddress);
    TargetMdl->ByteOffset = (ULONG)(TargetStart & (PAGE_SIZE - 1));
    TargetMdl->ByteCount = Length;

    for (ULONG Index = 0; Index < TargetPageCount; Index++) {
        TargetMdl->PfnArray[Index] =
            SourceMdl->PfnArray[SourcePageIndex + Index];
    }

    return STATUS_SUCCESS;
}

VOID
VEAPI
ObioFreeMdl(
    IN PMDL Mdl
)
{
    if (Mdl != NULL) {
        UlFreePoolWithTag(Mdl, OBIO_MDL_TAG);
    }
}
