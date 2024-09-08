//
// Created by Omar Ameen on 11/11/23.
//

#ifndef INC_23FA_CIS3800_PENNOS_39_SHELL_COMMANDS_H
#define INC_23FA_CIS3800_PENNOS_39_SHELL_COMMANDS_H

typedef void (*command_function2)(char**);

/** 
 * @brief Registers signal handlers.
 */
void register_sig_handlers();

/** 
 * @brief Concatenates and prints arguments.
 * @param args Arguments to be concatenated.
 */
void s_cat (char** args);

/** 
 * @brief Spawns a sleep process.
 * @param args Duration for the sleep.
 */
void s_sleep (char** args);

/** 
 * @brief Busy waits indefinitely.
 * @param args Arguments to be ignored.
 */
void s_busy (char** args);

/** 
 * @brief Echoes arguments to the output, similar to 'echo' command in bash.
 * @param args Arguments to be echoed.
 */
void s_echo (char** args);

/** 
 * @brief Lists all files in the working directory, similar to 'ls -il' command in bash.
 */
void s_ls (char** args);

/** 
 * @brief Creates an empty file or updates timestamp of existing files, similar to 'touch' command in bash.
 * @param args File names to be created or updated.
 */
void s_touch (char** args);

/** 
 * @brief Renames a file from source to destination, similar to 'mv' command in bash.
 * @param args Array containing source and destination file names.
 */
void s_mv (char** args);

/** 
 * @brief Copies a file from source to destination, similar to 'cp' command in bash.
 * @param args Array containing source and destination file names.
 */
void s_cp (char** args);

/** 
 * @brief Removes files, similar to 'rm' command in bash.
 * @param args File names to be removed.
 */
void s_rm (char** args);

/** 
 * @brief Changes file modes, similar to 'chmod' command in bash.
 * @param args Arguments specifying file mode changes.
 */
void s_chmod (char** args);

/** 
 * @brief Lists all processes on PennOS, displaying pid, ppid, and priority.
 */
void s_ps (char** args);

/** 
 * @brief Sends a specified signal to specified processes, similar to '/bin/kill' in bash.
 * @param args Array containing signal name and process IDs.
 */
void s_kill (char** args);

/** 
 * @brief Sets the priority of the command and executes it.
 * @param args Array containing the priority and the command (with optional arguments).
 */
void s_nice (char** args);

/** 
 * @brief Adjusts the nice level of a process to a specified priority.
 * @param args Array containing the priority and the process ID.
 */
void s_nice_pid(char** args);

/** 
 * @brief Lists all available commands.
 * @param args Arguments to be ignored.
 */
void s_man (char** args);

/** 
 * @brief Continues the last stopped job, or the job specified by job_id.
 * @param args Optional job_id to be continued; if not provided, continues the last stopped job.
 */
void s_bg (char** args);

/** 
 * @brief Brings the last stopped or backgrounded job to the foreground, or the job specified by job_id.
 * @param args Optional job_id to be brought to the foreground; if not provided, brings the last job to the foreground.
 */
void s_fg (char** args);

/** 
 * @brief Lists all jobs.
 * @param args Arguments to be ignored.
 */
void s_jobs (char** args);

/** 
 * @brief Exits the shell and shuts down PennOS.
 * @param args Arguments to be ignored.
 */
void s_logout (char** args);

/**
 * @brief Spawns a child process that will become a zombie.
 * @param args Arguments to be ignored.
*/
void s_zombify (char** args);

/**
 * @brief Spawns a child process that will become an orphan.
 * @param args Arguments to be ignored.
*/
void s_orphanify (char** args);

/**
 * @brief Spawns a child process that idle waits by calling sigsuspend until a signal is received.
 * @param args Arguments to be ignored.
*/
void idle();

#endif //INC_23FA_CIS3800_PENNOS_39_SHELL_COMMANDS_H
