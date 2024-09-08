/**
 * @file logger.h
 * @brief Header file for the PennOS logger, containing function declarations for the logger.
 */

// Created by Omar Ameen on 11/11/23.

#ifndef INC_23FA_CIS3800_PENNOS_39_LOGGER_H
#define INC_23FA_CIS3800_PENNOS_39_LOGGER_H

#include <sys/types.h>

/// Enumerates the types of log entries
typedef enum logtype {
    LOG_SCHEDULE = 1,    ///< Log type for SCHEDULE EVENT
    LOG_CREATE = 2,      ///< Log type for CREATE/SPAWN a new process
    LOG_EXITED = 3,      ///< Log type for EXIT EVENT
    LOG_SIGNALED = 4,    ///< Log type for SIGNALED EVENT
    LOG_ZOMBIE = 5,      ///< Log type for ZOMBIE EVENT
    LOG_ORPHAN = 6,      ///< Log type for ORPHAN EVENT
    LOG_WAITED = 7,      ///< Log type for WAITED EVENT 
    LOG_NICE = 8,        ///< Log type for NICE EVENT (change priority of a process)
    LOG_BLOCKED = 9,     ///< Log type for BLOCK EVENT
    LOG_UNBLOCKED = 10,  ///< Log type for UNBLOCKED EVENT
    LOG_STOPPED = 11,    ///< Log type for STOP SIGNAL (ctrl-z)
    LOG_CONTINUED = 12   ///< Log type for CONTINUE SIGNAL (kill -cont pid) 
} logtype;

/// Writes a log entry
void write_log(logtype log_type, pid_t pid, int priority, char* process_name);

/// Opens the log file
FILE* log_file();

/// Writes a 'nice' log entry, logging priority changes
void write_log_nice(logtype log_type, pid_t pid, int oldprio, int newprio, char* name);

#endif //INC_23FA_CIS3800_PENNOS_39_LOGGER_H
