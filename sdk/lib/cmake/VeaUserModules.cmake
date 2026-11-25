# ==============================================================================
# VeaOS User-Mode & Subsystem Build Rules (ReactOS-style CMake Helpers)
# ==============================================================================

# 1. Konfigurasi NASM & Include Global User-Mode
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
enable_language(ASM_NASM)

set(VEA_USER_INCLUDES
    ${CMAKE_SOURCE_DIR}/sdk/include
    ${CMAKE_SOURCE_DIR}/sdk/include/ndk
    ${CMAKE_SOURCE_DIR}/sdk/include/psdk
    ${CMAKE_SOURCE_DIR}/veakrnl/include
)

set(VEA_USER_C_FLAGS
    --target=i686-pc-none-elf
    -march=i686
    -m32
    -ffreestanding
    -nostdlib
    -fno-builtin
    -fno-stack-protector
    -fno-omit-frame-pointer
    -std=gnu23
    -fshort-wchar
    -fms-extensions
    -ferror-limit=0
)

# 2. Peta Base Address Terpusat (Mirip baseaddress.cmake di ReactOS)
set(baseaddress_veaso    0x7C920000)
set(baseaddress_kernel32 0x7C800000)
set(baseaddress_user32   0x77D40000)
set(baseaddress_userapi  0x77E00000)
set(baseaddress_uitheme  0x77C80000)
set(baseaddress_gdi32    0x77F10000)

# 3. Helper: Set Image Base
function(set_image_base MODULE BASE_ADDRESS)
    target_link_options(${MODULE} PRIVATE "-Wl,--image-base=${BASE_ADDRESS}")
endfunction()

# 4. Helper: Set Entry Point
function(set_entrypoint MODULE ENTRYPOINT)
    if(NOT "${ENTRYPOINT}" STREQUAL "0")
        target_link_options(${MODULE} PRIVATE "-Wl,-e,${ENTRYPOINT}")
    endif()
endfunction()

# 5. Helper: Link Import Libraries (.so)
function(add_importlibs MODULE)
    target_link_libraries(${MODULE} PRIVATE ${ARGN})
endfunction()

# 6. Fungsi Utama: set_module_type(<target> <type> [ENTRYPOINT <symbol>])
#    Tipe yang didukung:
#      - veadll    : Shared library (.so) dengan base address unik & export penuh
#      - nativeapp : Executable Ring 3 (.elf) seperti smss.elf, csrss.elf, winlogon.elf
#      - objectlib : Kumpulan objek C/ASM reusable (seperti rtl_user / veacrt)
#      - staticlib : Static library (.a)
function(set_module_type MODULE TYPE)
    cmake_parse_arguments(ARG "" "ENTRYPOINT" "" ${ARGN})

    # Semua tipe modul otomatis dapat include & C flags freestanding VeaOS
    target_compile_definitions(${MODULE} PRIVATE BUILD_USER)
    target_include_directories(${MODULE} PRIVATE ${VEA_USER_INCLUDES})
    target_compile_options(${MODULE} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${VEA_USER_C_FLAGS}>
    )

    get_target_property(_MOD_SOURCES ${MODULE} SOURCES)
    set(_HAS_RSRC FALSE)

    if(_MOD_SOURCES)
        set(_INC_FLAGS "")
        foreach(_INC ${VEA_USER_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR})
            list(APPEND _INC_FLAGS "-I${_INC}")
        endforeach()

        foreach(_SRC ${_MOD_SOURCES})
            if("${_SRC}" MATCHES "\\.rc$")
                set(_HAS_RSRC TRUE)
                get_filename_component(_RC_ABS "${_SRC}" ABSOLUTE)
                get_filename_component(_RC_NAME "${_SRC}" NAME_WE)
                set(_RSRC_OBJ "${CMAKE_CURRENT_BINARY_DIR}/${MODULE}_${_RC_NAME}.rsrc.o")

                # Tandai .rc asli agar tidak dilempar mentah ke linker
                set_source_files_properties("${_SRC}" PROPERTIES HEADER_FILE_ONLY TRUE)

                add_custom_command(
                    OUTPUT "${_RSRC_OBJ}"
                    COMMAND "${VEA_COMRC_EXECUTABLE}" ${_INC_FLAGS} "${_RC_ABS}" -o "${_RSRC_OBJ}"
                    DEPENDS "${_RC_ABS}" comrc_tool
                    COMMENT "[COMRC] Compiling resource ${_RC_NAME}.rc for ${MODULE}..."
                )

                target_sources(${MODULE} PRIVATE "${_RSRC_OBJ}")
            endif()
        endforeach()
    endif()

    if(_HAS_RSRC)
        if(EXISTS "${VEA_TOOL_DIR}/veadll.ld")
            target_link_options(${MODULE} PRIVATE "-Wl,-T,${VEA_TOOL_DIR}/veadll.ld")
        endif()
        # Pastikan __vea_rsrc_start ikut masuk .dynsym meski targetnya .elf (veagui/nativeapp)
        target_link_options(${MODULE} PRIVATE "-Wl,--export-dynamic")
    endif()

    if("${TYPE}" STREQUAL "veadll")
        if(DEFINED baseaddress_${MODULE})
            set(_BASE ${baseaddress_${MODULE}})
        else()
            message(FATAL_ERROR "[VeaBuild] baseaddress_${MODULE} not registered in VeaUserModules.cmake!")
        endif()

        set_target_properties(${MODULE} PROPERTIES
            PREFIX ""
            SUFFIX ".so"
            OUTPUT_NAME "${MODULE}"
            LINKER_LANGUAGE C
        )

        target_compile_options(${MODULE} PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-fPIC -fvisibility=default>
        )

        target_link_options(${MODULE} PRIVATE
            -m32
            -shared
            -nostdlib
            -Wl,-soname,${MODULE}.so
            -Wl,-Bsymbolic
            -Wl,--export-dynamic
            -Wl,--hash-style=sysv
            -Wl,--no-undefined
        )

        set_image_base(${MODULE} ${_BASE})
        if(DEFINED ARG_ENTRYPOINT)
            set_entrypoint(${MODULE} ${ARG_ENTRYPOINT})
        endif()

    elseif("${TYPE}" STREQUAL "nativeapp")
        set_target_properties(${MODULE} PROPERTIES
            SUFFIX ".elf"
            OUTPUT_NAME "${MODULE}"
            LINKER_LANGUAGE C
        )

        target_compile_options(${MODULE} PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-fno-pie>
        )

        target_link_options(${MODULE} PRIVATE
            -m32
            -no-pie
            -nostdlib
            -Wl,--hash-style=sysv
            -Wl,--no-undefined
        )

        set_image_base(${MODULE} 0x00400000)

        if(DEFINED ARG_ENTRYPOINT)
            set_entrypoint(${MODULE} ${ARG_ENTRYPOINT})
        else()
            set_entrypoint(${MODULE} VeaProcessStartup)
        endif()
        
    elseif("${TYPE}" STREQUAL "objectlib" OR "${TYPE}" STREQUAL "staticlib")
        target_compile_options(${MODULE} PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-fPIC -fvisibility=default>
        )
    elseif("${TYPE}" STREQUAL "veacui" OR "${TYPE}" STREQUAL "veagui")
        set_target_properties(${MODULE} PROPERTIES
            SUFFIX ".elf"
            OUTPUT_NAME "${MODULE}"
            LINKER_LANGUAGE C
        )

        target_compile_options(${MODULE} PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-fno-pie>
        )

        target_link_options(${MODULE} PRIVATE
            -m32
            -no-pie
            -nostdlib
            -Wl,--hash-style=sysv
            -Wl,--no-undefined
        )

        set_image_base(${MODULE} 0x00400000)

        if(DEFINED ARG_ENTRYPOINT)
            set_entrypoint(${MODULE} ${ARG_ENTRYPOINT})
        elseif("${TYPE}" STREQUAL "veacui")
            set_entrypoint(${MODULE} mainCRTStartup)
        elseif("${TYPE}" STREQUAL "veagui")
            set_entrypoint(${MODULE} WinMainCRTStartup)
        endif()

        if(TARGET veacrt)
            target_link_libraries(${MODULE} PRIVATE veacrt)
        endif()
    else()
        message(FATAL_ERROR "[VeaBuild] Unknown Type Module: '${TYPE}'")
    endif()
endfunction()