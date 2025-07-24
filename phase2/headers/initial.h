/**
 * @file initial.h
 * @brief Header file for the initial module.
 * 
 * This file contains the necessary includes and function declarations
 * for the initial module of the project.
 */
#ifndef INITIAL_H
#define INITIAL_H

#include <uriscv/const.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../../headers/types.h"

#include "./scheduler.h"
#include "./exceptions.h"

// Semaphore helper function declarations
int* getPseudoClockSemaphore(void);
// int* getDeviceSemaphore(int commandAddr);

extern void test();
extern void uTLB_RefillHandler();

int main();

#endif // INITIAL_H
