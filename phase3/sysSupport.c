/**
 * ============================== SUPPORT SYSCALLS ==============================
 *
 * @file sysSupport.c
 *
 * This file contains the implementation of system support functions for U-Processes,
 * including handling system calls, writing to devices, and managing exceptions.
 */
#include "headers/sysSupport.h"

/**
 * @brief check if the address is.
 *
 * this function checks if the virtual address is in a valid range 
 * and if the length is within the maximum allowed length.
 *
 * @param virtAddr The virtual address to check.
 * @param len The length of the memory region to check.
 * 
 * @return 1 if the address is valid, 0 otherwise.
 */
static inline int _is_valid_address(memaddr virtAddr, int len) {
  int inTextData  = (virtAddr >= 0x80000000) && (virtAddr + len <= 0x8001E000);
  int inStack     = (virtAddr >= 0xBFFFF000) && (virtAddr + len <= USERSTACKTOP);
  int validLength = (len >= 0 && len <= MAXSTRLENG);
 
  return (inTextData || inStack) && validLength;
}

/**
 * @brief terminate the U-Proc
 * This function is called when a U-Proc needs to be terminated.
 * It releases all device semaphores, invalidates the private page table entries,
 * and releases the swap pool semaphore.
 * Finally, it releases the MasterSemaphore and terminates the process.
 * 
 * @param supp Pointer to the support structure of the U-Proc to terminate.
 */
void terminateUProc(support_t* supp) {
  // Release all device semaphores
  for(int i = 3; i < 9; i++){
    int index = getDeviceSemIndex(i, supp->sup_asid - 1);
    if (SupportDeviceSemaphores[index] == 0){
      SYSCALL(VERHOGEN, (int)&SupportDeviceSemaphores[index], 0, 0);
    }
  }

  for (int i = 0; i < SWAP_POOL_SIZE; i++) {
    if (SwapTable[i].sw_asid == supp->sup_asid) {
      SwapTable[i].sw_asid = -1; // Invalidate the swap entry
      SwapTable[i].sw_pageNo = -1;
      SwapTable[i].sw_pte = NULL;
    }
  }

  if (AsidInSwapPool == supp->sup_asid) {
    AsidInSwapPool = 0;
    SYSCALL(VERHOGEN, (int)&SwapPoolSemaphore, 0, 0);
  }

  SYSCALL(VERHOGEN, (int)&MasterSemaphore, 0, 0);
  SYSCALL(TERMPROCESS, 0, 0, 0);
}

/**
 * writePrinter - Writes a string to the printer.
 * @param virtAddr: The virtual address of the string to write.
 * @param len: The length of the string to write.
 * 
 * This function writes a string to the printer device associated with the U-Proc.
 * 
 */
void writePrinter(char* virtAddr, int len, support_t* supp) {
  if (!_is_valid_address((memaddr)virtAddr, len)) {
    terminateUProc(supp);
  }

  int semIndex = getDeviceSemIndex(6, supp->sup_asid - 1);
  SYSCALL(PASSEREN, (int)&SupportDeviceSemaphores[semIndex],0, 0); /* P(sem_printer_mut) */

  dtpreg_t* printer_base = (dtpreg_t*)GET_DEV_BASE(6, (supp->sup_asid - 1));

  char* current_char = virtAddr;
  int chars_transmitted = 0;

  memaddr status;
  while (*current_char != EOS && chars_transmitted++ < len) {
    printer_base->data0 = *current_char++;
    status = SYSCALL(DOIO, (int)&printer_base->command, PRINTCHR, 0);
    
    if ((status) != READY) {
      supp->sup_exceptState[GENERALEXCEPT].reg_a0 = -status;
      
      SYSCALL(VERHOGEN, (int)&SupportDeviceSemaphores[semIndex], 0, 0);
      return;
    }
  }

  supp->sup_exceptState[GENERALEXCEPT].reg_a0 = len;
  SYSCALL(VERHOGEN, (int)&SupportDeviceSemaphores[semIndex] ,0, 0);
}


/**
 * @brief Writes a string to the terminal.
 *
 * This function writes a string to the terminal device associated with the U-Proc.
 * 
 * @param virtAddr The virtual address of the string to write.
 * @param len The length of the string to write.
 * @param supp Pointer to the support structure of the U-Proc.
 * 
 */
void writeTerminal(char* virtAddr, int len, support_t* supp) {
  if(!_is_valid_address((memaddr)virtAddr , len)) {
    terminateUProc(supp);
    return; 
  }

  int semIndex = getDeviceSemIndex(7, supp->sup_asid - 1);
  SYSCALL(PASSEREN, (int)&SupportDeviceSemaphores[semIndex],  0, 0); /* P(sem_term_mut) */

  int dev_no = supp->sup_asid - 1;
  
  termreg_t* terminal_dev = (termreg_t*)GET_DEV_BASE(7, dev_no);
  
  char* current_char = virtAddr;
  int chars_transmitted = 0;

  unsigned int status;
  while (*current_char != EOS && chars_transmitted++ < len) {
    int command_value = PRINTCHR | (*current_char++ << 8);;
    status = SYSCALL(DOIO, (int)&terminal_dev->transm_command, command_value, 0);
    
    if ((status & 0xFF) != CHARTRANSM) { 
      supp->sup_exceptState[GENERALEXCEPT].reg_a0 = -status;
      
      SYSCALL(VERHOGEN, (int)&SupportDeviceSemaphores[semIndex], 0, 0); /* V(sem_term_mut) */
      return;
    }
  }

  supp->sup_exceptState[GENERALEXCEPT].reg_a0 = chars_transmitted; 
  SYSCALL(VERHOGEN, (int)&SupportDeviceSemaphores[semIndex] ,0, 0); /* V(sem_term_mut) */
}

/**
 * @brief Reads a string from the terminal.
 *
 * When requested, causes the requesting U-proc to be suspended until a line of input 
 * has been transmitted from the terminal device associated with the U-proc.
 *
 * @param virtAddr The virtual address of a string buffer where the data read is placed.
 * @param supp Pointer to the support structure of the U-Proc.
 */
void readTerminal(char* virtAddr, support_t* supp) {
  int semIndex = getDeviceSemIndex(8, supp->sup_asid - 1);
  SYSCALL(PASSEREN, (int)&SupportDeviceSemaphores[semIndex], 0, 0); /* P(sem_term_mut) */

  int dev_no = supp->sup_asid - 1;
  
  termreg_t* terminal_dev = (termreg_t*)GET_DEV_BASE(7, dev_no);

  int chars_received = 0;
  char current_char = ' ';
  
  unsigned int status;
  while (current_char != EOS) {
    status = (unsigned int)SYSCALL(DOIO, (int)&terminal_dev->recv_command, RECEIVECHAR, 0);
    
    if ((status & 0xFF) != CHARRECV) { 
      supp->sup_exceptState[GENERALEXCEPT].reg_a0 = -status;
      
      SYSCALL(VERHOGEN, (int)&SupportDeviceSemaphores[semIndex] ,0, 0);
      return;
    }
    
    current_char = status >> 8;
    if (current_char == '\n' || current_char == '\r') {
      current_char = EOS;
    }

    *virtAddr++ = current_char; 
    chars_received++;
    
  }

  supp->sup_exceptState[GENERALEXCEPT].reg_a0 = chars_received; 
  SYSCALL(VERHOGEN, (int)&SupportDeviceSemaphores[semIndex] , 0, 0);
}

/**
 * @brief Handles system calls made by U-Processes.
 *
 * This function processes the system calls made by U-Processes based on the value of reg_a0 in the exception state.
 * It handles termination, writing to the printer, writing to the terminal, and reading from the terminal.
 *
 * @param supp Pointer to the support structure of the U-Proc making the system call.
 */
void syscallHandler(support_t* supp) {
  state_t* state = &supp->sup_exceptState[GENERALEXCEPT];

  switch (state->reg_a0) {
    case TERMINATE:
      terminateUProc(supp);
      break;    
    case WRITEPRINTER:
      writePrinter((char*)state->reg_a1, state->reg_a2, supp);
      break;
    case WRITETERMINAL:
      writeTerminal((char*)state->reg_a1, state->reg_a2, supp);
      break;
    case READTERMINAL:
      readTerminal((char*)state->reg_a1, supp);
      break;
  }
  
  state->pc_epc += 4;
  LDST(state);
}

/**
 * @brief Handles program trap exceptions.
 *
 * This function is called when a program trap exception occurs.
 * It terminates the U-Process.
 *
 * @param supp Pointer to the support structure of the U-Proc that caused the exception.
 */
void programTrapExceptionHandler(support_t* supp) { 
  terminateUProc(supp);
}

/**
 * @brief General exception handler for U-Processes.
 *
 * This function check the cause of the exception by looking at the current support structure's exception state.
 * If the cause indicates a system call, it calls the syscallHandler function.
 * Otherwise, it calls the programTrapExceptionHandler function.
 */
void generalExceptionHandler() {
  support_t* curr_supp = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

  int exception_code = curr_supp->sup_exceptState[GENERALEXCEPT].cause & GETEXECCODE;

  if (exception_code == SYSEXCEPTION) {
    syscallHandler(curr_supp);
  } else {
    programTrapExceptionHandler(curr_supp);
  }
}
