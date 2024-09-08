/**
 * @file kernel.c
 * @brief C file for kernel-level functions related to process management: k_process_kill(), k_process_cleanup(), k_process_create(), setStack(), cleanup_context().
 */

#include "kernel.h"
#include "PCB.h"
#include <ucontext.h>
#include <valgrind/valgrind.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <stdio.h>
#include "../util/linked_list.h"
#include "scheduler.h"
#include "../util/signal.h"
#include "../util/logger.h"
#include <regex.h>
#include "../util/errno.h"

#define STACK_SIZE 819200

#define RNG_A 16807
#define RNG_B 2147483647

int curr_pid = 1;

/// Cleans up orphans(children) after a process exits/terminates right before the process is cleaned up.
void cleanup_orphans(LinkedList* children){
    Node* curr_orphan_pid_node = children->head; // potential error here?
    while (curr_orphan_pid_node != NULL) {
        Node* temp = curr_orphan_pid_node->next;
        int* orphan_pid = (int*) curr_orphan_pid_node->data;
        PCB* orphan_pcb = get_PCB_by_pid(pcb_list, *orphan_pid); // potential error here?
        if (orphan_pcb == NULL) {
            ERRNO = CLEANUP_ORPHANS;
            p_perror("Failed to get orphan PCB by pid");
        }
        write_log(LOG_ORPHAN, orphan_pcb->pid, orphan_pcb->priority, orphan_pcb->name);
        k_process_cleanup(orphan_pcb); // potential error here?
        curr_orphan_pid_node = temp;
    }
}

/// Sets stack for a process's context
void setStack(stack_t *stack) {
    void *sp = malloc(STACK_SIZE); // potential error here
    if (sp == NULL){
        ERRNO = SETSTACK;
        p_perror("Failed to malloc stack");
        perror("malloc failed");
    }
    VALGRIND_STACK_REGISTER(sp, sp + STACK_SIZE);
    *stack = (stack_t) { .ss_sp = sp, .ss_size = STACK_SIZE };
}

/// Creates a Process/PCB with the given parameters
PCB* k_process_create(PCB* parent) {
    ucontext_t* uc = malloc(sizeof(ucontext_t)); // potential error here
    if (uc == NULL){
        ERRNO = K_PROCESS_CREATE;
        p_perror("Failed to malloc ucontext");
        perror("malloc failed");
    }
    getcontext(uc);
    sigemptyset(&uc->uc_sigmask);
    setStack(&uc->uc_stack);
    PCB* pcb;
    if (parent == NULL){
        // if k_process_create for the shell (and scheduler?)
        // sigaddset(&uc->uc_sigmask, SIGALRM);
        pcb = create_PCB(uc, curr_pid, 1, RUNNING, 0);
    } else {
        pcb = create_PCB(uc, curr_pid, parent->pid, RUNNING, 0);
        // ADD JUST SPAWNED PID TO PARENT'S CHILDREN LIST
        int* child_pid = malloc(sizeof(int)); // potential error here
        *child_pid = pcb->pid;
        insert_end(parent->children, child_pid); // potential error here
    }
    uc->uc_link = schedulerContext->context;
    curr_pid++;
    return pcb;
}

/// Checks whether a process is a sleep process based on its name
int is_sleep_2(const char* str) {
    regex_t regex;
    int result;

    // Compile the regular expression
    if (regcomp(&regex, "sleep", REG_EXTENDED) != 0) {
        ERRNO = IS_SLEEP_2;
        p_perror("regcomp");
        perror("regcomp");
        return -1; // Return -1 to indicate a regex compilation error
    }

    // Execute the regex on the string
    result = regexec(&regex, str, 0, NULL, 0);

    // Free the compiled regex
    regfree(&regex);

    if (result == 0) {
        return 1; // Match found
    } else if (result == REG_NOMATCH) {
        return 0; // No match found
    } else {
        ERRNO = IS_SLEEP_2;
        p_perror("regexec failed.");
        return -1; // Return -1 to indicate a regex execution error
    }
}

/// Sends/kills a process with the given signal
void k_process_kill(PCB* process, int signal) {
    PCB* recipient = process;
    LinkedList* sender_pq;
    if (recipient == NULL) {
        pid_t sender_pid = shellProcess->waiting_on;
        recipient = get_PCB_by_pid(pcb_list, sender_pid);
        sender_pq = get_process_pq(recipient);
    } else {
        sender_pq = get_process_pq(recipient);
        // sender_node = get_node(sender_pq, sender);
    }
    if (signal == S_SIGSTOP) {
        if (recipient->pid == shellProcess->pid) { // not sure why this is broken
            printf("$ ");
        }
        recipient->state = STOPPED;
        write_log(LOG_STOPPED, recipient->pid, recipient->priority, recipient->name);
        recipient->state_change_checked = false;
        // set Shell to be RUNNING?
        shellProcess->state = RUNNING;
        write_log(LOG_UNBLOCKED, shellProcess->pid, shellProcess->priority, shellProcess->name);
        add_signal(recipient->pid, recipient->parent_pid, R_SIGSTOP);
        swapcontext(recipient->context, schedulerContext->context);
    } else if (signal == S_SIGCONT) {
        if (is_sleep_2(recipient->name)) {
            recipient->state = BLOCKED;
        } else {
            write_log(LOG_CONTINUED, recipient->pid, recipient->priority, recipient->name);
            recipient->state = RUNNING;
        }
        recipient->state_change_checked = false;
    } else if (signal == S_SIGTERM) {
        // SIGTERM means it should be terminated - what state does that mean?
        add_signal(recipient->pid, recipient->parent_pid, R_SIGTERM);

        // WHAT SHOULD STATE BE?
        // recipient->state = TERMINATED;
        recipient->state = ZOMBIE;
        recipient->state_change_checked = false;
        write_log(LOG_SIGNALED, recipient->pid, recipient->priority, recipient->name);
        write_log(LOG_ZOMBIE, recipient->pid, recipient->priority, recipient->name);

        // cleanup orphans
        cleanup_orphans(recipient->children);


        // IF SHELL IS WAITING ON THIS PROCESS THAT JUST FINISHED, SET SHELL TO RUNNING
        if (shellProcess->waiting_on == recipient->pid) {
            shellProcess->state = RUNNING;
            write_log(LOG_UNBLOCKED, shellProcess->pid, shellProcess->priority, shellProcess->name);
            setcontext(schedulerContext->context);
        }
    } else if (signal == S_SIGCHLD) {
        write_log(LOG_ZOMBIE, recipient->pid, recipient->priority, recipient->name);
        recipient->state = ZOMBIE;
        recipient->state_change_checked = false;
        add_signal(recipient->pid, recipient->parent_pid, R_SIGEXIT);
        PCB* parent = get_PCB_by_pid(pcb_list, recipient->parent_pid);
        if (parent == NULL) {
            ERRNO = K_PROCESS_KILL;
            p_perror("Failed to get parent PCB by pid");
        }
        Node* sender_node = get_node(sender_pq, recipient);
        if (sender_node == NULL) {
            ERRNO = K_PROCESS_KILL;
            p_perror("Failed to get sender node");
        }
        delete_node(sender_pq, sender_node);

        // CLEANUP THIS NODE'S CHILDREN
        cleanup_orphans(recipient->children);

        // IF SHELL IS WAITING ON THIS PROCESS THAT JUST FINISHED, SET SHELL TO RUNNING  
        // AND SWITCH TO SCHEDULER SO THAT WAITPID CAN BE CONTINUED
        
        // CHANGE THIS? IF ANY PROCESS FINISHES, IT SHOULDN'T ALWAYS SET SHELL BACK TO RUNNING
        // IF I HAVE SLEEP 10 IN FOREGROUND AND SLEEP 2 IN BACKGROUND, 
        // JUST CAUSE SLEEP 2 FINISHED DOESN"T MEAN SHELL SHOULD BE ABLE TO BE SCHEULDED AGAIN
        
        // CHANGE BELOW TO PARENT INSTEAD OF SHELL
        if (shellProcess->in_waitpid_neg_one_hanging) {
            if (recipient->parent_pid == shellProcess->pid) {
                shellProcess->state = RUNNING;
                write_log(LOG_UNBLOCKED, shellProcess->pid, shellProcess->priority, shellProcess->name);
                setcontext(schedulerContext->context);
            }
        } else if (parent->waiting_on == recipient->pid) {
            parent->state = RUNNING;
            write_log(LOG_UNBLOCKED, parent->pid, parent->priority, parent->name);
            setcontext(schedulerContext->context);
        }

    }
}

/// Cleans up a process
void k_process_cleanup(PCB* process) {
    // CALL FROM WAITPID()
    // DELETE FROM PCB LISTS
    // FREE NODES / ANYTHING MALLOCED

    // REMOVE PID FROM PARENT'S CHILDREN LIST
    if (process == NULL) {
        ERRNO = K_PROCESS_CLEANUP;
        p_perror("Process is NULL");
        return;
    }
    PCB* parent = get_PCB_by_pid(pcb_list, process->parent_pid);
    if (parent == NULL) {
        ERRNO = K_PROCESS_CLEANUP;
        p_perror("Failed to get parent PCB by pid");
    }
    Node* child_node = get_node_int(parent->children, process->pid);
    if (child_node == NULL) {
        ERRNO = K_PROCESS_CLEANUP;
        p_perror("Failed to get child node");
    }
    delete_node(parent->children, child_node);

    if (process->priority == -1) {
        delete_node(pq_neg_one, get_node(pq_neg_one, process));
    } else if (process->priority == 0) {
        delete_node(pq_zero, get_node(pq_zero, process));
    } else if (process->priority == 1) {
        delete_node(pq_one, get_node(pq_one, process));
    } else {
        ERRNO = K_PROCESS_CLEANUP;
        p_perror("Invalid priority");
    }
    delete_node(pcb_list, get_node(pcb_list, process));
    free_PCB(process);
}