/**
 * @file StackHeapCalc.cpp
 * @brief Parses the /proc/<pid>/status file for heap and stack sizes to detect
 * excess usage.
 *
 * @author Sudhir Vijay
 * @version 1.0
 * @date 2015-10-03
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "StackHeapCalc.h"

int StackHeapCalc() {
    char proc_file[50];
    sprintf(proc_file, "/proc/%u/status", (unsigned) getpid());
    //printf("[StackHeapCalc]: proc_file: %s \n", proc_file);

    FILE *proc_fd = fopen(proc_file, "r");
    int heap_size = 0, stack_size = 0;
    assert(proc_fd != NULL);

    char* line;
    size_t len = 0;

    while (getline(&line, &len, proc_fd) != -1) {
        if (sscanf(line, "VmData:    %d kB", &heap_size)) {
            //printf("[StackHeapCalc]: Heap size: %d kB\n", heap_size);
        }
        if (sscanf(line, "VmStk:    %d kB", &stack_size)) {
            //printf("[StackHeapCalc]: Stack size: %d kB\n", stack_size);
            break;
        }
    }

    if (stack_size > MAX_STACK_SIZE) {
        printf("[StackHeapCalc]: FAIL! Measured stack size: %d |\
                Allowed stack size: %d\n", stack_size, MAX_STACK_SIZE);
        return (STACK_EXCEED_ERR);
    }

    if (stack_size > MAX_STACK_SIZE) {
        printf("[StackHeapCalc]: FAIL! Measured heap size: %d |\
                Allowed heap size: %d\n", heap_size, MAX_HEAP_SIZE);
        return (HEAP_EXCEED_ERR);
    }

    //printf("[StackHeapCalc]: Stack and heap within limits \n");
    return heap_size + stack_size;
}
