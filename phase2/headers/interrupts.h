/**
 * @file interrupts.h
 *
 * @brief Header file for interrupt handling in the system.
 *
 * This file contains the function declarations and includes necessary for
 * handling interrupts in the system.
 */
#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <uriscv/const.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../headers/listx.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include "initial.h"

int  getLineNo();
int  getHighestPriorityDeviceNumber();
int  getDeviceSemaphoreIndex(int* commandAddr);
void handleDeviceInterrupt();
void handlePseudoClockInterrupt();
void handleProcessLocalTimerInterrupt();
void INTERRUPT_handler();

extern pcb_t* CurrentProcess[NCPU];
extern unsigned int ProcessCount;
extern struct list_head ReadyQueue;
extern int DeviceSemaphores[SEMDEVLEN];
extern int* getPseudoClockSemaphore();
extern unsigned int GlobalLock;
extern void verhogen(int* semAddr);
extern void* memcpy(void* dest, const void* src, size_tt n);
extern cpu_t getTimeElapsed(void);

extern cpu_t lastTOD;
#endif // INTERRUPTS_H
