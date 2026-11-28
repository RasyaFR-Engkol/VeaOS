add_library(gdiapi SHARED
    gdimain.c
    gdiobj.c
)

target_include_directories(gdiapi PRIVATE
    ${CMAKE_SOURCE_DIR}/veapi/include
    ${CMAKE_SOURCE_DIR}/so/veaso
)

set_module_type(gdiapi veadll ENTRYPOINT DllMain)
add_importlibs(gdiapi veaso)