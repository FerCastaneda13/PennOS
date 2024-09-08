#ifndef KERNEL_H
#define KERNEL_H

#include <ucontext.h>
#include "PCB.h"

/**
 * @file kernel.h
 * @brief Header file for kernel-level functions related to process management.
 */

/**
 * @brief Creates a new child thread and associated PCB.
 *
 * The new thread retains many properties of the parent. The function returns a reference to the new PCB.
 *
 * @param parent Pointer to the parent PCB.
 * @return Pointer to the newly created child PCB.
 */
PCB* k_process_create(PCB *parent);

/**
 * @brief Kills the process referenced by `process` with the specified `signal`.
 *
 * @param process Pointer to the PCB of the process to be terminated.
 * @param signal The signal used to terminate the process.
 */
void k_process_kill(PCB *process, int signal);

/**
 * @brief Cleans up resources of a terminated/finished thread.
 *
 * This function is called when a terminated/finished thread's resources need to be cleaned up. Cleanup may include
 * freeing memory, setting the status of the child, etc.
 *
 * @param process Pointer to the PCB of the terminated thread.
 */
void k_process_cleanup(PCB *process);

/**
 * @brief Retrieves the cleanup context for a process.
 *
 * @param process Pointer to the PCB of the process.
 * @return Pointer to the cleanup context.
 */
ucontext_t* cleanup_context(PCB* process);

/**
 * @brief Sets the stack for a process.
 *
 * @param stack Pointer to the stack structure.
 */
void setStack(stack_t *stack);

#endif // KERNEL_H
