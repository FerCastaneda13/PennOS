#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

/**
 * @file system-calls.h
 * @brief Header file for system-related calls for file manipulation
 */

/**
 * @brief Enum for different f_open modes.
 */
typedef enum {
    F_WRITE,   /**< Write mode */
    F_READ,    /**< Read mode */
    F_APPEND   /**< Append mode */
} mode;

/**
 * @brief Open a file with the specified mode.
 *
 * @param fname Constant char pointer to the filename to be opened.
 * @param mode Mode in which the file will be opened (F_READ, F_WRITE, F_APPEND).
 * @return File descriptor on success, or a negative value on error.
 */
int f_open(const char *fname, int mode);

/**
 * @brief Read n bytes from the file referenced by fd.
 *
 * @param fd File descriptor of the file to read from.
 * @param n Number of bytes to be read.
 * @param buf Pointer to a buffer where the read bytes should be stored.
 * @return Number of bytes read (0 if EOF, negative on error).
 */
int f_read(int fd, int n, char *buf);

/**
 * @brief Write n bytes of the string referenced by str to the file fd and increment the file pointer by n.
 *
 * @param fd File descriptor of the file to write to.
 * @param str Pointer to a buffer containing the bytes to be written.
 * @param n Number of bytes to be written.
 * @return Number of bytes written, or a negative value on error.
 */
int f_write(int fd, const char *str, int n);

/**
 * @brief Close the file referenced by fd.
 *
 * @param fd File descriptor of the file to be closed.
 * @return 0 on success, or a negative value on failure.
 */
int f_close(int fd);

/**
 * @brief Remove a file, waiting if multiple processes are currently using it.
 *
 * @param fname Pointer to the filename to be removed.
 * @return 0 on success, or a negative value on failure.
 */
int f_unlink(const char *fname);

/**
 * @brief Reposition the file pointer for fd to the offset relative to whence.
 *
 * @param fd File descriptor of the file.
 * @param offset Number of bytes the file pointer will be offset by.
 * @param whence Whence will be performed (F_SEEK_SET, F_SEEK_CUR, F_SEEK_END).
 * @return Current offset of the file.
 */
off_t f_lseek(int fd, int offset, int whence);

/**
 * @brief List the file filename in the current directory. If filename is NULL, list all files in the current directory.
 *
 * @param filename Pointer to the name of the filesystem for which f_ls will be performed.
 */
void f_ls(const char *filename);

/**
 * @brief Create or update a file's timestamp.
 *
 * If the file exists, the timestamp will be updated.
 *
 * @param file_name Pointer to the name of the file to be created/updated.
 * @return 0 on success, and -1 on failure.
 */
int f_touch(char* file_name);

/**
 * @brief Helper function that will copy the name of the source to the destination.
 *
 * @param files Char double pointer to the names of the source file and the destination file.
 * @return True on success and false on failure.
 */
bool f_move(char** files);

/**
 * @brief Helper function that will change the permissions of a file.
 *
 * @param file_name Char double pointer to the new permissions and the file that will be changed.
 */
void f_chmod(char** file_name);

/**
 * @brief Mount the current filesystem.
 *
 * @param filename Pointer to the filename to be mounted.
 */
void mount(char* filename);
