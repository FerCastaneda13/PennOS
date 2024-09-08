/**
 * @file errno.h
 * @brief Header file for the PennOS errno, containing error number definitions and p_perror function declaration
 */

// Created by Omar Ameen on 11/11/23.

#ifndef INC_23FA_CIS3800_PENNOS_39_ERRNO_H
#define INC_23FA_CIS3800_PENNOS_39_ERRNO_H

extern int ERRNO;

// make an enum for error codes
typedef enum error_codes {
    CREATE_PCB = -1,
    GET_PCB_BY_PID = -2,
    SIG_HANDLER = -3,
    S_CAT = -4,
    S_SLEEP = -5,
    LOOKUP = -6,
    S_ECHO = -7,
    LIST_DIRECTORY = -8,
    S_LS = -9,
    TOUCH_FILE = -10,
    S_TOUCH = -11,
    S_MV = -12,
    S_CP = -13,
    S_RM = -14,
    CHANGE_MODE = -15,
    S_CHMOD = -16,
    S_KILL = -18,
    S_NICE = -19,
    S_NICE_PID = -20,
    S_MAN = -21,
    S_BG = -22,
    S_FG = -23,
    S_JOBS = -24,
    ZOMBIE_CHILD = -25,
    S_ZOMBIFY = -26,
    S_ORPHANIFY = -28,
    INSERT = -30,
    ADD_SIGNAL = -32,
    ADD_TIMER = -33,
    P_SPAWN = -34,
    P_SPAWN_SHELL = -35,
    P_WAITPID = -36,
    P_KILL = -38,
    P_EXIT = -39,
    P_NICE = -40,
    P_SLEEP = -41,
    CLEANUP_ORPHANS = -43,
    SETSTACK = -44,
    K_PROCESS_CREATE = -45,
    IS_SLEEP_2 = -46,
    K_PROCESS_KILL = -47,
    K_PROCESS_CLEANUP = -48,
    GET_PROCESS_PQ = -49,
    GET_NEXT_PQ = -51,
    MAKECONTEXT = -52,
    SWAPPROCESS = -53,
    IS_SLEEP = -54,
    TIMERSERVICE = -55,
    MAKEPROCESSES = -57,
    SCHEDULEPROCESS = -60,
    EXECUTE_SCRIPT = -61,
    READ_LINE = -63,
    CREATE_LIST = -65,
    CREATE_NODE = -66,
    INSERT_FRONT = -67,
    INSERT_END = -68,
    INSERT_BEFORE = -69,
    DELETE_NODE = -70,
    GET_NODE = -71,
    GET_NODE_INT = -72,
    WRITE_LOG = -74
} error_codes;

void p_perror(char* message);


// functions to make error codes for:
// create_PCB, get_PCB_by_pid

// sig_handler, s_cat, s_sleep, s_busy, s_echo, list_directory, s_ls, touch_file, s_touch s_mv, s_cp, s_rm, change_mode,
// s_chmod, s_ps, s_kill, s_nice, s_nice_pid, s_man, s_bg, s_fg, s_jobs, zombie_child, s_zombify, orphan_child,
// s_orphanify

// hash, insert, lookup

// add_signal, add_timer

// p_spawn, p_spawn_shell, p_waitpid, collectprocess, p_kill, p_exit, p_nice, p_sleep, idle, cleanup_orphans, setStack

// k_process_create, is_sleep_2, k_process_kill, k_process_cleanup

// get_process_pq, pq_all_blocked, get_next_pq

// makeContext, swapProcess, is_sleep, timerService, scheduler, makeProcesses, alarmHandler, freeStacks,
// scheduleProcess,

// execute_script, init_shell_commands, read_line, to_upper

// create_list, create_node, insert_front, insert_end,
// insert_before, delete_node, get_node, get_node_int, move_head_to_end



#endif //INC_23FA_CIS3800_PENNOS_39_ERRNO_H
