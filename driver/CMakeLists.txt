# =====================================================================
# HELPER FUNCTION FOR VEAOS DRIVERS
# =====================================================================
function(add_vea_driver)
    # 1. Otomatis ambil nama driver dari nama folder (misal: "kbdclass")
    get_filename_component(DRIVER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

    # 2. Cari semua file C dan ASM di folder driver ini
    file(GLOB_RECURSE DRIVER_ASM_SOURCES CONFIGURE_DEPENDS "*.asm")
    file(GLOB_RECURSE DRIVER_C_SOURCES CONFIGURE_DEPENDS "*.c")

    set(OBJ_DIR ${CMAKE_CURRENT_BINARY_DIR})
    set(DRIVER_ASM_OBJS "")

    # Compile ASM Files via NASM
    foreach(ASM_FILE ${DRIVER_ASM_SOURCES})
        file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${ASM_FILE})
        string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
        set(OBJ_FILE ${OBJ_DIR}/drv_${DRIVER_NAME}_${SAFE_NAME}.o)
        
        add_custom_command(
            OUTPUT ${OBJ_FILE}
            COMMAND nasm -f elf32 ${ASM_FILE} -I${ASM_INCLUDE_DIR}/ -o ${OBJ_FILE}
            DEPENDS ${ASM_FILE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "[${DRIVER_NAME}] Compiling ASM: ${SAFE_NAME}"
        )
        list(APPEND DRIVER_ASM_OBJS ${OBJ_FILE})
    endforeach()

    # 3. Daftarkan sebagai native target (hilang status UTILITY + otomatis terbaca clangd)
    add_executable(driver_${DRIVER_NAME}
        ${DRIVER_C_SOURCES}
        ${DRIVER_ASM_OBJS}
    )

    target_compile_options(driver_${DRIVER_NAME} PRIVATE
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
        -g
        ${VEA_INCLUDES}
    )

    target_include_directories(driver_${DRIVER_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${KERNEL_INCLUDE_DIR}
        ${NDK_INCLUDE_DIR}
        ${DDK_INCLUDE_DIR}
    )

    # 4. Override linker khusus di directory driver ini agar tetap menghasilkan relocatable .ko
    set(CMAKE_C_LINK_EXECUTABLE
        "ld.lld -r -m elf_i386 -e DriverEntry <OBJECTS> -o <TARGET>"
        PARENT_SCOPE
    )

    set_target_properties(driver_${DRIVER_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}"
        OUTPUT_NAME "${DRIVER_NAME}"
        SUFFIX ".ko"
    )
endfunction()

# =====================================================================
# AUTOMATIC SUBDIRECTORY SCANNER
# =====================================================================
file(GLOB_RECURSE ALL_DRIVER_CMAKES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*/CMakeLists.txt")

foreach(CMAKE_PATH ${ALL_DRIVER_CMAKES})
    get_filename_component(DRIVER_DIR ${CMAKE_PATH} DIRECTORY)
    message(STATUS "[VeaOS Driver] Found driver module: ${DRIVER_DIR}")
    add_subdirectory(${DRIVER_DIR})
endforeach()