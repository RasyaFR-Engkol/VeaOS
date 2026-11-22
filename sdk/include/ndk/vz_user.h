/* VeaOS SPDX License ----------------------------------------------------
 * Auto-generated Kernel Vz* Syscall Declarations
 * DO NOT EDIT MANUALLY
 * ----------------------------------------------------------------------- */

#ifndef _VZ_WRAPPERS_H
#define _VZ_WRAPPERS_H

/* kernel essential */
#include <procbind.h>
#include <ldrtypes.h>
#include <ul.h>
#include <kdp.h>
#include <mmtype.h>
#include <mm.h>
#include <rtl.h>
#include <ks.h>
#include <hct.h>
#include <obtype.h>
#include <obfunc.h>
#include <veastatus.h>
#include <tag.h>
#include <sftypes.h>
#include <sffunc.h>
#include <pttypes.h>
#include <ptfunc.h>
#include <vktypes.h>
#include <vkfunc.h>
#include <guid.h>
#include <potypes.h>
#include <pofunc.h>
#include <../psdk/veaseh.h>

#ifdef __cplusplus
extern "C" {
#endif

VEASTATUS VEAPI VzCreateFile(OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, OUT POIP_INFORMATION IoStatusBlock, IN PLARGE_INTEGER AllocationSize OPTIONAL, IN ULONG FileAttributes, IN ULONG ShareAccess, IN ULONG CreateDisposition, IN ULONG CreateOptions, IN PVOID EaBuffer OPTIONAL, IN ULONG EaLength);
VEASTATUS VEAPI VzOpenFile(OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, OUT POIP_INFORMATION IoStatusBlock, IN ULONG ShareAccess, IN ULONG OpenOptions);
VEASTATUS VEAPI VzLoadDriver(IN PANSI_STRING DriverServiceName);
VEASTATUS VEAPI VzCreateDirectoryNamespace(OUT PHANDLE Handle, IN ACCESS_MASK Mask, IN POBJECT_ATTRIBUTES ObjectAttributes);
VEASTATUS VEAPI VzClose(IN HANDLE Handle);
VEASTATUS VEAPI VzOpenVeaKey(OUT PHANDLE KeyHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes);
VEASTATUS VEAPI VzQueryKeyPath(IN HANDLE KeyHandle, OUT PANSI_STRING KeyPath);
VEASTATUS VEAPI VzQueryValueKey(IN HANDLE KeyHandle, IN PANSI_STRING ValueName, OUT PULONG Type, OUT PVOID Data, IN ULONG DataSize, OUT PULONG ResultLength);
VEASTATUS VEAPI VzSetValueKey(IN HANDLE KeyHandle, IN PANSI_STRING ValueName, IN ULONG TitleIndex, IN ULONG Type, IN PVOID Data, IN ULONG DataSize);
VEASTATUS VEAPI VzAllocateVirtualMemory(IN HANDLE ProcessHandle, IN OUT PVOID *UBaseAddress, IN ULONG_PTR ZeroBits, IN OUT PSIZE_T URegionSize, IN ULONG AllocationType, IN ULONG Protect);
VEASTATUS VEAPI VzReadVirtualMemory(IN HANDLE ProcessHandle, IN PVOID BaseAddress, OUT PVOID Buffer, IN SIZE_T NumberOfBytesToRead, OUT PSIZE_T NumberOfBytesRead OPTIONAL);
VEASTATUS VEAPI VzWriteVirtualMemory(IN HANDLE ProcessHandle, IN PVOID BaseAddress, IN PVOID Buffer, IN SIZE_T NumberOfBytesToWrite, OUT PSIZE_T NumberOfBytesWritten OPTIONAL);
VEASTATUS VEAPI VzFreeVirtualMemory(IN HANDLE ProcessHandle, IN OUT PVOID *UBaseAddress, IN OUT PSIZE_T URegionSize, IN ULONG FreeType);
VEASTATUS VEAPI VzQuerySection(IN HANDLE SectionHandle, IN SECTION_INFORMATION_CLASS SectionInformationClass, OUT PVOID SectionInformation, IN SIZE_T SectionInformationLength, OUT PSIZE_T ResultLength OPTIONAL);
VEASTATUS VEAPI VzMapViewOfSection(IN HANDLE SectionHandle, IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress, IN ULONG_PTR ZeroBits, IN SIZE_T CommitSize, IN OUT PLARGE_INTEGER SectionOffset OPTIONAL, IN OUT PSIZE_T ViewSize, IN SECTION_INHERIT InheritDisposition, IN ULONG AllocationType, IN ULONG Win32Protect);
VEASTATUS VEAPI VzUnmapViewOfSection(IN HANDLE ProcessHandle, IN PVOID BaseAddress);
VEASTATUS VEAPI VzReadFile(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL, OUT POIP_INFORMATION IoStatusBlock, OUT PVOID Buffer, IN ULONG Length, IN PLARGE_INTEGER ByteOffset OPTIONAL, IN PULONG Key OPTIONAL);
#ifdef __cplusplus
}
#endif

#endif /* _VZ_WRAPPERS_H */
