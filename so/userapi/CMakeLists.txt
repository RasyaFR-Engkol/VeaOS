add_library(userapi SHARED
    userapi.c
    class.c
    uiprotocol.c
    cursor.c
    desktop.c
    wnd.c
    resources/userapi.rc
    resource.c
)

set_source_files_properties(resources/icon.asm PROPERTIES
    COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

target_include_directories(userapi PRIVATE
    ${CMAKE_SOURCE_DIR}/veapi/include
    ${CMAKE_SOURCE_DIR}/so/veaso
)

set_module_type(userapi veadll ENTRYPOINT DllMain)
add_importlibs(userapi veaso gdiapi)