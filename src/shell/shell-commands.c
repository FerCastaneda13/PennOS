/**
 * @file   shell-commands.c
 * @brief  This file implements all shell commands and built-in functions for the PennOS project. 
 *         It includes functions like 'cat', 'sleep', 'echo', as well as unique commands like 'zombify' 
 *         and 'orphanify'. The file also contains signal handlers that send signals to the foreground process.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#include "shell-commands.h"
#include "../kernel/scheduler.h"
#include "../kernel/user.h"
#include "../kernel/kernel.h"
#include "shell-parser.h"
#include "../util/logger.h"
#include "../util/errno.h"
#include "../FAT/system-calls.h"

void sig_handler(int signum) {
    // SIGNALS CAN ONLY BE SENT TO THE FOREGROUND PROCESS
    int foreground_pid = foregroundProcess->pid;
    if (foreground_pid == 0) {
        ERRNO = SIG_HANDLER;
        p_perror("No foreground process");
        return;
    }
    // Call the add_signal() function correctly
    if (signum == SIGTSTP) {
        printf("SIGTSTP: %d\n", foreground_pid);
        if (foreground_pid != shellProcess->pid){
            p_kill(foreground_pid, S_SIGSTOP);
        }   
    } else if (signum == SIGCONT) {
        p_kill(foreground_pid, S_SIGCONT);
    } else if (signum == SIGINT) {
        if (foreground_pid != shellProcess->pid){
            p_kill(foreground_pid, S_SIGTERM);
        }   
    } else {
        ERRNO = SIG_HANDLER;
        p_perror("Invalid signal");
        return;
    }
}

/// Registers signal handlers that will be used by the shell
void register_sig_handlers() {
    signal(SIGTSTP, sig_handler);
    signal(SIGCONT, sig_handler);
    signal(SIGINT, sig_handler);
}

/// Concatenates and prints arguments
void s_cat (char** args) {
    int argc = 0;
    for (int i = 0; args[i] != NULL; i++) {
        argc++;
    }
    if (argc == 0) {
        char * buff = (char*) malloc(sizeof(char)*100);
        if(f_read(STDIN_FILENO, sizeof(buff), buff) == -1){
            ERRNO = S_CAT;
            p_perror("f_read error");
            perror("cat error:");
            return;
        }
        do {
            if(f_write(STDOUT_FILENO, buff, sizeof(buff)) == -1) {
                ERRNO = S_CAT;
                p_perror("f_write error");
                perror("cat error:");
                return;
            }
            free(buff);
            buff = (char*) malloc(sizeof(char)*100);
        } while(f_read(STDIN_FILENO, sizeof(buff), buff));
        free(buff);
    } else {
        for (int i = 0; i < argc; i++) {
            int file = f_open(args[i], 2);
            if (file == -1) {
                ERRNO = S_CAT;
                p_perror("fopen error");
                perror("cat error:");
                return;
            }

            int off = f_lseek(file, 0, SEEK_SET);
            off= f_lseek(file, 0, SEEK_END);
            f_lseek(file, 0, SEEK_CUR);

            char buff[off];
            if(f_read(file, off, buff) == -1){
                ERRNO = S_CAT;
                p_perror("f_read error");
                perror("cat error:");
                return;
            }
            if(f_write(STDOUT_FILENO, buff, off) == -1) {
                ERRNO = S_CAT;
                p_perror("f_write error");
                perror("cat error:");
                return;
            }
            
            if (f_close(file) == EOF) {
                ERRNO = S_CAT;
                p_perror("fclose error");
                perror("cat error:");
                return;
            }
        }
    }
    if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
        ERRNO = S_CAT;
        p_perror("p_kill error");
        return;
    }
}

/// Spawns a sleep process that will sleep for the specified duration
void s_sleep (char** args) {
    PCB* curr = k_current_process();
    if (args[0] == NULL) {
        ERRNO = S_SLEEP;
        p_perror("args is null");
        if (p_kill(shellProcess->waiting_on, S_SIGCHLD) == -1) {
            ERRNO = S_SLEEP;
            p_perror("p_kill error");
            return;
        }
        return;
    }
    unsigned int duration = 100 * centisecond * atoi(args[0]);
    add_timer(curr, duration);
    write_log(LOG_BLOCKED, curr->pid, curr->priority, curr->name);
    curr->state = BLOCKED;
    // oh im done signal
}

/// Spawns a busy process that will busy wait indefinitely
void s_busy (char** args) {
    while(true){
        // do nothing
    }
}

//int echo_helper(char* string) {
//    // Parse string and return 1 if string contains >, 2 if string contains >>, 0 otherwise
//    int i = 0;
//    while (string[i] != '\0') {
//        if (string[i] == '>') {
//            if (string[i + 1] == '>') {
//                return 2;
//            } else {
//                return 1;
//            }
//        }
//        i++;
//    }
//    return 0;
//}

/// Prints arguments to the console
void s_echo(char** args) {
    // INSTEAD OF CALLING PRINTF, ECHO SHOULD BE CALLING FWRITE TO CURRENT PROCESS'S FILE DESCRIPTOR 1
    //int fd_in = currentProcess->fd_in;
    int fd_out = currentProcess->fd_out;
    printf("fd_out: %d\n", fd_out);

    if (args == NULL) {
        ERRNO = S_ECHO;
        p_perror("args is null");
        if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
            ERRNO = S_ECHO;
            p_perror("p_kill error");
            return;
        }
        return;
    }

    int total_bytes_written = 0;
    int bytes_written = 0;  // ssize_t is typically used for functions that return -1 on error

    for (int i = 0; args[i] != NULL; i++) {
        bytes_written = f_write(fd_out, args[i], strlen(args[i]));
        if (bytes_written == -1) {
            printf("REACHED F_WRITE ERROR1\n");
            break;
        }
        total_bytes_written += bytes_written;

        if (args[i+1] != NULL) {
            printf("REACHED F_WRITE EMPTY QUOTATIONS\n");
            bytes_written = f_write(fd_out, " ", strlen(" "));
            if (bytes_written == -1) {
                printf("REACHED F_WRITE ERROR2\n");
                break;
            }
            total_bytes_written += bytes_written;
        }

        // if (args[i+1] == NULL) {
        //     bytes_written = f_write(fd_out, "\n", strlen("\n"));
        //     if (bytes_written == -1) {
        //         printf("REACHED F_WRITE ERROR3\n");
        //         break;
        //     }
        //     total_bytes_written += bytes_written;
        // }
    }
    printf("TOTAL_BYTES_WRITTEN: %d\n", total_bytes_written);

    char buf[total_bytes_written];
    int num_read = f_read(fd_out, total_bytes_written, buf);
    printf("NUM_READ: %d\n", num_read);
    printf("buffer: %s\n", buf);
    printf("REACHED FINSIHED F_WRITES\n");
    if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
        ERRNO = S_ECHO;
        p_perror("p_kill error");
        return;
    }
}

/// Helper function: Lists the contents of the specified directory
void list_directory(const char* path) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        ERRNO = LIST_DIRECTORY;
        p_perror("opendir error");
        perror("opendir");
        p_kill(currentProcess->pid, S_SIGCHLD);
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }

    if (closedir(dir) == -1) {
        ERRNO = LIST_DIRECTORY;
        p_perror("closedir error");
        perror("closedir");
        p_kill(currentProcess->pid, S_SIGCHLD);
        return;
    }
}

/// Lists the contents of the specified directory
void s_ls (char** args) {
    if (args[0] == NULL) {
        list_directory(".");
    } else {
        f_ls(args[0]);
    }
    
    if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
        ERRNO = S_LS;
        p_perror("p_kill error");
        return;
    }
}

/// Helper function: Updates the access and modification times of the specified file
void touch_file(char* filename) {
    f_touch(filename);
    // int fd = f_open(filename, 1);
    // printf("fd = %d", fd);
    // if (fd == -1) {
    //     ERRNO = TOUCH_FILE;
    //     p_perror("open error");
    //     perror("open");
    //     return;
    // }
    // if (f_close(fd) == -1) {
    //     ERRNO = TOUCH_FILE;
    //     p_perror("close error");
    //     perror("close");
    //     return;
    // }

    // // Update the access and modification times to the current time
    // if (utime(filename, NULL) == -1) {
    //     ERRNO = TOUCH_FILE;
    //     p_perror("utime error");
    //     perror("utime");
    // }
}

/// Creates an empty file or updates timestamp of existing files, similar to 'touch' command in bash.
void s_touch (char** args) {
    int argc = 0;
    for (int i = 0; args[i] != NULL; ++i) {
        argc++;
    }
    if (argc == 0) {
        //fprintf(stderr, "Usage: touch <file1> [<file2> ...]\n");
        if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
            ERRNO = S_TOUCH;
            p_perror("p_kill error");
            return;
        }
        return;
    }

    for (int i = 0; i < argc; i++) {
        f_touch(args[i]);
        // touch_file(args[i]);
    }
    if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
        ERRNO = S_TOUCH;
        p_perror("p_kill error");
        return;
    }
}

/// Renames a file from source to destination, similar to 'mv' command in bash.
void s_mv (char** args) {
    int argc = 0;
    for (int i = 0; args[i] != NULL; ++i) {
        argc++;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: mv <Source> <Destination>\n");
        if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
            ERRNO = S_MV;
            p_perror("p_kill error");
            return;
        }
        return;
    }
    
    if (!f_move(args)) {
        ERRNO = S_MV;
        p_perror("rename error");
        perror("rename");
        p_kill(currentProcess->pid, S_SIGCHLD);
        return;
    }
    if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
        ERRNO = S_MV;
        p_perror("p_kill error");
        return;
    }
}

/// Copies a file from source to destination, similar to 'cp' command in bash.
void s_cp (char** args) {
    int argc = 0;
    for (int i = 0; args[i] != NULL; ++i) {
        argc++;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: cp <Source> <Destination>");
        if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
            ERRNO = S_CP;
            p_perror("p_kill error");
            return;
        }
        return;
    }

    FILE* src = fopen(args[0], "rb");
    if (src == NULL) {
        ERRNO = S_CP;
        p_perror("fopen src error");
        perror("fopen src");
        p_kill(currentProcess->pid, S_SIGCHLD);
        return;
    }

    FILE* dest = fopen(args[1], "wb");
    if (dest == NULL) {
        ERRNO = S_CP;
        p_perror("fopen dest error");
        perror("fopen dest");
        if (fclose(src) == EOF) {
            p_perror("fclose src error");
            perror("fclose src");
            return;
        }
        p_kill(currentProcess->pid, S_SIGCHLD);
        return;
    }

    char buffer[BUFSIZ];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFSIZ, src)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }

    if (fclose(src) == EOF) {
        ERRNO = S_CP;
        p_perror("fclose src error");
        perror("fclose src");
        return;
    }
    if (fclose(dest) == EOF) {
        ERRNO = S_CP;
        p_perror("fclose dest error");
        perror("fclose dest");
        return;
    }

    if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
        ERRNO = S_CP;
        p_perror("p_kill error");
        return;
    }
}

/// Removes files, similar to 'rm' command in bash.
void s_rm (char** args) {
    int argc = 0;
    for (int i = 0; args[i] != NULL; ++i) {
        argc++;
    }

    if (argc != 1) {
        fprintf(stderr, "Usage: rm <File>\n");
        if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
            ERRNO = S_RM;
            p_perror("p_kill error");
            return;
        }
        return;
    }
    for (int i = 0; i < argc; i++) {
        if (f_unlink(args[i]) == -1){
            ERRNO = S_RM;
            p_perror("remove error");
            perror("remove");
            p_kill(currentProcess->pid, S_SIGCHLD);
            return;
        }
    }
    if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
        ERRNO = S_RM;
        p_perror("p_kill error");
        return;
    }
}

/// Helper function: Changes the mode of the specified file
void change_mode(mode_t *curr_mode, const char* changes) {
    const char* ptr = changes;
    while (*ptr) {
        char op = *ptr;
        ptr++;

        mode_t mask = 0;
        int done = 0;
        while (!done && *ptr) {
            switch (*ptr) {
                case 'r': mask |= S_IRUSR | S_IRGRP | S_IROTH; break;
                case 'w': mask |= S_IWUSR | S_IWGRP | S_IWOTH; break;
                case 'x': mask |= S_IXUSR | S_IXGRP | S_IXOTH; break;
                case 's': mask |= S_ISUID | S_ISGID; break;
                case 't': mask |= S_ISVTX; break;
                case '+': case '-': case '=': done = 1; continue;
                default: ERRNO = CHANGE_MODE; p_perror("change_mode pointer error"); return;
            }
            ptr++;
        }

        switch (op) {
            case '+': *curr_mode |= mask; break;
            case '-': *curr_mode &= mask; break;
            case '=': *curr_mode = (*curr_mode & ~07777) | mask; break;
        }
    }
}

/// Changes file modes, similar to 'chmod' command in bash.
void s_chmod (char** args) {
    int argc = 0;
    for (int i = 0; args[i] != NULL; ++i) {
        argc++;
    }

    if (argc != 2) {
        printf("Usage: %s <mode> <file> \n", args[0]);
        if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
            ERRNO = S_CHMOD;
            p_perror("p_kill error");
            return;
        }
        return;
    }

    f_chmod(args);

    printf("Permissions of %s modified \n", args[1]);
    if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
        ERRNO = S_CHMOD;
        p_perror("p_kill error");
        return;
    }
    return;
}

/// Lists all processes on PennOS, displaying pid, ppid, and priority.
void s_ps (char** args) {
    printf("PID \t PPID \t PRI \t STAT \t CMD \n");
    Node* node = pcb_list->head;

    while (node != NULL) {
        PCB* pcb = node->data;
        char ch;
        switch (pcb->state) {
            case 1: ch = 'R'; break;
            case 0: ch = 'S'; break;
            case -1: ch = 'B'; break;
            case -2: ch = 'Z'; break;
            case -3: ch = 'E'; break;
            case -4: ch = 'T'; break;
        }
        printf("%d \t %d \t %d \t %c \t %s \n", pcb->pid, pcb->parent_pid, pcb->priority, ch, pcb->name);
        node = node->next;
    }

    printf("ps waiting on: %d\n", currentProcess->pid);
//    p_kill(currentProcess->pid, S_SIGCHLD);
    p_exit();
}

/// Sends a signal to a process
void s_kill (char** args) {
    int argc = 0;
    for (int i = 0; args[i] != NULL; i++) {
        argc++;
    }

    if (argc != 2) {
        printf("Usage: kill [ -SIGNAL_NAME ] <PID> \n");
        if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
            ERRNO = S_KILL;
            p_perror("p_kill error");
            return;
        }
        return;
    }

    int pid = atoi(args[1]);

    if (strcmp(args[0], "-term") == 0) {
        if (p_kill(pid, S_SIGTERM) == -1) {
            ERRNO = S_KILL;
            p_perror("p_kill error");
            return;
        }
        if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
            ERRNO = S_KILL;
            p_perror("p_kill error");
            return;
        }
        return;
    } else if (strcmp(args[0], "-stop") == 0) {
        if (p_kill(pid, S_SIGSTOP) == -1) {
            ERRNO = S_KILL;
            p_perror("p_kill error");
            return;
        }
        if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
            ERRNO = S_KILL;
            p_perror("p_kill error");
            return;
        }
        return;
    } else if (strcmp(args[0], "-cont") == 0) {
        if (p_kill(pid, S_SIGCONT) == -1) {
            ERRNO = S_KILL;
            p_perror("p_kill error");
            return;
        }
        if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
            ERRNO = S_KILL;
            p_perror("p_kill error");
            return;
        }
        return;
    } else {
        ERRNO = S_KILL;
        p_perror("Invalid signal");
        p_kill(currentProcess->pid, S_SIGCHLD);
        return;
    }
}

/// Runs a command with a modified priority
void s_nice (char** args) {
    // args is a string array starting with "nice"
    // Make last string in array the index
    int argc = 0;
    for (int i = 0; args[i] != NULL; i++) {
        argc++;
    }

    if (argc < 4) {
        perror("Usage: nice priority command [arg] \n");
        return;
    }

    // argc -1, leave 1 room for null terminator
    char** passed_args = malloc(sizeof(char*) * (argc - 1));
    if (passed_args == NULL) {
        ERRNO = S_NICE;
        p_perror("malloc error");
        perror("malloc");
        return;
    }
    for (int i = 2; i < argc; i++) {
        passed_args[i - 2] = args[i];
    }
    passed_args[argc - 2] = NULL;

    int index = lookup(passed_args[0]);

    // Will need to spawn new process - not sure if need to wait for it or not though
    pid_t pid = p_spawn((void (*)())commands[index], passed_args, 0, 1); // LOGS CREATE
    if (pid == -1) {
        ERRNO = S_NICE;
        p_perror("p_spawn error");
        return;
    }
    PCB* child = get_PCB_by_pid(pcb_list, pid);
    if (child == NULL) {
        ERRNO = S_NICE;
        p_perror("get_PCB_by_pid error");
        return;
    }
    child->parent_pid = shellProcess->pid;
    if (p_nice(pid, atoi(args[1])) == -1) {
        ERRNO = S_NICE;
        p_perror("p_nice error");
        return;
    }
    shellProcess->waiting_on = pid;
    foregroundProcess = child;
    int status;
    if (p_waitpid(pid, &status, 0) == -1) {
        ERRNO = S_NICE;
        p_perror("p_waitpid error");
        return;
    }
//    pid_t pt = p_waitpid(pid, &status, 0);
}

/// Adjusts the priority of a process
void s_nice_pid(char** args) {
    int new_priority = atoi(args[1]);
    int pid = atoi(args[2]);
    if (p_nice(pid, new_priority) == -1) {
        ERRNO = S_NICE_PID;
        p_perror("p_nice error");
        return;
    }
}
void s_man (char** args) {
    if (printf("Penn-Shell Commands: \n"
           "cat [filenames] --- concatenate files and print on the standard output\n"
           "sleep [duration] --- suspend execution for an interval of time\n"
           "busy --- busy wait indefinitely\n"
           "echo [args] --- display a line of text\n"
           "ls [filenames] --- list directory contents\n"
           "touch [filenames] --- change file timestamps\n"
           "mv [source] [destination] --- move files\n"
           "cp [source] [destination] --- copy files\n"
           "rm [filenames] --- remove files\n"
           "chmod [mode] [filename] --- change file mode bits\n"
           "ps --- report a snapshot of the current processes\n"
           "kill [-term | -stop | -cont] [pid] --- send a signal to a process\n"
           "nice [priority] [command] [args] --- run a command with a modified priority\n"
           "nice_pid [priority] --- adjust the priority of a process\n"
           "man --- list all available commands\n"
           "fg [job_id] --- bring a background job to the foreground\n"
           "bg [job_id] --- resume a stopped background job\n"
           "jobs --- list all jobs\n"
           "logout --- exit the shell\n") < 0) {
        ERRNO = S_MAN;
        p_perror("printf error");
        perror("printf");
        return;
    }
}

/// Continues the last stopped job, or the job specified by job_id.
void s_bg (char** args) {
    // bg jobid or just bg
    Node* node_to_resume = pcb_list->tail;
    if (node_to_resume == NULL) {
        ERRNO = S_BG;
        p_perror("pcb_list is null");
        return;
    }
    // BG WITH NO INPUT, JUST FIND LAST STOPPED JOB IN PCB LIST
    if (args[1] == NULL){
        while (node_to_resume != NULL && ((PCB*)node_to_resume->data)->state != STOPPED){
            node_to_resume = node_to_resume->prev;
        }
    } else {
        // BG WITH INPUT, FIND JOBID IN PCB LIST
        int jobid = atoi(args[1]);
        while (node_to_resume != NULL && ((PCB*)node_to_resume->data)->pid != jobid){
            node_to_resume = node_to_resume->prev;
        }
    }
    if (node_to_resume == NULL) {
        return;
    }
    PCB* pcb_to_resume = node_to_resume->data;
    if (pcb_to_resume->state != STOPPED){
        return;
    }
    int pid_to_resume = pcb_to_resume->pid;
    p_kill(pid_to_resume, S_SIGCONT);
}

/// Brings the last stopped or backgrounded job to the foreground, or the job specified by job_id.
void s_fg (char** args) {
    // FOREGROUND PROCESS IS CURRENTLY SHELL
    int shell_pid = foregroundProcess->pid;
    Node* bring_to_foreground = pcb_list->tail;
    if (bring_to_foreground == NULL) {
        ERRNO = S_FG;
        p_perror("pcb_list is null");
        return;
    }
    // FG WITH NO INPUT, JUST FIND LAST BACKGROUND JOB 
    if (args[1] == NULL){
        while (bring_to_foreground != NULL){
            if (((PCB*)bring_to_foreground->data)->pid != shell_pid){
                // FIND FIRST PCB IN PCB_LIST THAT IS NOT SHELL aka FOREGROUND PROCESS
                break;
            }
            bring_to_foreground = bring_to_foreground->prev;
        }
    } else {
        int jobid = atoi(args[1]);
        while (bring_to_foreground != NULL && ((PCB*)bring_to_foreground->data)->pid != jobid){
            bring_to_foreground = bring_to_foreground->prev;
        }
    }
    if (bring_to_foreground == NULL){
        return;
    }
    PCB* pcb_to_foreground = bring_to_foreground->data;
    if (pcb_to_foreground->state == STOPPED){
        p_kill(pcb_to_foreground->pid, S_SIGCONT);
    }
    // WHAT IF STATE IS BLOCKED?

    foregroundProcess = pcb_to_foreground;
    shellProcess->waiting_on = pcb_to_foreground->pid;

    // FOR FOREGROUND, CALL WAITPID ON FOREGROUND HERE OR WITHIN THE FOREGROUND PROCESS ITSELF?
    int status;
    if (p_waitpid(foregroundProcess->pid, &status, 0) == -1) {
        ERRNO = S_FG;
        p_perror("p_waitpid error");
        return;
    }
}

/// Lists all jobs.
void s_jobs (char** args) {
    // args will be jobs
    int foreground_pid = foregroundProcess->pid;
    int shell_pid = shellProcess->pid;
    for (Node* node = pcb_list->head; node != NULL; node = node->next) {
        PCB* curr_pcb = node->data;
        int curr_pid = curr_pcb->pid;
        if (curr_pid == foreground_pid || curr_pid == shell_pid) {
            continue;
        }
        // is a background job, so also get its name and state
        char* name = curr_pcb->name;
        char* state = getStateName(curr_pcb->state);
        if (printf("[%d] %s (%s)\n", curr_pid, name, state) < 0) {
            ERRNO = S_JOBS;
            p_perror("printf error");
            perror("printf");
            return;
        }
    }
}

/// Exits the shell.
void s_logout (char** args) {
    // free shell
    // free pcb lists and prioritiy queues
    // free all processes
    // free all memory
    Node* node = pcb_list->head;
    while (node != NULL) {
        Node* temp = node->next;
        k_process_cleanup(node->data);
        node = temp;
    }
    exit(EXIT_SUCCESS);
}

/// A process is spawned with this ucontext function in s_zombify()
void zombie_child(char** args){
    if (printf("ZOMBIE CHILD: MMM BRAINS") < 0) {
        ERRNO = ZOMBIE_CHILD;
        p_perror("printf error");
        perror("printf");
        return;
    }
    p_kill(currentProcess->pid, S_SIGCHLD);
}

/// Spawns a child process that will become a zombie.
void s_zombify(char** args){
    if (printf("S_ZOMBIFY: REACHED\n") < 0) {
        ERRNO = S_ZOMBIFY;
        p_perror("printf error");
        perror("printf");
        return;
    }
    char** new_args = malloc(2 * sizeof(char*));
    if (new_args == NULL) {
        ERRNO = S_ZOMBIFY;
        p_perror("malloc error");
        perror("malloc");
        return;
    }
    new_args[0] = strdup("zombie_child");
    if (new_args[0] == NULL) {
        ERRNO = S_ZOMBIFY;
        p_perror("strdup error");
        perror("strdup");
        return;
    }
    new_args[1] = NULL;
    p_spawn(zombie_child, new_args, 0, 1);
    while(1);
    p_kill(currentProcess->pid, S_SIGCHLD);
}

/// A process is spawned with this ucontext function in s_orphanify()
void orphan_child(char** args){
    while(1);
    p_kill(currentProcess->pid, S_SIGCHLD);
}

/// Spawns a child process that will become an orphan.
void s_orphanify(char** args){
    char** new_args = malloc(2 * sizeof(char*));
    if (new_args == NULL) {
        ERRNO = S_ORPHANIFY;
        p_perror("malloc error");
        perror("malloc");
        return;
    }
    new_args[0] = strdup("orphan_child");
    if (new_args[0] == NULL) {
        ERRNO = S_ORPHANIFY;
        p_perror("strdup error");
        perror("strdup");
        return;
    }
    new_args[1] = NULL;

    if (p_spawn(orphan_child, new_args, 0, 1) == -1) {
        ERRNO = S_ORPHANIFY;
        p_perror("p_spawn error");
        return;
    }
    if (p_kill(currentProcess->pid, S_SIGCHLD) == -1) {
        ERRNO = S_ORPHANIFY;
        p_perror("p_kill error");
        return;
    }
}

/// Idle function for Idle Context
void idle() {
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGALRM);
    sigdelset(&mask, SIGSTOP);
    sigdelset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    sigsuspend(&mask);


    PCB* parent = get_PCB_by_pid(pcb_list, 1);
    setcontext(parent->context);
}