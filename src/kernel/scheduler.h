#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "PCB.h"
#include "../util/linked_list.h"

/**
 * @file scheduler.h
 * @brief Header file for the scheduler module.
 */

/**
 * @struct SchedulerContext
 * @brief Structure for scheduler context.
 *
 * This structure holds the context information for the scheduler, including the ucontext, restartFlag, and other state variables.
 */
typedef struct {
    ucontext_t *context; ///< Pointer to the ucontext information.
    int restartFlag;     ///< Flag indicating whether a restart is required.
    // Other state variables...
} SchedulerContext;

/**
 * @brief Pointer to the scheduler context.
 */
extern SchedulerContext* schedulerContext;

/**
 * @brief Pointer to the PCB list.
 */
extern LinkedList* pcb_list;

/**
 * @brief Pointer to priority queue with priority -1.
 */
extern LinkedList* pq_neg_one;

/**
 * @brief Pointer to priority queue with priority 0.
 */
extern LinkedList* pq_zero;

/**
 * @brief Pointer to priority queue with priority 1.
 */
extern LinkedList* pq_one;

/**
 * @brief Pointer to the list of signals.
 */
extern LinkedList* signals;

/**
 * @brief Pointer to the shell context.
 */
extern ucontext_t* shellContext;

/**
 * @brief Pointer to the shell process.
 */
extern PCB* shellProcess;

/**
 * @brief Pointer to the foreground process.
 */
extern PCB* foregroundProcess;

/**
 * @brief Pointer to the current process.
 */
extern PCB* currentProcess;

/**
 * @brief Centisecond constant.
 */
extern const int centisecond;

/**
 * @brief Pointer to the list of waiting processes.
 */
extern LinkedList* waiting_processes;

/**
 * @brief Current ticks count.
 */
extern unsigned int curr_ticks;

/**
 * @brief Pointer to the timer queue.
 */
extern LinkedList* timerQueue;

/**
 * @brief Schedule a process for execution.
 *
 * @param process Pointer to the PCB of the process to be scheduled.
 */
void scheduleProcess(PCB* process);

/**
 * @brief Get a pointer to the currently executing process's PCB.
 *
 * @return Pointer to the PCB of the currently executing process.
 */
PCB* k_current_process();

/**
 * @brief Get the priority queue number for the current process.
 *
 * @return The priority queue number.
 */
int k_get_pq_num();

/**
 * @brief Get the priority queue of a process.
 *
 * @param pcb Pointer to the PCB of the process.
 * @return Pointer to the priority queue of the process.
 */
LinkedList* get_process_pq(PCB* pcb);

/**
 * @brief Timer service routine.
 */
void timerService();

#endif // SCHEDULER_H
