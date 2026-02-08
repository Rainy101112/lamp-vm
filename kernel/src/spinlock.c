#include "../include/kernel/spinlock.h"

void spinlock_init(spinlock_t *lock) {
    lock->v = 0u;
}

void spinlock_lock(spinlock_t *lock) {
    /*
     * Temporary single-core fallback:
     * lamp backend atomic builtins are not fully lowered yet.
     * Replace with CAS/LDAR/STLR path after backend atomic emission is stable.
     */
    while (lock->v != 0u) {
        __asm__ __volatile__("" ::: "memory");
    }
    lock->v = 1u;
    __asm__ __volatile__("" ::: "memory");
}

void spinlock_unlock(spinlock_t *lock) {
    __asm__ __volatile__("" ::: "memory");
    lock->v = 0u;
}
