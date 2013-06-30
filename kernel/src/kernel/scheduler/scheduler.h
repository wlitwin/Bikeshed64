#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "pcb.h"
#include "kernel/timer/defs.h"

void scheduler_init(void);

uint8_t schedule(PCB* pcb);

void create_init_process(void);

void sleep_pcb(PCB* pcb, time_t time);

void cleanup_pcb(PCB* pcb);

PCB* alloc_pcb(void);

void free_pcb(PCB* pcb);

void dispatch(void);

#endif
