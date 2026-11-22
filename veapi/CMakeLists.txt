# =====================================================================
# 1. INCLUDE DIRECTORIES & SUBDIRECTORIES
# =====================================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${KERNEL_INCLUDE_DIR}
    ${NDK_INCLUDE_DIR}
    ${DDK_INCLUDE_DIR}
)

add_subdirectory(veagdi)
add_subdirectory(veauser)

# =====================================================================
# 2. SOURCE & ASSEMBLY DISCOVERY
# =====================================================================
file(GLOB VEAPI_ROOT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
file(GLOB_RECURSE VEAPI_ASM_SOURCES CONFIGURE_DEPENDS "*.asm")

set(VEAPI_ASM_OBJS "")
foreach(ASM_FILE ${VEAPI_ASM_SOURCES})
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${ASM_FILE})
    string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
    set(OBJ_FILE ${CMAKE_CURRENT_BINARY_DIR}/veapi_${SAFE_NAME}.o)

    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND nasm -f elf32 ${ASM_FILE} -I${ASM_INCLUDE_DIR}/ -o ${OBJ_FILE}
        DEPENDS ${ASM_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "[veapi] Compiling ASM: ${SAFE_NAME}"
    )
    list(APPEND VEAPI_ASM_OBJS ${OBJ_FILE})
endforeach()

# =====================================================================
# 3. SYSCALL GENERATION (RING 0 WRAPPERS & RING 3 STUBS)
# =====================================================================
file(GLOB VZ_SYSCALL_SCRIPT
    "${CMAKE_SOURCE_DIR}/*/vz_syscall.py"
    "${CMAKE_SOURCE_DIR}/*/*/vz_syscall.py"
)
file(GLOB VZ_SYSCALL_LIST
    "${CMAKE_SOURCE_DIR}/*/syscall.lst"
    "${CMAKE_SOURCE_DIR}/*/*/syscall.lst"
)
file(GLOB VEASO_VZ_SCRIPT
    "${CMAKE_SOURCE_DIR}/*/veaso_vz_syscall.py"
    "${CMAKE_SOURCE_DIR}/*/*/veaso_vz_syscall.py"
)

list(GET VZ_SYSCALL_SCRIPT 0 VZ_SYSCALL_SCRIPT)
list(GET VZ_SYSCALL_LIST   0 VZ_SYSCALL_LIST)
list(GET VEASO_VZ_SCRIPT   0 VEASO_VZ_SCRIPT)

# 3A. Ring 0 Vz* syscall wrappers untuk dipakai internal oleh veapi.ko
set(VEAPI_VZ_ASM "${CMAKE_CURRENT_BINARY_DIR}/veapi_vz_wrappers.asm")
set(VEAPI_VZ_OBJ "${CMAKE_CURRENT_BINARY_DIR}/veapi_vz_wrappers.o")

add_custom_command(
    OUTPUT ${VEAPI_VZ_ASM}
    COMMAND python3 ${VZ_SYSCALL_SCRIPT} ${VZ_SYSCALL_LIST} ${VEAPI_VZ_ASM} NONE ${KERNEL_INCLUDE_DIR}
    DEPENDS ${VZ_SYSCALL_SCRIPT} ${VZ_SYSCALL_LIST}
    COMMENT "[veapi] Generating Ring 0 Vz syscall wrappers"
)

add_custom_command(
    OUTPUT ${VEAPI_VZ_OBJ}
    COMMAND nasm -f elf32 ${VEAPI_VZ_ASM} -I${ASM_INCLUDE_DIR}/ -o ${VEAPI_VZ_OBJ}
    DEPENDS ${VEAPI_VZ_ASM}
    COMMENT "[veapi] Assembling Ring 0 Vz syscall wrappers"
)

# 3B. Ring 3 VzUser* SYSENTER stubs & header untuk user-mode (veaso.so)
set(VEAPI_SYSCALL_LIST "${CMAKE_CURRENT_SOURCE_DIR}/veapi_syscall.lst")
set(VEAPI_USER_H       "${SDK_INCLUDE_DIR}/psdk/veapi.h")
set(VEAPI_USER_ASM     "${CMAKE_BINARY_DIR}/generated/veapi_vz_user.asm")

add_custom_command(
    OUTPUT ${VEAPI_USER_H} ${VEAPI_USER_ASM}
    COMMAND python3 ${VEASO_VZ_SCRIPT}
            ${VEAPI_SYSCALL_LIST}
            ${VEAPI_USER_H}
            ${VEAPI_USER_ASM}
            ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DEPENDS ${VEASO_VZ_SCRIPT} ${VEAPI_SYSCALL_LIST} "${CMAKE_CURRENT_SOURCE_DIR}/include/veapi.h"
    COMMENT "[veapi] Generating Ring 3 VzUser* SYSENTER stubs & veapi_user.h"
)

add_custom_target(veapi_user_stubs DEPENDS ${VEAPI_USER_H} ${VEAPI_USER_ASM})

# =====================================================================
# 4. TARGET DEFINITION, COMPILER FLAGS & LINKER (veapi.ko)
# =====================================================================
add_executable(veapi
    ${VEAPI_ROOT_SOURCES}
    ${VEAUSER_SOURCES}
    ${VEAGDI_SOURCES}
    ${VEAPI_ASM_OBJS}
    ${VEAPI_VZ_OBJ}
)

target_include_directories(veapi PRIVATE ${VEAGDI_INCLUDES})

add_dependencies(veapi veapi_user_stubs)

target_compile_options(veapi PRIVATE
    --target=i686-pc-none-elf
    -march=i686
    -m32
    -ffreestanding
    -nostdlib
    -fno-builtin
    -fno-pic
    -fno-pie
    -fno-common
    -fno-asynchronous-unwind-tables
    -fmacro-prefix-map=/home/xvrasya/VeaOS/=
    -std=gnu23
    -fshort-wchar
    -fms-extensions
    -mno-sse
    -mno-sse2
    -mno-mmx
    -msoft-float
    ${VEA_INCLUDES}
)

set(CMAKE_C_LINK_EXECUTABLE
    "ld.lld -r -m elf_i386 -e DriverEntry <OBJECTS> -o <TARGET>"
)

set_target_properties(veapi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}"
    OUTPUT_NAME "veapi"
    SUFFIX ".ko"
)

add_custom_command(TARGET veapi POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:veapi> $<TARGET_FILE:veapi>.dbg
    COMMAND ${CMAKE_STRIP} --strip-debug $<TARGET_FILE:veapi>
    COMMENT "Stripping DWARF debug info dari veapi.ko (simbol disimpan di veapi.ko.dbg)"
)