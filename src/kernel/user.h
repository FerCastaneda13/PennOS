#ifndef USER_H
#define USER_H

#include <sys/types.h> // For data types like pid_t
#include <stdbool.h>   // For boolean data type
#include "PCB.h"

/**
 * @file user.h
 * @brief Header file for user-level functions and system calls.
 */

/**
 * @brief Check if the child process has exited.
 *
 * @param wstatus Pointer to the status of the child process.
 * @return True if the child process has exited, false otherwise.
 */
bool W_WIFEXITED(int* wstatus);

/**
 * @brief Check if the child process has been stopped.
 *
 * @param wstatus Pointer to the status of the child process.
 * @return True if the child process has been stopped, false otherwise.
 */
bool W_WIFSTOPPED(int* wstatus);

/**
 * @brief Check if the child process has been signaled.
 *
 * @param wstatus Pointer to the status of the child process.
 * @return True if the child process has been signaled, false otherwise.
 */
bool W_WIFSIGNALED(int* wstatus);

/**
 * @brief Forks a new thread that retains most attributes of the parent thread and executes a function with arguments.
 *
 * This function creates a new thread that executes the function referenced by `func` with the argument array `argv`.
 * `fd0` is the file descriptor for the input file, and `fd1` is the file descriptor for the output file.
 *
 * @param func Pointer to the function to execute.
 * @param argv Argument array for the function.
 * @param fd0 File descriptor for input.
 * @param fd1 File descriptor for output.
 * @return The PID of the child thread on success, or -1 on error.
 */
pid_t p_spawn(void (*func)(), char *argv[], int fd0, int fd1);

/**
 * @brief Forks a new thread that retains most attributes of the parent thread and executes a function.
 *
 * This function creates a new thread that executes the function referenced by `func`. `fd0` is the file descriptor for the input file,
 * and `fd1` is the file descriptor for the output file.
 *
 * @param func Pointer to the function to execute.
 * @param fd0 File descriptor for input.
 * @param fd1 File descriptor for output.
 * @return The PID of the child thread on success, or -1 on error.
 */
pid_t p_spawn_shell(void (*func)(), int fd0, int fd1);

/**
 * @brief Wait for a child process to change state.
 *
 * This function sets the calling thread as blocked (if `nohang` is false) until a child of the calling thread changes state.
 * It is similar to Linux `waitpid(2)`. If `nohang` is true, `p_waitpid` does not block but returns immediately.
 *
 * @param pid PID of the child process to wait for.
 * @param wstatus Pointer to the status of the child process.
 * @param nohang If true, do not block and return immediately.
 * @return The PID of the child which has changed state on success, or -1 on error.
 */
pid_t p_waitpid(pid_t pid, int* wstatus, bool nohang);

/**
 * @brief Send a signal to a thread.
 *
 * This function sends the signal `sig` to the thread referenced by `pid`.
 *
 * @param pid PID of the thread to send the signal to.
 * @param sig Signal to send.
 * @return 0 on success, or -1 on error.
 */
int p_kill(pid_t pid, int sig);

/**
 * @brief Collect information about a child process.
 *
 * @param child Pointer to the PCB of the child process.
 * @param wstatus Pointer to the status of the child process.
 */
void collectChild(PCB* child, int* wstatus);

/**
 * @brief Exit the current thread unconditionally.
 */
void p_exit(void);

/**
 * @brief Change the priority of a process.
 *
 * @param pid PID of the process to change priority.
 * @param priority New priority level.
 * @return 0 on success, or -1 on error.
 */
int p_nice(pid_t pid, int priority);

/**
 * @brief Sleep for a specified number of ticks.
 *
 * @param ticks Number of ticks to sleep.
 */
void p_sleep(unsigned int ticks);

#endif // USER_H
