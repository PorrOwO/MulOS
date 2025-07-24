#ifndef INITPROC_H
#define INITPROC_H

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/const.h>
#include <uriscv/types.h>

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"

#define TERM0ADDR 0x10000254
#define TERMSTATMASK 0xFF
#define GET_FLASH_BASE(int_line, asid) (START_DEVREG + (int_line * 0x04) + ((asid - 1) * DEVREGSIZE))

void test(void);

extern void generalExceptionHandler(void);
extern void TLB_Handler(void);
extern void initSwapPoolTable(void);
extern void initSwapStructs(void);

#endif // INITPROC_H
