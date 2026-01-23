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
| imm    | 32 bit | Immediate value (unsigned) |

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

### Instructions that update **ZF and SF only**, and **clear CF and OF**

- `MUL`
- `DIV` (when divisor ≠ 0)
- `MOD` (when divisor ≠ 0)
- `AND`
- `OR`
- `XOR`
- `NOT`
- `SHL`
- `SHR`
- `MOV`
- `MOVI`
- `LOAD`
- `LOAD32`
- `LOADX32`
- `POP`

### Instructions that do **not guarantee FLAGS state**

- `STORE`
- `STORE32`
- `STOREX32`
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

### AND / OR / XOR / NOT / SHL / SHR

Logical and shift instructions update ZF and SF, and clear CF and OF.

---

## 7. Comparison Instructions

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

## 8. Control Flow Instructions

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
| JLE         | ZF == 1 or SF != OF  |

Signed comparison semantics are used.

---

## 9. Memory Access

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

## 10. Stack Model

LampVM has **three independent stacks**:

| Stack      | Purpose                |
|------------|------------------------|
| Call Stack | CALL / RET             |
| Data Stack | PUSH / POP             |
| ISR Stack  | Interrupt context save |

---

## 11. Interrupt Model

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

## 12. IO Instructions

### IN rd, [rs1]

Reads from IO port.

---

### OUT [rs1], rd

Writes to IO port.

Out-of-range access causes VM panic.

---

## 13. System Instruction

### HALT

Stops VM execution.

---

## 14. Undefined Behavior

The following are undefined and may cause VM panic:

- Undefined opcode
- IRET outside ISR
- Out-of-range memory access
- Invalid IO port
- Misaligned LOAD32/STORE32

---

## 15. Compatibility Policy

Once defined in this document, instruction semantics are considered ABI-stable.
Existing instructions must not change behavior.

Extensions must use new opcodes or mechanisms.
