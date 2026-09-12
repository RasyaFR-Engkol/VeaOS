# =====================================================================
# HELPER FUNCTION FOR VEAOS DRIVERS
# =====================================================================
function(add_vea_driver)
    # 1. Otomatis ambil nama driver dari nama folder (misal: "ahci")
    get_filename_component(DRIVER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

    # 2. Otomatis cari semua file C dan ASM di folder driver ini
    file(GLOB_RECURSE DRIVER_ASM_SOURCES CONFIGURE_DEPENDS "*.asm")
    file(GLOB_RECURSE DRIVER_C_SOURCES CONFIGURE_DEPENDS "*.c")

    # Folder file objek sementara (.o) -> build/driver/...
    set(OBJ_DIR ${CMAKE_CURRENT_BINARY_DIR})
    set(DRIVER_OBJS "")

    # Compile ASM Files
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
        list(APPEND DRIVER_OBJS ${OBJ_FILE})
    endforeach()

    # Compile C Files (Flag khusus Kernel Object .ko)
    foreach(C_FILE ${DRIVER_C_SOURCES})
        file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${C_FILE})
        string(REPLACE "/" "_" SAFE_NAME ${REL_PATH})
        set(OBJ_FILE ${OBJ_DIR}/drv_${DRIVER_NAME}_${SAFE_NAME}.o)
        
        add_custom_command(
            OUTPUT ${OBJ_FILE}
            COMMAND clang --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin 
                    -fno-pic -fno-pie -fno-common -fno-asynchronous-unwind-tables
                    ${VEA_INCLUDES} 
                    -I${CMAKE_CURRENT_SOURCE_DIR}
                    -c ${C_FILE} -o ${OBJ_FILE} -std=gnu23 -fshort-wchar -fms-extensions
            DEPENDS ${C_FILE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "[${DRIVER_NAME}] Compiling C: ${SAFE_NAME}"
        )
        list(APPEND DRIVER_OBJS ${OBJ_FILE})
    endforeach()

    # Dummy target Clangd per-driver (Autocompletion/LSP)
    if(DRIVER_C_SOURCES)
        add_library(clangd_drv_${DRIVER_NAME}_dummy OBJECT EXCLUDE_FROM_ALL ${DRIVER_C_SOURCES})
        target_compile_options(clangd_drv_${DRIVER_NAME}_dummy PRIVATE 
            --target=i686-pc-none-elf -march=i686 -m32 -ffreestanding -nostdlib -fno-builtin -std=gnu23 -fms-extensions
        )
        target_include_directories(clangd_drv_${DRIVER_NAME}_dummy PRIVATE  
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${KERNEL_INCLUDE_DIR}
            ${NDK_INCLUDE_DIR} 
            ${DDK_INCLUDE_DIR} 
        )
    endif()

    # Linker: Gabungkan .o menjadi Relocatable ELF (.ko) ke build/out/
    set(KO_FILE ${OUT_DIR}/${DRIVER_NAME}.ko)

    add_custom_command(
        OUTPUT ${KO_FILE}
        COMMAND ld.lld -r -m elf_i386 -e DriverEntry ${DRIVER_OBJS} -o ${KO_FILE}
        DEPENDS ${DRIVER_OBJS}
        COMMENT "[${DRIVER_NAME}] Linking Kernel Object (${DRIVER_NAME}.ko)"
    )

    # Target build
    add_custom_target(driver_${DRIVER_NAME} ALL DEPENDS ${KO_FILE})
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