#ifndef LAMP_KERNEL_SCHED_H
#define LAMP_KERNEL_SCHED_H

void sched_init(void);
void sched_run(void);
void schedule_tick(void);
unsigned int sched_ticks(void);

#endif
