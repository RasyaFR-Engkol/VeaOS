include(${CMAKE_SOURCE_DIR}/sdk/lib/cmake/VeaUserModules.cmake)

file(GLOB_RECURSE SESSION_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.asm"
)

add_executable(smss ${SESSION_SOURCES})
set_module_type(smss nativeapp)

target_include_directories(smss PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# vea (berisi VeaProcessStartup) harus sebelum veaso agar simbol entry point ketarik linker
add_importlibs(smss vea veaso)