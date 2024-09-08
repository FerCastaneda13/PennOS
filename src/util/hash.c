/**
 * @file hash.c
 * @brief Implementation of hash table functions for command processing.
 *
 * This file contains the implementations of the functions defined in hash.h. 
 * It includes the creation and manipulation of a hash table used for storing 
 * and retrieving command-related data efficiently. The functions provide 
 * capabilities to hash strings, insert commands into the hash table, and 
 * look up commands based on their string representation.
 */

#include "hash.h"
#include <ctype.h>
#include "../shell/shell-parser.h"
#include "../util/errno.h"

// Global hash table array
HashNode* hashTable[TABLE_SIZE];

// Computes the hash value for a given string
unsigned int hash(char *str) {
    unsigned int hashValue = 0;
    while (*str != '\0') {
        hashValue = (hashValue << 5) + *str++;
    }
    return hashValue % TABLE_SIZE;
}

// Inserts a command and its index into the hash table
void insert(char *command, int function_index) {
    unsigned int index = hash(command);
    HashNode* newNode = (HashNode*) malloc(sizeof(HashNode));
    if (newNode == NULL) {
        ERRNO = INSERT;
        p_perror("Failed to malloc hash node");
        perror("malloc failed");
        return;
    }
    newNode->command = strdup(command); // Copying the string
    if (newNode->command == NULL) {
        ERRNO = INSERT;
        p_perror("Failed to strdup command");
        perror("strdup failed");
        return;
    }
    newNode->command = strdup(command); // Duplicate the command string
    newNode->index = function_index;
    newNode->next = hashTable[index];
    hashTable[index] = newNode;
}

// Looks up a command in the hash table
int lookup(char *command) {
    unsigned int index = hash(command);
    HashNode* list = hashTable[index];
    while (list != NULL) {
        if (strcmp(list->command, command) == 0) {
            int neg = list->index * -1;

            // Check for special commands that need negative index
            if (strcmp(command, "nice") == 0 || strcmp(command, "nice_pid") == 0 || strcmp(command, "man") == 0 ||
            strcmp(command, "bg") == 0 || strcmp(command, "fg") == 0 || strcmp(command, "jobs") == 0 ||
            strcmp(command, "logout") == 0 || strcmp(command, "hang") == 0 || strcmp(command, "nohang") == 0
            || strcmp(command, "recur") == 0) {
                return neg;
            }

            return list->index; // Return the found command's index
        }
        list = list->next;
    }
    return -1; // Command not found
}
