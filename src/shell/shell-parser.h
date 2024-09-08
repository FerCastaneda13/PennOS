/**
 * @file shell-parser.h
 * @brief Defines the interface for shell command parsing in the PennOS project.
 *        It includes function prototypes for reading and processing shell commands,
 *        as well as the enumeration of supported shell commands.
 */

// Created by Omar Ameen on 11/11/23.

#ifndef INC_23FA_CIS3800_PENNOS_39_SHELL_PARSER_H
#define INC_23FA_CIS3800_PENNOS_39_SHELL_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/hash.h"

typedef void (*command_function)(char**);

/**
 * @brief Reads a line from the standard input.
 * @return A pointer to the read line.
 */
char* read_line(void);

/**
 * @enum shell_command
 * @brief Enumerates the available shell commands.
 */
typedef enum shell_command {
    CAT = 0,
    SLEEP = 1,
    BUSY = 2,
    ECHO = 3,
    LS = 4,
    TOUCH = 5,
    MV = 6,
    CP = 7,
    RM = 8,
    CHMOD = 9,
    PS = 10,
    KILL = 11,
    NICE = 12,
    NICE_PID = 13,
    MAN = 14,
    BG = 15,
    FG = 16,
    JOBS = 17,
    LOGOUT = 18,
    ZOMBIFY = 19,
    ORPHANIFY = 20,
    HANG = 21,
    NOHANG = 22,
    RECUR = 23
} shell_command;

/**
 * @brief Initializes shell commands.
 */
void init_shell_commands(void);

/**
 * @brief Identifies a command from a given string.
 * @param command The command string.
 * @return Integer representing the command.
 */
int identify_command(char* command);

/**
 * @brief Converts a string to uppercase.
 * @param string The string to be converted.
 * @return A pointer to the uppercase string.
 */
char* to_upper(char* string);

extern command_function commands[];

#endif //INC_23FA_CIS3800_PENNOS_39_SHELL_PARSER_H
