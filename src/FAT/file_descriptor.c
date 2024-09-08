#include "file_descriptor.h"



file_descriptor* fd_create(int fd, int start_block, int offset, int perm, char* name, int file_size){
    file_descriptor* new_fd = malloc(sizeof(file_descriptor));
    if(new_fd == NULL){
        // ERRNO = 1;
        // p_perror("Failed to malloc file descriptor");
        perror("malloc failed");
        return NULL;
    }
    new_fd->fd = fd;
    new_fd->start_block = start_block;
    new_fd->offset = offset;
    new_fd->perm = perm;
    new_fd->name = name;
    new_fd->file_size = file_size;
    return new_fd;
}