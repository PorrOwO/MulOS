/**
 * ==========================================================================
 * |                              SCHEDULER                                 |                       
 * ==========================================================================
 * @file scheduler.c
 *
 * @brief Scheduler module for the kernel.
 *
 * This file contains the implementation of the scheduler function,
 * which is responsible for managing the process scheduling in the kernel.
 * It handles the process dispatching and waiting for processes.
 *
 * @details
 * - The scheduler function is called when a process needs to be scheduled.
 * - It checks if the ready queue is empty and either halts or waits for processes.
 * - If there are processes in the ready queue, it dispatches the next process.
 */

#include <uriscv/const.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include "./headers/scheduler.h"

/** 
 * @brief Scheduler function.
 *
 * This function is responsible for managing the process scheduling in the kernel.
 * It checks if the ready queue is empty and either halts or waits for processes.
 * If there are processes in the ready queue, it dispatches the next process.
 */
void scheduler() {
  ACQUIRE_LOCK(&GlobalLock);
  if (emptyProcQ(&ReadyQueue)) {
    RELEASE_LOCK(&GlobalLock);
    if (ProcessCount == 0) {
      unsigned int *irt_entry = (unsigned int*) IRT_START;
      for (int i = 0; i < IRT_NUM_ENTRY; i++) {
        *irt_entry = getPRID();
        irt_entry++;
      }
      HALT();
    } else {
      setMIE(MIE_ALL & ~MIE_MTIE_MASK);
      unsigned int status = getSTATUS();
      status |= MSTATUS_MIE_MASK;
      setSTATUS(status);
      *((memaddr*)TPR) = 1;
      
      WAIT();
    }
  } else {
    CurrentProcess[getPRID()] = removeProcQ(&ReadyQueue); // now it's running
    setTIMER(TIMESLICE * (*(cpu_t*)TIMESCALEADDR));
    *((memaddr*)TPR) = 0;
    
    RELEASE_LOCK(&GlobalLock);

    LDST(&CurrentProcess[getPRID()]->p_s);
  }
}
