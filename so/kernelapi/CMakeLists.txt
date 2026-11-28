add_library(kernelapi SHARED
    processthread.c
    error.c
    main.c
)

target_include_directories(kernelapi PRIVATE
    ${CMAKE_SOURCE_DIR}/veapi/include
    ${CMAKE_SOURCE_DIR}/so/veaso
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

set_module_type(kernelapi veadll ENTRYPOINT DllMain)
add_importlibs(kernelapi veaso)
target_link_libraries(kernelapi PRIVATE csrlib)