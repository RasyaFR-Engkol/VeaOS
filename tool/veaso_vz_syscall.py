#!/usr/bin/env python3
"""
VeaOS SPDX License ----------------------------------------------------
SPDX-License-Identifier: GPL-2.0-only
FILE        : veaso_vz_syscall.py
CREATOR     : RasyaFR-Engkol / VeaOS Team
PURPOSE     : Generator for Ring 3 Vz* SYSENTER NASM Stubs & vz_user.h Header
-----------------------------------------------------------------------
"""

import os
import re
import sys

def remove_comments(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    text = re.sub(r'//.*', '', text)
    return text

def extract_return_type(proto_stmt, symbol):
    idx = proto_stmt.find(symbol)
    if idx == -1:
        return "VEASTATUS"
    prefix = proto_stmt[:idx].strip()
    prefix = re.sub(
        r'\b(VEAPI|NTAPI|WINAPI|CALLBACK|APIENTRY|FASTCALL|CDECL|EXTERN_C|extern|_Must_inspect_result_)\b',
        '',
        prefix
    )
    ret_type = re.sub(r'\s+', ' ', prefix).strip()
    return ret_type if ret_type else "VEASTATUS"

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

            non_macro_lines = [l for l in logical_lines if not l.strip().startswith('#')]
            clean_text = "\n".join(non_macro_lines)
            for stmt in split_c_statements(clean_text):
                s_clean = re.sub(r'\s+', ' ', stmt).strip()
                if s_clean:
                    statements.append(s_clean)
    return statements

def find_prototype(symbol, statements):
    pattern = r'\b' + re.escape(symbol) + r'\b'
    for stmt in statements:
        if '{' not in stmt and re.search(pattern, stmt) and '(' in stmt and ')' in stmt:
            return stmt
    return None

def extract_params_from_prototype(proto_stmt, symbol):
    match = re.search(re.escape(symbol) + r'\s*\((.*?)\)', proto_stmt, re.DOTALL)
    if match:
        return match.group(1).strip()
    return None

def main():
    if len(sys.argv) < 4:
        print("Usage: python3 veaso_vz_syscall.py <syscall.lst> <out_vz_user.h> <out_vz_syscall_veaso.asm> [search_include_dir]")
        sys.exit(1)

    syscall_lst_path = sys.argv[1]
    output_h = sys.argv[2]
    output_asm = sys.argv[3]
    search_dir_arg = sys.argv[4] if len(sys.argv) > 4 else None

    syscall_dir = os.path.dirname(os.path.abspath(syscall_lst_path))
    project_root = os.path.abspath(os.path.join(syscall_dir, ".."))

    search_dirs = []
    if search_dir_arg:
        search_dirs.append(os.path.abspath(search_dir_arg))
    search_dirs.extend([
        os.path.join(project_root, "sdk", "include"),
        os.path.join(project_root, "sdk", "include", "ndk"),
        os.path.join(project_root, "veakrnl", "include")
    ])

    header_files = []
    for s_dir in search_dirs:
        if os.path.exists(s_dir):
            for root, _, files in os.walk(s_dir):
                for file in files:
                    if file.endswith('.h') and not file.endswith('vz_user.h') and not file.endswith('vz_wrappers.h'):
                        header_files.append(os.path.join(root, file))

    statements = parse_header_files(header_files)

    asm_header = """; VeaOS SPDX License ----------------------------------------------------
; Auto-generated Ring 3 Vz* SYSENTER Stubs for veaso.so (NASM 32-bit)
; DO NOT EDIT MANUALLY
; -----------------------------------------------------------------------

[BITS 32]
SECTION .text

"""

    h_header = """/* VeaOS SPDX License ----------------------------------------------------
 * Auto-generated User-Mode Vz* Syscall Declarations
 * DO NOT EDIT MANUALLY
 * ----------------------------------------------------------------------- */

#ifndef _VZ_USER_H
#define _VZ_USER_H

/* kernel essential */
#include <procbind.h>
#include <ldrtypes.h>
#include <ul.h>
#include <kdp.h>
#include <mmtype.h>
#include <mm.h>
#include <rtl.h>
#include <ks.h>
#include <hct.h>
#include <obtype.h>
#include <obfunc.h>
#include <veastatus.h>
#include <tag.h>
#include <sftypes.h>
#include <sffunc.h>
#include <pttypes.h>
#include <ptfunc.h>
#include <vktypes.h>
#include <vkfunc.h>
#include <guid.h>
#include <potypes.h>
#include <pofunc.h>
#include <../psdk/veaseh.h>

#ifdef __cplusplus
extern "C" {
#endif

"""

    h_footer = """
#ifdef __cplusplus
}
#endif

#endif /* _VZ_USER_H */
"""

    asm_stubs = []
    h_declarations = []

    if os.path.exists(syscall_lst_path):
        with open(syscall_lst_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                parts = line.split()
                if len(parts) >= 4:
                    raw_ssn = parts[0]
                    name = parts[1]
                    arg_count = int(parts[2])
                    stub_symbol = parts[3]

                    try:
                        ssn_val = int(raw_ssn, 0)
                        ssn_str = f"0x{ssn_val:02X}"
                    except ValueError:
                        ssn_str = raw_ssn

                    if stub_symbol.startswith("Vea"):
                        vz_symbol = "Vz" + stub_symbol[3:]
                        vea_symbol = stub_symbol
                    else:
                        vz_symbol = f"Vz{name}"
                        vea_symbol = f"Vea{name}"

                    found_symbol = vea_symbol
                    proto_stmt = find_prototype(vea_symbol, statements) or find_prototype(stub_symbol, statements) or find_prototype(name, statements)
                    if proto_stmt:
                        if find_prototype(vea_symbol, statements):
                            found_symbol = vea_symbol
                        elif find_prototype(stub_symbol, statements):
                            found_symbol = stub_symbol
                        else:
                            found_symbol = name

                    params_decl = None
                    if proto_stmt:
                        extracted_params = extract_params_from_prototype(proto_stmt, found_symbol)
                        if extracted_params:
                            params_decl = extracted_params

                    if not params_decl:
                        if arg_count == 0:
                            params_decl = "VOID"
                        else:
                            params_decl = ", ".join([f"ULONG_PTR Arg{i+1}" for i in range(arg_count)])

                    ret_type = extract_return_type(proto_stmt, found_symbol) if proto_stmt else "VEASTATUS"
                    h_declarations.append(f"{ret_type} VEAPI {vz_symbol}({params_decl});")

                    ret_inst = f"RET {arg_count * 4}" if arg_count > 0 else "RET"

                    asm_code = f"""GLOBAL {vz_symbol}:function ({vz_symbol}.end - {vz_symbol})
{vz_symbol}:
    MOV eax, {ssn_str}
    CALL .sysenter
    {ret_inst}
.sysenter:
    MOV edx, esp
    SYSENTER
.end:
"""
                    asm_stubs.append(asm_code)

    os.makedirs(os.path.dirname(os.path.abspath(output_h)), exist_ok=True)
    with open(output_h, 'w', encoding='utf-8') as f:
        f.write(h_header + "\n".join(h_declarations) + h_footer)

    os.makedirs(os.path.dirname(os.path.abspath(output_asm)), exist_ok=True)
    with open(output_asm, 'w', encoding='utf-8') as f:
        f.write(asm_header + "\n".join(asm_stubs))

    print(f"[GENVEASO_VZ] Successfully generated {len(h_declarations)} Vz user stubs:")
    print(f"  -> Header : {output_h}")
    print(f"  -> ASM    : {output_asm}")

if __name__ == '__main__':
    main()