/**
 * @file shell.c
 * @brief C file for the PennOS shell, containing functions for the shell.
 */

// Created by Omar Ameen on 11/11/23.

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>

#include "shell.h"
#include "shell-parser.h"
#include "parser.h"
#include "../kernel/user.h"
#include "../kernel/scheduler.h"
#include "../util/logger.h"
#include "../util/errno.h"
#include "../FAT/system-calls.h"

// Function to process each line from the buffer
int processLines(char *buffer, long length) {
    int start = 0;
    for (long i = 0; i < length; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\0') {
            buffer[i] = '\0';
            char* line = &buffer[start];
            struct parsed_command* script_cmd = malloc(sizeof(struct parsed_command));
            if (!script_cmd) {
                ERRNO = EXECUTE_SCRIPT;
                p_perror("Failed to malloc script command");
                perror("malloc failed");
                return -1;
            }
            if (parse_command(line, &script_cmd) != 0) {
                ERRNO = EXECUTE_SCRIPT;
                p_perror("Failed to parse script command");
                return -1;
            }
            if (script_cmd->num_commands == 0) {
                free(script_cmd);
                return -1;
            }
            char* script_command = *script_cmd->commands[0];
            int script_index = lookup(script_command);
            int fd_input = f_open(script_cmd->stdin_file, 2);
            int fd_output = f_open(script_cmd->stdout_file, 3);
            if (script_index >= 0) {
                // IS A INDEPENDENT PROCESS
                pid_t pid = p_spawn((void (*)())commands[script_index], script_cmd->commands[0], fd_input, fd_output); // LOGS CREATE
                if (pid == -1) {
                    ERRNO = EXECUTE_SCRIPT;
                    p_perror("Failed to spawn process");
                    return -1;
                }
                PCB* new_process = get_PCB_by_pid(pcb_list, pid);
                if (new_process == NULL) {
                    ERRNO = EXECUTE_SCRIPT;
                    p_perror("Failed to get PCB by pid");
                    return -1;
                }
                new_process->parent_pid = shellProcess->pid;
                if (!script_cmd->is_background) {
                    foregroundProcess = new_process;
                    shellProcess->waiting_on = pid;
                    int status;
                    pid_t waitpid = p_waitpid(pid, &status, 0);
                    if (waitpid == -1) {
                        ERRNO = EXECUTE_SCRIPT;
                        p_perror("Failed to wait for process");
                        return -1;
                    }
                    // pid_t pt = p_waitpid(pid, &status, 0);
                    // What to do with pt?
                } else {
                    shellProcess->waiting_on = -1;
                    swapcontext(shellProcess->context, schedulerContext->context);
                }
            } else {
                if (script_index == -1) {
                    int fd = f_open(line, 1);
                    off_t num = f_lseek(fd, 0, SEEK_SET);
                    num = f_lseek(fd, 0, SEEK_END);
                    f_lseek(fd, 0, SEEK_SET);
                    if (processLines(*script_cmd->commands[0], num) == -1) {
                        ERRNO = EXECUTE_SCRIPT;
                        p_perror("Script not found");
                        return -1;
                    }
                } else { // Built in command
                    script_index = script_index * -1;
                    commands[script_index](script_cmd->commands[0]);
                }
            }
            free(script_cmd);
            start = i + 1;
        }
    }
    return 0;
}

/// Spawns processes by reading a script file line by line and executing each line as a command
int execute_script(char* filename) {
    // might be a script, look for directory in command
    // FILE* file = fopen(filename, "r");
    int fd = f_open(filename, F_READ);
    // Check if file is null
    if (fd == -1) {
        ERRNO = EXECUTE_SCRIPT;
        p_perror("Failed to open file");
        perror("File not found. \n");
    } else {
        // File exists, run it
        char* line = NULL;
        off_t num = f_lseek(fd, 0, SEEK_SET);
        num = f_lseek(fd, 0, SEEK_END);
        f_lseek(fd, 0, SEEK_SET);
        char* buf = malloc(sizeof(char) * (num+1));
        if (f_read(fd, num, buf) != 0) {
            // printf("Retrieved line of length %zu:\n", read);
            // printf("%s", line);
            processLines(buf, num);
        }
        if (f_close(fd) == EOF) {
            ERRNO = EXECUTE_SCRIPT;
            p_perror("Failed to close file");
            return -1;
        }
        if (line) {
            free(line);
        }
    }
    return 0;
}

/// Runs the shell loop that spawns commands and waits for them if the command is in the foreground
void shell_loop() {
    init_shell_commands();

    char* buffer = NULL;
    struct parsed_command* cmd = NULL;

    while (true) {
        printf("$ ");
        buffer = read_line();
        cmd = malloc(sizeof(struct parsed_command)); // Allocate memory for cmd each iteration
        if (!cmd) { // WE MAKE IT HERE
            perror("malloc"); // WE MAKE IT HERE
            exit(EXIT_FAILURE);
        }
        if (parse_command(buffer, &cmd) != 0) {
            free(buffer); // Free buffer if command is not parsed
            free(cmd);    // Free cmd as well
            continue;
        }// WE MAKE IT HERE
        if (cmd->num_commands == 0) {
            free(buffer); // Free buffer if no commands are parsed
            free(cmd);    // Free cmd as well
            continue;
        }
        char* command = *cmd->commands[0];// WE MAKE IT HERE
        int index = lookup(command);
        
        int fd_input;
        int fd_output;
        if (cmd->stdin_file != NULL) {
            fd_input = f_open(cmd->stdin_file, F_READ);
            printf("f_open OUTPUT %d\n", fd_input);
        } else {
            fd_input = 0;
        }
        if (cmd->stdout_file != NULL) {
            if (cmd->is_file_append) {
                fd_output = f_open(cmd->stdout_file, F_APPEND);
            } else {
                fd_output = f_open(cmd->stdout_file, F_WRITE);
            }
            printf("f_open OUTPUT %d\n", fd_output);
        } else {
            fd_output = 1;
        }
        if (index >= 0) {
            // IS A INDEPENDENT PROCESS
            pid_t pid = p_spawn((void (*)())commands[index], cmd->commands[0], fd_input, fd_output); // LOGS CREATE
            if (pid == -1) {
                ERRNO = P_SPAWN;
                p_perror("Failed to spawn process from shell");
                continue;
            }
            PCB* new_process = get_PCB_by_pid(pcb_list, pid);
            if (new_process == NULL) {
                ERRNO = GET_PCB_BY_PID;
                p_perror("Failed to get PCB by pid from shell");
                continue;
            }
            new_process->parent_pid = shellProcess->pid;
            
            if (!cmd->is_background) {
                foregroundProcess = new_process;
                shellProcess->waiting_on = pid;
                int status;
                pid_t wait = p_waitpid(pid, &status, 0);
                if (wait == -1) {
                    ERRNO = P_WAITPID;
                    p_perror("Failed to wait for process from shell");
                    continue;
                }
                // pid_t pt = p_waitpid(pid, &status, 0);
                // What to do with pt?
            } else {
                shellProcess->waiting_on = -1;
                swapcontext(shellProcess->context, schedulerContext->context);
            }

        } else {
            if (index == -1) {
                if (execute_script(command) == -1) {
                    ERRNO = EXECUTE_SCRIPT;
                    p_perror("Failed to execute script");
                }
            } else { // Built in command
                index = index * -1;
                commands[index](cmd->commands[0]);
            }
        }

        free(buffer); // Free buffer after use
        free(cmd);    // Free cmd after use

        // NOHANG WAIT ON ALL NODES IN PCB LIST WHOSE PARENT IS SHELL
        Node* process = pcb_list->tail;
        while (process != NULL) {
            Node* temp = process->prev;
            PCB* pcb = process->data;
            if (pcb->parent_pid == shellProcess->pid) {
                int status;
                p_waitpid(pcb->pid, &status, 1);
//                pid_t wait = p_waitpid(pcb->pid, &status, 1);
//                if (wait == -1) {
//                    ERRNO = P_WAITPID;
//                    p_perror("Failed to wait for process from shell");
//                    continue;
//                }
            }
            process = temp;
        }
    }
}
