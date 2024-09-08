/**
 * @file logger.c
 * @brief Implementation of logging functions for PennOS.
 *
 * Created by Omar Ameen on 11/19/23.
 * This file contains the implementation of logging functions defined in logger.h.
 * It includes functions to write different types of log entries to a log file.
 */

#include <stdio.h>

#include "logger.h"
#include "../kernel/scheduler.h"
#include "../util/errno.h"

/// Writes a 'nice' log entry, logging priority changes
void write_log_nice(logtype log_type, pid_t pid, int oldprio, int newprio, char* process_name){
    // [ticks] NICE PID OLD_PRIORITY NEW_PRIORITY PROCESS_NAME
    FILE* log_file = fopen("log/log.txt", "a");
    if (log_file == NULL) {
        ERRNO = WRITE_LOG;
        p_perror("Error opening log file");
        perror("Error opening log file");
        return;
    }
    fprintf(log_file, "[%4d]\tNICE\t\t%d\t%d\t%d %s\n", curr_ticks / 100000, pid, oldprio, newprio, process_name);

    // Close the file
    fclose(log_file);
}

/// Opens the log file for appending
FILE* log_file() {
    return fopen("log/log.txt", "a");
}

/// Writes a general log entry based on the specified log type
void write_log(logtype log_type, pid_t pid, int priority, char* process_name){
    // [ticks] OPERATION PID NICE_VALUE PROCESS_NAME
    FILE* log_file = fopen("log/log.txt", "a");
    if (log_file == NULL) {
        ERRNO = WRITE_LOG;
        p_perror("Error opening log file");
        perror("Error opening log file");
        return;
    }
    if (log_type == LOG_SCHEDULE){
        fprintf(log_file, "[%4d]\tSCHEDULE\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_CREATE){
        fprintf(log_file, "[%4d]\tCREATE\t\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_EXITED){
        fprintf(log_file, "[%4d]\tEXITED\t\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_SIGNALED){
        fprintf(log_file, "[%4d]\tSIGNALED\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_ZOMBIE){
        fprintf(log_file, "[%4d]\tZOMBIE\t\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_ORPHAN){
        fprintf(log_file, "[%4d]\tORPHAN\t\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_WAITED){
        fprintf(log_file, "[%4d]\tWAITED\t\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_NICE){
        fprintf(log_file, "[%4d]\tNICE\t\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_BLOCKED){
        fprintf(log_file, "[%4d]\tBLOCKED\t\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_UNBLOCKED){
        fprintf(log_file, "[%4d]\tUNBLOCKED\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_STOPPED){
        fprintf(log_file, "[%4d]\tSTOPPED\t\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    } else if (log_type == LOG_CONTINUED){
        fprintf(log_file, "[%4d]\tCONTINUED\t%d\t%d\t%s\n", curr_ticks / 100000, pid, priority, process_name);
    }
    // Close the file
    fclose(log_file);
}
