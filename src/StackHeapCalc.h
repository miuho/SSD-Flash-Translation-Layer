/* Include file for StackHeapCalc.cpp */

#ifndef __STACKHEAPCALC_H_
#define __STACKHEAPCALC_H_

/* Defines for sizes */
#define KB 1024
#define MB 1024*KB
#define MAX_HEAP_SIZE 5000*KB
#define MAX_STACK_SIZE 5000*KB

/* Return errors in case of excesses */
#define STACK_EXCEED_ERR -1
#define HEAP_EXCEED_ERR -2

/* Function declaration */
int StackHeapCalc();

#endif
