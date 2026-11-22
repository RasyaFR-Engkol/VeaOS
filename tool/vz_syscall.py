#!/usr/bin/env python3
"""
VeaOS SPDX License ----------------------------------------------------
SPDX-License-Identifier: GPL-2.0-only
FILE        : vz_syscall.py
CREATOR     : RasyaFR-Engkol / VeaOS Team
PURPOSE     : Dynamic Header-based Generator for Vz NASM Syscall Wrappers
-----------------------------------------------------------------------
"""

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
    if len(sys.argv) < 3:
        print("Usage: python3 vz_syscall.py <syscall.lst> <out_vz_wrappers.asm> [out_vz_wrappers.h] [kernel_include_dir]")
        sys.exit(1)

    syscall_lst_path = sys.argv[1]
    output_asm = sys.argv[2]
    output_h = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] != "NONE" else None
    kernel_include_dir = sys.argv[4] if len(sys.argv) > 4 else None

    syscall_dir = os.path.dirname(os.path.abspath(syscall_lst_path))
    project_root = os.path.abspath(os.path.join(syscall_dir, ".."))

    search_dirs = []
    if kernel_include_dir:
        search_dirs.append(os.path.abspath(kernel_include_dir))
    search_dirs.extend([
        os.path.join(syscall_dir, "include"),
        os.path.join(project_root, "veakrnl", "include"),
        os.path.join(project_root, "sdk", "include")
    ])

    header_files = []
    for s_dir in search_dirs:
        if os.path.exists(s_dir):
            for root, _, files in os.walk(s_dir):
                for file in files:
                    if file.endswith('.h') and not file.endswith('vz_wrappers.h'):
                        header_files.append(os.path.join(root, file))

    statements = parse_header_files(header_files)

    asm_header = """[BITS 32]

; VeaOS SPDX License ----------------------------------------------------
; Auto-generated Ring 0 Kernel Vz* NASM Syscall Stubs
; DO NOT EDIT MANUALLY
; -----------------------------------------------------------------------

%define KGDT32_R0_CODE 0x08
%define KGDT32_R0_DATA 0x10
%define KGDT32_R0_PCR  0x30

STRUC KTRAP_FRAME
    .DbgEbp               RESD 1
    .DbgEip               RESD 1
    .DbgArgMark           RESD 1
    .DbgArgPointer        RESD 1
    .TempSegCs            RESD 1
    .TempEsp              RESD 1
    .Dr0                  RESD 1
    .Dr1                  RESD 1
    .Dr2                  RESD 1
    .Dr3                  RESD 1
    .Dr6                  RESD 1
    .Dr7                  RESD 1
    .SegGs                RESD 1
    .SegEs                RESD 1
    .SegDs                RESD 1
    .Edx                  RESD 1
    .Ecx                  RESD 1
    .Eax                  RESD 1
    .PreviousPreviousMode RESD 1
    .ExceptionList        RESD 1
    .SegFs                RESD 1
    .Edi                  RESD 1
    .Esi                  RESD 1
    .Ebx                  RESD 1
    .Ebp                  RESD 1
    .ErrCode              RESD 1
    .Eip                  RESD 1
    .SegCs                RESD 1
    .EFlags               RESD 1
    .HardwareEsp          RESD 1
    .HardwareSegSs        RESD 1
    .V86Es                RESD 1
    .V86Ds                RESD 1
    .V86Fs                RESD 1
    .V86Gs                RESD 1
ENDSTRUC

EXTERN RiSyscallDispatcherHandler

SECTION .text
"""

    h_header = """/* VeaOS SPDX License ----------------------------------------------------
- Auto-generated Kernel Vz* Syscall Declarations
- DO NOT EDIT MANUALLY
- ----------------------------------------------------------------------- */

#ifndef _VZ_WRAPPERS_H
#define _VZ_WRAPPERS_H

#include <veakrnl.h>

#ifdef __cplusplus
extern "C" {
#endif

"""

    h_footer = """
#ifdef __cplusplus
}
#endif

#endif /* _VZ_WRAPPERS_H */
"""

    asm_wrappers = []
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
                    elif stub_symbol.startswith("Zw") or stub_symbol.startswith("Nt"):
                        vz_symbol = "Vz" + stub_symbol[2:]
                    else:
                        vz_symbol = f"Vz{name}"

                    found_symbol = stub_symbol
                    proto_stmt = find_prototype(stub_symbol, statements)
                    if not proto_stmt:
                        alt_symbol = f"Vea{name}"
                        proto_stmt = find_prototype(alt_symbol, statements)
                        if proto_stmt:
                            found_symbol = alt_symbol
                        else:
                            proto_stmt = find_prototype(name, statements)
                            if proto_stmt:
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

                    arg_bytes = arg_count * 4
                    edx_arg_setup = f"lea eax, [esp + KTRAP_FRAME_size + 4]" if arg_count > 0 else "mov eax, 0"

                    asm_code = f"""GLOBAL {vz_symbol}
ALIGN 16
{vz_symbol}:
    sub esp, KTRAP_FRAME_size
    mov [esp + KTRAP_FRAME.Ebp], ebp
    mov [esp + KTRAP_FRAME.Ebx], ebx
    mov [esp + KTRAP_FRAME.Esi], esi
    mov [esp + KTRAP_FRAME.Edi], edi
    mov dword [esp + KTRAP_FRAME.SegDs], 0
    mov dword [esp + KTRAP_FRAME.SegEs], 0
    mov dword [esp + KTRAP_FRAME.SegFs], 0
    mov dword [esp + KTRAP_FRAME.SegGs], 0
    mov dword [esp + KTRAP_FRAME.Eax], {ssn_str}
    {edx_arg_setup}
    mov [esp + KTRAP_FRAME.Edx], eax
    mov dword [esp + KTRAP_FRAME.PreviousPreviousMode], 0 ; KernelMode
    mov dword [esp + KTRAP_FRAME.Eip], .{vz_symbol}_ret
    mov dword [esp + KTRAP_FRAME.SegCs], KGDT32_R0_CODE
    mov dword [esp + KTRAP_FRAME.EFlags], 0x0202

    push dword 0
    lea eax, [esp + 4]
    push eax
    call RiSyscallDispatcherHandler

.{vz_symbol}_ret:
    ret {arg_bytes}
"""
                    asm_wrappers.append(asm_code)
                    h_declarations.append(f"VEASTATUS VEAPI {vz_symbol}({params_decl});")

    os.makedirs(os.path.dirname(os.path.abspath(output_asm)), exist_ok=True)
    with open(output_asm, 'w', encoding='utf-8') as f:
        f.write(asm_header + "\n")
        f.write("\n".join(asm_wrappers))

    if output_h:
        os.makedirs(os.path.dirname(os.path.abspath(output_h)), exist_ok=True)
        with open(output_h, 'w', encoding='utf-8') as f:
            f.write(h_header)
            f.write("\n".join(h_declarations))
            f.write(h_footer)

    print(f"[GENVZ] NASM stub generation complete. Generated {len(asm_wrappers)} Vz syscall wrappers.")

if __name__ == '__main__':
    main()