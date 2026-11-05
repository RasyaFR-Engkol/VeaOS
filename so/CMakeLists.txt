cmake_minimum_required(VERSION 3.20)

# Flag kompilasi khusus Ring 3 Shared Library (32-bit x86)
set(SO_C_FLAGS
    -m32
    -fPIC
    -ffreestanding
    -nostdlib
    -fno-builtin
    -fno-stack-protector
    -Wall
    -Wextra
)

set(SO_LINK_FLAGS
    -m32
    -shared
    -nostdlib
    -Wl,-Bsymbolic
    -Wl,--no-undefined
)

# Shared Include Directories (SDK & NDK)
set(SO_INCLUDES
    ${CMAKE_SOURCE_DIR}/sdk/include
    ${CMAKE_SOURCE_DIR}/sdk/include/ndk
    ${CMAKE_SOURCE_DIR}/sdk/include/psdk
)

# ==============================================================================
# 1. BUILD TARGET: veaso.so (NTDLL-equivalent)
# ==============================================================================

set(VEASO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/veaso)
set(VEASO_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/veaso)

# Lokasi Script Generator & Syscall List
set(STUB_GEN_TOOL ${CMAKE_SOURCE_DIR}/tool/gen_syscall.py)
set(SYSCALL_LST   ${CMAKE_SOURCE_DIR}/veakrnl/syscall.lst)

# Target Output NASM Assembly & Compiled Object File
set(VEASO_STUB_ASM ${VEASO_BUILD_DIR}/veaso_stubs.asm)
set(VEASO_STUB_OBJ ${VEASO_BUILD_DIR}/veaso_stubs.o)

# Pastikan folder build untuk veaso dibuat
file(MAKE_DIRECTORY ${VEASO_BUILD_DIR})

# ------------------------------------------------------------------------------
# Step 1: Generate NASM Assembly Stubs dari syscall.lst
# ------------------------------------------------------------------------------
add_custom_command(
    OUTPUT ${VEASO_STUB_ASM}
    COMMAND python3 ${STUB_GEN_TOOL} ${SYSCALL_LST} ${VEASO_STUB_ASM}
    DEPENDS ${SYSCALL_LST} ${STUB_GEN_TOOL}
    COMMENT "[veaso] Generating NASM syscall stubs from ${SYSCALL_LST}..."
    VERBATIM
)

# ------------------------------------------------------------------------------
# Step 2: Compile NASM Assembly ke Object File elf32
# ------------------------------------------------------------------------------
add_custom_command(
    OUTPUT ${VEASO_STUB_OBJ}
    COMMAND nasm -f elf32 ${VEASO_STUB_ASM} -o ${VEASO_STUB_OBJ}
    DEPENDS ${VEASO_STUB_ASM}
    COMMENT "[veaso] Compiling NASM syscall stubs to elf32 object..."
    VERBATIM
)

# Source C tambahan untuk veaso (RTL, Heap, Init)
file(GLOB VEASO_C_SOURCES
    "${VEASO_DIR}/*.c"
    "${VEASO_DIR}/*/*.c"
)

# Gabungkan C Sources + Generated NASM Object File
add_library(veaso SHARED
    ${VEASO_C_SOURCES}
    ${VEASO_STUB_OBJ}
)

# Set Properties & Include Dirs untuk veaso
target_include_directories(veaso PRIVATE ${SO_INCLUDES} ${VEASO_DIR})
target_compile_options(veaso PRIVATE ${SO_C_FLAGS})
target_link_options(veaso PRIVATE ${SO_LINK_FLAGS})

# Output nama file menjadi veaso.so
set_target_properties(veaso PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    OUTPUT_NAME "veaso"
)

# ==============================================================================
# 2. TARGET SHARED OBJECT LAINNYA (Contoh: veart.so, dll.)
# ==============================================================================
# add_subdirectory(veart)