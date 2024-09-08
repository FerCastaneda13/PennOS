#ifndef PCB_H
#define PCB_H

#include <ucontext.h>
#include <stdbool.h>

/**
 * @file PCB.h
 * @brief Header file for Process Control Block (PCB) related definitions and functions.
 */

/**
 * @enum process_state
 * @brief Enumerated type representing process states.
 *
 * This enum defines various process states such as RUNNING, STOPPED, BLOCKED, ZOMBIE, and EXITED.
 */
typedef enum {
    RUNNING = 1,   ///< The process is running.
    STOPPED = 0,   ///< The process is stopped.
    BLOCKED = -1,  ///< The process is blocked.
    ZOMBIE = -2,   ///< The process is a zombie.
    EXITED = -3,   ///< The process has exited.
    TERMINATED = -4 ///< The process has been terminated.
} process_state;

// Forward declaration of Node and Linked List
typedef struct Node Node; ///< Forward declaration of the Node struct.
typedef struct LinkedList LinkedList; ///< Forward declaration of the LinkedList struct.

/**
 * @struct PCB
 * @brief Process Control Block (PCB) structure.
 *
 * The PCB contains information about a thread or process, including its context, process ID, parent process ID,
 * children process IDs, open file descriptors, priority level, and more.
 */

typedef struct PCB {
    ucontext_t* context;        ///< Pointer to the ucontext information.
    int pid;                    ///< The thread's process ID.
    int parent_pid;             ///< The parent process ID.
    LinkedList* children;       ///< Pointer to a linked list of children process IDs.
    LinkedList* zombie_q;       ///< Pointer to a linked list of zombie processes.
    LinkedList* wait_q;         ///< Pointer to a linked list of processes waiting on this process.
    int state;                  ///< The process state (e.g., RUNNING, STOPPED, BLOCKED).
    int priority;               ///< The priority level of the process.
    int exit_status;            ///< The exit status of the process.
    int waiting_on;             ///< The process ID the current process is waiting on.
    bool state_change_checked;  ///< Flag indicating if the state change has been checked.
    bool in_waitpid_neg_one_hanging; ///< Flag indicating if the process is in a waitpid(-1, ...) hanging state.
    char* name;                 ///< The name or identifier of the process.
    int fd_in;                  ///< The file descriptor for stdin.
    int fd_out;                 ///< The file descriptor for stdout.
} PCB;

/**
 * @brief Creates a new Process Control Block (PCB) with the specified parameters.
 *
 * @param context Pointer to the ucontext information.
 * @param pid The process ID.
 * @param parent_pid The parent process ID.
 * @param state The initial state of the process.
 * @param priority The priority level of the process.
 * @return Pointer to the newly created PCB.
 */
PCB* create_PCB(ucontext_t* context, int pid, int parent_pid, int state, int priority);

/**
 * @brief Retrieves a PCB by its process ID from a linked list of PCBs.
 *
 * @param list Pointer to the linked list of PCBs.
 * @param pid The process ID to search for.
 * @return Pointer to the PCB with the specified process ID if found, or NULL if not found.
 */
PCB* get_PCB_by_pid(LinkedList* list, int pid);

/**
 * @brief Frees the memory occupied by a PCB.
 *
 * @param pcb Pointer to the PCB to be freed.
 */
void free_PCB(PCB* pcb);

/**
 * @brief Gets the name of a process state based on its state code.
 *
 * @param state The process state code.
 * @return A string representing the name of the process state.
 */
char* getStateName(int state);

#endif // PCB_H
