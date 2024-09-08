//
// Created by Omar Ameen on 12/1/23.
//
#include "errno.h"
#include <stdio.h>
#include "../kernel/scheduler.h"

// You should also define a function called p_perror (similar to perror(3) in Linux) that allows the shell to pass in a
// message describing what error occurred, which the function then concatenates to the default error message based on
// your ERRNO. Note that the file system and the kernel should set ERRNO and return -1 when a PennOS system call fails,
// and then the shell should call p_perror to report the error that occurred.

int ERRNO = 0;

char* get_error_message(int error) {
    switch (error) {
        case CREATE_PCB: return "create_PCB: Error creating PCB"; // done
        case GET_PCB_BY_PID: return "get_PCB_by_pid: Error getting PCB by pid"; // done
        case LOOKUP: return "lookup: Error looking up command"; // done
        case SIG_HANDLER: return "sig_handler: Error handling signal";
        case S_CAT: return "s_cat: Error concatenating strings";
        case S_SLEEP: return "s_sleep: Error sleeping";
        case S_ECHO: return "s_echo: Error echoing";
        case LIST_DIRECTORY: return "list_directory: Error listing directory";
        case S_LS: return "s_ls: Error listing directory";
        case TOUCH_FILE: return "touch_file: Error touching file";
        case S_TOUCH: return "s_touch: Error touching file";
        case S_MV: return "s_mv: Error moving file";
        case S_CP: return "s_cp: Error copying file";
        case S_RM: return "s_rm: Error removing file";
        case CHANGE_MODE: return "change_mode: Error changing mode";
        case S_CHMOD: return "s_chmod: Error changing mode";
        case S_KILL: return "s_kill: Error killing process";
        case S_NICE: return "s_nice: Error changing priority";
        case S_NICE_PID: return "s_nice_pid: Error changing priority";
        case S_MAN: return "s_man: Error printing manual";
        case S_BG: return "s_bg: Error running process in background";
        case S_FG: return "s_fg: Error running process in foreground";
        case S_JOBS: return "s_jobs: Error printing jobs";
        case ZOMBIE_CHILD: return "zombie_child: Error creating zombie child";
        case S_ZOMBIFY: return "s_zombify: Error creating zombie child";
        case S_ORPHANIFY: return "s_orphanify: Error creating orphan child";
        case INSERT: return "insert: Error inserting into linked list";
        case ADD_SIGNAL: return "add_signal: Error adding signal";
        case ADD_TIMER: return "add_timer: Error adding timer";
        case P_SPAWN: return "p_spawn: Error spawning process";
        case P_SPAWN_SHELL: return "p_spawn_shell: Error spawning shell";
        case P_WAITPID: return "p_waitpid: Error waiting for process";
        case P_KILL: return "p_kill: Error sending signal to process";
        case P_EXIT: return "p_exit: Error exiting process";
        case P_NICE: return "p_nice: Error changing priority";
        case P_SLEEP: return "p_sleep: Error sleeping";
        case CLEANUP_ORPHANS: return "cleanup_orphans: Error cleaning up orphans";
        case SETSTACK: return "setStack: Error setting stack";
        case K_PROCESS_CREATE: return "k_process_create: Error creating process";
        case IS_SLEEP_2: return "is_sleep_2: Error checking if string is sleep";
        case K_PROCESS_KILL: return "k_process_kill: Error killing process";
        case K_PROCESS_CLEANUP: return "k_process_cleanup: Error cleaning up process";
        case GET_PROCESS_PQ: return "get_process_pq: Error getting process priority queue";
        case GET_NEXT_PQ: return "get_next_pq: Error getting next priority queue";
        case MAKECONTEXT: return "makecontext: Error making context";
        case SWAPPROCESS: return "swapProcess: Error swapping process";
        case IS_SLEEP: return "is_sleep: Error checking if string is sleep";
        case TIMERSERVICE: return "timerService: Error servicing timer";
        case MAKEPROCESSES: return "makeProcesses: Error making processes";
        case SCHEDULEPROCESS: return "scheduleProcess: Error scheduling process";
        case EXECUTE_SCRIPT: return "execute_script: Error executing script";
        case READ_LINE: return "read_line: Error reading line";
        case CREATE_LIST: return "create_list: Error creating linked list";
        case CREATE_NODE: return "create_node: Error creating node";
        case INSERT_FRONT: return "insert_front: Error inserting node at front";
        case INSERT_END: return "insert_end: Error inserting node at end";
        case INSERT_BEFORE: return "insert_before: Error inserting node before";
        case DELETE_NODE: return "delete_node: Error deleting node";
        case GET_NODE: return "get_node: Error getting node";
        case GET_NODE_INT: return "get_node_int: Error getting node";
        case WRITE_LOG: return "write_log: Error writing to log";
    }
    return "Unknown error";
}

void p_perror(char* message) {
    char* err = get_error_message(ERRNO);
    printf("ERROR: %s: %s\n", err, message);
    return;
}


