/**
 * @file scheduler.h
 *
 * @brief Header file for the scheduler module.
 *
 * This file contains the necessary includes and function declarations for the scheduler module of the project.
 */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <uriscv/const.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../headers/listx.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"

extern unsigned int ProcessCount;
extern struct list_head ReadyQueue;
extern pcb_t* CurrentProcess[NCPU];
extern int DeviceSemaphores[SEMDEVLEN];
extern int PseudoClock;
extern unsigned int GlobalLock;

void scheduler();

#endif // SCHEDULER_H
