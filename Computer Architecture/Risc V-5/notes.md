# RISC-V 64 Instruction Set Reference 🚀

## Table of Contents
1. [Register Overview](#register-overview)
2. [Data Movement Instructions](#data-movement-instructions)
3. [Arithmetic Instructions](#arithmetic-instructions)
4. [Memory Instructions (Load/Store)](#memory-instructions-loadstore)
5. [Logical Instructions](#logical-instructions)
6. [Shift Instructions](#shift-instructions)
7. [Branch/Jump Instructions](#branchjump-instructions)
8. [Pseudoinstructions (Shortcuts)](#pseudoinstructions-shortcuts)
9. [Assembler Directives](#assembler-directives)
10. [Instruction Formats](#instruction-formats)
11. [Syscall Reference](#syscall-reference)
12. [Quick Reference Card](#quick-reference-card)

---

## Register Overview

| Register | ABI Name | Purpose | Preserved? |
|----------|----------|---------|------------|
| `x0` | `zero` | Always 0 (read-only) | - |
| `x1` | `ra` | Return Address | Caller |
| `x2` | `sp` | Stack Pointer | Callee |
| `x3` | `gp` | Global Pointer | - |
| `x4` | `tp` | Thread Pointer | - |
| `x5-7` | `t0-2` | Temporaries | Caller |
| `x8` | `s0/fp` | Saved/Frame Pointer | Callee |
| `x9` | `s1` | Saved register | Callee |
| `x10-11` | `a0-1` | Args/Return values | Caller |
| `x12-17` | `a2-7` | Function arguments | Caller |
| `x18-27` | `s2-11` | Saved registers | Callee |
| `x28-31` | `t3-6` | Temporaries | Caller |

**Note:** *Caller-saved* = Function must save if needed; *Callee-saved* = Function must restore original value

---

## Data Movement Instructions

| Instruction | Full Name | Syntax | Operation | Example |
|-------------|-----------|--------|-----------|---------|
| `li` | Load Immediate | `li rd, imm` | `rd = imm` | `li t0, 42` |
| `la` | Load Address | `la rd, label` | `rd = &label` | `la t1, my_data` |
| `mv` | Move | `mv rd, rs` | `rd = rs` | `mv a0, t2` |

---

## Arithmetic Instructions

| Instruction | Full Name | Syntax | Operation | Example |
|-------------|-----------|--------|-----------|---------|
| `add` | Add | `add rd, rs1, rs2` | `rd = rs1 + rs2` | `add t2, t0, t1` |
| `addi` | Add Immediate | `addi rd, rs, imm` | `rd = rs + imm` | `addi t0, t0, 5` |
| `sub` | Subtract | `sub rd, rs1, rs2` | `rd = rs1 - rs2` | `sub t2, t0, t1` |
| `mul` | Multiply | `mul rd, rs1, rs2` | `rd = rs1 × rs2` | `mul t2, t0, t1` |
| `div` | Divide | `div rd, rs1, rs2` | `rd = rs1 ÷ rs2` | `div t2, t0, t1` |
| `rem` | Remainder | `rem rd, rs1, rs2` | `rd = rs1 % rs2` | `rem t2, t0, t1` |
| `neg` | Negate (pseudo) | `neg rd, rs` | `rd = -rs` | `neg t2, t0` |

---

## Memory Instructions (Load/Store)

### Load Instructions (Memory → Register)

| Instruction | Full Name | Syntax | Operation | Example |
|-------------|-----------|--------|-----------|---------|
| `lb` | Load Byte | `lb rd, offset(rs)` | Load 1 byte (sign-extend) | `lb t2, 0(t1)` |
| `lbu` | Load Byte Unsigned | `lbu rd, offset(rs)` | Load 1 byte (zero-extend) | `lbu t2, 0(t1)` |
| `lh` | Load Halfword | `lh rd, offset(rs)` | Load 2 bytes (sign-extend) | `lh t2, 0(t1)` |
| `lhu` | Load Halfword Unsigned | `lhu rd, offset(rs)` | Load 2 bytes (zero-extend) | `lhu t2, 0(t1)` |
| `lw` | Load Word | `lw rd, offset(rs)` | Load 4 bytes (32-bit) | `lw t2, 0(t1)` |
| `lwu` | Load Word Unsigned | `lwu rd, offset(rs)` | Load 4 bytes (zero-extend) | `lwu t2, 0(t1)` |
| `ld` | Load Doubleword | `ld rd, offset(rs)` | Load 8 bytes (64-bit) | `ld t2, 0(t1)` |

### Store Instructions (Register → Memory)

| Instruction | Full Name | Syntax | Operation | Example |
|-------------|-----------|--------|-----------|---------|
| `sb` | Store Byte | `sb rs, offset(rd)` | Store 1 byte | `sb t0, 0(t1)` |
| `sh` | Store Halfword | `sh rs, offset(rd)` | Store 2 bytes | `sh t0, 0(t1)` |
| `sw` | Store Word | `sw rs, offset(rd)` | Store 4 bytes | `sw t0, 4(t1)` |
| `sd` | Store Doubleword | `sd rs, offset(rd)` | Store 8 bytes | `sd t0, 8(t1)` |

**Address Calculation:** `address = base_register + offset`
**Offset Range:** -2048 to +2047 (12-bit signed)

---

## Logical Instructions

| Instruction | Full Name | Syntax | Operation | Example |
|-------------|-----------|--------|-----------|---------|
| `and`       | AND | `and rd, rs1, rs2` | `rd = rs1 & rs2` | `and t2, t0, t1` |
| `or`        | OR | `or rd, rs1, rs2` | `rd = rs1 | rs2` | `or t2, t0, t1` |
| `xor`       | XOR | `xor rd, rs1, rs2` | `rd = rs1 ^ rs2` | `xor t2, t0, t1` |
| `andi`      | AND Immediate | `andi rd, rs, imm` | `rd = rs & imm` | `andi t2, t0, 0xFF` |
| `ori`       | OR Immediate | `ori rd, rs, imm` | `rd = rs | imm` | `ori t2, t0, 0xF0` |
| `xori`      | XOR Immediate | `xori rd, rs, imm` | `rd = rs ^ imm` | `xori t2, t0, -1` |
| `not`       | NOT (pseudo) | `not rd, rs` | `rd = ~rs` | `not t2, t0` |

---

## Shift Instructions

| Instruction | Full Name | Syntax | Operation | Example |
|-------------|-----------|--------|-----------|---------|
| `sll` | Shift Left Logical | `sll rd, rs1, rs2` | `rd = rs1 << rs2` | `sll t2, t0, t1` |
| `slli` | Shift Left Logical Imm | `slli rd, rs1, imm` | `rd = rs1 << imm` | `slli t2, t0, 4` |
| `srl` | Shift Right Logical | `srl rd, rs1, rs2` | `rd = rs1 >> rs2` (0 fill) | `srl t2, t0, t1` |
| `srli` | Shift Right Logical Imm | `srli rd, rs1, imm` | `rd = rs1 >> imm` (0 fill) | `srli t2, t0, 4` |
| `sra` | Shift Right Arithmetic | `sra rd, rs1, rs2` | `rd = rs1 >> rs2` (sign fill) | `sra t2, t0, t1` |
| `srai` | Shift Right Arith Imm | `srai rd, rs1, imm` | `rd = rs1 >> imm` (sign fill) | `srai t2, t0, 4` |

**Shift Amount:** For 64-bit, only lower 6 bits of shift amount are used (0-63)

---

## Branch/Jump Instructions

### Conditional Branches

| Instruction | Full Name | Syntax | Condition | Example |
|-------------|-----------|--------|-----------|---------|
| `beq`       | Branch if Equal | `beq rs1, rs2, label` | `rs1 == rs2` | `beq t0, t1, loop` |
| `bne`       | Branch if Not Equal | `bne rs1, rs2, label` | `rs1 != rs2` | `bne t0, t1, else` |
| `blt`       | Branch if Less Than | `blt rs1, rs2, label` | `rs1 < rs2` (signed) | `blt t0, t1, case` |
| `bge`       | Branch if Greater/Equal | `bge rs1, rs2, label` | `rs1 >= rs2` (signed) | `bge t0, t1, done` |
| `bltu`      | Branch Less Than Unsigned | `bltu rs1, rs2, label` | `rs1 < rs2` (unsigned) | `bltu t0, t1, label` |
| `bgeu`      | Branch Greater/Equal Unsigned | `bgeu rs1, rs2, label` | `rs1 >= rs2` (unsigned) | `bgeu t0, t1, label` |

### Unconditional Jumps

| Instruction | Full Name              | Syntax                | Operation                   | Example |
|-------------|-----------             |--------               |-----------                  |---------|
| `j`         | Jump (pseudo)          | `j label`             | `PC = label`                | `j done` |
| `jr`        | Jump Register          | `jr rs`               | `PC = rs`                   | `jr ra` |
| `jal`       | Jump And Link          | `jal rd, label`       | `rd = PC+4, PC = label`     | `jal ra, function` |
| `jalr`      | Jump And Link Register | `jalr rd, rs, offset` | `rd = PC+4, PC = rs+offset` | `jalr ra, t0, 0` |
| `ret`       | Return (pseudo)        | `ret`                 | `PC = ra`                   | `ret` |

**Function Call Pattern:**
```assembly
main:
    li a0, 5           # argument
    jal ra, my_func    # call function
    # returns here (ra points here)

my_func:
    add a0, a0, a0     # do something
    jr ra              # return



## Pseudoinstructions (Shortcuts)

These aren't real instructions - the assembler converts them to real ones:

| Pseudo            | Real Instructions | Meaning |
|--------           |-------------------|---------|
| `nop`             | `addi x0, x0, 0`  | No operation |
| `ret`             | `jalr x0, ra, 0`  | Return from function |
| `j label`         | `jal x0, label`   | Jump to label |
| `jal label`       | `jal ra, label`   | Call function (using ra) |
| `jr rs`           | `jalr x0, rs, 0`  | Jump to address in rs |
| `mv rd, rs`       | `addi rd, rs, 0`  | Copy register |
| `not rd, rs`      | `xori rd, rs, -1` | Bitwise NOT |
| `neg rd, rs`      | `sub rd, x0, rs`  | Negate (two's complement) |
| `bgt rs1, rs2, L` | `blt rs2, rs1, L` | Branch if greater than |
| `ble rs1, rs2, L` | `bge rs2, rs1, L` | Branch if less or equal |
| `call func`       | `auipc ra, offset` + `jalr ra, ra, offset` | Call far function |

---

## Assembler Directives

| Directive  | Description                   | Example |
|----------- |-------------                  |---------|
| `.globl`   | Make symbol visible to linker | `.globl main` |
| `.text`    | Start of code section         | `.text` |
| `.data`    | Start of data section         | `.data` |
| `.rodata`  | Start of read-only data       | `.rodata` |
| `.section` | Custom section                | `.section .mysec` |

### Data Allocation

| Directive | Size                 | Example |
|-----------|------                | ---------|
| `.byte`   | 1 byte               | `my_byte: .byte 0xFF` |
| `.half`   | 2 bytes              | `my_half: .half 0x1234` |
| `.word`   | 4 bytes              | `my_word: .word 0x12345678` |
| `.dword`  | 8 bytes              | `my_dword: .dword 0x123456789ABCDEF0` |
| `.asciz`  | String + null        | `str: .asciz "Hello"` |
| `.ascii`  | String only          | `str: .ascii "Hello"` |
| `.space`  | Reserve bytes        | `buffer: .space 100` |
| `.zero`   | Reserve zeroed bytes | `array: .zero 64` |
| `.align`  | Align to power of 2  | `.align 3` (8-byte align) |

### Examples:
```assembly
.data
var1:      .word 42                 # single word
array:     .word 1, 2, 3, 4, 5      # array of 5 words
buffer:    .space 256                # 256 bytes of space
message:   .asciz "Hello World\n"    # null-terminated string
.align 3                             # align to 8-byte boundary
new_var:   .dword 0xDEADBEEF         # 8-byte value




| a7 | Name    | Description    | a0            | a1 | a2 |
|----|------   |-------------   |---------------|-----|-----|
| 64 | `write` | Write to file  | fd (1=stdout) | buffer* | length |
| 63 | `read`  | Read from file | fd (0=stdin)  | buffer* | length |
| 93 | `exit`  | Exit program   | exit code     | - | - |
| 57 | `close` | Close file     | fd            | - | - |
| 56 | `open`  | Open file      | filename*     | flags | mode |
| 62 | `lseek` | Seek in file   | fd | offset   | whence |
| 214| `brk`   | Change heap    | new address   | - | - |