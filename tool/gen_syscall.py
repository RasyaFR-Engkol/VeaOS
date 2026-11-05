#!/usr/bin/env python3
import sys

if len(sys.argv) < 3:
    print("Usage: gen_syscall.py <input syscall.lst> <output stub.asm>")
    sys.exit(1)

input_lst = sys.argv[1]
output_asm = sys.argv[2]

header = """; VeaOS SPDX License ----------------------------------------------------
; Auto-generated Syscall Stubs for veaso.so (NASM 32-bit)
; DO NOT EDIT MANUALLY
; -----------------------------------------------------------------------

[BITS 32]

SECTION .text

"""

stubs = []

with open(input_lst, 'r') as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        parts = line.split()
        if len(parts) >= 4:
            ssn = parts[0]
            symbol = parts[3]
            
            stub_code = f"""GLOBAL {symbol}
{symbol}:
    MOV eax, {ssn}
    LEA edx, [esp + 4]
    SYSENTER
    RET
"""
            stubs.append(stub_code)

with open(output_asm, 'w') as f:
    f.write(header)
    f.write("\n".join(stubs))

print(f"[GENSYSCALL] Generated {len(stubs)} NASM stubs into {output_asm}")