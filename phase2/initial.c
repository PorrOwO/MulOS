/**
 * ===================================================================
 * |                            KERNEL                               |
 * ===================================================================
 *
 * @file initial.c
 * @brief Kernel initialization code.
 *
 * This file contains the main function for the kernel, which initializes
 * various components of the system, including the process control blocks,
 * the ready queue, and the device semaphores. It also sets up the
 * interrupt handling and starts the scheduler.
 *
 * @details
 * - Initializes the process control blocks and the ready queue.
 * - Sets up the interrupt handling for each CPU.
 * - Initializes the device semaphores.
 * - Loads the system-wide interval timer.
 * - Allocates a process control block for the initial process and sets its state.
 * - Inserts the initial process into the ready queue.
 * - Sets up the interrupt vector table.
 * - Initializes the device semaphores.
 * - Sets the TPR (Timer Priority Register) to 0.
 * - Allocates process control blocks for the other CPUs and sets their state.
 * - Starts the scheduler.
 */

#include "./headers/initial.h"
#include <uriscv/const.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

// Declaration of the kernel variables that will also be shared with the other modules
unsigned int ProcessCount;
struct list_head ReadyQueue;
pcb_t* CurrentProcess[NCPU];
int DeviceSemaphores[NRSEMAPHORES];
unsigned int GlobalLock;

/**
 * @brief Initializes the device semaphores.
 *
 * This function sets all device semaphores to 0.
 */
static inline void _initDeviceSemaphores(void) {
  for (int i = 0; i < NRSEMAPHORES; i++) {
    DeviceSemaphores[i] = 0;
  }
}

/**
 * @brief Initializes the array of current processes.
 *
 * This function sets all elements of the CurrentProcess array to NULL.
 */
static inline void _initCurrentProcessArray(void) {
  for (int i = 0; i < NCPU; i++) {
    CurrentProcess[i] = NULL;
  }
}

/*
 * @brief Initializes the passup vector for a CPU.
 *
 * This function sets up:
 * - TLB refill stack pointer.
 * - Exception stack pointer, 
 * - TLB refill handler.
 * - Exception handler for a given CPU ID.
 *
 */
static inline void _initPassupVector(void) {
  for (int i = 0; i < NCPU; i++) {
    passupvector_t* passupVector = (passupvector_t*)(BIOSDATAPAGE + (0x900 + (0x10 * i)));
    passupVector->tlb_refill_stackPtr = (i == 0) ? KERNELSTACK : 0x20020000 + (i * PAGESIZE);
    passupVector->exception_stackPtr = (i == 0) ? KERNELSTACK : 0x20020000 + (i * PAGESIZE);
    passupVector->tlb_refill_handler = (memaddr) uTLB_RefillHandler;
    passupVector->exception_handler = (memaddr) exceptionHandler;
  }
}

/**
 * @brief Initializes the process control blocks.
 *
 * This function initializes the process control blocks for the kernel.
 *
 * @return Pointer to the first process control block.
 */
static inline pcb_t* _initFirstPCB(void) {
  pcb_t* p = allocPcb();
  if (!p) PANIC();
  p->p_s.mie = MIE_ALL;
  p->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;
  p->p_semAdd = NULL;
  p->p_time = 0;
  p->p_supportStruct = NULL;
  RAMTOP(p->p_s.reg_sp);
  p->p_s.pc_epc = (memaddr) test;
  
  return p;
}

/**
 * @brief Initializes the other process control blocks.
 *
 * This function allocates process control blocks for the other CPUs and sets their state.
 */
static inline void _initOtherPCBs(void) {
  for (int i = 1; i < NCPU; ++i) {
    pcb_t* p = allocPcb();
    if (!p) PANIC();

    p->p_s.status = MSTATUS_MPP_M;
    p->p_s.pc_epc = (memaddr) scheduler;
    p->p_s.reg_sp = 0x20020000 + (i * PAGESIZE);
    
    p->p_semAdd = NULL;
    p->p_time = 0;
    p->p_supportStruct = NULL; 

    INITCPU(i, &p->p_s);
  }
}

/**
 * @brief Gets the semaphore for the pseudo clock.
 *
 * This function returns the address of the semaphore for the pseudo clock.
 * 
 * @details
 *  The pseudo clock semaphore is used to synchronize access to the pseudo clock.
 *  We use the NSUPPSEM constant to identify the area after all the device semaphores where the pseudo clock semaphore should be.
 *
 * @return Pointer to the pseudo clock semaphore.
 */
int* getPseudoClockSemaphore(void) {
  return &DeviceSemaphores[NSUPPSEM];
}

/**
 * @brief Gets the semaphore of a device.
 *
 * This function returns the address of the semaphore for a device.
 * 
 * @details
 *  The device semaphore is used to synchronize access to the device.
 *  To calculate the semaphore index, we subtract the START_DEVREG from the command address,
 *  then divide by DEVREGSIZE to get the device number.
 *  We also check the 4th bit of the command address to determine if we need to add DEVPERINT.
 *  This is done to ensure that we are accessing the correct semaphore for the device.
 *
 * @param commandAddr The command address to calculate the semaphore index.
 * @return if the semaphore index is valid, return the address of the semaphore, otherwise return NULL.
 */
// int* getDeviceSemaphore(int commandAddr) {
//   int devIdx = ((commandAddr - START_DEVREG) / DEVREGSIZE) + ((commandAddr & 0x8) ? 0 : DEVPERINT);
//   if (devIdx < 0 || devIdx >= SEMDEVLEN) return NULL;
//   
//   return &DeviceSemaphores[devIdx];
// }

/**
 * @brief Initializes the interrupts.
 *
 * This function sets up the interrupt vector table and initializes the TPR.
 */
static inline void _initInterrupts(void) {
    for (int i = 0; i < IRT_NUM_ENTRY; i++) {
        memaddr* entry = (memaddr*)(IRT_START + (i * 0x4)); // 4 bytes per entry
        *entry = IRT_RP_BIT_ON | ((1 << NCPU) - 1); // RP=1, all CPUs enabled
    }

    // Initialize TPR
    *((memaddr*)TPR) = 0;
}

/**
 * @brief Kernel main function.
 *
 * This function initializes the kernel, sets up the process control blocks,
 * and starts the scheduler.
 *
 * @return 0 on success.
 */
int main(void) {

  // Initialize the passup vector for each CPU
  _initPassupVector();

  // Initialize the data structures of the phase 1 modules
  initPcbs();
  initASL();

  // Initialize all the previously declared variables 
  ProcessCount = 0;
  GlobalLock = 0;
  INIT_LIST_HEAD(&ReadyQueue);
  _initDeviceSemaphores();
  _initCurrentProcessArray();

  // load the system wide interval timer
  LDIT(PSECOND); 
  
  // Initialize the first process control block
  pcb_t* p = _initFirstPCB();
  insertProcQ(&ReadyQueue, p);
  ProcessCount++;
 
  // Interrupts
  _initInterrupts();

  // Initialize other process control blocks
  _initOtherPCBs();


  scheduler();
  return 0;
}
