/**
 * @file user.c
 * @brief C file for user-level functions and system calls.
 */

#include "user.h"
#include <stdlib.h>
#include <ucontext.h>
#include "PCB.h"
#include "kernel.h"
#include "scheduler.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>     // read, usleep, write
#include <signal.h>
#include "../util/errno.h"

#include "../shell/shell-commands.h"
#include "../util/linked_list.h"
#include "../util/signal.h"
#include "../util/logger.h"

const int MAX_CHAR_SIZE = 10000;

pid_t p_spawn(void (*func)(), char *argv[], int fd0, int fd1) {
    PCB* parent = k_current_process(); // NOT ACCURATE!!! TODO: FIX
    PCB* new_thread = k_process_create(parent);
    if (new_thread == NULL) {
        ERRNO = P_SPAWN;
        p_perror("Failed to create new thread");
        return -1;
    }

    // ORPHANIFY AND ZOMBIFY: ARGV WILL BE NULL
    // if (argv == NULL) {

    // }
    makecontext(new_thread->context, func, 1, &argv[1]);

    int argc = 0;
    char* name = malloc(sizeof(char) * MAX_CHAR_SIZE);
    if (name == NULL){
        ERRNO = P_SPAWN;
        p_perror("Failed to malloc name");
        perror("malloc");
        exit(EXIT_FAILURE); // TODO: Replace with p_exit
    }
    name[0] = '\0';
    for (int i = 0; argv[i] != NULL; i++) {
        argc++;
        if (argv[i] != NULL) {
            strcat(name, argv[i]); // not sure if need to malloc, revisit
            strcat(name, " ");
        }
    }
    new_thread->name = name;

    new_thread->fd_in = fd0;
    new_thread->fd_out = fd1;
    
    scheduleProcess(new_thread);
    write_log(LOG_CREATE, new_thread->pid, new_thread->priority, new_thread->name);
    return new_thread->pid;
}

pid_t p_spawn_shell(void (*func)(), int fd0, int fd1){
    // NULL parent or some blank parent?
    PCB* shell_thread = k_process_create(NULL);
    shell_thread->priority = 0;
    shell_thread->name = strdup("shell");
    if (shell_thread == NULL) {
        ERRNO = P_SPAWN_SHELL;
        p_perror("Failed to create shell thread");
        perror("malloc");
        return -1;
    }
    // Schedule shell thread/process
    scheduleProcess(shell_thread);

    shell_thread->fd_in = fd0;
    shell_thread->fd_out = fd1;
    
    
//    // Block SIGALRM in Shell Thread/Process
//    sigset_t mask;
//    sigemptyset(&mask);
//    sigaddset(&mask, SIGALRM);
//    sigprocmask(SIG_BLOCK, &mask, NULL);
//    shell_thread->context->uc_sigmask = mask;
    sigemptyset(&shell_thread->context->uc_sigmask);
    makecontext(shell_thread->context, (void (*)(void))func, 1, shell_thread);

    write_log(LOG_CREATE, shell_thread->pid, shell_thread->priority, shell_thread->name);
    if (p_nice(shell_thread->pid, -1) == -1) {
        ERRNO = P_SPAWN_SHELL;
        p_perror("Failed to set shell priority");
        exit(EXIT_FAILURE);
    }

    // what to return?
    return shell_thread->pid;
}

bool W_WIFEXITED(int* wstatus) {
    return *wstatus == E_EXITED;
}

bool W_WIFSTOPPED(int* wstatus) {
    return *wstatus == E_STOPPED;
}

bool W_WIFSIGNALED(int* wstatus) {
    return *wstatus == E_TERMINATE_SIGNALED;
}

//void remove_all_signals_with_pid(PCB* child) {
//    pid_t pid = child->pid;
//    PCB* parent_pcb = get_PCB_by_pid(pcb_list, child->parent_pid);
//    LinkedList* signals = parent_pcb->wait_q;
//    Node* node = signals->head;
//    signal_t* sig;
//    while (node != NULL) {
//        sig = node->data;
//        Node* temp = node->next;
//        if (sig->sender_pid == pid) {
//            delete_node(signals, node);
//        }
//        node = temp;
//    }
//}

void collectProcess(PCB* process, int* wstatus){
    int state = process->state;
    if (state == STOPPED || state == RUNNING || state == BLOCKED) {
        return;
    }
    *wstatus = process->state;
    k_process_cleanup(process);
}

pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang) {
    currentProcess->waiting_on = pid;
    int num_children = currentProcess->children->size;
    if (num_children == 0) {
//        ERRNO = P_WAITPID;
//        p_perror("No children to wait for");
        return -1;
    }
    
    if (pid > 0) {
        PCB* waiting_on = get_PCB_by_pid(pcb_list, pid);
        if (waiting_on == NULL){
            ERRNO = P_WAITPID;
            p_perror("Failed to get PCB by pid");
            return -1;
        }
        PCB* process_that_is_waiting = get_PCB_by_pid(pcb_list, waiting_on->parent_pid);
        if (process_that_is_waiting == NULL){
            ERRNO = P_WAITPID;
            p_perror("Failed to get PCB by pid");
            return -1;
        }
        if (!nohang){
            // BLOCKING WAIT
            write_log(LOG_BLOCKED, process_that_is_waiting->pid, process_that_is_waiting->priority, process_that_is_waiting->name);
            process_that_is_waiting->state = BLOCKED;

            // INSTEAD OF SWAPPING TO SCHEDULER, LET THAT BE HANDLED BY ALARM SIGNAL
            // PREVENTS RACE CONDITIONS BECAUSE WE DON'T HAVE TO WORRY ABOUT THE SCHEDULER CONTEXT BEING SWAPPED TO BEFORE THE ALARM SIGNAL
            sigset_t mask;
            sigfillset(&mask);
            sigdelset(&mask, SIGALRM);
            sigprocmask(SIG_BLOCK, &mask, NULL);
            sigsuspend(&mask);
            // swapcontext(process_that_is_waiting->context, schedulerContext->context);

            // RETURNED BACK TO SHELL, SHELL SET TO RUNNING BY SOMETHING
            if (!waiting_on->state_change_checked) {
                write_log(LOG_WAITED, waiting_on->pid, waiting_on->priority, waiting_on->name);
                if (waiting_on->state == ZOMBIE || waiting_on->state == EXITED || waiting_on->state == TERMINATED) {
                    collectProcess(waiting_on, wstatus);
                } else {
                    *wstatus = waiting_on->state;
                }
                return pid;
            }
        } else {
            // NOHANG WAIT
            if (!waiting_on->state_change_checked) {
                write_log(LOG_WAITED, waiting_on->pid, waiting_on->priority, waiting_on->name);
                if (waiting_on->state == ZOMBIE || waiting_on->state == EXITED || waiting_on->state == TERMINATED) {
                    collectProcess(waiting_on, wstatus);
                } else {
                    *wstatus = waiting_on->state;
                }
                return pid;
            } else {
                // NO STATE CHANGE FOUND, RETURN 0
//                FILE* logfile = log_file();
//                fprintf(logfile, "[%4d] NO STATE CHANGE FOUND, RETURNING 0 \n", curr_ticks / 10000);
//                fclose(logfile);
                return 0;
            }
        }
    } else if (pid == -1){
        // WAIT FOR ANY CHILD
        if (!nohang){
            currentProcess->in_waitpid_neg_one_hanging = true;
            // BLOCKING WAIT FOR ANY CHILD, RETURN WHEN ANY CHILD CHANGES STATE
            // LOOP THROUGH ALL CHILDREN IN PCB LIST, IF ANY ARE ZOMBIE, EXITED, OR TERMINATED, COLLECT THEM AND EXIT
            Node* node = pcb_list->tail;
            while (node != NULL) {
                PCB* process = node->data;
                if (process->parent_pid == currentProcess->pid) {
                    // IF PROCESS HAS A STATE CHANGE THAT HASN'T BEEN CHECKED THROUGH WAITPID, 
                    // RETURN THE PID OF THE PROCESS AND SET WSTATUS TO THE NEW STATE AND SET STATE_CHANGE_CHECKED OF PROCESS TO BE TRUE
                    // STATE CHANGE COULD BE ANY OF STOPPED, RUNNING, BLOCKED, ZOMBIE, EXITED, TERMINATED
                    if (!process->state_change_checked){
                        int pid_to_return = process->pid;
                        write_log(LOG_WAITED, process->pid, process->priority, process->name);
                        if (process->state == ZOMBIE || process->state == EXITED || process->state == TERMINATED) {
                            collectProcess(process, wstatus);
                        } else {
                            *wstatus = process->state;
                            process->state_change_checked = true;
                        }
                        currentProcess->in_waitpid_neg_one_hanging = false;
                        return pid_to_return;
                    }
  
                }
                node = node->prev;
            }

            // IF WE REACH HERE, IT MEANS
            // NO CHILDREN ARE ZOMBIE, EXITED, OR TERMINATED, BLOCKED, SO WAIT FOR A CHILD TO CHANGE STATE
            write_log(LOG_BLOCKED, currentProcess->pid, currentProcess->priority, currentProcess->name);
            currentProcess->state = BLOCKED;
            
            // INSTEAD OF SWAPPING TO SCHEDULER, LET THAT BE HANDLED BY ALARM SIGNAL
            // PREVENTS RACE CONDITIONS BECAUSE WE DON'T HAVE TO WORRY ABOUT THE SCHEDULER CONTEXT BEING SWAPPED TO BEFORE THE ALARM SIGNAL
            sigset_t mask;
            sigfillset(&mask);
            sigdelset(&mask, SIGALRM);
            sigprocmask(SIG_BLOCK, &mask, NULL);
            sigsuspend(&mask);

            // RETURNED BACK TO SHELL, SHELL SET TO RUNNING A CHILD
            // FIND THE CHILD THAT CHANGED AND COLLECT IT
            node = pcb_list->tail;
            while (node != NULL) {
                PCB* process = node->data;
                if (process->parent_pid == currentProcess->pid) {
                    // IF PROCESS HAS A STATE CHANGE THAT HASN'T BEEN CHECKED THROUGH WAITPID, 
                    // RETURN THE PID OF THE PROCESS AND SET WSTATUS TO THE NEW STATE AND SET STATE_CHANGE_CHECKED OF PROCESS TO BE TRUE
                    if (!process->state_change_checked){
                        int pid_to_return = process->pid;
                        write_log(LOG_WAITED, process->pid, process->priority, process->name);
                        if (process->state == ZOMBIE || process->state == EXITED || process->state == TERMINATED) {
                            collectProcess(process, wstatus);
                        } else {
                            *wstatus = process->state;
                            process->state_change_checked = true;
                        }
                        currentProcess->in_waitpid_neg_one_hanging = false;
                        return pid_to_return;
                    }
  
                }
                node = node->prev;
            }

        } else {
            // NON BLOCKING, NOHANG WAIT FOR ANY CHILD, RETURN WHEN ANY CHILD CHANGES STATE
            Node* node = pcb_list->tail;
            while (node != NULL) {
                PCB* process = node->data;
                if (process->parent_pid == currentProcess->pid) {
                    // IF PROCESS HAS A STATE CHANGE THAT HASN'T BEEN CHECKED THROUGH WAITPID, 
                    // RETURN THE PID OF THE PROCESS AND SET WSTATUS TO THE NEW STATE AND SET STATE_CHANGE_CHECKED OF PROCESS TO BE TRUE
                    // STATE CHANGE COULD BE ANY OF STOPPED, RUNNING, BLOCKED, ZOMBIE, EXITED, TERMINATED
                    if (!process->state_change_checked){
                        int pid_to_return = process->pid;
                        write_log(LOG_WAITED, process->pid, process->priority, process->name);
                        if (process->state == ZOMBIE || process->state == EXITED || process->state == TERMINATED) {
                            collectProcess(process, wstatus);
                        } else {
                            *wstatus = process->state;
                            process->state_change_checked = true;
                        }
                        currentProcess->in_waitpid_neg_one_hanging = false;
                        return pid_to_return;
                    }
  
                }
                node = node->prev;
            }
            // IF WE REACH HERE, IT MEANS NO CHILDREN HAVE STAGE CHANGE, SO RETURN 0
            return 0;
        }
    }
    ERRNO = P_WAITPID;
    p_perror("Failed to wait for process");
    return -1;
}

int p_kill(pid_t pid, int sig) { // SIGNAL AND PROCESS TO DELIVER IT TO
    // pid -> PCB
    printf("p_kill pid: %d\n", pid);
    PCB* process = (PCB*) get_PCB_by_pid(pcb_list, pid);
    if (process == NULL) {
        ERRNO = P_KILL;
        p_perror("Failed to get PCB by pid");
        return -1;
    }
    k_process_kill(process, sig);
    // return -1 on failure
    return 0;
}

void p_exit(void) {
    write_log(LOG_EXITED, currentProcess->pid, currentProcess->priority, currentProcess->name);
    if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
        ERRNO = P_EXIT;
        p_perror("Failed to kill process");
        exit(EXIT_FAILURE); // TODO: not sure what to do here
    }
}

int p_nice(pid_t pid, int priority) {
    PCB* pcb = get_PCB_by_pid(pcb_list, pid);
    if (pcb == NULL) {
        ERRNO = P_NICE;
        p_perror("Failed to get PCB by pid");
        return -1;
    }
    LinkedList* old_pq = get_process_pq(pcb);
    if (old_pq == NULL) {
        ERRNO = P_NICE;
        p_perror("Failed to get process priority queue");
        return -1;
    }
    delete_node(old_pq, get_node(old_pq, pcb));
    int oldprio = pcb->priority;
    
    pcb->priority = priority;
    if (priority == 1) {
        insert_end(pq_one, pcb);
    } else if (priority == 0) {
        insert_end(pq_zero, pcb);
    } else if (priority == -1) {
        insert_end(pq_neg_one, pcb);
    } else {
        ERRNO = P_NICE;
        p_perror("Invalid priority");
        return -1;
    }

    write_log_nice(LOG_NICE, pcb->pid, oldprio, pcb->priority, pcb->name);
    return 0;
}

void p_sleep(unsigned int ticks) { // TODO: Implement p_sleep
    // sets the calling process to blocked until ticks of the system clock elapse, and then sets the thread to running
    // Importantly, p_sleep should not return until the thread resumes running; however, it can be interrupted by a
    // S_SIGTERM signal. Like sleep(3) in Linux, the clock keeps ticking even when p_sleep is interrupted.
    unsigned int end = curr_ticks + ticks - centisecond * 10;

    PCB* curr = k_current_process();

    add_timer(curr, end);

    curr->state = BLOCKED;

    usleep(centisecond * 10);
}