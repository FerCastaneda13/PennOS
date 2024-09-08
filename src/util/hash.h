/**
 * @file hash.h
 * @brief Header file for hash table implementation in PennOS.
 *
 * Defines structures and functions for managing a hash table used in PennOS,
 * primarily for command processing and lookup.
 * Created by Omar Ameen on 11/14/23.
 */

#ifndef INC_23FA_CIS3800_PENNOS_39_HASH_H
#define INC_23FA_CIS3800_PENNOS_39_HASH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../shell/shell-commands.h"

#define TABLE_SIZE 100 ///< Size of the hash table

/**
 * @brief Type definition for a command function.
 */
typedef void (*command_function)(char**);

/**
 * @brief Node structure for the hash table.
 *
 * Represents a single entry in the hash table, containing a command, its index,
 * the corresponding function to execute, and a pointer to the next node in the chain
 * for handling collisions.
 */
typedef struct HashNode {
    char* command;                ///< Command string
    int index;                    ///< Index of the command in the hash table
    command_function* function;   ///< Function pointer to the command's implementation
    struct HashNode* next;        ///< Pointer to the next node in the chain
} HashNode;

extern HashNode* hashTable[TABLE_SIZE]; ///< Global hash table array

/**
 * @brief Hash function for command strings.
 *
 * Computes the hash value for a given command string.
 *
 * @param str The command string to hash.
 * @return The computed hash value.
 */
unsigned int hash(char *str);

/**
 * @brief Inserts a command into the hash table.
 *
 * Adds a new command and its associated function to the hash table.
 *
 * @param command The command string.
 * @param index The index in the hash table.
 */
void insert(char *command, int index);

/**
 * @brief Looks up a command in the hash table.
 *
 * Searches for a command in the hash table and returns its index if found.
 *
 * @param command The command string to look up.
 * @return The index of the command in the hash table or -1 if not found.
 */
int lookup(char *command);

#endif //INC_23FA_CIS3800_PENNOS_39_HASH_H
