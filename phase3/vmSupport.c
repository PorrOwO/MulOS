/**
 * ============================== VIRTUAL MEMORY ==============================
 *
 * @file vmSupport.c
 *
 * This file contains the implementation of virtual memory support functions,
 * including TLB management, swap table initialization, and interrupt handling.
 */
#include "headers/vmSupport.h"

int SwapPoolSemaphore = 1;
int AsidInSwapPool = 0;
swap_t SwapTable[SWAP_POOL_SIZE];

/** 
 * @brief _getFreeSwapFrameIndex
 * 
 * this function retrieves the index of a free frame in the swap pool.
 * 
 * @returns the index of a free frame in the swap pool.
 *
 */
static inline int _getFreeSwapFrameIndex(void) {
  static int free_frame_index = 0;
  
  int i = 0;
  while ((i < (SWAP_POOL_SIZE)) && SwapTable[(free_frame_index + i) % (SWAP_POOL_SIZE)].sw_asid != -1) {
    i++;
  }

  if (i == SWAP_POOL_SIZE) {
    return 1; 
  }

  free_frame_index = (free_frame_index + i) % (SWAP_POOL_SIZE);
  return free_frame_index;
}

/** 
 * @brief _flashIO
 * @param asid The address space identifier (ASID) of the process.
 * @param vpn The virtual page number to access.
 * @param command The command to execute (FLASHREAD or FLASHWRITE).
 * @param starting_frame_addr The starting address of the frame to read/write.
 * 
 * This function performs I/O operations on the flash device.
 * It uses the ASID and VPN to determine the device and the command to execute.
 * It locks the device semaphore, performs the I/O operation, and then releases the semaphore.
 * 
 */
static inline void _flashIO(int asid, int vpn, int command, memaddr starting_frame_addr) {
  int dev = asid - 1; // ASID starts from 1, so we subtract 1 to get the index
  support_t* supp = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
  
  dtpreg_t* flash_base = (dtpreg_t*)GET_DEV_BASE(4, dev);
  flash_base->data0 = starting_frame_addr;
  
  int device_block_number = (int)GET_PAGE_INDEX(vpn);
  int command_value = (device_block_number << 8) | command;

  unsigned int status = (unsigned int)SYSCALL(DOIO, (int)&flash_base->command, command_value, 0);
 
  if (status != READY) {
    programTrapExceptionHandler(supp);
  }
}

/**
 * @brief disable Interrupts
 * 
 * This function disables interrupts by clearing the interrupt enable bit in the STATUS register.
 */
static inline void disableInterrupts(void) {
  setSTATUS(getSTATUS() & DISABLEINTS);
}

/**
 * @brief enable Interrupts
 * 
 * This function enables interrupts by setting the interrupt enable bit in the STATUS register.
 */
static inline void enableInterrupts(void) {
  setSTATUS(getSTATUS() | IECON);
}

/**
 * @brief initSwapStructs
 * 
 * This function initializes the swap table by setting all entries to an invalid state.
 * It sets the ASID and page number to -1 and the page table entry pointer to NULL.
 */
void initSwapStructs(void) {
  for (int i = 0; i < SWAP_POOL_SIZE; i++) {
    SwapTable[i].sw_asid = -1;
    SwapTable[i].sw_pageNo = -1;
    SwapTable[i].sw_pte = NULL;
  }
}

/**
 * @brief updateTLB_Clear
 * 
 * This function clears the TLB and writes a new page table entry into it.
 * It sets the ENTRYHI and ENTRYLO registers with the provided page table entry.
 *
 * @param page Pointer to the page table entry to be written into the TLB.
 */
static inline void updateTLB_Clear(pteEntry_t* page) {
  TLBCLR();
  setENTRYHI(page->pte_entryHI);
  setENTRYLO(page->pte_entryLO);
  TLBWR();
}

/**
 * @brief updateTLB_Probe
 * 
 * This function probes the TLB for a specific page table entry.
 * If the entry is not present, it writes the entry into the TLB.
 *
 * @param page Pointer to the page table entry to be probed and potentially written into the TLB.
 */
static inline void updateTLB_Probe(pteEntry_t* page) {
  setENTRYHI(page->pte_entryHI);
  TLBP();

  if ((getINDEX() & PRESENTFLAG) == 0) {
    setENTRYLO(page->pte_entryLO);
    TLBWI();
  }
}

/**
 * @brief Handles the TLB (Translation Lookaside Buffer) exception.
 *
 * This function is called when a page fault occurs, and it retrieves the
 * current support structure for the process that caused the exception.
 * It then updates the TLB with the appropriate page table entry
 * and handles the page fault accordingly.
 */
void TLB_Handler(void){
  /* Obtain the pointer to the current support structure */
  support_t* curr_supp = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

  /* Get the current state of the exception */
  state_t* saved_exception_state = &curr_supp->sup_exceptState[PGFAULTEXCEPT];

  /* Get the cause of the exception */
  int cause = saved_exception_state->cause; 

  if (cause != 25 && cause != 26) {
    programTrapExceptionHandler(curr_supp);
  }

  /* Gain mutual exclusion over the Swap Pool semaphore */
  SYSCALL(PASSEREN, (int)&SwapPoolSemaphore, 0, 0);
  AsidInSwapPool = curr_supp->sup_asid;

  /* Determine the missing page number, found in the saved exception state */
  int missing_page_num = (saved_exception_state->entry_hi & 0xFFFFF000) >> VPNSHIFT;

  int index = (int)GET_PAGE_INDEX(missing_page_num);
  // int index = missing_page_num % USERPGTBLSIZE;

  /* Check if the page is loaded in the swap pool */
  for (int i = 0; i < SWAP_POOL_SIZE; i++) {
    if (SwapTable[i].sw_asid == curr_supp->sup_asid && SwapTable[i].sw_pageNo == missing_page_num) {
      /* Update the TLB with the page table entry */
      // disableInterrupts();
      updateTLB_Probe(SwapTable[i].sw_pte);
      // enableInterrupts();
      
      if (SwapTable[i].sw_pte->pte_entryLO & VALIDON) {
        AsidInSwapPool = 0;
        SYSCALL(VERHOGEN, (int)&SwapPoolSemaphore, 0, 0);
        
        LDST(saved_exception_state);
      }
    }
  }

  /* Pick a victim frame to evict */
  int victim_frame_index = _getFreeSwapFrameIndex();
  memaddr frame_addr = (memaddr)(SWAP_POOL_STARTADDR + (victim_frame_index * PAGESIZE));
  
  /* Check if the victim frame is occupied */
  swap_t* swap_entry = &SwapTable[victim_frame_index];
  if (swap_entry->sw_asid != -1) {
    pteEntry_t* victim_page = swap_entry->sw_pte;

    disableInterrupts();
    victim_page->pte_entryLO &= ~VALIDON; /* Invalidate the page */
    updateTLB_Probe(victim_page);         /* Update TLB */

    /* update process's backing store */
    _flashIO(swap_entry->sw_asid, swap_entry->sw_pageNo, FLASHWRITE, frame_addr);
    enableInterrupts();
  }

  /* Read the contents of the current process backing store */
  _flashIO(curr_supp->sup_asid, missing_page_num, FLASHREAD, frame_addr);

  /* Update the swap table entry */
  swap_entry->sw_asid = curr_supp->sup_asid;
  swap_entry->sw_pageNo = missing_page_num;
  swap_entry->sw_pte = &(curr_supp->sup_privatePgTbl[index]);

  disableInterrupts();
  /* Update the page table entry */
  curr_supp->sup_privatePgTbl[index].pte_entryLO = frame_addr | VALIDON | DIRTYON;
  /* Update the TLB with the new page table entry */
  updateTLB_Probe(&curr_supp->sup_privatePgTbl[index]);
  enableInterrupts();

  /* Release the Swap Pool semaphore */
  AsidInSwapPool = 0;
  SYSCALL(VERHOGEN, (int)&SwapPoolSemaphore, 0, 0);

  /* Return control to the saved exception state */
  LDST(saved_exception_state);
}
