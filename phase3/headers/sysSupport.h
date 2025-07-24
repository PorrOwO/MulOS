#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/const.h>
#include <uriscv/types.h>

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include "../../phase3/headers/vmSupport.h"

#include "vmSupport.h"

#define CHARTRANSM 5

void terminateUProc(support_t* supp);
void writePrinter(char* virtAddr, int len, support_t* supp);
void writeTerminal(char* virtAddr, int len, support_t* supp);
void readTerminal(char* virtAddr, support_t* supp);

void syscallHandler(support_t* supp);
void programTrapExceptionHandler(support_t* supp);
void generalExceptionHandler();

extern int MasterSemaphore;
extern int SwapPoolSemaphore;
extern int AsidInSwapPool;
extern int SupportDeviceSemaphores[SEMDEVLEN - 1];
extern swap_t SwapTable[SWAP_POOL_SIZE];

extern int getDeviceSemIndex(int line, int dev);

#endif // SYSSUPPORT_H
