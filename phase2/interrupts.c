/**
 * ===============================================================
 * |                         INTERRUPTS                          |
 * ===============================================================
 *
 * @file interrupts.h
 * @brief Header file for interrupt handling in the system.
 *
 * This file contains the function declarations and includes necessary for
 * handling interrupts in the system.
 */

#include "./headers/interrupts.h"
#include "headers/exceptions.h"
#include "headers/initial.h"
#include <uriscv/const.h>
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#define START_DEVREG     0x10000054
#define INT_LINE_OFFSET  0x80
#define DEVS_PER_LINE    8
#define RECV_COMMAND_OFFSET  0x4
#define TRANSM_COMMAND_OFFSET 0xc
#define IL_FIRST_DEVICE_LINE 3
#define RECV_STATUS_OFFSET 0x0
#define TRANSM_STATUS_OFFSET 0x8

/**
 * @brief getLineNo
 * This function calculate the line number of the interrupt.
 * It uses the CAUSE register to determine the line number.
 */
int getLineNo() {
  int cause = getCAUSE() & CAUSE_EXCCODE_MASK;
  
  if (cause == IL_CPUTIMER) {
    return 1; // PLT
  }

  if (cause == IL_TIMER) {
    return 2; // PLT
  }

  if (cause == IL_DISK) {
    return 3; // Disk Device
  }

  if (cause == IL_FLASH) {
    return 4; // Flash Device
  }

  if (cause == IL_ETHERNET) {
    return 5; // Net Device
  }

  if (cause == IL_PRINTER) {
    return 6; // Printer Device
  }

  if (cause == IL_TERMINAL) {
    return 7; // Terminal Device
  }

  return 0; // Invalid line number
}

/**
 * @brief getHighestPriorityDeviceNumber
 * This function calculates the highest priority device number.
 * It uses the device word to determine the highest priority device.
 *
 * @return The highest priority device number.
 */
int getHighestPriorityDeviceNumber() {
  int dev_no = 0;
  
  memaddr* word = (memaddr*)0x10000040;
  unsigned int dev_word = word[getLineNo() - 3];

  while (dev_no < 8 && !(dev_word & (1 << dev_no))) dev_no++;
  
  if (dev_no >= 8) PANIC();

  return dev_no;
}

/**
 * @brief getDeviceSemaphoreIndex
 * This function calculates the semaphore index for a device based on its command address.
 * It handles both terminal and non-terminal devices.
 *
 * @details
 *  The function first checks if the command address is within the range of terminal devices.
 *  If it is, it calculates the device number and offset to determine the semaphore index.
 *    + 4 * DEVS_PER_LINE + dev_no * 2 for receive command
 *    + 4 * DEVS_PER_LINE + dev_no * 2 + 1 for transmit command
 *  If it is not, it calculates the semaphore index based on the command address and the device register size.
 *    + ((int_line - 3) * DEVS_PER_LINE) + dev_no
 * 
 * @param commandAddr The command address of the device.
 * @return The semaphore index for the device.
 */
int getDeviceSemaphoreIndex(int* commandAddr) {
  memaddr cmdAddr = (memaddr)commandAddr;

  // Check if the command address is within terminal device line (line 7)
  if (cmdAddr >= 0x10000254 && cmdAddr < 0x10000354) { // Assuming terminal devices start at 0x10000254 (line7)
    int devOffset = cmdAddr - 0x10000254;
    int dev_no = devOffset / 0x10; // Each terminal device occupies 16 bytes
    int offset = devOffset % 0x10;

    if (offset == 0x4) { // Receive command offset
      return (4 * DEVS_PER_LINE) + (dev_no * 2);
    } else if (offset == 0xC) { // Transmit command offset
      return (4 * DEVS_PER_LINE) + (dev_no * 2 + 1);
    } else {
      return -1; // Invalid offset
    }
  } else {
    // Handle non-terminal devices
    memaddr base = cmdAddr - 0x4; // Command register is at offset 0x4 from status
    int offset = base - START_DEVREG;

    if (offset < 0 || offset > (INT_LINE_OFFSET * 5)) {
      PANIC(); // Invalid device address
    }

    int int_line_index = offset / INT_LINE_OFFSET;
    int dev_no = (offset % INT_LINE_OFFSET) / DEVREGSIZE;
    int int_line = IL_FIRST_DEVICE_LINE + int_line_index;

    return ((int_line - 3) * DEVS_PER_LINE) + dev_no;
  }
}

/**
 * @brief handleDeviceInterrupt
 * 
 * This function handles device interrupts. It checks the interrupt line and
 * determines the device number. It then reads the status registers and acknowledges the interrupt.
 *
 * @details
 *  - For terminal devices, it checks the transmit and receive status.
 *  - If the transmit status indicates a received character, it acknowledges the transmit interrupt.
 *  - If the receive status indicates a received character, it acknowledges the receive interrupt.
 *  - For non-terminal devices, it reads the receive status and acknowledges the interrupt.
 *  - It then unblocks the corresponding semaphore and sets the return value in the PCB.
 *  - Finally, it checks if the current process is null and either schedules or loads the state of the current process.
 */
void handleDeviceInterrupt() {
  int int_line = getLineNo();
  int dev_no = getHighestPriorityDeviceNumber();

  memaddr dev_base = START_DEVREG + ((int_line - 3) * INT_LINE_OFFSET) + (dev_no * DEVREGSIZE);

  ACQUIRE_LOCK(&GlobalLock);
  if (int_line == 7) {
    // Read status registers first
    memaddr transm_status = *(memaddr*)(dev_base + TRANSM_STATUS_OFFSET);
    memaddr recv_status = *(memaddr*)(dev_base + RECV_STATUS_OFFSET);

    // Handle transmit first (higher priority)
    if ((*((memaddr*)(dev_base + 0x8)) & 0xFF) == RECVD) {
      // Acknowledge transmit interrupt
      *(memaddr*)(dev_base + TRANSM_COMMAND_OFFSET) = ACK;

      int semIndex = getDeviceSemaphoreIndex((int*)(dev_base + TRANSM_COMMAND_OFFSET));
      int* semaddr = &DeviceSemaphores[semIndex];
      
      pcb_t* unblocked = headBlocked(semaddr);
      if (!unblocked) {
        RELEASE_LOCK(&GlobalLock);
        return; // No process waiting on the semaphore
      }

      unblocked->p_s.reg_a0 = transm_status;

      *semaddr = 1;
      insertProcQ(&ReadyQueue, unblocked);
    } else {
      // handle receive interrupt
      *(memaddr*)(dev_base + RECV_COMMAND_OFFSET) = ACK;

      int semIndex = getDeviceSemaphoreIndex((int*)(dev_base + RECV_COMMAND_OFFSET));
      int* semaddr = &DeviceSemaphores[semIndex];

      pcb_t* unblocked = headBlocked(semaddr);
      if (!unblocked) {
        RELEASE_LOCK(&GlobalLock);
        return; // No process waiting on the semaphore
      }

      unblocked->p_s.reg_a0 = recv_status;
      *semaddr = 1;
      insertProcQ(&ReadyQueue, unblocked);
    }
  } else {
    
    // Non-terminal device handling
    memaddr status = *(memaddr*)(dev_base + RECV_STATUS_OFFSET);
    *(memaddr*)(dev_base + RECV_COMMAND_OFFSET) = ACK;

    int semIndex = getDeviceSemaphoreIndex((int*)(dev_base + 0x4));
    int* semaddr = &DeviceSemaphores[semIndex];

    pcb_t* unblocked = headBlocked(semaddr);
    if (!unblocked) {
      RELEASE_LOCK(&GlobalLock);
      return; // No process waiting on the semaphore
    }

    unblocked->p_s.reg_a0 = status;
    *semaddr = 1;
    insertProcQ(&ReadyQueue, unblocked);
  }

  RELEASE_LOCK(&GlobalLock);

  if (!CurrentProcess[getPRID()]) {
    scheduler();
  } else {
    LDST(&CurrentProcess[getPRID()]->p_s);
  }
}

/**
 * @brief handleProcessLocalTimerInterrupt
 * 
 * This function handles the process local timer interrupt.
 * It sets the timer to the timeslice value and saves the current process state.
 * It then inserts the current process into the ready queue and calls the scheduler.
 *
 * @details
 *   - It acquires the global lock to ensure mutual exclusion.
 *   - It sets the timer to the timeslice value.
 *   - It saves the current process state in the saved_state variable.
 *   - It then inserts the current process into the ready queue.
 *   - Finally, it releases the lock and calls the scheduler.
 */
void handleProcessLocalTimerInterrupt() {
  ACQUIRE_LOCK(&GlobalLock);
  
  setTIMER(TIMESLICE * TIMESCALEADDR);
  
  state_t* saved_state = (state_t*)GET_EXCEPTION_STATE_PTR(getPRID());
  CurrentProcess[getPRID()]->p_s = *saved_state;
 
  insertProcQ(&ReadyQueue, CurrentProcess[getPRID()]);

  RELEASE_LOCK(&GlobalLock);
  scheduler();
}

/**
 * @brief handlePseudoClockInterrupt
 *
 * This function handles the pseudo clock interrupt.
 * It loads the system-wide interval timer and unblocks any processes waiting on the pseudo clock semaphore.
 * It then checks if the current process is null and either schedules or loads the state of the current process.
 *  
 * @details
 *   - It acquires the global lock to ensure mutual exclusion.
 *   - It loads the system-wide interval timer.
 *   - It retrieves the pseudo clock semaphore address.
 *   - It unblocks any processes waiting on the pseudo clock semaphore.
 *   - It checks if the current process is null.
 *   - If it is, it releases the lock and calls the scheduler.
 *   - If it is not, it releases the lock and loads the state of the current process.
 */
void handlePseudoClockInterrupt() {
  ACQUIRE_LOCK(&GlobalLock);
  LDIT(PSECOND); // load the system wide interval timer
  
  int* semAddr = getPseudoClockSemaphore();
  
  pcb_t* unblocked = headBlocked(semAddr);

  if(unblocked){
    while ((unblocked = removeBlocked(semAddr))) {
      insertProcQ(&ReadyQueue, unblocked);
    }
  }
  RELEASE_LOCK(&GlobalLock);

  if (!CurrentProcess[getPRID()]) {
    scheduler();
  } else {
    LDST(&CurrentProcess[getPRID()]->p_s);
  }
}

/**
 * @brief INTERRUPT_handler
 * 
 * This function is the main interrupt handler. It determines the type of interrupt
 * and calls the appropriate handler function.
 *
 * @details
 *   - It uses the getLineNo() function to determine the type of interrupt.
 *   - It then calls the appropriate handler function based on the line number.
 */
void INTERRUPT_handler() {
  switch (getLineNo()) {
    case 1:
      handleProcessLocalTimerInterrupt();
      break;
    case 2:
      handlePseudoClockInterrupt();
      break;
    default: // Device Interrupt
      handleDeviceInterrupt();
      break;
  }
}
