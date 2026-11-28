include(${CMAKE_SOURCE_DIR}/sdk/lib/cmake/VeaUserModules.cmake)

file(GLOB_RECURSE SESSION_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.asm"
)

add_executable(VeaLogonScr ${SESSION_SOURCES})
set_module_type(VeaLogonScr veagui)

target_include_directories(VeaLogonScr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_importlibs(VeaLogonScr vea veaso userapi kernelapi)