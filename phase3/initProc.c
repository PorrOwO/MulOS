/** 
 * ============================== INSTANTIATOR PROCESS ==============================
 * 
 * @file initProc.c
 * @brief Initialization functions for the system.
 *
 * This file contains the initialization functions for the support level, including
 * setting up the initial state of processes and other necessary components.
 */
#include "headers/initProc.h"

/**
 * @brief getDeviceSemIndex
 * 
 * This function calculates the index of the device semaphore based on the line number and device number.
 * The line number starts from 3, and the device number is zero-indexed.
 *
 * @param line The line number of the device.
 * @param dev The device number (zero-indexed).
 * @return The index of the device semaphore.
 */
int getDeviceSemIndex(int line, int dev){
  int x = (line - 3) * 8 + dev;
  return x;
}

/* ============================== VARIABLES DECLARATION ============================== */

/* Support structures for U-processes */
static support_t _SupportPool[UPROCMAX];

/*Device semaphores*/
int SupportDeviceSemaphores[NSUPPSEM]  = { [0 ... NSUPPSEM - 1] = 1 };

/* Master semaphore for U-Procs management */
int MasterSemaphore = 0;

/* ============================== PRIVATE FUNCTIONS ============================== */

/**
 * @brief Initializes the state of a U-Proc of a given asid.
 *
 * This function sets the initial state of a U-Proc, including the program counter,
 * stack pointer, status register, and entry_hi field.
 *
 * @param state Pointer to the state structure to initialize.
 * @param asid The address space identifier (ASID) for the U-Proc.
 * @return void
 */
static inline void _initState(state_t* state, int asid) {
  state->pc_epc = UPROCSTARTADDR;
  state->reg_sp = USERSTACKTOP;
  state->status = IMON | TEBITON | USERPON | IEPON;
  state->entry_hi = (asid << ASIDSHIFT);
}

/**
 * @brief Initializes the support structure for a U-Proc.
 *
 * This function initializes the support structure for a U-Proc, including the
 * private page table and exception states.
 *
 * @param supp Pointer to the support structure to initialize.
 * @param asid The address space identifier (ASID) for the U-Proc.
 * @return void
 */
static inline void _initSupport(support_t* supp, int asid) {
  supp->sup_asid = asid;

  supp->sup_exceptContext[GENERALEXCEPT].pc = (memaddr)generalExceptionHandler;
  supp->sup_exceptContext[GENERALEXCEPT].status = IEPON | IMON | TEBITON;
  supp->sup_exceptContext[GENERALEXCEPT].stackPtr = (memaddr)&(supp->sup_stackGen[499]);

  supp->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)TLB_Handler;
  supp->sup_exceptContext[PGFAULTEXCEPT].status = IEPON | IMON | TEBITON;
  supp->sup_exceptContext[PGFAULTEXCEPT].stackPtr = (memaddr)&(supp->sup_stackTLB[499]);

  /* Initialize the private page table */
  for (int i = 0; i < USERPGTBLSIZE - 1; i++) {
    supp->sup_privatePgTbl[i].pte_entryHI = 0x80000000 + (i << VPNSHIFT) + (asid << ASIDSHIFT);
    supp->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
  }
  supp->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI = 0xBFFFF000 + (asid << ASIDSHIFT);
  supp->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON;
}

/* ============================== PUBLIC FUNCTIONS ============================== */


/**
 * @brief Initializes the support level for U-Processes.
 *
 * This function initializes the support structures and semaphores for U-Processes.
 * It sets up the initial state of the support structures and prepares the system
 * for U-Processes management.
 *
 * @return void
 */
void test(void) {
  initSwapStructs(); /* Initialize the swap pool table */

  for (int i = 0; i < UPROCMAX; i++) {
    state_t state;
    _initState(&state, i + 1); /* ASID starts from 1 */

    support_t* supp = &_SupportPool[i];
    _initSupport(supp, i + 1); /* ASID starts from 1 */
 
    SYSCALL(CREATEPROCESS, (int)&state, PROCESS_PRIO_LOW, (int)supp); 
  }

  // for a more graceful shutdown, we need to wait for all U-Processes to be terminated.
  for(int i = 0; i < UPROCMAX; i++) {
    SYSCALL(PASSEREN, (int)&MasterSemaphore, 0, 0); /* Wait for the master semaphore */
  }

  SYSCALL(TERMPROCESS, 0, 0, 0); /* Terminate the initialization process */
}
