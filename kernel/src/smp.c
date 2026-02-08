#include "../include/kernel/smp.h"

static volatile unsigned int g_online_cpus;

void smp_init_bsp(void) {
    g_online_cpus = 1;
}

void smp_init_ap(void) {
    g_online_cpus++;
}
