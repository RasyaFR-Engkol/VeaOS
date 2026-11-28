include(${CMAKE_SOURCE_DIR}/sdk/lib/cmake/VeaUserModules.cmake)

# CSRSRV.SO
add_library(csrsrv SHARED
    csrsrv/init.c
    csrsrv/api.c
    csrsrv/procsup.c
    csrsrv/server.c
    csrsrv/session.c
)
set_module_type(csrsrv veadll)
target_include_directories(csrsrv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/csrsrv
)
add_importlibs(csrsrv veaso)

# BASESRV.SO
add_library(basesrv SHARED
    basesrv/basesrv.c
)
set_module_type(basesrv veadll)
target_include_directories(basesrv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)
add_importlibs(basesrv csrsrv veaso)

# WINSRV.SO
add_library(winsrv SHARED
    winsrv/winsrv.c
)
set_module_type(winsrv veadll)
target_include_directories(winsrv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)
add_importlibs(winsrv csrsrv veaso)

# CSRSS.ELF
add_executable(csrss
    csrss/main.c
)
set_module_type(csrss nativeapp)
target_include_directories(csrss PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)
add_importlibs(csrss vea veaso csrsrv)

add_subdirectory(csrlib)