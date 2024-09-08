/**
 * @file shell-parser.c
 * @brief Implements the parsing and execution of shell commands for the PennOS project.
 *        This file contains the implementation of functions to read and parse shell commands,
 *        convert strings to uppercase, initialize shell commands, and identify commands.
 */

// Created by Omar Ameen on 11/11/23.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "shell-parser.h"
#include "stress.h"
#include "../util/errno.h"

#define SHELL_TOK_BUFSIZE 64
#define SHELL_TOK_DELIM " \t\r\n\a"

/**
 * @var commands
 * @brief Array of function pointers to shell command functions.
 */
command_function commands[] = {
        s_cat,
        s_sleep,
        s_busy,
        s_echo,
        s_ls,
        s_touch,
        s_mv,
        s_cp,
        s_rm,
        s_chmod,
        s_ps,
        s_kill,
        s_nice,
        s_nice_pid,
        s_man,
        s_bg,
        s_fg,
        s_jobs,
        s_logout,
        s_zombify,
        s_orphanify,
        (void (*)(char **))hang,
        (void (*)(char **))nohang,
        (void (*)(char **))recur
};

/// Reads a line of input from stdin and returns it
char* read_line(void) {
    char *line = NULL;
    size_t bufsize = 0; // have getline allocate a buffer for us


    if (getline(&line, &bufsize, stdin) == -1){
        if (feof(stdin)) {
            exit(EXIT_SUCCESS); // We received an EOF
        } else  {
            ERRNO = READ_LINE;
            p_perror("Failed to read line");
            perror("readline");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

/// Converts a string to uppercase 
char* to_upper(char* string) {
    char* uppercase = malloc(sizeof(char) * strlen(string));
    for (int i = 0; string[i] != '\0'; i++) {
        uppercase[i] = toupper((char)string[i]);
    }
    return uppercase;
}

/// Initializes shell commands
void init_shell_commands(void) {
    insert("cat", CAT);
    insert("sleep", SLEEP);
    insert("busy", BUSY);
    insert("echo", ECHO);
    insert("ls", LS);
    insert("touch", TOUCH);
    insert("mv", MV);
    insert("cp", CP);
    insert("rm", RM);
    insert("chmod", CHMOD);
    insert("ps", PS);
    insert("kill", KILL);
    insert("nice", NICE);
    insert("nice_pid", NICE_PID);
    insert("man", MAN);
    insert("bg", BG);
    insert("fg", FG);
    insert("jobs", JOBS);
    insert("logout", LOGOUT);
    insert("zombify", ZOMBIFY);
    insert("orphanify", ORPHANIFY);
    insert("hang", HANG);
    insert("nohang", NOHANG);
    insert("recur", RECUR);
}
