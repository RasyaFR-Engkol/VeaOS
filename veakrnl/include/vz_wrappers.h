/* VeaOS SPDX License ----------------------------------------------------
- Auto-generated Kernel Vz* Syscall Declarations
- DO NOT EDIT MANUALLY
- ----------------------------------------------------------------------- */

#ifndef _VZ_WRAPPERS_H
#define _VZ_WRAPPERS_H

#include <veakrnl.h>

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
VEASTATUS VEAPI VzCreateSection(OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN PLARGE_INTEGER MaximumSize OPTIONAL, IN ULONG SectionPageProtection OPTIONAL, IN ULONG AllocationAttributes, IN HANDLE FileHandle OPTIONAL);
VEASTATUS VEAPI VzCreateProcess(OUT PHANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN HANDLE ParentProcess OPTIONAL, IN ULONG Flags, IN HANDLE SectionHandle OPTIONAL, IN HANDLE DebugPort OPTIONAL, IN HANDLE ExceptionPort OPTIONAL);
VEASTATUS VEAPI VzQueryInformationProcess(IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass, OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength, OUT PULONG ReturnLength OPTIONAL);
VEASTATUS VEAPI VzDuplicateObject(IN HANDLE SourceProcessHandle, IN HANDLE SourceHandle, IN HANDLE TargetProcessHandle OPTIONAL, OUT PHANDLE TargetHandle OPTIONAL, IN ACCESS_MASK DesiredAccess, IN ULONG HandleAttributes, IN ULONG Options);
VEASTATUS VEAPI VzQuerySystemInformation(IN SYSTEM_INFORMATION_CLASS SystemInformationClass, OUT PVOID SystemInformation, IN ULONG SystemInformationLength, OUT PULONG ReturnedLength);
VEASTATUS VEAPI VzCreateThread(OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN HANDLE ProcessHandle, OUT PCLIENT_ID ClientId OPTIONAL, IN PKTRAP_FRAME ThreadContext, IN PINITIAL_TEB InitialTeb, IN BOOLEAN CreateSuspended);
VEASTATUS VEAPI VzResumeThread(IN HANDLE ThreadHandle, OUT PULONG PreviousSuspendCount OPTIONAL);
VEASTATUS VEAPI VzCreatePort(OUT PHANDLE PortHandle, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN ULONG MaxConnectionInfoLength, IN ULONG MaxMessageLength, IN KPC_PORT_MODE Mode);
VEASTATUS VEAPI VzRequestWaitReplyPort(IN HANDLE PortHandle, IN PKPC_MESSAGE_HEADER RequestMessage, OUT PKPC_MESSAGE_HEADER ReplyMessage OPTIONAL);
VEASTATUS VEAPI VzReplyWaitReceivePort(IN HANDLE PortHandle, OUT PVOID Context OPTIONAL, IN PKPC_MESSAGE_HEADER ReplyMessage OPTIONAL, OUT PKPC_MESSAGE_HEADER ReceiveMessage);
VEASTATUS VEAPI VzConnectPort(OUT PHANDLE ClientPortHandle, IN PANSI_STRING PortName, IN PSECURITY_QUALITY_OF_SERVICE SecurityQos OPTIONAL, IN OUT PKPC_SECTION_WRITE WriteSection OPTIONAL, IN OUT PKPC_SECTION_READ ReadSection OPTIONAL, IN KPC_PORT_MODE RequestedMode, IN PVOID ConnectionInformation OPTIONAL, IN OUT PULONG ConnectionInformationLength OPTIONAL);
VEASTATUS VEAPI VzAcceptConnectPort(OUT PHANDLE ServerPortHandle, IN PVOID Context OPTIONAL, IN PKPC_MESSAGE_HEADER Message, IN BOOLEAN AcceptConnection, IN OUT PKPC_SECTION_WRITE WriteSection OPTIONAL, IN OUT PKPC_SECTION_READ ReadSection OPTIONAL);
VEASTATUS VEAPI VzWriteRequestData(IN HANDLE PortHandle, IN PKPC_MESSAGE_HEADER Message, IN ULONG DataOffset, IN PVOID Buffer, IN SIZE_T BufferSize, OUT PSIZE_T NumberOfBytesWritten OPTIONAL);
VEASTATUS VEAPI VzReadRequestData(IN HANDLE PortHandle, IN PKPC_MESSAGE_HEADER Message, IN ULONG DataOffset, OUT PVOID Buffer, IN SIZE_T BufferSize, OUT PSIZE_T NumberOfBytesRead OPTIONAL);
VEASTATUS VEAPI VzReadPort(IN HANDLE PortHandle, OUT PVOID Buffer, IN ULONG Length, OUT PULONG BytesRead);
VEASTATUS VEAPI VzContinue(IN PKTRAP_FRAME ContextFrame, IN BOOLEAN TestAlert);
VEASTATUS VEAPI VzTerminateProcess(IN HANDLE ProcessHandle, IN VEASTATUS ExitStatus);
VEASTATUS VEAPI VzKdPrint(IN PCHAR Message);
VEASTATUS VEAPI VzYieldExecution(VOID);
VEASTATUS VEAPI VzWriteFile(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL, OUT POIP_INFORMATION OipStatusBlock, IN PVOID Buffer, IN ULONG Length, IN PLARGE_INTEGER ByteOffset OPTIONAL, IN PULONG Key OPTIONAL);
VEASTATUS VEAPI VzWriteFileGather(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL, IN PVOID UserApcContext OPTIONAL, OUT POIP_INFORMATION IoStatusBlock, IN FILE_SEGMENT_ELEMENT BufferDescription[], IN ULONG BufferLength, IN PLARGE_INTEGER ByteOffset, IN PULONG Key OPTIONAL);
VEASTATUS VEAPI VzReadFileScatter(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL, IN PVOID UserApcContext OPTIONAL, OUT POIP_INFORMATION IoStatusBlock, IN FILE_SEGMENT_ELEMENT BufferDescription[], IN ULONG BufferLength, IN PLARGE_INTEGER ByteOffset, IN PULONG Key OPTIONAL);
VEASTATUS VEAPI VzDeviceIoControlFile(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL, IN PVOID UserApcContext OPTIONAL, OUT POIP_INFORMATION IoStatusBlock, IN ULONG IoControlCode, IN PVOID InputBuffer OPTIONAL, IN ULONG InputBufferLength, OUT PVOID OutputBuffer OPTIONAL, IN ULONG OutputBufferLength);
VEASTATUS VEAPI VzFsControlFile(IN HANDLE DeviceHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL, IN PVOID UserApcContext OPTIONAL, OUT POIP_INFORMATION IoStatusBlock, IN ULONG IoControlCode, IN PVOID InputBuffer, IN ULONG InputBufferLength OPTIONAL, OUT PVOID OutputBuffer, IN ULONG OutputBufferLength OPTIONAL);
VEASTATUS VEAPI VzDeleteFile(IN POBJECT_ATTRIBUTES ObjectAttributes);
VEASTATUS VEAPI VzFlushBuffersFile(IN HANDLE FileHandle, OUT POIP_INFORMATION IoStatusBlock);
VEASTATUS VEAPI VzQueryInformationFile(IN HANDLE FileHandle, OUT POIP_INFORMATION IoStatusBlock, OUT PVOID FileInformation, IN ULONG Length, IN FILE_INFORMATION_CLASS FileInformationClass);
VEASTATUS VEAPI VzSetInformationFile(IN HANDLE FileHandle, OUT POIP_INFORMATION IoStatusBlock, IN PVOID FileInformation, IN ULONG Length, IN FILE_INFORMATION_CLASS FileInformationClass);
VEASTATUS VEAPI VzQueryDirectoryFile(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL, OUT POIP_INFORMATION IoStatusBlock, OUT PVOID FileInformation, IN ULONG Length, IN FILE_INFORMATION_CLASS FileInformationClass, IN BOOLEAN ReturnSingleEntry, IN PANSI_STRING FileName OPTIONAL, IN BOOLEAN RestartScan);
VEASTATUS VEAPI VzQueryVolumeInformationFile(IN HANDLE FileHandle, OUT POIP_INFORMATION IoStatusBlock, OUT PVOID FsInformation, IN ULONG Length, IN FS_INFORMATION_CLASS FsInformationClass);
VEASTATUS VEAPI VzCreateToken(OUT PHANDLE TokenHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN TOKEN_TYPE TokenType, IN PLUID AuthenticationId, IN PLARGE_INTEGER ExpirationTime, IN PTOKEN_USER User, IN PTOKEN_GROUPS Groups OPTIONAL, IN PTOKEN_PRIVILEGES Privileges OPTIONAL, IN PTOKEN_OWNER Owner OPTIONAL, IN PTOKEN_PRIMARY_GROUP PrimaryGroup, IN PTOKEN_DEFAULT_DACL DefaultDacl OPTIONAL, IN PTOKEN_SOURCE TokenSource);
VEASTATUS VEAPI VzEnumerateKey(IN HANDLE KeyHandle, IN ULONG Index, OUT PCHAR SubKeyName, IN ULONG SubKeyNameSize, OUT PULONG ResultNameLength OPTIONAL, OUT PULONG SubKeyCount OPTIONAL, OUT PULONG Flags OPTIONAL);
VEASTATUS VEAPI VzEnumerateValueKey(IN HANDLE KeyHandle, IN ULONG Index, OUT PCHAR ValueName, IN ULONG ValueNameSize, OUT PULONG ResultNameLength OPTIONAL, OUT PULONG Type OPTIONAL, OUT PVOID Data OPTIONAL, IN ULONG DataSize, OUT PULONG ResultDataLength OPTIONAL);
VEASTATUS VEAPI VzSetSystemInformation(IN SYSTEM_INFORMATION_CLASS SystemInformationClass, IN PVOID SystemInformation, IN ULONG SystemInformationLength);
VEASTATUS VEAPI VzCallbackReturn(IN PVOID Result OPTIONAL, IN ULONG ResultLength, IN VEASTATUS CallbackStatus);
#ifdef __cplusplus
}
#endif

#endif /* _VZ_WRAPPERS_H */
