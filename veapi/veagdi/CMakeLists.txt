# 1. Ambil semua .c milik veagdi sendiri, KECUALI folder freetype2
file(GLOB_RECURSE VEAGDI_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
list(FILTER VEAGDI_CORE_SOURCES EXCLUDE REGEX ".*/freetype2/.*")

# 2. Daftarkan 12 file unity-build induk FreeType 2 saja
set(FT2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/freetype2")
set(FT2_SOURCES
    "${FT2_DIR}/src/base/ftbase.c"
    "${FT2_DIR}/src/base/ftinit.c"
    "${FT2_DIR}/src/base/ftglyph.c"
    "${FT2_DIR}/src/base/ftbitmap.c"
    "${FT2_DIR}/src/base/ftsystem.c"
    "${FT2_DIR}/src/truetype/truetype.c"
    "${FT2_DIR}/src/sfnt/sfnt.c"
    "${FT2_DIR}/src/cff/cff.c"
    "${FT2_DIR}/src/psaux/psaux.c"
    "${FT2_DIR}/src/pshinter/pshinter.c"
    "${FT2_DIR}/src/smooth/smooth.c"
    "${FT2_DIR}/src/autofit/autofit.c"
    "${FT2_DIR}/src/base/ftdebug.c"
)

# 3. Pasang macro FT2_BUILD_LIBRARY khusus ke file FreeType saja (nempel ke properti file)
set_source_files_properties(${FT2_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS "FT2_BUILD_LIBRARY"
)

# 4. Gabungkan dan lempar ke CMakeLists induk (veapi)
set(VEAGDI_SOURCES
    ${VEAGDI_CORE_SOURCES}
    ${FT2_SOURCES}
    PARENT_SCOPE
)

set(VEAGDI_INCLUDES
    "${FT2_DIR}/include"
    PARENT_SCOPE
)