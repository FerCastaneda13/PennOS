#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * @file file_descriptor.h
 * @brief Header file for a struct containing a file information and the creation of it.
 */

/**
 * @brief Creates an struct that contains information from a file inside a filesytem
 *
 */
typedef struct file_descriptor{
    int fd;
    int start_block;
    int offset;
    int file_size;
    int perm;
    char* name;
} file_descriptor;

/**
 * @brief Creates a file_descriptor struct with the necessary information.
 *
 * @param file_descriptor int of an open file
 * @param start_block int of a file inside a filesystem
 * @param offset int for the current location of the information in a file inside the filesystem
 * @param permissions int describing the current permission of the file
 * @param file_name char pointer to the name of the file
 * @param file_size int containing the size of the file
 * @return file_descriptor pointer on success with the information that was inputted and NULL on error
 */
file_descriptor* fd_create(int fd, int start_block, int offset, int perm, char* name, int file_size);
