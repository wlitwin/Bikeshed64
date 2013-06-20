#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "pcb.h"

void scheduler_init(void);

void schedule(PCB* pcb);

void create_init_process(void);

void dispatch(void);

#endif
