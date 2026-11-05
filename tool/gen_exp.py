import os
import re
import sys

def remove_comments(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    text = re.sub(r'//.*', '', text)
    return text

def split_c_statements(text):
    statements = []
    start = 0
    brace_depth = 0
    function_body = False
    quote = None
    escaped = False

    for index, char in enumerate(text):
        if quote:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == quote:
                quote = None
            continue

        if char in ('"', "'"):
            quote = char
        elif char == '{':
            if brace_depth == 0:
                prefix = text[start:index]
                function_body = (
                    '(' in prefix
                    and ')' in prefix
                    and '=' not in prefix
                    and not re.search(r'\b(struct|union|enum)\b', prefix)
                )
            brace_depth += 1
        elif char == '}':
            brace_depth -= 1
            if brace_depth == 0 and function_body:
                statement = text[start:index + 1].strip()
                if statement:
                    statements.append(statement)
                start = index + 1
                function_body = False
        elif char == ';' and brace_depth == 0:
            statement = text[start:index].strip()
            if statement:
                statements.append(statement + ';')
            start = index + 1

    remainder = text[start:].strip()
    if remainder:
        statements.append(remainder)

    return statements

def parse_header_files(header_files):
    macros = {}
    statements = []

    for hf in header_files:
        if not os.path.exists(hf):
            continue
        with open(hf, 'r', encoding='utf-8', errors='ignore') as f:
            content = remove_comments(f.read())
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

            clean_text = "\n".join(non_macro_lines)
            for stmt in split_c_statements(clean_text):
                s_clean = re.sub(r'\s+', ' ', stmt).strip()
                if s_clean:
                    statements.append(s_clean)

    return macros, statements

def find_prototype(symbol, statements):
    pattern = r'\b' + re.escape(symbol) + r'\b'
    for stmt in statements:
        if '{' not in stmt and re.search(pattern, stmt, re.IGNORECASE) and '(' in stmt and ')' in stmt:
            return stmt
    return None

def main():
    if len(sys.argv) < 4:
        print("Usage: python3 gen_exp.py <export.lst> <kernel_include_dir> <output_autexp.c>")
        sys.exit(1)

    export_lst_path = sys.argv[1]
    kernel_include_dir = sys.argv[2]
    output_c_path = sys.argv[3]

    project_root = os.path.abspath(os.path.join(kernel_include_dir, "../.."))
    
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
                    if file.endswith('.h') and not file.endswith('veaddk.h'):
                        header_files.append(os.path.join(root, file))

    macros, statements = parse_header_files(header_files)

    symbols = []
    if os.path.exists(export_lst_path):
        with open(export_lst_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'): continue
                parts = line.split()
                if len(parts) >= 2 and parts[0] == 'export': symbols.append(parts[1])
                elif len(parts) == 1: symbols.append(parts[0])

    unique_symbols = list(dict.fromkeys(symbols))

    valid_exports = []
    for sym in unique_symbols:
        # Cek apakah simbol punya prototipe fungsi nyata
        if find_prototype(sym, statements):
            valid_exports.append(sym)
        else:
            # Jika berupa macro yang memanggil fungsi internal (misal ...Internal)
            if sym in macros:
                internal_match = re.findall(r'\b([A-Za-z0-9_]+Internal)\b', macros[sym])
                if internal_match and find_prototype(internal_match[0], statements):
                    valid_exports.append(internal_match[0])

    valid_exports = list(dict.fromkeys(valid_exports))

    c_content = [
        "// Auto-generated by gen_exp.py during build configuration phase.",
        "// DO NOT EDIT DIRECTLY! Edit veakrnl/export.lst instead.",
        "",
        "#include <procbind.h>",
        "",
        "typedef struct _VEA_EXPORT_ENTRY {",
        "    const char *Name;",
        "    const void *Address;",
        "} VEA_EXPORT_ENTRY, *PVEA_EXPORT_ENTRY;",
        "",
        "/* Forward Declarations */"
    ]

    for sym in valid_exports:
        c_content.append(f"extern void {sym}(void);")

    c_content.extend([
        "",
        '__attribute__((section(".veasymboltable"), used))',
        "const VEA_EXPORT_ENTRY VeaKernelExportTable[] = {"
    ])

    for sym in valid_exports:
        c_content.append(f'    {{ "{sym}", (const void*)&{sym} }},')

    c_content.extend([
        "    { 0, 0 }",
        "};",
        ""
    ])

    os.makedirs(os.path.dirname(os.path.abspath(output_c_path)), exist_ok=True)
    with open(output_c_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(c_content))

    print(f"[VeaExport] Generated {output_c_path} successfully ({len(valid_exports)} valid function symbols).")

if __name__ == '__main__':
    main()