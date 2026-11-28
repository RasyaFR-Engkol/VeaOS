/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : veaerror.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : User-Mode Error Code Definitions (Win32 Compatible)
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#pragma once

/* -----------------------------------------------------------------------
 * General & Core System Errors
 * --------------------------------------------------------------------- */
#define ERROR_SUCCESS                       0L
#define NO_ERROR                            0L
#define ERROR_INVALID_FUNCTION              1L
#define ERROR_GEN_FAILURE                   31L
#define ERROR_NOT_SUPPORTED                 50L
#define ERROR_INVALID_PARAMETER             87L
#define ERROR_CALL_NOT_IMPLEMENTED          120L
#define ERROR_INSUFFICIENT_BUFFER           122L
#define ERROR_INVALID_LEVEL                 124L
#define ERROR_BUSY                          170L
#define ERROR_MORE_DATA                     234L
#define ERROR_NO_MORE_ITEMS                 259L
#define ERROR_MR_MID_NOT_FOUND              317L
#define ERROR_INVALID_FLAGS                 1004L
#define ERROR_OPERATION_ABORTED             995L
#define ERROR_IO_INCOMPLETE                 996L
#define ERROR_IO_PENDING                    997L
#define ERROR_BAD_DEVICE                    1200L
#define ERROR_CANCELLED                     1223L
#define ERROR_INTERNAL_ERROR                1359L
#define ERROR_TIMEOUT                       1460L

/* -----------------------------------------------------------------------
 * File, Directory & Storage I/O Errors
 * --------------------------------------------------------------------- */
#define ERROR_FILE_NOT_FOUND                2L
#define ERROR_PATH_NOT_FOUND                3L
#define ERROR_TOO_MANY_OPEN_FILES           4L
#define ERROR_ACCESS_DENIED                 5L
#define ERROR_INVALID_HANDLE                6L
#define ERROR_INVALID_DRIVE                 15L
#define ERROR_CURRENT_DIRECTORY             16L
#define ERROR_NOT_SAME_DEVICE               17L
#define ERROR_NO_MORE_FILES                 18L
#define ERROR_WRITE_PROTECT                 19L
#define ERROR_BAD_UNIT                      20L
#define ERROR_NOT_READY                     21L
#define ERROR_BAD_COMMAND                   22L
#define ERROR_CRC                           23L
#define ERROR_BAD_LENGTH                    24L
#define ERROR_SEEK                          25L
#define ERROR_SECTOR_NOT_FOUND              27L
#define ERROR_WRITE_FAULT                   29L
#define ERROR_READ_FAULT                    30L
#define ERROR_SHARING_VIOLATION             32L
#define ERROR_LOCK_VIOLATION                33L
#define ERROR_HANDLE_EOF                    38L
#define ERROR_HANDLE_DISK_FULL              39L
#define ERROR_FILE_EXISTS                   80L
#define ERROR_CANNOT_MAKE                   82L
#define ERROR_OPEN_FAILED                   110L
#define ERROR_BUFFER_OVERFLOW               111L
#define ERROR_DISK_FULL                     112L
#define ERROR_INVALID_TARGET_HANDLE         114L
#define ERROR_INVALID_NAME                  123L
#define ERROR_DIRECT_ACCESS_HANDLE          130L
#define ERROR_NEGATIVE_SEEK                 131L
#define ERROR_SEEK_ON_DEVICE                132L
#define ERROR_DIR_NOT_ROOT                  144L
#define ERROR_DIR_NOT_EMPTY                 145L
#define ERROR_ALREADY_EXISTS                183L
#define ERROR_FILENAME_EXCED_RANGE          206L
#define ERROR_DIRECTORY                     267L
#define ERROR_UNRECOGNIZED_VOLUME           1005L
#define ERROR_FILE_INVALID                  1006L

/* -----------------------------------------------------------------------
 * Memory & Heap Errors
 * --------------------------------------------------------------------- */
#define ERROR_ARENA_TRASHED                 7L
#define ERROR_NOT_ENOUGH_MEMORY             8L
#define ERROR_INVALID_BLOCK                 9L
#define ERROR_INVALID_ACCESS                12L
#define ERROR_INVALID_DATA                  13L
#define ERROR_OUTOFMEMORY                   14L
#define ERROR_INVALID_ADDRESS               487L
#define ERROR_NOACCESS                      998L
#define ERROR_SWAPERROR                     999L
#define ERROR_STACK_OVERFLOW                1001L

/* -----------------------------------------------------------------------
 * Process, Thread, Module & Synchronization Errors
 * --------------------------------------------------------------------- */
#define ERROR_BAD_ENVIRONMENT               10L
#define ERROR_BAD_FORMAT                    11L
#define ERROR_NO_PROC_SLOTS                 89L
#define ERROR_SEM_TIMEOUT                   121L
#define ERROR_MOD_NOT_FOUND                 126L
#define ERROR_PROC_NOT_FOUND                127L
#define ERROR_WAIT_NO_CHILDREN              128L
#define ERROR_CHILD_NOT_COMPLETE            129L
#define ERROR_BAD_EXE_FORMAT                193L
#define ERROR_ENVVAR_NOT_FOUND              203L
#define ERROR_NOT_OWNER                     288L
#define ERROR_TOO_MANY_POSTS                298L
#define ERROR_PARTIAL_COPY                  299L
#define ERROR_PROCESS_ABORTED               1067L
#define ERROR_DLL_INIT_FAILED               1114L
#define ERROR_SHUTDOWN_IN_PROGRESS          1115L
#define ERROR_NO_SHUTDOWN_IN_PROGRESS       1116L
#define ERROR_POSSIBLE_DEADLOCK             1131L

/* -----------------------------------------------------------------------
 * IPC, Pipe & Security / Logon Errors
 * --------------------------------------------------------------------- */
#define ERROR_BROKEN_PIPE                   109L
#define ERROR_PIPE_BUSY                     231L
#define ERROR_NO_DATA                       232L
#define ERROR_PIPE_NOT_CONNECTED            233L
#define ERROR_NO_TOKEN                      1008L
#define ERROR_PRIVILEGE_NOT_HELD            1314L
#define ERROR_NO_SUCH_USER                  1317L
#define ERROR_WRONG_PASSWORD                1323L
#define ERROR_LOGON_FAILURE                 1326L
#define ERROR_NONE_MAPPED                   1332L

/* -----------------------------------------------------------------------
 * GUI & Window Manager Errors (WinSrv / User)
 * --------------------------------------------------------------------- */
#define ERROR_INVALID_WINDOW_HANDLE         1400L
#define ERROR_INVALID_MENU_HANDLE           1401L
#define ERROR_INVALID_CURSOR_HANDLE         1402L

/* EOF */