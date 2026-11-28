# ==============================================================================
# VeaOS User-Mode & Subsystem Build Rules (ReactOS-style CMake Helpers)
# ==============================================================================

# 1. Deteksi Root Directory VeaOS otomatis (aman untuk build internal & eksternal)
if(NOT DEFINED VEAOS_SOURCE_DIR)
    set(_VEA_PROBE_DIR "${CMAKE_CURRENT_LIST_DIR}")
    while(NOT "${_VEA_PROBE_DIR}" STREQUAL "/" AND NOT EXISTS "${_VEA_PROBE_DIR}/sdk/include")
        get_filename_component(_VEA_PROBE_DIR "${_VEA_PROBE_DIR}" DIRECTORY)
    endwhile()
    if(EXISTS "${_VEA_PROBE_DIR}/sdk/include")
        set(VEAOS_SOURCE_DIR "${_VEA_PROBE_DIR}")
    else()
        set(VEAOS_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
    endif()
endif()

if(NOT DEFINED VEA_TOOL_DIR)
    set(VEA_TOOL_DIR "${VEAOS_SOURCE_DIR}/tool")
endif()

if(NOT DEFINED VEA_COMRC_EXECUTABLE)
    if(TARGET comrc_tool)
        set(VEA_COMRC_EXECUTABLE comrc_tool)
    elseif(EXISTS "${VEAOS_SOURCE_DIR}/build/tool/comrc")
        set(VEA_COMRC_EXECUTABLE "${VEAOS_SOURCE_DIR}/build/tool/comrc")
    elseif(EXISTS "${VEAOS_SOURCE_DIR}/build/out/comrc")
        set(VEA_COMRC_EXECUTABLE "${VEAOS_SOURCE_DIR}/build/out/comrc")
    endif()
endif()

# 2. Konfigurasi NASM & Include Global User-Mode
if(NOT CMAKE_ASM_NASM_COMPILER_LOADED)
    set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
    enable_language(ASM_NASM)
endif()

set(VEA_USER_INCLUDES
    ${VEAOS_SOURCE_DIR}/sdk/include
    ${VEAOS_SOURCE_DIR}/sdk/include/ndk
    ${VEAOS_SOURCE_DIR}/sdk/include/psdk
    ${VEAOS_SOURCE_DIR}/veakrnl/include
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

# 3. Peta Base Address Terpusat
set(baseaddress_csrsrv   0x75B00000)
set(baseaddress_basesrv  0x75B40000)
set(baseaddress_winsrv   0x75B80000)
set(baseaddress_veaso    0x7C920000)
set(baseaddress_kernelapi 0x7C800000)
set(baseaddress_user32   0x77D40000)
set(baseaddress_userapi  0x77E00000)
set(baseaddress_uitheme  0x77C80000)
set(baseaddress_gdiapi    0x77F10000)

# 4. Helper: Set Image Base
function(set_image_base MODULE BASE_ADDRESS)
    target_link_options(${MODULE} PRIVATE "-Wl,--image-base=${BASE_ADDRESS}")
endfunction()

# 5. Helper: Set Entry Point
function(set_entrypoint MODULE ENTRYPOINT)
    if(NOT "${ENTRYPOINT}" STREQUAL "0")
        target_link_options(${MODULE} PRIVATE "-Wl,-e,${ENTRYPOINT}")
    endif()
endfunction()

# 6. Helper: Link Import Libraries (.so tanpa prefix 'lib')
function(add_importlibs MODULE)
    foreach(_LIB ${ARGN})
        if(TARGET ${_LIB} OR EXISTS "${_LIB}" OR "${_LIB}" MATCHES "^-")
            target_link_libraries(${MODULE} PRIVATE ${_LIB})
        elseif("${_LIB}" MATCHES "\\.(so|a)$")
            target_link_libraries(${MODULE} PRIVATE "-l:${_LIB}")
        else()
            target_link_libraries(${MODULE} PRIVATE "-l:${_LIB}.so")
        endif()
    endforeach()
endfunction()

# 7. Fungsi Utama: set_module_type(<target> <type> [ENTRYPOINT <symbol>])
function(set_module_type MODULE TYPE)
    cmake_parse_arguments(ARG "" "ENTRYPOINT" "" ${ARGN})

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

        set(_COMRC_DEPENDS "")
        if(TARGET comrc_tool)
            set(_COMRC_DEPENDS comrc_tool)
        endif()

        foreach(_SRC ${_MOD_SOURCES})
            if("${_SRC}" MATCHES "\\.rc$")
                set(_HAS_RSRC TRUE)
                get_filename_component(_RC_ABS "${_SRC}" ABSOLUTE)
                get_filename_component(_RC_NAME "${_SRC}" NAME_WE)
                set(_RSRC_OBJ "${CMAKE_CURRENT_BINARY_DIR}/${MODULE}_${_RC_NAME}.rsrc.o")

                set_source_files_properties("${_SRC}" PROPERTIES HEADER_FILE_ONLY TRUE)

                add_custom_command(
                    OUTPUT "${_RSRC_OBJ}"
                    COMMAND "${VEA_COMRC_EXECUTABLE}" ${_INC_FLAGS} "${_RC_ABS}" -o "${_RSRC_OBJ}"
                    DEPENDS "${_RC_ABS}" ${_COMRC_DEPENDS}
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
            --target=i686-pc-linux-gnu
            -fuse-ld=lld
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
            $<$<COMPILE_LANGUAGE:C>:-fno-pic -fno-pie>
        )

        target_link_options(${MODULE} PRIVATE
            --target=i686-pc-linux-gnu
            -fuse-ld=lld
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
            $<$<COMPILE_LANGUAGE:C>:-fno-pic -fno-pie>
        )

        target_link_options(${MODULE} PRIVATE
            --target=i686-pc-linux-gnu
            -fuse-ld=lld
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