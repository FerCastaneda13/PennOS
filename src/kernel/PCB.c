/**
 * @file PCB.c
 * @brief C file for Process Control Block (PCB) related definitions and functions.
 */

//
// Created by Omar Ameen on 11/5/23.
//

#include "PCB.h"
#include "../util/linked_list.h"
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include "../util/errno.h"

/// Creates a new Process Control Block (PCB) with the specified parameters.
PCB* create_PCB (ucontext_t* context, int pid, int parent_pid, int state, int priority) {
    PCB* pcb = (PCB*) malloc(sizeof(PCB)); // potential error here
    if (pcb == NULL) {
        ERRNO = CREATE_PCB;
        p_perror("Failed to malloc PCB");
        perror("malloc failed");
        return NULL;
    }
    pcb->context = context;
    pcb->pid = pid;
    pcb->parent_pid = parent_pid;
    pcb->state = state;
    pcb->priority = priority;
    pcb->children = create_list(); // potential error here
    if (pcb->children == NULL) {
        ERRNO = CREATE_PCB;
        p_perror("Failed to malloc children list");
        perror("malloc failed");
        return NULL;
    }
    pcb->zombie_q = create_list();// potential error here
    if (pcb->zombie_q == NULL) {
        ERRNO = CREATE_PCB;
        p_perror("Failed to malloc zombie queue");
        perror("malloc failed");
        return NULL;
    }
    pcb->wait_q = create_list();// potential error here
    if (pcb->wait_q == NULL) {
        ERRNO = CREATE_PCB;
        p_perror("Failed to malloc wait queue");
        perror("malloc failed");
        return NULL;
    }
    pcb->fd_in = 0;
    pcb->fd_out = 1; 

    pcb->exit_status = E_RUNNING;
    pcb->state_change_checked = true;
    pcb->waiting_on = 0;
    pcb->in_waitpid_neg_one_hanging = false;
    pcb->name = NULL;
    return pcb;
}

/// Retrieves a PCB by its process ID from a linked list of PCBs.
PCB* get_PCB_by_pid(LinkedList* list, int pid) {
    Node* curr = list->head; // potential error here
    if (curr == NULL) {
        ERRNO = GET_PCB_BY_PID;
        p_perror("Failed to get list head");
        return NULL;
    }
    while (curr != NULL) {
        PCB* pcb = (PCB*) curr->data;
        if (pcb->pid == pid) {
            return pcb;
        }
        curr = curr->next;
    }
    return NULL; // potential error here
}

/// Frees the memory occupied by a PCB.
void free_PCB(PCB* pcb) {
    free(pcb->context->uc_stack.ss_sp);
    free(pcb->context);
    free(pcb->children);
    free(pcb);
}

/// Retrieves the cleanup context for a process.
char* getStateName(int state) {
    switch (state) {
        case RUNNING:
            return "RUNNING";
        case STOPPED:
            return "STOPPED";
        case BLOCKED:
            return "BLOCKED";
        case ZOMBIE:
            return "ZOMBIE";
        case EXITED:
            return "EXITED";
        case TERMINATED:
            return "TERMINATED";
        default:
            return "UNKNOWN";
    }
}
