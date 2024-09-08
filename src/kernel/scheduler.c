#include <signal.h>     // sigaction, sigemptyset, sigfillset, signal
#include <stdio.h>      // dprintf, fputs, perror
#include <stdlib.h>     // malloc, free
#include <sys/time.h>   // setitimer
#include <ucontext.h>   // getcontext, makecontext, setcontext, swapcontext
#include <unistd.h>     // read, usleep, write
#include <valgrind/valgrind.h>
#include "../util/linked_list.h"
#include <string.h>
#include "../util/errno.h"

#include <regex.h>

#include "user.h"
#include "PCB.h"
#include "kernel.h"
#include "../shell/shell.h"
#include "../util/signal.h"
#include "../shell/shell-commands.h"
#include "../util/logger.h"
#include "../FAT/system-calls.h"

ucontext_t* mainContext;
SchedulerContext* schedulerContext;
ucontext_t* shellContext;
PCB* shellProcess;
PCB* foregroundProcess;
ucontext_t* idleContext;
PCB* currentProcess;
LinkedList* timerQueue;

const int centisecond = 10000; // 10 milliseconds
int pq_sequence[] = { -1, 0, 1, 
                    -1, 0, 1,
                    -1, 0, 1,
                    -1, 0, 1,
                    -1, 0, -1,
                     0, -1, -1,
                     -1};
static int currIndex = 0;

LinkedList* pcb_list;
LinkedList* pq_neg_one;
LinkedList* pq_zero;
LinkedList* pq_one;

LinkedList* signals;
LinkedList* waiting_processes;// for processes that are waiting on any children to update or a specific child to update
// We should store the parent pointer and what children it's waiting for in the waiting_processes queue
// Whenever we add a signal to the signals queue, we should look up its parent in the waiting_processes queue
// If it's there and it was waiting for that child then we can set its status to running and let it continue in waitpid

unsigned int curr_ticks; // TODO: should initialize to 0

struct sigaction act;
struct sigaction ign_act;

PCB* k_current_process() {
    return currentProcess;
}

// WE NEED TO BE VERY CAREFUL ABOUT WHERE WE'RE CALLING THIS FROM AS IT UPDATES OUR PQ_NUM
int k_get_pq_num() {
    int pq_num = pq_sequence[currIndex];
    currIndex = (currIndex+1) % 19;
    return pq_num;
}

LinkedList* get_process_pq(PCB* process) {
    if (process == NULL) {
//        ERRNO = GET_PROCESS_PQ;
//        p_perror("Process is NULL");
        return NULL;
    }
    int prio = process->priority;
    if (prio == -1) {
        return pq_neg_one;
    } else if (prio == 0) {
        return pq_zero;
    } else return pq_one;
}

Node* pq_all_blocked(LinkedList* pq) {
    Node* currNode = pq->head;
    while (currNode != NULL && ((PCB*) currNode->data)->state != RUNNING) {
        currNode = currNode->next;
    }
    return currNode;
}


Node* get_next_pq() {
    int attempts = 0;
    bool neg_one_blocked = false;
    bool zero_blocked = false;
    bool one_blocked = false;
//    printf("Selecting next process queue...\n");

    while (attempts < 20 && (!neg_one_blocked || !zero_blocked || !one_blocked)) {
        int pq_num = k_get_pq_num();
//        printf("Current pq_sequence index: %d, pq_num: %d\n", currIndex, pq_num);

        if (!neg_one_blocked && pq_num == -1 && pq_neg_one->size > 0){
//            printf("Checking pq_neg_one queue\n");
            Node* currNode = pq_all_blocked(pq_neg_one);
            if (currNode == NULL){
//                printf("All processes in pq_neg_one are blocked.\n");
                neg_one_blocked = true;
            } else {
//                printf("Selected pq_neg_one with runnable process.\n");
                return currNode;
            }
        } else if (!zero_blocked && pq_num == 0 && pq_zero->size > 0){
//            printf("Checking pq_zero queue\n");
            Node* currNode = pq_all_blocked(pq_zero);
            if (currNode == NULL){
//                printf("All processes in pq_zero are blocked.\n");
                zero_blocked = true;
            } else {
//                printf("Selected pq_zero with runnable process.\n");
                return currNode;
            }
        } else if (!one_blocked && pq_num == 1 && pq_one->size > 0) {
//            printf("Checking pq_one queue\n");
            Node* currNode = pq_all_blocked(pq_one);
            if (currNode == NULL){
//                printf("All processes in pq_one are blocked.\n");
                one_blocked = true;
            } else {
//                printf("Selected pq_one with runnable process.\n");
                return currNode;
            }
        }
        attempts++;
    }
//    ERRNO = GET_NEXT_PQ;
//    p_perror("Failed to get next process queue");
    return NULL;
}


static ucontext_t* makeContext(void (*func)()) {
    ucontext_t* context = malloc(sizeof(ucontext_t));
    if (context == NULL) {
        ERRNO = MAKECONTEXT;
        p_perror("Failed to malloc context");
        perror("malloc");
        return NULL;
    }
    getcontext(context);
    context->uc_link = schedulerContext->context;
    setStack(&context->uc_stack);
    sigemptyset(&context->uc_sigmask);
    makecontext(context, func, 0);
    return context;
}

static void swapProcess() {
    Node* next_process_node = get_next_pq();
    PCB* next_pcb = NULL;
    if (next_process_node != NULL) {
        next_pcb = next_process_node->data;
    }
    if (next_pcb == NULL) {
        currentProcess = NULL;
        setcontext(idleContext);
    } else {
        if (next_pcb->state != RUNNING) {
            ERRNO = SWAPPROCESS;
            p_perror("Next process is not running");
            return; // Or handle the error appropriately
        }
        if (currentProcess != next_pcb) {
            currentProcess = next_pcb;
            write_log(LOG_SCHEDULE, currentProcess->pid, currentProcess->priority, currentProcess->name);
        }

        // SPECIAL CASE: IF WE SCHEDULE THE SHELL, SET SHELL TO BE THE FOREGROUND PROCESS SO IT CAN RECEIVE SIGNALS
        if (strcmp(next_pcb->name, "shell") == 0){
            foregroundProcess = next_pcb;
        }
        setcontext(currentProcess->context);
    }
}

int is_sleep(const char* str) {
    regex_t regex;
    int result;

    // Compile the regular expression
    if (regcomp(&regex, "sleep", REG_EXTENDED) != 0) {
        ERRNO = IS_SLEEP;
        p_perror("Failed to compile regex");
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
        ERRNO = IS_SLEEP;
        p_perror("Failed to execute regex");
        perror("regexec");
        return -1; // Return -1 to indicate a regex execution error
    }
}

void timerService() {
    // 1. Check if timer queue is empty
    // 2. If not, check if the first timer is ready to be woken up (our list will be in order of time so starting
    // with the head will be fine)
    // printf("timer-------\n");
    if (timerQueue->size == 0) {
        return;
    }

    Node* node = timerQueue->head;
    timerstruct_t* timer;
    while (node != NULL) {
        timer = (timerstruct_t*) node->data;
        if (timer->process->state == STOPPED){
            node = node->next;
            continue;
        }
        if (timer->end_time <= curr_ticks) {
            // wake up process
            PCB* process = (PCB*) timer->process;
            if (is_sleep(process->name)) {
                process->state = EXITED;
                Node* temp = node;
                node = node->next;
                delete_node(timerQueue, temp);
                currentProcess = shellProcess;
                write_log(LOG_UNBLOCKED, process->pid, process->priority, process->name);
                currentProcess->state = RUNNING;
                if (p_kill(process->pid, S_SIGCHLD) == -1) {
                    ERRNO = TIMERSERVICE;
                    p_perror("Failed to send SIGCHLD to process");
                }
                setcontext(schedulerContext->context);
            }
        } else {
            return;
        }
    }
}

static void scheduler(void) {
    timerService();
    // poll_signals();
    swapProcess();
}


static void makeProcesses(void) {
    // create main PCB so it can be used as parent for other processes
    mainContext = malloc(sizeof(ucontext_t));
    if (mainContext == NULL) {
        ERRNO = MAKEPROCESSES;
        p_perror("Failed to malloc main context");
        perror("malloc");
        return;
    }
    getcontext(mainContext);
    mainContext->uc_link = NULL;
    setStack(&mainContext->uc_stack);
    sigemptyset(&mainContext->uc_sigmask);
    // add sigalrm to mask
    currentProcess = create_PCB(mainContext, 0, 0, RUNNING, -1);
    if (currentProcess == NULL) {
        ERRNO = MAKEPROCESSES;
        p_perror("Failed to create main PCB");
        return;
    }

    // Create scheduler context
    schedulerContext = malloc(sizeof(*schedulerContext));
    if (schedulerContext == NULL) {
        ERRNO = MAKEPROCESSES;
        p_perror("Failed to malloc scheduler context");
        perror("malloc");
        return;
    }
    schedulerContext->context = malloc(sizeof(ucontext_t));
    if (schedulerContext->context == NULL) {
        ERRNO = MAKEPROCESSES;
        p_perror("Failed to malloc scheduler context");
        perror("malloc");
        return;
    }
    schedulerContext->context = makeContext(scheduler);
    if (schedulerContext->context == NULL) {
        ERRNO = MAKEPROCESSES;
        p_perror("Failed to make scheduler context");
        return;
    }
    sigaddset(&schedulerContext->context->uc_sigmask, SIGALRM);

    // Create Idle context
    idleContext = makeContext(idle);
    if (idleContext == NULL) {
        ERRNO = MAKEPROCESSES;
        p_perror("Failed to make idle context");
        return;
    }
    pid_t shell_pid = p_spawn_shell(shell_loop, 0, 1);
    if (shell_pid == -1) {
        ERRNO = MAKEPROCESSES;
        p_perror("Failed to spawn shell");
        return;
    }
    shellProcess = get_PCB_by_pid(pcb_list, shell_pid);
    if (shellProcess == NULL) {
        ERRNO = MAKEPROCESSES;
        p_perror("Failed to get shell PCB");
        return;
    }
    shellContext = ((PCB*) pq_neg_one->head->data)->context;
    if (shellContext == NULL) {
        ERRNO = MAKEPROCESSES;
        p_perror("Failed to get shell context");
        return;
    }
    curr_ticks = 0;
    timerQueue = create_list();
    if (timerQueue == NULL) {
        ERRNO = MAKEPROCESSES;
        p_perror("Failed to create timer queue");
        return;
    }
    
    // DEFAULT SET FOREGROUND PROCESS TO SHELL?
    foregroundProcess = shellProcess;
}

static void alarmHandler(int signum) {
    curr_ticks += centisecond * 10;
    
    Node* node = get_node(pcb_list, currentProcess);
    LinkedList* pq = get_process_pq(currentProcess);
    if (node != NULL && pq->size > 1) {
        move_head_to_end(pq);
    }
    
    // Case 1: currentProcess is idleContext, which means currentProcess is set to NULL
    // Case 2: When would currentProcess not be NULL but currentProcess->context be null?
    // After k_process_cleanup(), the process is not NULL but process->context is NULL. 
    // But that should be cleaned up in waitpid(), so we should never reach Case 2?
    // Right now the cleaned up Process PCB struct is never freed - should we free it waitpid()?
    if (currentProcess == NULL || currentProcess->context == NULL) {
        setcontext(schedulerContext->context);
    } else {
        swapcontext(currentProcess->context, schedulerContext->context);
    }
}

static void setAlarmHandler(void) {
    act.sa_handler = alarmHandler;
    act.sa_flags = SA_RESTART;
    sigemptyset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
}


static void setTimer(void) {
    struct itimerval it;
    it.it_interval = (struct timeval) { .tv_usec = centisecond * 10 };
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
}

static void freeStacks(void) {
    free(schedulerContext->context->uc_stack.ss_sp);
}


int main(int argc, char *argv[]) {
    // Register Signal handlers
    // SIGSTOP CONTROL Z NOT WORKING
    register_sig_handlers();

    char* fatfile = argv[1];
    printf("fatfile: %s\n", fatfile);
    if (fatfile != NULL){
        mount(fatfile);
        // int fd_fatfile = f_open(fatfile, 1);
        // printf("fd_fatfile: %d\n", fd_fatfile);
    }

    pcb_list = create_list();
    pq_neg_one = create_list();
    pq_zero = create_list();
    pq_one = create_list();
    signals = create_list();
    makeProcesses();
    setAlarmHandler();
    setTimer();
    // another swap function that swaps from current (shell) to scheduler
    swapProcess();
    freeStacks();
}

void scheduleProcess(PCB* process) {
    if (process->priority == -1) {
        insert_end(pq_neg_one, process);
    } else if (process->priority == 0) {
        insert_end(pq_zero, process);
    } else if (process->priority == 1) {
        insert_end(pq_one, process);
    } else {
        ERRNO = SCHEDULEPROCESS;
        p_perror("Invalid priority");
    }
    insert_end(pcb_list, process);
}