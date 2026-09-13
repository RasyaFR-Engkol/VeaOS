import os
import re
import sys

def remove_comments(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    text = re.sub(r'//.*', '', text)
    return text

def parse_header_files(header_files):
    macros = {}
    statements = []

    for hf in header_files:
        if not os.path.exists(hf):
            continue
        with open(hf, 'r', encoding='utf-8', errors='ignore') as f:
            content = remove_comments(f.read())
            
            # Beresin Multi-line Backslash (\) dari Macro
            lines = content.splitlines()
            logical_lines = []
            current_line = ""
            for line in lines:
                stripped = line.rstrip()
                if stripped.endswith('\\'):
                    current_line += stripped[:-1] + " "
                else:
                    current_line += line
                    logical_lines.append(current_line)
                    current_line = ""
            if current_line:
                logical_lines.append(current_line)

            non_macro_lines = []

            # Ekstrak Macro & Abaikan preprocessor lain (#include, #ifdef, dll)
            for line in logical_lines:
                l_strip = line.strip()
                if l_strip.startswith('#define'):
                    match = re.match(r'#define\s+([A-Za-z0-9_]+)', l_strip)
                    if match:
                        macro_name = match.group(1)
                        clean_macro = re.sub(r'\s+', ' ', l_strip).strip()
                        macros[macro_name] = clean_macro
                elif l_strip.startswith('#'):
                    continue
                else:
                    non_macro_lines.append(line)

            # Split C Statement berdasarkan titik koma (;)
            clean_text = "\n".join(non_macro_lines)
            for stmt in clean_text.split(';'):
                s_clean = re.sub(r'\s+', ' ', stmt).strip()
                if s_clean:
                    statements.append(s_clean + ";")

    return macros, statements

def find_prototype(symbol, statements):
    pattern = r'\b' + re.escape(symbol) + r'\b'
    for stmt in statements:
        if re.search(pattern, stmt, re.IGNORECASE) and '(' in stmt and ')' in stmt:
            return stmt
    return None

def main():
    if len(sys.argv) < 4:
        print("Usage: python3 gen_ddk.py <export.lst> <veakrnl_include_dir> <output_target>")
        sys.exit(1)

    export_lst_path = sys.argv[1]
    kernel_include_dir = sys.argv[2]
    output_target = sys.argv[3]

    if output_target.endswith('.h') or os.path.isfile(output_target):
        output_dir = os.path.dirname(output_target)
    else:
        output_dir = output_target

    project_root = os.path.abspath(os.path.join(kernel_include_dir, "../.."))
    
    # Folder pencarian header kernel
    search_dirs = [
        kernel_include_dir,
        os.path.join(kernel_include_dir, "internal"),
        os.path.join(project_root, "sdk", "include")
    ]

    header_files = []
    for s_dir in search_dirs:
        if os.path.exists(s_dir):
            for root, _, files in os.walk(s_dir):
                for file in files:
                    if file.endswith('.h') and not file.endswith('veaddk.h') and not file.endswith('veadrvtype.h'):
                        header_files.append(os.path.join(root, file))

    macros, statements = parse_header_files(header_files)

    # 1. Baca export.lst
    symbols = []
    if os.path.exists(export_lst_path):
        with open(export_lst_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'): continue
                parts = line.split()
                if len(parts) >= 2 and parts[0] == 'export': symbols.append(parts[1])
                elif len(parts) == 1: symbols.append(parts[0])

    # 2. Ekstrak Deklarasi API & Macro untuk veaddk.h
    declarations = []
    processed_symbols = set()
    macros_lower = {k.lower(): k for k in macros.keys()}

    for sym in symbols:
        sym_lower = sym.lower()
        if sym_lower in processed_symbols:
            continue

        # Jika simbol adalah Macro (#define)
        if sym_lower in macros_lower:
            actual_macro_name = macros_lower[sym_lower]
            macro_def = macros[actual_macro_name]
            
            # Cari jika ada fungsi internal pasangan macro-nya (misal: KsAcquireSpinLockInternal)
            internal_func_match = re.findall(r'\b([A-Za-z0-9_]+Internal)\b', macro_def, re.IGNORECASE)
            if internal_func_match:
                internal_func = internal_func_match[0]
                proto = find_prototype(internal_func, statements)
                if proto:
                    declarations.append(f"VEA_EXPORT {proto}")

            declarations.append(macro_def)
            processed_symbols.add(sym_lower)

        # Jika simbol adalah Fungsi Biasa
        else:
            proto = find_prototype(sym, statements)
            if proto:
                declarations.append(f"VEA_EXPORT {proto}")
                processed_symbols.add(sym_lower)
            else:
                declarations.append(f"// WARNING: Prototype/Macro for '{sym}' was not found in kernel headers!")

    os.makedirs(output_dir, exist_ok=True)

    # 3. Buat template veadrvtype.h HANYA JIKA file tersebut belum ada di disk
    veadrvtype_path = os.path.join(output_dir, "veadrvtype.h")
    if not os.path.exists(veadrvtype_path):
        drvtype_template = [
            "// DDK Driver Types",
            "// Edit file ini secara manual untuk menambah struct / typedef driver DDK.",
            "",
            "#ifndef _VEADRVTYPE_H_",
            "#define _VEADRVTYPE_H_",
            "",
            "#include <procbind.h>",
            "",
            "/* Tulis struct & typedef DDK kamu di sini */",
            "",
            "#endif // _VEADRVTYPE_H_",
            ""
        ]
        with open(veadrvtype_path, 'w', encoding='utf-8') as f:
            f.write("\n".join(drvtype_template))

    # 4. Generate veaddk.h
    ddk_content = [
        "// Auto-generated by CMake & gen_ddk.py during configuration phase",
        "// DO NOT EDIT DIRECTLY! Edit veakrnl/export.lst instead.",
        "",
        "#ifndef _VEADDK_H_",
        "#define _VEADDK_H_",
        "",
        "#include <procbind.h>",
        '#include "veadrvtype.h"',
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        ""
    ]
    ddk_content.extend(declarations)
    ddk_content.extend([
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
        "",
        "#endif // _VEADDK_H_",
        ""
    ])

    veaddk_path = os.path.join(output_dir, "veaddk.h")
    with open(veaddk_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(ddk_content))

    print(f"[VeaDDK] Generated {veaddk_path} successfully ({len(declarations)} exports).")

if __name__ == '__main__':
    main()