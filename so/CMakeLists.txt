cmake_minimum_required(VERSION 3.20)
include(${CMAKE_SOURCE_DIR}/sdk/lib/cmake/VeaUserModules.cmake)

set(VEASO_DIR       ${CMAKE_CURRENT_SOURCE_DIR}/veaso)
set(VEASO_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/veaso)
file(MAKE_DIRECTORY ${VEASO_BUILD_DIR})

# --- Syscall Kernel (0x0000+) ---
set(SYSCALL_LST       ${CMAKE_SOURCE_DIR}/veakrnl/syscall.lst)
set(NDK_DIR           ${CMAKE_SOURCE_DIR}/sdk/include/ndk)
set(VEASO_STUB_ASM    ${VEASO_BUILD_DIR}/veaso_stubs.asm)
set(VZ_USER_H         ${VEASO_DIR}/vz_user.h)
set(VZ_VEASO_STUB_ASM ${VEASO_BUILD_DIR}/vz_syscall_veaso.asm)

# --- Syscall VEAPI / Shadow SSDT (0x1000+) ---
set(VEAPI_SYSCALL_LST     ${CMAKE_SOURCE_DIR}/veapi/veapi_syscall.lst)
set(VEAPI_INC_DIR         ${CMAKE_SOURCE_DIR}/veapi/include)
set(VEAPI_STUB_ASM        ${VEASO_BUILD_DIR}/veapi_stubs.asm)
set(VEAPI_USER_H          ${VEASO_DIR}/veapi_user.h)
set(VZ_VEAPI_STUB_ASM     ${VEASO_BUILD_DIR}/vz_veapi_veaso.asm)

# Generator syscall stubs: VEAKRNL
add_custom_command(
    OUTPUT ${VEASO_STUB_ASM}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tool/gen_syscall.py ${SYSCALL_LST} ${VEASO_STUB_ASM}
    DEPENDS ${SYSCALL_LST} ${CMAKE_SOURCE_DIR}/tool/gen_syscall.py
    COMMENT "[veaso] Generating Kernel Vea* stubs..."
    VERBATIM
)

add_custom_command(
    OUTPUT ${VZ_USER_H} ${VZ_VEASO_STUB_ASM}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tool/veaso_vz_syscall.py ${SYSCALL_LST} ${VZ_USER_H} ${VZ_VEASO_STUB_ASM} ${NDK_DIR}
    DEPENDS ${SYSCALL_LST} ${CMAKE_SOURCE_DIR}/tool/veaso_vz_syscall.py
    COMMENT "[veaso] Generating vz_user.h & Kernel Vz* stubs..."
    VERBATIM
)

# Generator syscall stubs: VEAPI (Shadow SSDT)
add_custom_command(
    OUTPUT ${VEAPI_STUB_ASM}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tool/gen_syscall.py ${VEAPI_SYSCALL_LST} ${VEAPI_STUB_ASM}
    DEPENDS ${VEAPI_SYSCALL_LST} ${CMAKE_SOURCE_DIR}/tool/gen_syscall.py
    COMMENT "[veaso] Generating VEAPI VeaUser* stubs..."
    VERBATIM
)

add_custom_command(
    OUTPUT ${VEAPI_USER_H} ${VZ_VEAPI_STUB_ASM}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tool/veaso_vz_syscall.py ${VEAPI_SYSCALL_LST} ${VEAPI_USER_H} ${VZ_VEAPI_STUB_ASM} ${VEAPI_INC_DIR}
    DEPENDS ${VEAPI_SYSCALL_LST} ${CMAKE_SOURCE_DIR}/tool/veaso_vz_syscall.py ${VEAPI_INC_DIR}/veapi.h
    COMMENT "[veaso] Generating veapi_user.h & VEAPI VzUser* stubs..."
    VERBATIM
)

file(GLOB_RECURSE VEASO_C_SOURCES   CONFIGURE_DEPENDS "${VEASO_DIR}/*.c")
file(GLOB_RECURSE VEASO_ASM_SOURCES CONFIGURE_DEPENDS "${VEASO_DIR}/*.asm")
file(GLOB_RECURSE RTL_SOURCES       CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/sdk/lib/rtl/*.c")

# RTL User Object Library
add_library(rtl_user OBJECT ${RTL_SOURCES})
set_module_type(rtl_user objectlib)

# veaso.so
add_library(veaso SHARED
    ${VEASO_C_SOURCES}
    ${VEASO_ASM_SOURCES}
    ${VEASO_STUB_ASM}
    ${VZ_VEASO_STUB_ASM}
    ${VZ_USER_H}
    ${VEAPI_STUB_ASM}
    ${VZ_VEAPI_STUB_ASM}
    ${VEAPI_USER_H}
    $<TARGET_OBJECTS:rtl_user>
)
target_include_directories(veaso PRIVATE ${VEASO_DIR})
set_module_type(veaso veadll)

add_subdirectory(userapi)