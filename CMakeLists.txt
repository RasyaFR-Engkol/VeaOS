cmake_minimum_required(VERSION 3.16)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER "clang")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(VeaOS LANGUAGES C)

# Single Output Directory: build/out
set(OUT_DIR ${CMAKE_BINARY_DIR}/out)
file(MAKE_DIRECTORY ${OUT_DIR})

# DEFINISIKAN INCLUDE PATH
set(SDK_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/sdk/include)
set(KERNEL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/veakrnl/include)
set(NDK_INCLUDE_DIR ${SDK_INCLUDE_DIR}/ndk)
set(DDK_INCLUDE_DIR ${SDK_INCLUDE_DIR}/ddk)
set(PSDK_INCLUDE_DIR ${SDK_INCLUDE_DIR}/psdk)
set(ASM_INCLUDE_DIR ${SDK_INCLUDE_DIR}/asm)
set(LOADER_INCLUDE_DIR ${SDK_INCLUDE_DIR}/ldr)
set(RTL_DIR ${CMAKE_SOURCE_DIR}/sdk/lib/rtl)
file(GLOB_RECURSE RTL_SOURCES CONFIGURE_DEPENDS "${RTL_DIR}/*.c")

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
enable_language(ASM_NASM)

if(NOT CMAKE_ASM_NASM_COMPILE_OBJECT)
    set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> <FLAGS> -f elf32 -o <OBJECT> <SOURCE>")
endif()

set(VEA_INCLUDES
    -I${KERNEL_INCLUDE_DIR}
    -I${SDK_INCLUDE_DIR}
    -I${NDK_INCLUDE_DIR}
    -I${DDK_INCLUDE_DIR}
    -I${PSDK_INCLUDE_DIR}
)

# =====================================================================
# VEAOS KERNEL CODEGEN (DDK, Export Table, & Vz Syscall Wrappers)
# =====================================================================
set(EXPORT_LST_FILE "${CMAKE_SOURCE_DIR}/veakrnl/export.lst")
set(DDK_HEADER_FILE "${DDK_INCLUDE_DIR}/veaddk.h")
set(AUTEXP_C_FILE "${CMAKE_BINARY_DIR}/veakrnl/autexp.c")

set(GEN_DDK_SCRIPT "${CMAKE_SOURCE_DIR}/tool/gen_ddk.py")
set(GEN_EXP_SCRIPT "${CMAKE_SOURCE_DIR}/tool/gen_exp.py")

set(SYSCALL_LST_FILE "${CMAKE_SOURCE_DIR}/veakrnl/syscall.lst")
set(VZ_WRAPPERS_ASM_FILE "${CMAKE_BINARY_DIR}/veakrnl/vz_wrappers.asm")
set(VZ_WRAPPERS_H_FILE "${KERNEL_INCLUDE_DIR}/vz_wrappers.h")
set(GEN_VZ_SCRIPT "${CMAKE_SOURCE_DIR}/tool/vz_syscall.py")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/veakrnl")

if(EXISTS ${EXPORT_LST_FILE})
    execute_process(
        COMMAND python3 ${GEN_DDK_SCRIPT}
                ${EXPORT_LST_FILE}
                ${KERNEL_INCLUDE_DIR}
                ${DDK_INCLUDE_DIR}
        RESULT_VARIABLE GEN_DDK_RESULT
    )
    if(NOT GEN_DDK_RESULT EQUAL 0)
        message(WARNING "[VeaDDK] Failed to generate veaddk.h!")
    endif()

    execute_process(
        COMMAND python3 ${GEN_EXP_SCRIPT}
                ${EXPORT_LST_FILE}
                ${KERNEL_INCLUDE_DIR}
                ${AUTEXP_C_FILE}
        RESULT_VARIABLE GEN_EXP_RESULT
    )
    if(NOT GEN_EXP_RESULT EQUAL 0)
        message(WARNING "[VeaExport] Failed to generate autexp.c!")
    endif()
else()
    message(FATAL_ERROR "[VeaExport] export.lst not found in ${EXPORT_LST_FILE}!")
endif()

if(EXISTS ${SYSCALL_LST_FILE})
    execute_process(
        COMMAND python3 ${GEN_VZ_SCRIPT}
                ${SYSCALL_LST_FILE}
                ${VZ_WRAPPERS_ASM_FILE}
                ${VZ_WRAPPERS_H_FILE}
                ${KERNEL_INCLUDE_DIR}
        RESULT_VARIABLE GEN_VZ_RESULT
    )
    if(NOT GEN_VZ_RESULT EQUAL 0)
        message(WARNING "[VeaVz] Failed to generate vz_wrappers!")
    else()
        message(STATUS "[VeaVz] Successfully generated Vz syscall wrappers.")
    endif()
else()
    message(FATAL_ERROR "[VeaVz] syscall.lst not found in ${SYSCALL_LST_FILE}!")
endif()

add_custom_command(
    OUTPUT ${AUTEXP_C_FILE} ${DDK_HEADER_FILE} ${VZ_WRAPPERS_ASM_FILE} ${VZ_WRAPPERS_H_FILE}
    COMMAND python3 ${GEN_DDK_SCRIPT} ${EXPORT_LST_FILE} ${KERNEL_INCLUDE_DIR} ${DDK_INCLUDE_DIR}
    COMMAND python3 ${GEN_EXP_SCRIPT} ${EXPORT_LST_FILE} ${KERNEL_INCLUDE_DIR} ${AUTEXP_C_FILE}
    COMMAND python3 ${GEN_VZ_SCRIPT} ${SYSCALL_LST_FILE} ${VZ_WRAPPERS_ASM_FILE} ${VZ_WRAPPERS_H_FILE} ${KERNEL_INCLUDE_DIR}
    DEPENDS ${EXPORT_LST_FILE} ${SYSCALL_LST_FILE} ${GEN_DDK_SCRIPT} ${GEN_EXP_SCRIPT} ${GEN_VZ_SCRIPT}
    COMMENT "[VeaCodegen] Regenerating DDK, Export Table, & Vz Syscall Wrappers"
)

add_custom_target(veakrnl_codegen DEPENDS
    ${AUTEXP_C_FILE}
    ${DDK_HEADER_FILE}
    ${VZ_WRAPPERS_ASM_FILE}
    ${VZ_WRAPPERS_H_FILE}
)

add_subdirectory(entrystage0)
add_subdirectory(entryfat32)
add_subdirectory(osi386)
add_subdirectory(hct/x86)
add_subdirectory(veakrnl)
add_subdirectory(tool)
add_subdirectory(driver)
add_subdirectory(so)
add_subdirectory(base)
add_subdirectory(sdk/lib/vea)
add_subdirectory(veapi)