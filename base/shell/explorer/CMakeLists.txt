include(${CMAKE_SOURCE_DIR}/sdk/lib/cmake/VeaUserModules.cmake)

file(GLOB_RECURSE SESSION_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.asm"
)

add_executable(explorer ${SESSION_SOURCES})
set_module_type(explorer veagui)

target_include_directories(explorer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

add_importlibs(explorer vea veaso userapi gdiapi kernelapi)