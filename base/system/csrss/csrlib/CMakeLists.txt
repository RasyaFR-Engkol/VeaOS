include(${CMAKE_SOURCE_DIR}/sdk/lib/cmake/VeaUserModules.cmake)

add_library(csrlib STATIC
    connect.c
    api.c
)

target_include_directories(csrlib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Include
)

set_target_properties(csrlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_module_type(csrlib staticlib)