include(${CMAKE_SOURCE_DIR}/sdk/lib/cmake/VeaUserModules.cmake)

file(GLOB_RECURSE VEA_LIB_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.asm"
)

add_library(vea STATIC ${VEA_LIB_SOURCES})
set_module_type(vea staticlib)