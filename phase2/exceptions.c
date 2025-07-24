/**
 * ===============================================================
 * |                         EXCEPTIONS                          |
 * ===============================================================
 * 
 * @file interrupts.h
 * @brief Header file for interrupt handling in the system.
 *
 * This file contains the function declarations and includes necessary for
 * handling interrupts in the system.
 */

#include "headers/initial.h"
#include "./headers/exceptions.h"
#include "./headers/exceptions.h"
#include "headers/scheduler.h"
#include <uriscv/const.h>
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

cpu_t lastTOD;

/**
 * @brief getTimeElapsed
 * This function calculates the elapsed time since the last call.
 */
cpu_t getTimeElapsed() {
  cpu_t currTOD;
  STCK(currTOD);

  cpu_t elapsed = currTOD - lastTOD;
  STCK(lastTOD);

  return elapsed;
}

/* TLB-Refill Handler */
void uTLB_RefillHandler() {
  ACQUIRE_LOCK(&GlobalLock);
  state_t* saved_state = (state_t*)GET_EXCEPTION_STATE_PTR(getPRID());

  unsigned int entry_hi = saved_state->entry_hi;
  unsigned int vpn  = (entry_hi & 0xFFFFF000) >> VPNSHIFT; // Extract the VPN from entry_hi

  int index = (vpn == 0xBFFFF ? USERPGTBLSIZE - 1 : (vpn & 0xFF));
  pteEntry_t* pte = &(CurrentProcess[getPRID()]->p_supportStruct->sup_privatePgTbl[index]);

  setENTRYHI(pte->pte_entryHI);
  setENTRYLO(pte->pte_entryLO);
  TLBWR();

  RELEASE_LOCK(&GlobalLock);
  LDST(saved_state);
}

/**
 * @brief findPcb
 * This function searches for a process control block (PCB) with the given PID.
 * It first checks the ready queue and then checks the blocked queues of the device semaphores.
 * 
 * @param pid The process ID to search for.
 * @return A pointer to the PCB if found, NULL otherwise.
 */
static inline pcb_t* findPcb(int pid) {
  // if pid = 0 then the process that needs to be terminated is the caller, so the current process
  if (pid == 0) {
    return CurrentProcess[getPRID()];
  }

  // first i search it in the ready queue
  struct list_head* iter; 
  list_for_each(iter, &ReadyQueue) {
    pcb_t* pcb = container_of(iter, pcb_t, p_list);
    if(pcb->p_pid == pid) {
      return pcb;
    } 
  }

  // if not found in the ready queue, i search it in the blocked queues of the device semaphores
  pcb_t* pcb = outBlockedPID(pid);
  if (pcb != NULL) {
    return pcb;
  }

  // if it reaches this point, it means that no process was found with the given pid
  return NULL;
}

/**
 * @brief terminateProcessSubTree
 * This function recursively terminates a process and all its children and siblings.
 * It removes the process from the ready queue and the blocked queue, and frees its PCB.
 * 
 * @param target The target process to terminate.
 */
static inline void terminateProcessSubTree(pcb_t* target) {
  if (target == NULL) return;

  // Terminate all children and siblings recursively
  if (!emptyChild(target)) {
    terminateProcessSubTree(container_of(target->p_child.next, pcb_t, p_sib));
  }

  if (!list_is_last(&target->p_sib, &target->p_parent->p_child)) {
    terminateProcessSubTree(container_of(target->p_sib.next, pcb_t, p_sib));
  }
 
  // orphan the child
  outChild(target);
  
  // Remove from the ready queue
  outProcQ(&ReadyQueue, target);
  
  // Remove from the blocked queue
  outBlocked(target);

  // Update the process count
  ProcessCount--;
  
  // Free the PCB
  freePcb(target);

  //search for the target in the current process of each CPU and set it  to NULL
  for (int i = 0; i < NCPU; i++) {
    if (CurrentProcess[i] == target) {
      CurrentProcess[i] = NULL;
    }
  }
}

void* memcpy(void* dest, const void* src, size_tt n){
  char* d = dest;
  const char* s = src;
  for (; n > 0; n--)
    *d++ = *s++;
  return dest;
}

/**
* @brief createProcess
* this function creates a new process with the given state and support structure.
* the newly created process is placed on the ready queue with its parent being the current process.
*
* @param statep The state of the new process.
* @param supportStruct The support structure of the new process.
* @return The PID of the newly created process.
*/
void createProcess(state_t* statep, support_t* supportStruct) {
  ACQUIRE_LOCK(&GlobalLock);
  state_t* saved_state = (state_t*)GET_EXCEPTION_STATE_PTR(getPRID());

  //tries allocate a new PCB and if it's not possible return -1
  pcb_t* newProcess = allocPcb();
  if (!newProcess) {
    saved_state->reg_a0 = -1;
    RELEASE_LOCK(&GlobalLock);
    return;
  }

  // Copy the state of the current process to the new process
  newProcess->p_s = *statep;

  // Copy the support structure to the new process
  newProcess->p_supportStruct = supportStruct ? supportStruct : NULL;

  // Set process queue fields
  insertProcQ(&ReadyQueue, newProcess);
  
  // Set process tree fields
  insertChild(CurrentProcess[getPRID()], newProcess);

  // Update process count
  ProcessCount++;

  // Set accumulated CPU time to zero// block the current process
  newProcess->p_time = 0;

  // Set semaphore address to NULL
  newProcess->p_semAdd = NULL;

  saved_state->reg_a0 = newProcess->p_pid;
  RELEASE_LOCK(&GlobalLock);
}

/**
* @brief terminateProcess
* this function terminates the process with the given pid, this includes all its children.
*
* @param pid The process ID to terminate. If pid is 0, the current process is terminated.
*/
void terminateProcess(int pid){
  ACQUIRE_LOCK(&GlobalLock);

  pcb_t* target;

  if (pid == 0) { //if the pid refers to the current process
    target = CurrentProcess[getPRID()];

    if (!emptyChild(target)) {
      terminateProcessSubTree(container_of(target->p_child.next, pcb_t, p_sib));
    }
  } else {
    target = findPcb(pid);
    
    if (!target) {
      RELEASE_LOCK(&GlobalLock); 
      return;
    }

    if (!emptyChild(target)) {
      terminateProcessSubTree(container_of(target->p_child.next, pcb_t, p_sib));
    }
  }
  
  // orphan the child
  outChild(target);
  
  // Remove from the ready queue
  outProcQ(&ReadyQueue, target);
  
  outBlocked(target);

  // Update the process count
  ProcessCount--;
  
  // Free the PCB
  freePcb(target); // TODO: check if the function is correct

  //search for the target in the current process of each CPU and set it  to NULL
  for (int i = 0; i < NCPU; i++) {
    if (CurrentProcess[i] == target) {
      CurrentProcess[i] = NULL;
    }
  }
  
  RELEASE_LOCK(&GlobalLock);
  scheduler();
}

/**
 * @brief passeren
 * this function is called when a process wants to wait on a semaphore.
 * if the semaphore is 0, the process is blocked and added to the semaphore's blocked queue.
 * if the semaphore is 1, the process is unblocked and added to the ready queue.
 * 
 * @param semAddr The address of the semaphore to wait on.
 */
void passeren(int* semAddr) {
  ACQUIRE_LOCK(&GlobalLock);
    
  if (*semAddr == 0) {//the current process must be blocked
    state_t* saved_state = (state_t*)GET_EXCEPTION_STATE_PTR(getPRID());

    //update the fields of the current process
    CurrentProcess[getPRID()]->p_s = *saved_state;
    CurrentProcess[getPRID()]->p_time += getTimeElapsed();
    CurrentProcess[getPRID()]->p_semAdd = semAddr;

    // remove from ready queue and insert into the semaphore's blocked queue
    if(CurrentProcess[getPRID()]){
      insertBlocked(semAddr, CurrentProcess[getPRID()]);
    }

    // increment program counter by 4 so that the process skips the syscall instruction
    CurrentProcess[getPRID()]->p_s.pc_epc += 4;
    
    CurrentProcess[getPRID()] = NULL;

    RELEASE_LOCK(&GlobalLock);
    
  /*since in this case passeren blocks the current process,
    the scheduler needs to be called in order to make another process execute
  */
    scheduler();
  } else {
  /*in this case the call to passeren doesn't block the current process,
    but unblocks the first processes that was waiting on the semaphore
  */
    pcb_t* unblocked = removeBlocked(semAddr);
    if (unblocked) {  
      insertProcQ(&ReadyQueue, unblocked);
    } else {
      *semAddr = 0;
    }

    RELEASE_LOCK(&GlobalLock);
  }
}


/**
 * @brief verhogen
 * this function is called when a process wants to signal a semaphore.
 * if the semaphore is 1, the process is blocked and added to the semaphore's blocked queue.
 * if the semaphore is 0, the process is unblocked and added to the ready queue.
 * 
 * @param semAddr The address of the semaphore to signal.
 */
void verhogen(int* semAddr) {
  ACQUIRE_LOCK(&GlobalLock);
  if (*semAddr == 1) {//the current process must be blocked
    // block the current process
    state_t* saved_state = (state_t*)GET_EXCEPTION_STATE_PTR(getPRID());

    //update the fields of the current process
    CurrentProcess[getPRID()]->p_s = *saved_state;
    CurrentProcess[getPRID()]->p_time += getTimeElapsed();
    CurrentProcess[getPRID()]->p_semAdd = semAddr;

    // remove from ready queue and insert into the semaphore's blocked queue
    insertBlocked(semAddr, CurrentProcess[getPRID()]);
    
    // increment program counter by 4 so that the process skips the syscall instruction
    CurrentProcess[getPRID()]->p_s.pc_epc += 4;

    CurrentProcess[getPRID()] = NULL;

    RELEASE_LOCK(&GlobalLock);
    
  /*since in this case verhogen blocks the current process,
    the scheduler needs to be called in order to make another process execute
  */
    scheduler();
  } else {


  /*in this case the call to verhogen doesn't block the current process,
    but unblocks the first processes that was waiting on the semaphore
  */
    pcb_t* unblocked = removeBlocked(semAddr);
    if (unblocked) {
      insertProcQ(&ReadyQueue, unblocked);
    } else {
      *semAddr = 1;
    }
    RELEASE_LOCK(&GlobalLock);
  }
}

/**
 * @brief doIo
 * this function is called when a process wants to perform an I/O operation.
 * it sets the command value in the command address and waits for the semaphore to be signaled.
 * 
 * @param commandAddr The address of the command to perform.
 * @param commandValue The value of the command to perform.
 */
 void doIo(int* commandAddr, int commandValue) {
  ACQUIRE_LOCK(&GlobalLock);
  
  int sem_index = getDeviceSemaphoreIndex(commandAddr);
  int* semaddr = &DeviceSemaphores[sem_index];
  
  // Issue the I/O command WHILE the lock is held
  *((memaddr*)commandAddr) = commandValue;
  
  // Now, block the current process using the logic from passeren
  // We assume the device semaphore is 0, indicating a process must wait.
  state_t* saved_state = (state_t*)GET_EXCEPTION_STATE_PTR(getPRID());
  
  CurrentProcess[getPRID()]->p_s = *saved_state;
  CurrentProcess[getPRID()]->p_time += getTimeElapsed();
  CurrentProcess[getPRID()]->p_semAdd = semaddr;

  // Add the process to the semaphore's blocked queue
  insertBlocked(semaddr, CurrentProcess[getPRID()]);
  
  CurrentProcess[getPRID()]->p_s.pc_epc += 4;
  // Mark the current CPU as having no running process
  CurrentProcess[getPRID()] = NULL;
  
  // Release the lock BEFORE calling the scheduler
  RELEASE_LOCK(&GlobalLock);
  
  // Yield the CPU
  scheduler(); 
}
/**
 * @brief waitForClock
 * this function is called when a process wants to wait for the clock.
 * it waits for the pseudo clock semaphore to be signaled.
 */
void waitForClock(){
  int* semaddr = getPseudoClockSemaphore();
  passeren(semaddr);
}

/**
 * @brief getCPUTime
 * this function is called when a process wants to get its CPU time.
 * it returns the current process's CPU time plus the amount of CPU time used during the current time slice.
 */
void getCPUTime() {
  ACQUIRE_LOCK(&GlobalLock);
  
  state_t* savedState = (state_t*)GET_EXCEPTION_STATE_PTR(getPRID());

  // should return current process p_time + amount of cpu time used during the current time slice
  savedState->reg_a0 = CurrentProcess[getPRID()]->p_time + getTimeElapsed();

  RELEASE_LOCK(&GlobalLock);
}

/**
 * @brief getSupportData
 * this function is called when a process wants to get its support data.
 * it returns the address of the support structure of the current process.
 */
void getSupportData() {
  ACQUIRE_LOCK(&GlobalLock);
  
  support_t* supportData = CurrentProcess[getPRID()]->p_supportStruct;

  state_t* savedState = (state_t*)GET_EXCEPTION_STATE_PTR(getPRID());

  if (supportData) {
    savedState->reg_a0 = (memaddr)supportData;
  } else {
    savedState->reg_a0 = 0;
  }
  
  RELEASE_LOCK(&GlobalLock);
}

/**
 * @brief getProcessID
 * this function is called when a process wants to get the process ID of its parent.
 * it returns the process ID of the current process or the parent process.
 * 
 * @param parent of the process to get the ID of.
 * @return The process ID of the current process or the parent process.
 */
void getProcessID(int parent) {
  ACQUIRE_LOCK(&GlobalLock);
  
  state_t* savedState = (state_t*)GET_EXCEPTION_STATE_PTR(getPRID());

  if (parent == 0) {
    savedState->reg_a0 = CurrentProcess[getPRID()]->p_pid;
  } else {
    savedState->reg_a0 = CurrentProcess[getPRID()]->p_parent->p_pid;
  }
  
  RELEASE_LOCK(&GlobalLock);
}

static inline void passUpToSupportLevel(int exceptionType, state_t* savedState) {
  ACQUIRE_LOCK(&GlobalLock);

  support_t* currentSupport = (support_t*)CurrentProcess[getPRID()]->p_supportStruct;

  currentSupport->sup_exceptState[exceptionType] = *savedState;

  context_t* currentContext = &currentSupport->sup_exceptContext[exceptionType];

  RELEASE_LOCK(&GlobalLock);
  LDCXT(currentContext->stackPtr, currentContext->status, currentContext->pc);
}

/**
 * @brief handleProgramTrap
 * this function is called when a program trap occurs.
 * if the current process has a support structure, it passes the exception to the support level.
 * otherwise it terminates the process.
 */
static inline void handleProgramTrap(state_t* savedState) {
  ACQUIRE_LOCK(&GlobalLock);
  support_t* currentSupport = CurrentProcess[getPRID()]->p_supportStruct;
  RELEASE_LOCK(&GlobalLock);

  if (currentSupport) {
    passUpToSupportLevel(GENERALEXCEPT, savedState);
  } else {
    terminateProcess(0);
  }
}

static inline void handleTLBException(state_t* savedState) {
  ACQUIRE_LOCK(&GlobalLock);
  support_t* currentSupport = CurrentProcess[getPRID()]->p_supportStruct;
  RELEASE_LOCK(&GlobalLock);
  
  if (currentSupport) {
    passUpToSupportLevel(PGFAULTEXCEPT, savedState);
  } else {
    terminateProcess(0);
  }
}

void SYSCALL_handler(state_t* exceptionState) {
  if (!(exceptionState->status & MSTATUS_MPP_MASK)) {
    exceptionState->cause = PRIVINSTR;
    handleProgramTrap(exceptionState);
  } else {
    switch (exceptionState->reg_a0) {
      case CREATEPROCESS:
        createProcess((state_t*)exceptionState->reg_a1, (support_t*)exceptionState->reg_a3);
        break;
      case TERMPROCESS:
        terminateProcess(exceptionState->reg_a1); //termina il controllo
        break;
      case PASSEREN: // blocking
        passeren((int*)exceptionState->reg_a1);       
        break;
      case VERHOGEN: // blocking
        verhogen((int*)exceptionState->reg_a1);
        break;
      case DOIO: // blocking
        doIo((int*)exceptionState->reg_a1, exceptionState->reg_a2);
        break;
      case GETTIME:
        getCPUTime();
        break;
      case CLOCKWAIT: // blocking
        waitForClock();
        break;
      case GETSUPPORTPTR:
        getSupportData();
        break;
      case GETPROCESSID:
        getProcessID(exceptionState->reg_a1);
        break;
      default:
        handleProgramTrap(exceptionState);
        break;
    }
    exceptionState->pc_epc += 4;
    LDST(exceptionState);
  }
}

void exceptionHandler(void) {
  state_t* exceptionState = (state_t*)GET_EXCEPTION_STATE_PTR(getPRID());

  int cause = getCAUSE();

  //interrupt 
  if (CAUSE_IS_INT(cause)) {
    INTERRUPT_handler();
    return;
  }
  //TLB exception
  if (cause >= 24 && cause <= 28 ) {
    handleTLBException(exceptionState);
    return;      
  } 
  //SYSCALL exception
  if (cause == 8 || cause == 11) {
    SYSCALL_handler(exceptionState);
    return;
  }
  //trap exception
  if ((cause >= 0 && cause <= 7) || cause == 9 || cause == 10 || (cause >= 12 && cause <= 23)) {
    handleProgramTrap(exceptionState);
    return;
  }
  
  PANIC();
}
