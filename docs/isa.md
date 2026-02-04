# LampVM ISA Specification

## 1. General

LampVM is a **register-based virtual machine** with **64-bit fixed-width instructions**
and **32-bit general-purpose registers**, designed for system-level and educational use.

- Instruction width: 64 bit
- Register count: 32 (`r0` – `r31`)
- Register width: 32 bit
- Address space: 32 bit, byte-addressed
- Byte order: Little Endian
- Execution model: Sequential execution with interrupt support

---

## 2. Instruction Format

All instructions use a unified 64-bit format:

```
63 56 55 48 47 40 39 32 31 0
+-------------+------------+------------+------------+-------------+
| opcode | rd | rs1 | rs2 | imm |
+-------------+------------+------------+------------+-------------+
```

### Instruction Fields

| Field  | Width  | Description                |
|--------|--------|----------------------------|
| opcode | 8 bit  | Operation code             |
| rd     | 8 bit  | Destination register       |
| rs1    | 8 bit  | Source register 1          |
| rs2    | 8 bit  | Source register 2          |
| imm    | 32 bit | Immediate field (instruction-defined interpretation) |

Unused fields **must be set to 0**.

---

## 3. Register Convention

- `r0` – `r30`: General-purpose registers
- `r31`: **Interrupt number parameter register**

All registers may be freely used by regular instructions and ISRs.

When entering an interrupt service routine (ISR), the VM **automatically saves and restores
all registers**.

---

## 4. FLAGS Register

FLAGS is a 32-bit integer register containing arithmetic and comparison status.

### Defined Flags

| Flag | Name          | Meaning                            |
|------|---------------|------------------------------------|
| ZF   | Zero Flag     | **Result = 0 / Compare Equal → 1** |
| SF   | Sign Flag     | Signed result is negative → 1      |
| CF   | Carry Flag    | Unsigned carry / borrow            |
| OF   | Overflow Flag | Signed overflow                    |

### Core Rule (Hard ABI Rule)

> **ZF = 1 means “result is zero” or “comparison is equal”.**

This rule is fixed and applies to all current and future instructions.

### ZF Update Semantics

- Arithmetic / logic instructions:  
  Result = 0 → `ZF = 1`
- `CMP` / `CMPI`:  
  Operands equal → `ZF = 1`

---

## 5. FLAGS Update Rules

### Instructions that update **all flags** (ZF, SF, CF, OF)

- `ADD`
- `SUB`
- `CMP`
- `CMPI`
- `INC`
- `ADDI`
- `SUBI`

### Instructions that update **ZF and SF only**, and **clear CF and OF**

- `MUL`
- `DIV` (when divisor ≠ 0)
- `MOD` (when divisor ≠ 0)
- `FADD`
- `FSUB`
- `FMUL`
- `FDIV`
- `FNEG`
- `FABS`
- `FSQRT`
- `ITOF`
- `FLOAD32`
- `AND`
- `OR`
- `XOR`
- `NOT`
- `SHL`
- `SHR`
- `SAR`
- `ANDI`
- `ORI`
- `XORI`
- `SHLI`
- `SHRI`
- `MOV`
- `MOVI`
- `LOAD`
- `LOAD32`
- `LOADX32`
- `POP`
- `FTOI` (when input is finite and in-range)

### Instructions that do **not guarantee FLAGS state**

- `STORE`
- `STORE32`
- `STOREX32`
- `FSTORE32`
- `CALL`
- `RET`
- `INT`
- `IRET`
- `HALT`
- `MEMSET`
- `MEMCPY`
- `IN`
- `OUT`

Programs must not rely on FLAGS after these instructions.

---

## 6. Arithmetic and Logic Instructions

### ADD rd, rs1, rs2

``` 
rd = rs1 + rs2
```

Updates ZF, SF, CF, OF.

---

### SUB rd, rs1, rs2

``` 
rd = rs1 - rs2
```

Updates ZF, SF, CF, OF.

---

### MUL rd, rs1, rs2

```
rd = rs1 * rs2
```

Updates ZF, SF.  
Clears CF, OF.

---

### DIV rd, rs1, rs2

``` 
rd = rs1 / rs2
```

- If `rs2 == 0`: triggers `INT_DIVIDE_BY_ZERO`
- Otherwise, updates ZF, SF and clears CF, OF

---

### MOD rd, rs1, rs2

``` 
rd = rs1 % rs2
```

- If `rs2 == 0`: triggers `INT_DIVIDE_BY_ZERO`

---

### AND / OR / XOR / NOT

Logical and shift instructions update ZF and SF, and clear CF and OF.

---

### SHL / SHR / SAR rd, rs1, rs2

```
sh = rs2 & 31
SHL: rd = (uint32)rs1 << sh
SHR: rd = (uint32)rs1 >> sh
SAR: rd = (int32)rs1 >> sh
```

- Update ZF and SF
- Clear CF and OF

---

### INC rd

```
rd = rd + 1
```

Updates ZF, SF, CF, OF (same as `ADD`).

---

### ADDI / SUBI rd, rs1, imm

```
ADDI: rd = rs1 + imm
SUBI: rd = rs1 - imm
```

- Same FLAGS semantics as `ADD` / `SUB`

---

### ANDI / ORI / XORI rd, rs1, imm

Bitwise immediate operations.

- Update ZF and SF
- Clear CF and OF

---

### SHLI / SHRI rd, rs1, imm

```
sh = imm & 31
SHLI: rd = (uint32)rs1 << sh
SHRI: rd = (uint32)rs1 >> sh
```

- Update ZF and SF
- Clear CF and OF

---

## 7. Floating-Point (F32) Instructions

All floating-point instructions interpret register contents as **IEEE 754 binary32** (32-bit float).
Values are stored in integer registers **bitwise**, with no conversion unless explicitly stated.

### FADD / FSUB / FMUL / FDIV rd, rs1, rs2

```
rd = (float)rs1 ⊕ (float)rs2
```

Where ⊕ is `+`, `-`, `*`, or `/`.

- Updates ZF and SF based on the float result (ZF = 1 if result is +0.0 or -0.0, SF = 1 if result < 0)
- Clears CF and OF
- Division by 0 follows IEEE 754 behavior (no interrupt)

---

### FNEG / FABS / FSQRT rd, rs1

```
FNEG:  rd = -rs1
FABS:  rd = abs(rs1)
FSQRT: rd = sqrt(rs1)
```

- Updates ZF and SF based on the float result
- Clears CF and OF

---

### FCMP rd, rs1

```
compare (float)rd vs (float)rs1
```

Flags are set as:

- If either operand is NaN: **OF = 1**, all other flags cleared
- Else if equal: **ZF = 1**
- Else if rd < rs1: **SF = 1**
- Else if rd > rs1: **CF = 1**

---

### ITOF rd, rs1

```
rd = (float)(int32)rs1
```

- Updates ZF and SF based on the float result
- Clears CF and OF

---

### FTOI rd, rs1

```
rd = (int32)(float)rs1
```

- If input is NaN or outside int32 range: `rd = 0`, **OF = 1**, ZF/SF/CF cleared
- Otherwise: updates ZF and SF based on the integer result; clears CF and OF

---

### FLOAD32 rd, [rs1 + imm]

- Reads 32-bit value and **reinterprets bits** as float32
- Updates ZF and SF based on the float value; clears CF and OF

---

### FSTORE32 [rs1 + imm], rd

- Stores lower 32 bits of `rd` as raw float32 bits
- Does not modify FLAGS

---

## 8. Comparison Instructions

### CMP rd, rs1

``` 
tmp = rd - rs1
```

- No register is written
- ZF = 1 if operands are equal
- SF, CF, OF are set according to subtraction result

---

### CMPI rd, imm

``` 
tmp = rd - imm
```

Same semantics as `CMP`.

---

## 9. Control Flow Instructions

All jump targets are **absolute addresses** (`imm`).

### JMP imm

Unconditional jump.

---

### Conditional Jumps

| Instruction | Condition            |
|-------------|----------------------|
| JZ          | ZF == 1              |
| JNZ         | ZF == 0              |
| JC          | CF == 1              |
| JNC         | CF == 0              |
| JG          | ZF == 0 and SF == OF |
| JGE         | SF == OF             |
| JL          | SF != OF             |
| JLE         | ZF == 1 or SF != OF  |

Signed comparison semantics are used.

---

## 10. Memory Access

### LOAD rd, [rs1 + imm]

- Reads 8-bit value
- Zero-extends to 32 bit

---

### LOAD32 rd, [rs1 + imm]

- Reads 32-bit value
- Address must be 4-byte aligned

---

### LOADX32 rd, [rs1 + rs2 + imm]

Indexed 32-bit load.

---

### STORE / STORE32 / STOREX32

Write operations do not modify FLAGS.

---

## 11. Stack Model

LampVM has **three independent stacks**:

| Stack      | Purpose                |
|------------|------------------------|
| Call Stack | CALL / RET             |
| Data Stack | PUSH / POP             |
| ISR Stack  | Interrupt context save |

All three stacks are implemented in memory-mapped stack regions (`CALL_STACK_BASE`, `DATA_STACK_BASE`, `ISR_STACK_BASE`).

---

## 12. Interrupt Model

### Interrupt Vector Table (IVT)

- Address: `IVT_BASE + int_no * 8`
- Maximum vectors: 256

---

### ISR ABI (Fixed)

On interrupt entry, VM automatically:

1. Sets `r31 = int_no`
2. Saves context to ISR stack (IP, FLAGS, r0–r31)
3. Jumps to ISR entry
4. Sets `in_interrupt = 1`

ISR must return using `IRET`.

Nested interrupts are **not supported**.

---

## 13. IO Instructions

### IN rd, [rs1]

Reads from IO port.

---

### OUT [rs1], rd

Writes to IO port.

Out-of-range access causes VM panic.

---

## 14. System Instruction

### HALT

Stops VM execution.

---

## 15. Stack Operations

### POP rd

Pop a data out of data stack to rd register.

---

### PUSH rd

Push a data in rd to data stack.

---
## 16. Undefined Behavior

The following are undefined and may cause VM panic:

- Undefined opcode
- IRET outside ISR
- Out-of-range memory access
- Invalid IO port
- Misaligned LOAD32/STORE32

---

## 17. Compatibility Policy

Once defined in this document, instruction semantics are considered ABI-stable.
Existing instructions must not change behavior.

Extensions must use new opcodes or mechanisms.
