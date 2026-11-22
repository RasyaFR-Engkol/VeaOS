add_library(userapi SHARED
    userapi.c
    class.c
    uiprotocol.c
)

target_include_directories(userapi PRIVATE
    ${CMAKE_SOURCE_DIR}/veapi/include
    ${CMAKE_SOURCE_DIR}/so/veaso
)

set_module_type(userapi veadll ENTRYPOINT DllMain)
add_importlibs(userapi veaso)