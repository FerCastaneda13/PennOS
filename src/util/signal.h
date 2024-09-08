/**
 * @file signal.h
 * @brief Header file for the PennOS signal, containing function declarations for signal system and enum definition. Note that some functions are not used in final code
 */

// Created by Omar Ameen on 11/11/23.

#ifndef INC_23FA_CIS3800_PENNOS_39_SIGNAL_H
#define INC_23FA_CIS3800_PENNOS_39_SIGNAL_H

#include "../kernel/PCB.h"
#include "sys/types.h"

/// Structure representing a signal in the system
typedef struct signal
{
  int sender_pid;   ///< PID of the sender of the signal
  int receiver_pid; ///< PID of the receiver of the signal
  int signal;       ///< Signal number
} signal_t;

/// Enum for different types of signals
typedef enum signal_enum
{
  S_SIGSTOP = -5,    ///< Signal to stop a process
  S_SIGCONT = -6,    ///< Signal to continue a stopped process
  S_SIGTERM = -7,    ///< Signal to terminate a process
  R_SIGSTOP = 5,     ///< Reply signal for stop
  R_SIGTERM = 6,     ///< Reply signal for terminate
  R_SIGEXIT = 7,     ///< Reply signal for exit
  R_SIGCONT = 11,    ///< Reply signal for continue
  S_SIGCHLD = 12     ///< Signal indicating a change in child process
} enum_signal;

/// Enum for different process exit statuses
typedef enum exit_enum
{
  E_EXITED = 7,              ///< Exited normally
  E_STOPPED = 8,             ///< Stopped by a signal
  E_TERMINATE_SIGNALED = 9,  ///< Terminated by a signal
  E_RUNNING = 10             ///< Currently running
} enum_exit;

/// Structure for processes waiting on a particular PID
typedef struct waiting_process
{
  PCB *pcb;  ///< Pointer to the PCB of the waiting process
  pid_t pid; ///< PID of the process being waited on
} waiting_process_t;

/// Structure for a timer associated with a process
typedef struct timer
{
  unsigned int end_time; ///< Time at which the timer ends
  PCB *process;          ///< Pointer to the process associated with the timer
} timerstruct_t;

/// Adds a process to the wait queue
void add_waiter(PCB *pcb, pid_t pid);

/// Adds a timer for a process to expire after a certain number of ticks
void add_timer(PCB *pcb, unsigned int ticks);

/// Retrieves a waiting process by PID
Node *get_waiting_process(pid_t pid);

/// Retrieves a waiting process by PCB pointer
Node *get_waiting_process_by_pointer(PCB *pcb, int waiting_on);

/// Adds a signal to the signal queue
signal_t *add_signal(int sender_pid, int receiver_pid, int signal);

#endif // INC_23FA_CIS3800_PENNOS_39_SIGNAL_H
