# lamp-vm

## Instructions

### Instructions Mapping

| Type       | rd          | rs1              | rs2     | imm                 |
|------------|-------------|------------------|---------|---------------------|
| Calc       | destination | source1          | source2 | none                |
| LOADI      | destination | /                | /       | Immediate Value     | 
| LOAD       | target      | address register | /       | /                   |
| STORE      | destination | address register | /       | /                   |
| JMP / JZ   | /           | /                | /       | Jump Instruction Id |
| PUSH / POP | register    | /                | /       | /                   |
| CMP        | register    | register         | /       | Immediate Value     |
| MOV        | register    | register         | /       | /                   |
| MOVI       | register    | /                | /       | Immediate Value     |

### Instructions Usage:

Calc: ALL math calculations such as ADD, SUB, MUL.

LOADI: Load a immediate value to a register

LOAD: Load a value in a memory address into other register

STORE: Store a value to a memory address

JMP: Jump to a command.

JZ: Jump to a command if flags(ZFLAGS) is 0.

PUSH: Push a register into stack.

POP: Pop a value out of the stack.

CMP: Compare two values and set ZFLAGS. If imm is 0, compare rd and rs1, or compare rd and imm. If they are equal, ZFLAG
will be 0, otherwise it will be 1.

MOV/MOVI: Move rs1's value or a immediate value to rd.