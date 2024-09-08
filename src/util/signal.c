/**
 * @file signal.c
 * @brief Implementation of signal handling functions for PennOS.
 *
 * Created by Omar Ameen on 11/10/23.
 * This file contains the implementations of the functions defined in signal.h.
 * It includes functions for adding signals, waiters, and timers, as well as 
 * retrieving waiting processes based on PID or PCB pointers.
 */

#include <stdlib.h>

#include "signal.h"
#include "linked_list.h"
#include "../util/errno.h"
#include <stdio.h>

/// Adds a signal to the signal queue
signal_t* add_signal(int sender_pid, int receiver_pid, int signal) {
    signal_t* signal_struct = (signal_t*) malloc(sizeof(signal_t));
    if (signal_struct == NULL) {
        ERRNO = ADD_SIGNAL;
        p_perror("Failed to malloc signal struct");
        perror("malloc failed");
        return NULL;
    }
    signal_struct->sender_pid = sender_pid;
    signal_struct->receiver_pid = receiver_pid;
    signal_struct->signal = signal;

    PCB* receiver = get_PCB_by_pid(pcb_list, receiver_pid);
    if (receiver == NULL) {
        ERRNO = ADD_SIGNAL;
        p_perror("Failed to get receiver PCB by pid");
        return NULL;
    }
    LinkedList* wait_q = receiver->wait_q;
    if (wait_q == NULL) {
        ERRNO = ADD_SIGNAL;
        p_perror("Failed to get receiver wait queue");
        return NULL;
    }

    insert_end(wait_q, signal_struct);
    return signal_struct;
}

void add_timer(PCB* pcb, unsigned int ticks) {
    timerstruct_t* new_timer = (timerstruct_t*) malloc(sizeof(timerstruct_t));
    if (new_timer == NULL) {
        ERRNO = ADD_TIMER;
        p_perror("Failed to malloc new timer");
        perror("malloc failed");
        return;
    }
    new_timer->process = pcb;
    new_timer->end_time = curr_ticks + ticks;

    Node* curr = timerQueue->head;
    if (curr == NULL) {
        insert_end(timerQueue, new_timer);
        return;
    } else {
        while (curr != NULL) {
            timerstruct_t* curr_timer = (timerstruct_t*) curr->data;
            if (curr_timer->end_time >= new_timer->end_time) {
                Node* prev = curr->prev;
                if (prev == NULL) {
                    // if only 1 node and 
                    insert_front(timerQueue, new_timer);
                    return;
                } else {
                    Node* new_timer_node = create_node(new_timer);
                    insert_before(timerQueue, curr, new_timer_node);
                    return;
                }
            }
            curr = curr->next;
        }
    }
}
