#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL_timer.h>
#include <pthread.h>

#include "fetch.h"
#include "vm.h"
#include "stack.h"
#include "io.h"
#include "panic.h"
#include "loadbin.h"
#include "interrupt.h"
#include "memory.h"
#include "io_devices/disk/disk.h"
#include "io_devices/frame/frame.h"
#include "io_devices/time/time_mmio_register.h"
#include "io_devices/vga_display/display.h"
#include "io_devices/vga_display/vga_mmio_register.h"
#include "float.h"
#include "flags.h"
#include "debug.h"
const size_t MEM_SIZE = 1048576 * 4; // 4MB
void update_zf_sf(VM *vm, int32_t result) {
    if (result == 0)
        vm->flags |= FLAG_ZF;
    else
        vm->flags &= ~FLAG_ZF;

    if (result < 0)
        vm->flags |= FLAG_SF;
    else
        vm->flags &= ~FLAG_SF;
}

void update_add_flags(VM *vm, int32_t a, int32_t b, int32_t result) {
    if ((uint32_t) a + (uint32_t) b < (uint32_t) a)
        vm->flags |= FLAG_CF;
    else
        vm->flags &= ~FLAG_CF;

    if (((a > 0 && b > 0 && result < 0) || (a < 0 && b < 0 && result > 0)))
        vm->flags |= FLAG_OF;
    else
        vm->flags &= ~FLAG_OF;

    update_zf_sf(vm, result);
}

void update_sub_flags(VM *vm, int32_t a, int32_t b, int32_t result) {
    if ((uint32_t) a < (uint32_t) b)
        vm->flags |= FLAG_CF;
    else
        vm->flags &= ~FLAG_CF;

    if (((a > 0 && b < 0 && result < 0) || (a < 0 && b > 0 && result > 0)))
        vm->flags |= FLAG_OF;
    else
        vm->flags &= ~FLAG_OF;

    update_zf_sf(vm, result);
}

static inline void clear_cf_of(VM *vm) {
    vm->flags &= ~(FLAG_CF | FLAG_OF);
}

static inline void update_logic_flags(VM *vm, int32_t result) {
    clear_cf_of(vm);
    update_zf_sf(vm, result);
}

void vm_instruction_case(VM *vm) {
    uint8_t op, rd, rs1, rs2;
    int32_t imm;
    FETCH64(vm, op, rd, rs1, rs2, imm);
    vm_debug_count_instruction(vm, op);
    vm->execution_times++;
    //printf("IP=%lu, executing opcode=%d\n", vm->ip, op);
    //printf("0x%08x,0x%08x,0x%08x,0x%08x\n", rd,rs1,rs2,imm);
    switch (op) {
        case OP_ADD: {
            const int32_t a = vm->regs[rs1];
            const int32_t b = vm->regs[rs2];
            const int32_t res = a + b;
            vm->regs[rd] = res;
            update_add_flags(vm, a, b, res);
            break;
        }

        case OP_SUB: {
            int32_t a = vm->regs[rs1];
            int32_t b = vm->regs[rs2];
            int32_t res = a - b;
            vm->regs[rd] = res;
            update_sub_flags(vm, a, b, res);
            break;
        }
        case OP_MUL: {
            vm->regs[rd] = vm->regs[rs1] * vm->regs[rs2];
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_HALT: {
            flush_screen_final();
            vm->halted = 1;
            return;
        }
        case OP_JMP: {
            vm->ip = imm;
            break;
        }
        case OP_PUSH: {
            data_push(vm, vm->regs[rd]);
            break;
        }
        case OP_POP: {
            vm->regs[rd] = data_pop(vm);
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_CALL: {
            call_push(vm, vm->ip);
            vm->ip = imm;
            break;
        }
        case OP_RET: {
            vm->ip = call_pop(vm);
            break;
        }
        case OP_LOAD: {
            const vm_addr_t addr = vm->regs[rs1] + imm;
            vm->regs[rd] = (uint32_t) vm_read8(vm, addr);
            update_zf_sf(vm, vm->regs[rd]);
            break;
        }
        case OP_LOAD32: {
            const vm_addr_t addr = vm->regs[rs1] + imm;
            vm->regs[rd] = vm_read32(vm, addr);
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_LOADX32: {
            const vm_addr_t addr = vm->regs[rs1] + vm->regs[rs2] + imm;
            vm->regs[rd] = vm_read32(vm, addr);
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_STORE: {
            const vm_addr_t addr = vm->regs[rs1] + imm;
            vm_write8(vm, addr, (uint8_t) vm->regs[rd]);
            break;
        }
        case OP_STORE32: {
            const vm_addr_t addr = vm->regs[rs1] + imm;
            vm_write32(vm, addr, (uint32_t) vm->regs[rd]);
            break;
        }
        case OP_STOREX32: {
            const vm_addr_t addr = vm->regs[rs1] + vm->regs[rs2] + imm;
            vm_write32(vm, addr, (uint32_t) vm->regs[rd]);
            break;
        }
        case OP_CMP: {
            const int32_t val1 = vm->regs[rd];
            const int32_t val2 = vm->regs[rs1];
            const int32_t res = val1 - val2;
            update_sub_flags(vm, val1, val2, res);
            break;
        }
        case OP_CMPI: {
            const int32_t val1 = vm->regs[rd];
            const int32_t val2 = imm;
            const int32_t res = val1 - val2;
            update_sub_flags(vm, val1, val2, res);
            break;
        }
        case OP_MOV: {
            vm->regs[rd] = vm->regs[rs1];
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_MOVI: {
            vm->regs[rd] = imm;
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_MEMSET: {
            const uint32_t base = (uint32_t) vm->regs[rd];
            const uint8_t value = (uint8_t) vm->regs[rs1];
            const uint32_t count = (uint32_t) imm;

            for (uint32_t i = 0; i < count; i++) {
                vm_write8(vm, base + i, value);
            }
            break;
        }
        case OP_MEMCPY: {
            const uint32_t dest = (uint32_t) vm->regs[rd];
            const uint32_t src = (uint32_t) vm->regs[rs1];
            const uint32_t count = (uint32_t) imm;

            for (uint32_t i = 0; i < count; i++) {
                uint8_t v = vm_read8(vm, src + i);
                vm_write8(vm, dest + i, v);
            }
            break;
        }
        case OP_IN: {
            const int addr = vm->regs[rs1];
            if (addr >= 0 && addr < IO_SIZE) {
                if (addr == KEYBOARD) {
                    const int v = vm->io[KEYBOARD];
                    vm->regs[rd] = v;
                    vm->io[KEYBOARD] = 0;
                    vm->io[SCREEN_ATTRIBUTE] &= ~SERIAL_STATUS_RX_READY;
                } else if (addr == SCREEN_ATTRIBUTE) {
                    vm->regs[rd] = vm->io[SCREEN_ATTRIBUTE] & 0xFF;
                } else {
                    vm->regs[rd] = vm->io[addr];
                }
            } else {
                panic(panic_format("IN invalid IO address %d", addr), vm);
            }
            break;
        }
        case OP_OUT: {
            const int addr = vm->regs[rs1];
            if (addr >= 0 && addr < IO_SIZE) {
                accept_io(vm, addr, vm->regs[rd]);
            } else {
                panic(panic_format("OUT invalid IO address %d\n", addr), vm);
            }
            break;
        }
        case OP_INT: {
            const uint32_t int_no = vm->regs[rd];
            vm_enter_interrupt(vm, int_no);
            break;
        }
        case OP_IRET: {
            vm_iret(vm);
            break;
        }
        case OP_AND: {
            vm->regs[rd] = vm->regs[rs1] & vm->regs[rs2];
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_OR: {
            vm->regs[rd] = vm->regs[rs1] | vm->regs[rs2];
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_XOR: {
            vm->regs[rd] = vm->regs[rs1] ^ vm->regs[rs2];
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_NOT: {
            vm->regs[rd] = ~vm->regs[rs1];
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_SHL: {
            uint32_t sh = (uint32_t)vm->regs[rs2] & 31u;
            vm->regs[rd] = (int32_t)((uint32_t)vm->regs[rs1] << sh);
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_SHR: {
            uint32_t sh = (uint32_t)vm->regs[rs2] & 31u;
            vm->regs[rd] = (int32_t)((uint32_t)vm->regs[rs1] >> sh); // 逻辑右移
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_SAR: {
            uint32_t sh = (uint32_t)vm->regs[rs2] & 31u;
            vm->regs[rd] = vm->regs[rs1] >> sh;
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_DIV: {
            if (vm->regs[rs2] != 0) {
                vm->regs[rd] = vm->regs[rs1] / vm->regs[rs2];
                update_logic_flags(vm, vm->regs[rd]);
            } else {
                trigger_interrupt(vm, INT_DIVIDE_BY_ZERO);
            }
            break;
        }
        case OP_MOD: {
            if (vm->regs[rs2] != 0) {
                vm->regs[rd] = vm->regs[rs1] % vm->regs[rs2];
                update_logic_flags(vm, vm->regs[rd]);
            } else {
                trigger_interrupt(vm, INT_DIVIDE_BY_ZERO);
            }
            break;
        }
        case OP_INC: {
            const int32_t a = vm->regs[rd];
            const int32_t b = 1;
            const int32_t res = a + b;
            vm->regs[rd] = res;
            update_add_flags(vm, a, b, res);
            break;
        }

        case OP_JZ: {
            if (vm->flags & FLAG_ZF) {
                vm->ip = imm;
            }
            break;
        }
        case OP_JNZ: {
            if (!(vm->flags & FLAG_ZF)) {
                vm->ip = imm;
            }
            break;
        }
        case OP_JG: {
            if (!(vm->flags & FLAG_ZF) && ((vm->flags & FLAG_SF) == (vm->flags & FLAG_OF))) {
                vm->ip = imm;
            }
            break;
        }
        case OP_JGE: {
            if ((vm->flags & FLAG_SF) == (vm->flags & FLAG_OF)) {
                vm->ip = imm;
            }
            break;
        }
        case OP_JL: {
            if ((vm->flags & FLAG_SF) != (vm->flags & FLAG_OF)) {
                vm->ip = imm;
            }
            break;
        }
        case OP_JLE: {
            if ((vm->flags & FLAG_ZF) || (vm->flags & FLAG_SF) != (vm->flags & FLAG_OF)) {
                vm->ip = imm;
            }
            break;
        }
        case OP_JC: {
            if (vm->flags & FLAG_CF) {
                vm->ip = imm;
            }
            break;
        }
        case OP_JNC: {
            if (!(vm->flags & FLAG_CF)) {
                vm->ip = imm;
            }
            break;
        }

        case OP_FADD: {
            float a = reg_as_f32(vm->regs[rs1]);
            float b = reg_as_f32(vm->regs[rs2]);
            float r = a + b;
            vm->regs[rd] = f32_as_reg(r);
            update_logic_flags(vm, (r == 0.0f) ? 0 : (r < 0.0f ? -1 : 1));
            break;
        }
        case OP_FSUB: {
            float a = reg_as_f32(vm->regs[rs1]);
            float b = reg_as_f32(vm->regs[rs2]);
            float r = a - b;
            vm->regs[rd] = f32_as_reg(r);
            update_logic_flags(vm, (r == 0.0f) ? 0 : (r < 0.0f ? -1 : 1));
            break;
        }
        case OP_FMUL: {
            float a = reg_as_f32(vm->regs[rs1]);
            float b = reg_as_f32(vm->regs[rs2]);
            float r = a * b;
            vm->regs[rd] = f32_as_reg(r);
            update_logic_flags(vm, (r == 0.0f) ? 0 : (r < 0.0f ? -1 : 1));
            break;
        }
        case OP_FDIV: {
            float a = reg_as_f32(vm->regs[rs1]);
            float b = reg_as_f32(vm->regs[rs2]);
            float r = a / b;
            vm->regs[rd] = f32_as_reg(r);
            update_logic_flags(vm, (r == 0.0f) ? 0 : (r < 0.0f ? -1 : 1));
            break;
        }
        case OP_FNEG: {
            float a = reg_as_f32(vm->regs[rs1]);
            float r = -a;
            vm->regs[rd] = f32_as_reg(r);
            update_logic_flags(vm, (r == 0.0f) ? 0 : (r < 0.0f ? -1 : 1));
            break;
        }
        case OP_FABS: {
            float a = reg_as_f32(vm->regs[rs1]);
            float r = fabsf(a);
            vm->regs[rd] = f32_as_reg(r);
            update_logic_flags(vm, (r == 0.0f) ? 0 : 1);
            break;
        }
        case OP_FSQRT: {
            float a = reg_as_f32(vm->regs[rs1]);
            float r = sqrtf(a);
            vm->regs[rd] = f32_as_reg(r);
            update_logic_flags(vm, (r == 0.0f) ? 0 : (r < 0.0f ? -1 : 1));
            break;
        }
        case OP_ITOF: {
            int32_t i = vm->regs[rs1];
            float f = (float) i;
            vm->regs[rd] = f32_as_reg(f);
            update_logic_flags(vm, (f == 0.0f) ? 0 : (f < 0.0f ? -1 : 1));
            break;
        }
        case OP_FTOI: {
            float f = reg_as_f32(vm->regs[rs1]);
            if (f32_is_nan(f) || f > 2147483647.0f || f < -2147483648.0f) {
                vm->regs[rd] = 0;
                vm->flags |= FLAG_OF;
                vm->flags &= ~(FLAG_ZF | FLAG_SF | FLAG_CF);
            } else {
                int32_t i = (int32_t) f;
                vm->regs[rd] = i;
                update_logic_flags(vm, i);
            }
            break;
        }

        case OP_FLOAD32: {
            const vm_addr_t addr = vm->regs[rs1] + imm;
            uint32_t bits = (uint32_t) vm_read32(vm, addr);
            vm->regs[rd] = (int32_t) bits;
            float f = reg_as_f32(vm->regs[rd]);
            update_logic_flags(vm, (f == 0.0f) ? 0 : (f < 0.0f ? -1 : 1));
            break;
        }
        case OP_FSTORE32: {
            const vm_addr_t addr = vm->regs[rs1] + imm;
            vm_write32(vm, addr, (uint32_t) vm->regs[rd]);
            break;
        }
        case OP_FCMP: {
            float a = reg_as_f32(vm->regs[rd]);
            float b = reg_as_f32(vm->regs[rs1]);
            update_fcmp_flags(vm, a, b);
            break;
        }
        case OP_ADDI: {
            const int32_t a = vm->regs[rs1];
            const int32_t b = imm;
            const int32_t res = a + b;
            vm->regs[rd] = res;
            update_add_flags(vm, a, b, res);
            break;
        }
        case OP_SUBI: {
            const int32_t a = vm->regs[rs1];
            const int32_t b = imm;
            const int32_t res = a - b;
            vm->regs[rd] = res;
            update_sub_flags(vm, a, b, res);
            break;
        }
        case OP_ANDI: {
            vm->regs[rd] = vm->regs[rs1] & imm;
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_ORI: {
            vm->regs[rd] = vm->regs[rs1] | imm;
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_XORI: {
            vm->regs[rd] = vm->regs[rs1] ^ imm;
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_SHLI: {
            uint32_t sh = (uint32_t)imm & 31u;
            vm->regs[rd] = (int32_t)((uint32_t)vm->regs[rs1] << sh);
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        case OP_SHRI: {
            uint32_t sh = (uint32_t)imm & 31u;
            vm->regs[rd] = (int32_t)((uint32_t)vm->regs[rs1] >> sh);
            update_logic_flags(vm, vm->regs[rd]);
            break;
        }
        default: {
            panic(panic_format("Unknown opcode %d\n", op), vm);
            return;
        }
    }
}

void *vm_thread(void *arg) {
    VM *vm = arg;
    while (!vm->halted) {
        if (vm->panic)
            return NULL;
        vm_debug_pause_if_needed(vm, (uint32_t) vm->ip);
        vm_handle_interrupts(vm);
        vm_instruction_case(vm);
        disk_tick(vm);
    }
    return NULL;
}

void display_loop(VM *vm) {
    vga_display_init();
    const int frame_delay = 16; // ~60FPS
    while (!vm->halted) {
        uint32_t frame_start = SDL_GetTicks();
        display_poll_events(vm);
        display_update(vm);
        uint32_t frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < frame_delay) {
            SDL_Delay(frame_delay - frame_time);
        }
    }
    display_shutdown();
}

void vm_run(VM *vm) {
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, vm_thread, vm);

    display_loop(vm);

    pthread_join(thread_id, NULL);
}

void vm_dump(const VM *vm, int mem_preview) {
    printf("VM dump:\n");
    printf("Registers:\n");
    for (int i = 0; i < REG_COUNT; i++) {
        printf("r%d = %d\n", i, vm->regs[i]);
    }

    printf("Call Stack (top -> bottom):\n");
    for (int i = vm->csp; i < CALL_STACK_SIZE; i++) {
        const uint32_t addr = CALL_STACK_BASE + (uint32_t) i * 8;
        const uint64_t v = (uint64_t) vm->memory[addr + 0]
            | ((uint64_t) vm->memory[addr + 1] << 8)
            | ((uint64_t) vm->memory[addr + 2] << 16)
            | ((uint64_t) vm->memory[addr + 3] << 24)
            | ((uint64_t) vm->memory[addr + 4] << 32)
            | ((uint64_t) vm->memory[addr + 5] << 40)
            | ((uint64_t) vm->memory[addr + 6] << 48)
            | ((uint64_t) vm->memory[addr + 7] << 56);
        printf("[%d] = %llu\n", i, (unsigned long long) v);
    }
    if (vm->csp == CALL_STACK_SIZE) {
        printf("<empty>\n");
    }
    printf("Data Stack (top -> bottom):\n");
    for (int i = vm->dsp; i < DATA_STACK_SIZE; i++) {
        const uint32_t addr = DATA_STACK_BASE + (uint32_t) i * 4;
        const uint32_t v = (uint32_t) vm->memory[addr + 0]
            | ((uint32_t) vm->memory[addr + 1] << 8)
            | ((uint32_t) vm->memory[addr + 2] << 16)
            | ((uint32_t) vm->memory[addr + 3] << 24);
        printf("[%d] = %u\n", i, v);
    }
    if (vm->dsp == DATA_STACK_SIZE) {
        printf("<empty>\n");
    }
    printf("Memory (first %d cells):\n", mem_preview);
    for (int i = 0; i < mem_preview && i < MEM_SIZE; i++) {
        printf("[%d] = %d\n", i, vm->memory[i]);
    }
    printf("IP = %lu\n", vm->ip);
    printf("ZF = %d\n", (vm->flags & FLAG_ZF) != 0);
}

VM *vm_create(size_t memory_size,
              const uint64_t *program,
              size_t program_size,
              const uint8_t *data,
              size_t data_size,
              const ProgramLayout *layout) {
    VM *vm = malloc(sizeof(VM));
    if (!vm)
        return NULL;

    memset(vm, 0, sizeof(VM));

    vm->memory_size = memory_size;
    vm->memory = malloc(memory_size);
    if (!vm->memory) {
        free(vm);
        return NULL;
    }
    memset(vm->memory, 0, memory_size);

    size_t fb_base = FB_BASE(memory_size);

    vm->fb = malloc(FB_SIZE);
    printf("vm->fb = %p\n", (void *) vm->fb);
    printf("fb_base = 0x%zx\n", fb_base);
    printf("fb address mod 4 = %zu\n", ((size_t) vm->fb) % 4);

    printf("Initializing MMIO.... \n");
    vm->mmio_count = 0;
    memset(vm->mmio_devices, 0, sizeof(vm->mmio_devices));
    register_fb_mmio(vm);
    register_time_mmio(vm);
    size_t prog_bytes = program_size * sizeof(uint64_t);
    uint32_t text_base = PROGRAM_BASE;
    uint32_t data_base = PROGRAM_BASE + (uint32_t) prog_bytes;
    uint32_t bss_base = data_base + (uint32_t) data_size;
    uint32_t bss_size = 0;

    if (layout) {
        text_base = layout->text_base;
        data_base = layout->data_base;
        bss_base = layout->bss_base;
        bss_size = layout->bss_size;
        if (layout->text_size != 0 && layout->text_size != prog_bytes) {
            printf("Warning: layout TEXT_SIZE (%u) != program size (%zu)\n",
                   layout->text_size, prog_bytes);
        }
    }

    if ((size_t) text_base + prog_bytes > memory_size) {
        panic("Program too large\n", vm);
        return vm;
    }

    memcpy(vm->memory + text_base, program, prog_bytes);
    if (data && data_size > 0) {
        if ((size_t) data_base + data_size > memory_size) {
            panic("Data segment out of range\n", vm);
            return vm;
        }
        memcpy(vm->memory + data_base, data, data_size);
    }
    if (bss_size > 0) {
        if ((size_t) bss_base + bss_size > memory_size) {
            panic("BSS segment out of range\n", vm);
            return vm;
        }
        memset(vm->memory + bss_base, 0, bss_size);
    }

    vm->ip = text_base;
    vm->csp = CALL_STACK_SIZE;
    vm->dsp = DATA_STACK_SIZE;
    vm->isp = ISR_STACK_SIZE;

    vm->start_realtime_ns = host_unix_time_ns();
    vm->start_monotonic_ns = host_monotonic_time_ns();
    vm->suspend_count = 0;
    vm->io[SCREEN_ATTRIBUTE] = SERIAL_STATUS_TX_READY;
    vm_debug_init(vm);
    return vm;
}

void vm_destroy(VM *vm) {
    if (!vm)
        return;
    vm_debug_destroy(vm);
    disk_close(vm);
    if (vm->memory)
        free(vm->memory);
    if (vm->fb)
        free(vm->fb);
    free(vm);
}

int main() {
    /*


    uint64_t program[] = {
        INST(OP_MOVI, 0, 0, 0, FB_BASE(MEM_SIZE)),
        INST(OP_MOVI, 1, 0, 0, 0x474A43),
        INST(OP_MOVI, 2, 0, 0, 0),
        INST(OP_MOVI, 7, 0, 0, 4),
        // LOOP_START (index 4)
        INST(OP_STORE32, 1, 0, 2, 0),
        INST(OP_ADD, 2, 2, 7, 0),
        INST(OP_CMPI, 2, 0, 0, FB_SIZE),
        INST(OP_JZ, 0, 0, 0, PROGRAM_BASE + 9 * 8),
        INST(OP_JMP, 0, 0, 0, PROGRAM_BASE + 4 * 8),

        // HALT (index 8)
        INST(OP_HALT, 0, 0, 0, 0)
    };

    size_t program_size = sizeof(program) / sizeof(program[0]);
    */

    const char *filename = "typing.bin";
    size_t program_size = 0;
    size_t data_size = 0;
    uint64_t *program = NULL;
    uint8_t *data = NULL;
    ProgramLayout layout;

    if (!load_program_single(filename, &program, &program_size, &data, &data_size, &layout)) {
        printf("Failed to load program from %s\n", filename);
        return 1;
    }

    printf("Loaded program from %s, %zu instructions.\n", filename, program_size);
    printf("Loaded data: %zu bytes.\n", data_size);
    printf("Layout: TEXT_BASE=0x%08X TEXT_SIZE=%u DATA_BASE=0x%08X DATA_SIZE=%u BSS_BASE=0x%08X BSS_SIZE=%u\n",
           layout.text_base, layout.text_size,
           layout.data_base, layout.data_size,
           layout.bss_base, layout.bss_size);

    VM *vm = vm_create(MEM_SIZE, program, program_size, data, data_size, &layout);
    disk_init(vm, "./disk.img");
    init_ivt(vm);
    printf("Loaded VM. \n Call Stack size: %d\n Data Stack size: %d \n Memory Size: %lu\n Memory "
           "Head: %p\n",
           CALL_STACK_SIZE,
           DATA_STACK_SIZE,
           MEM_SIZE,
           (void *) vm->memory);
    init_screen();
    vm_run(vm);
#ifdef DBEUG
    vm_dump(vm, 1024);
#endif

    flush_screen_final();
    printf("Execution complete in %lu cycles.\n", vm->execution_times);
    vm_debug_print_stats(vm);
    vm_destroy(vm);
    free(program);
    free(data);
    return 0;
}
