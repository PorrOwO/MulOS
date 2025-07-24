/**
 * ===============================================================
 * |                         EXCEPTIONS                          |
 * ===============================================================
 * @file exceptions.h
 *
 * @brief Header file for exception handling in the system.
 *
 * This file contains the necessary includes and function declarations
 * for handling exceptions, system calls, and interrupts.
 */
#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/const.h>
#include <uriscv/types.h>

//#include "./scheduler.h"

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include "initial.h"

void* memcpy(void* dest, const void* src, size_tt n);

cpu_t getTimeElapsed(void);

void createProcess(state_t *statep, support_t *supportStruct);
void terminateProcess(int pid);
void passeren(int* semAddr);
void verhogen(int* semAddr);
void doIo(int* commandAddr, int commandValue);
void getCPUTime(void);
void waitForClock(void);
void getSupportData(void);
void getProcessID(int parent);

void exceptionHandler(void);
void SYSCALL_handler(state_t* exceptionState);
void passupOrDie(int exceptionIndex);

// extern void uTLB_RefillHandler(void);

extern unsigned int ProcessCount;
extern struct list_head ReadyQueue;
extern pcb_t* CurrentProcess[NCPU];
extern int DeviceSemaphores[SEMDEVLEN];
extern int PseudoClock;
extern unsigned int GlobalLock;

extern void scheduler();

extern int* getPseudoClockSemaphore(void);
extern int getDeviceSemaphoreIndex(int* commandAddr);
extern int getHighestPriorityDeviceNumber(void);
extern int getLineNo(void);

extern void INTERRUPT_handler();

#endif // EXCEPTIONS_H
