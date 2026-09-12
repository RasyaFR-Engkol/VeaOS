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
file(GLOB_RECURSE RTL_SOURCES "${RTL_DIR}/*.c")

set(VEA_INCLUDES
    -I${KERNEL_INCLUDE_DIR}
    -I${SDK_INCLUDE_DIR}
    -I${NDK_INCLUDE_DIR}
    -I${DDK_INCLUDE_DIR}
    -I${PSDK_INCLUDE_DIR}
)

add_subdirectory(entrystage0)
add_subdirectory(entryfat32)
add_subdirectory(osi386)
add_subdirectory(hct/x86)
add_subdirectory(veakrnl)
add_subdirectory(tool)
add_subdirectory(driver)