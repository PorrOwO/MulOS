#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/const.h>
#include <uriscv/types.h>

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"

#define SWAP_POOL_SIZE 2 * UPROCMAX
#define SWAP_POOL_STARTADDR (RAMSTART + (64 * PAGESIZE) + (NCPU * PAGESIZE))

#define GET_PAGE_INDEX(vpn) (vpn == 0xBFFFF ? USERPGTBLSIZE - 1 : (vpn & 0xFF))

#define OFFSET_DATA0 0x8
#define OFFSET_COMMAND 0x4

#define GET_DEV_BASE(int_line, dev_num) (START_DEVREG + ((int_line - 3) * 0x80) + ((dev_num) * 0x10))

void initSwapStructs(void);
void TLB_Handler(void);

extern void uTLB_RefillHandler(void);
extern void programTrapExceptionHandler(support_t* supp);
extern int getDeviceSemIndex(int line, int dev);
#endif // VMSUPPORT.H
