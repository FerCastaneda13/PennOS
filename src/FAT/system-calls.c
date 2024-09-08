#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <time.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include "system-calls.h"
#include "../util/linked_list.h"
#include "file_descriptor.h"

// #define F_WRITE 1
// #define F_READ 2
// #define F_APPEND 3
#define F_SEEK_SET 0
#define F_SEEK_CUR 1
#define F_SEEK_END 2


LinkedList* file_list;//Linked list that contains the file descriptors for open files
LinkedList* mode_list;//Linked list that contains the modes for open files
LinkedList* name_list;//Linked list that contains the names for open files

LinkedList* fd_table;//Linked list that contains the file descriptors for open files
int curr_fd = 3; //Current file descriptor

uint16_t *fat;
int BLOCKS_IN_FAT;
int ENTRIES_IN_FAT;
int BLOCKS_IN_DATA;


//helper function for f_write
//takes in a string of characters, a file descirptor number which is the start block of the file in the fat table and the block size of the file we are writing to
int write_to_fat(char* file_data, file_descriptor* fd, int block_size){
    int first_block = fd->start_block;
    int file_offset = fd->offset;
    int file_size = fd->file_size;
    int len_write = strlen(file_data);

        // Write adjusted file size
        unsigned sizeb0 = ((file_size + len_write) & 0xff);
        unsigned sizeb1 = ((file_size + len_write) & 0xff00);
        unsigned sizeb2 = ((file_size + len_write) & 0xff0000) >> 16u;
        unsigned sizeb3 = ((file_size + len_write) & 0xff000000) >> 16u;
        fat[file_offset-2] = sizeb0 | sizeb1;
        fat[file_offset-1] = sizeb2 | sizeb3;

        // Move to place to read first block if it is defined
        // file_offset += 2;
        fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);
        // int first_block = fat[file_offset];

        // Find first open block to write file data
        if (first_block == 0)
        {
          int block_num = 0;
          while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
          {
            block_num++;
          }
          if (block_num == BLOCKS_IN_DATA)
          {
            printf("File system is full. Remove a file and try again.\n");
            return -1;
          }
          first_block = block_num + 1;
        }

        // // Move to place to write type and permissions
        // file_offset += 1;
        // fat[file_offset] = 0x0601;

        // // Move to place to write time
        // file_offset += 1;

        // Move to first block in root directory and write data
        int i = 0;
        int block_num = first_block - 1;
        // SWITCH
        int offset = ENTRIES_IN_FAT + block_size * block_num / 2 + file_size / 2;
        // int offset = file_offset;
        if (file_size % 2 == 1)
        {
          char c1 = file_data[0];
          fat[offset] = c1 << 8 | (fat[offset] & 0xFF);
          i = 1;
          offset++;
        }
        else
        {
          i = 0;
        }
        int j = 0;
        int curr_block = first_block;
        while (i < block_size && j < strlen(file_data))
        {
          // Write data
          if (strlen(file_data) < 2)
          {
            uint16_t c = file_data[i] << 8;
            fat[offset + i] = c;
            i++;
          }
          while (i < strlen(file_data))
          {
            char c1 = file_data[i];
            char c2 = file_data[i + 1];
            uint16_t c = c2 << 8 | c1;
            fat[offset + i / 2] = c;
            i += 2;
          }
          if (i == strlen(file_data) - 1)
          {
            char c1 = file_data[i];
            uint16_t c = c1 << 8 & 0xFF00;
            fat[offset + i / 2] = c;
          }
          j = i;

          // Check if block has overflown. If so, move to next free block
          if (i >= block_size && block_num < BLOCKS_IN_DATA)
          {
            while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
            {
              block_num++;
            }
            if (block_num == BLOCKS_IN_DATA)
            {
              printf("File system is full. Remove a file and try again.\n");
              continue;
            }
            i = 0;

            // Update FAT region to contain new pointers
            fat[curr_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
            curr_block++;
          }
        }

        // Change any remaining pointers in FAT to be undefined if necessary
        fat[curr_block] = 0xFFFF;
        curr_block++;
        while (fat[curr_block] != 0)
        {
          fat[curr_block] = 0x0000;
          curr_block++;
        }
        printf("WRITE TO FAT SUCCESSFUL SIZE OF THING WRITTEN %lu\n", sizeof(file_data));
        printf("WRITE TO FAT SUCCESSFUL STRLEN OF THING WRITTEN %lu\n", strlen(file_data));
        return sizeof(file_data);
}

//Helper function for f_read that takes in a buffer that is the size of the file we are reading,
// a file descriptor number that is the start block of the file number we are reading, and the block size of the file
int read_from_fat(char* buffer, file_descriptor* fd, int block_size) {
    int first_block = fd->start_block;
    printf("READ FROM FAT FIRST BLOCK = %d\n", first_block);
    int file_size = fd->file_size;
    printf("READ FROM FAT FILE SIZE = %d\n", file_size);
    int buffer_offset = 0;

    if (first_block == 0 || file_size == 0) {
        printf("File is empty or does not exist.\n");
        return -1;
    }

    int block_num = first_block - 1;
    int offset = ENTRIES_IN_FAT + block_size * block_num / 2;
    int byte_count = 0;
    int curr_block = first_block;

    while (byte_count < file_size) {
        for (int i = 0; i < block_size && byte_count < file_size; i += 2) {
            char c1 = (fat[offset + i / 2] & 0xFF);
            char c2 = (fat[offset + i / 2] & 0xFF00) >> 8;

            if (byte_count < file_size) {
                buffer[buffer_offset++] = c1;
                byte_count++;
            }

            if (byte_count < file_size && i + 1 < block_size) {
                buffer[buffer_offset++] = c2;
                byte_count++;
            }
        }

        // Move to the next block if necessary
        if (curr_block < BLOCKS_IN_DATA) {
            curr_block = fat[curr_block];
            offset = ENTRIES_IN_FAT + block_size * (curr_block - 1) / 2;
        } else {
            break; // End of file reached
        }
    }

    buffer[buffer_offset] = '\0'; // Null-terminate the buffer
    return buffer_offset; // Return the number of bytes read
}

//gets the node of the correct file descriptor of a file by searching through the file_descriptor linked list for files with the correct name
Node* get_fd_node_by_name(LinkedList* list, char* name){
    Node* curr = list->head;
    while(curr != NULL){
        file_descriptor* fd = (file_descriptor*)curr->data;
        if(strcmp(fd->name, name) == 0){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

//gets the node with the correct file descriptor of a file by searching through the file_descriptor linked list for files with the correct name and mode
Node* get_fd_node_by_name_and_mode(LinkedList* list, char* name, int mode){
    Node* curr = list->head;
    while(curr != NULL){
        file_descriptor* fd = (file_descriptor*)curr->data;
        if(strcmp(fd->name, name) == 0 && fd->perm == mode){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

//gets the the node by using the file discriptor number
Node* get_fd_node_by_fd_num(LinkedList* list, int fd_num){
    Node* curr = list->head;
    while(curr != NULL){
        file_descriptor* fd = (file_descriptor*)curr->data;
        if(fd->fd == fd_num){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

//open a file name fname with the mode mode and return a file descriptor
int f_open(const char *fname, int mode){
    if (fname == NULL){
        printf("File name is NULL\n");
        return -1;
    }
    
    // CHECK IF NAME IS VALID POSIX STANDARD
    char* file_name = malloc((strlen(fname) + 1) * (sizeof(char)));
    strcpy(file_name, fname);
    int i = 0;
    
    do{
        if(file_name[i] >= 65 && file_name[i] <= 90){
            i++;
            continue;
        } else if (file_name[i] >= 97 && file_name[i] <= 122){
            i++;
            continue;
        } else if(file_name[i] >= 48 && file_name[i] <= 57){
            i++;
            continue;
        } else if(file_name[i] == '.' || file_name[i] == '_' || file_name[i] == '-'){
            i++;
            continue;
        } else {
            free(file_name);
            perror("Name not POSIX Standard");
            return -1;
        }
    }while(file_name[i] != '\0');

    Node* fd_node = get_fd_node_by_name(fd_table, file_name);
    if (fd_node == NULL){
        printf("FILE DOESN'T EXIST\n");
        // BUT F_TOUCH NEEDS PERMISSIONS INPUT SO IT CAN TOUCH BASED ON A CERTAIN PERMISSION
        if (mode != F_READ){
            printf("CREATING NEW FILE MODE: %d\n", mode);
            f_touch(file_name);
            Node* touched_node = get_fd_node_by_name_and_mode(fd_table, file_name, mode);
            if (touched_node == NULL){
                printf("COULDN'T FIND TOUCHED NODE\n");
                return -1;
            }
            file_descriptor* touched_fd = (file_descriptor*)touched_node->data;
            return touched_fd->fd;
        }
    } else {
        Node* fd_node2 = get_fd_node_by_name_and_mode(fd_table, file_name, mode);
        if (fd_node2 != NULL){
            file_descriptor* fd_with_matching_name_and_mode = (file_descriptor*)fd_node2->data;
            printf("FILE ALREADY EXISTS\n");
            return fd_with_matching_name_and_mode->fd;
        } else{
            file_descriptor* fd = (file_descriptor*)fd_node->data;
            printf("FILE ALREADY EXISTS BUT WITH DIFFERENT PERMISSIONS\n");
            printf("CREATE NEW FD FOR THE SAME FILE BUT WITH DIFFERENT PERMISSIONS\n");
            file_descriptor* new_fd = fd_create(curr_fd, fd->start_block, fd->offset, mode, fd->name, fd->file_size);
            curr_fd++;
            insert_end(fd_table, new_fd);
            return new_fd->fd;
        }
    }
    return -1;
}
    

//     // Get the block size and number of blocks in the fat region
//       int block_size = fat[0] & 0x00FF;
//       switch (block_size)
//       {
//       case 0:
//         block_size = 256;
//         break;
//       case 1:
//         block_size = 512;
//         break;
//       case 2:
//         block_size = 1024;
//         break;
//       case 3:
//         block_size = 2048;
//         break;
//       case 4:
//         block_size = 4096;
//         break;
//       default:
//         printf("Invalid configuration size. Try Again.\n");
//         return -1;
//       }

//       int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
//       int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
//       int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;


//       // Make sure specified file doesn't already exist
//         bool found_file = false;
//         int start_block = 1;
//         uint16_t root_dir_path = fat[start_block];
//         printf("ROOT_DIR_PATH = %d\n", root_dir_path);
//         int directory_num = 0;
//         int max_directories_num = block_size / 64;
//         while (root_dir_path != 0xFFFF)
//         {
//           while (directory_num < max_directories_num)
//           {
//             char curr_file[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
//               curr_file[j] = c1;
//               curr_file[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(file_name);
//             found_file = strncmp(file_name, curr_file, len) == 0;
//             if (found_file)
//             {
//               break;
//             }
//             directory_num++;
//           }
//           // If we've maxed out this directory, then move to the next listed directory
//           if (directory_num != max_directories_num)
//           {
//             break;
//           }
//           else
//           {
//             start_block = root_dir_path;
//             root_dir_path = fat[start_block];
//           }


//           // Check the final directory to see if the file is there
//         if (root_dir_path == 0xFFFF && !found_file)
//         {
//           directory_num = 0;
//           while (directory_num < max_directories_num)
//           {
//             char curr_file[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
//               curr_file[j] = c1;
//               curr_file[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(file_name);
//             found_file = strncmp(file_name, curr_file, len) == 0;
//             if (found_file)
//             {
//               break;
//             }
//             directory_num++;
//           }
//         }

//         // Check if file already exists
//         if (found_file)
//         {
//           // find file descriptor node whose name is same as one passed in
//           // check if mode is write or append
//           Node* fd_node = get_fd_node_by_name(fd_table, file_name);
//             // file exists but with different permissions OR file doesn't exist?
//             if(fd_node == NULL){
//                 printf("File with name couldn't be found\n");
//                 return -1;
//             }
//             // we have mode, name, and fd
//             printf("CREATING NEW FILE DESCRIPTOR TO AN EXISTING FILE: START_BLOCK = %d\n", start_block);
//             // TODO
//             file_descriptor* new_fd = fd_create(curr_fd, start_block, offset, 0, mode, file_name);
//             insert_end(fd_table, new_fd);
//             curr_fd++;
//             return new_fd->fd;

//         } else {
//             // Scan to find open directories starting in original block
//             while (root_dir_path != 0xFFFF)
//             {
//             while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
//                     (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
//                     directory_num < max_directories_num)
//             {
//                 directory_num++;
//             }

//             // If we've maxed out this directory, then move to the next listed directory
//             if (directory_num != max_directories_num)
//             {
//                 break;
//             }
//             else
//             {
//                 start_block = root_dir_path;
//                 root_dir_path = fat[start_block];
//             }
//             }

//             // Check the final directory to see if there's an empty file
//             if (root_dir_path == 0xFFFF)
//             {
//             directory_num = 0;
//             while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
//                     (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
//                     directory_num < max_directories_num)
//             {
//                 directory_num++;
//             }
//             }

//             // If all directories in all listed blocks are full, make a new block
//             if (directory_num == max_directories_num)
//             {
//             int block_num = 0;
//             while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//             {
//                 block_num++;
//             }
//             if (block_num == BLOCKS_IN_DATA)
//             {
//                 printf("File system is full. Remove a file and try again.\n");
//                 continue;
//             }

//             // Once we've found the next open block, update FAT region
//             block_num++;
//             fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
//             start_block = block_num;
//             fat[block_num] = 0xFFFF;
//             directory_num = 0;
//             }

//             // Move to first open block in root directory and write name
//             int offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
//             int i = 0;
//             if (strlen(file_name) < 2)
//             {
//             uint16_t c = file_name[i] << 8;
//             fat[offset + i] = c;
//             }
//             while (i < strlen(file_name))
//             {
//             char c1 = file_name[i];
//             char c2 = file_name[i + 1];
//             uint16_t c = c2 << 8 | c1;
//             fat[offset + i / 2] = c;
//             i += 2;
//             }
//             if (i == strlen(file_name) - 1)
//             {
//             char c1 = file_name[i];
//             uint16_t c = c1 << 8 & 0xFF00;
//             fat[offset + i / 2] = c;
//             }
//             // Fill in remainders with 0's
//             while (fat[offset + i / 2] != 0 && i < 18)
//             {
//             fat[offset + i / 2] = 0;
//             i += 2;
//             }

//             // Move to place to write first block
//             offset += 18;
//             fat[offset] = 0xFFFF;

//             // Move to place to write type and permissions
//             if(mode == F_WRITE){
//                 offset += 1;
//                 fat[offset] = 0x0701;
//             } else if(mode == F_READ){
//                 offset += 1;
//                 fat[offset] = 0x0401;
//             } else if(mode == F_APPEND){
//                 offset += 1;
//                 fat[offset] = 0x0701;
//             }

//             // Move to place to write time
//             offset += 1;
//             time_t mtime = time(NULL);

//             unsigned b0 = (mtime & 0x00000000000000ff);
//             unsigned b1 = (mtime & 0x000000000000ff00);
//             unsigned b2 = (mtime & 0x0000000000ff0000) >> 16u;
//             unsigned b3 = (mtime & 0x00000000ff000000) >> 16u;
//             unsigned b4 = (mtime & 0x000000ff00000000) >> 32u;
//             unsigned b5 = (mtime & 0x0000ff0000000000) >> 32u;
//             unsigned b6 = (mtime & 0x00ff000000000000) >> 48u;
//             unsigned b7 = (mtime & 0xff00000000000000) >> 48u;

//             fat[offset] = b0 | b1;
//             fat[offset + 1] = b2 | b3;
//             fat[offset + 2] = b4 | b5;
//             fat[offset + 3] = b6 | b7;

//             // TODO
//             file_descriptor* new_fd = fd_create(curr_fd, start_block, 0, mode, file_name);
//             insert_end(fd_table, new_fd);
//             printf("F_OPEN: CREATING NEW FILE DESCRIPTOR: NAME = %s FD = %d START_BLOCK = %d\n", file_name, curr_fd, start_block);

//             curr_fd++;
//             return new_fd->fd;
//             }
//         }

//          // Check the final directory to see if the file is there
//         if (root_dir_path == 0xFFFF && !found_file)
//         {
//             printf("CHECKING FINAL DIRECTORY\n");
//           directory_num = 0;
//           while (directory_num < max_directories_num && !found_file)
//           {
//             char curr_file[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
//               curr_file[j] = c1;
//               curr_file[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(file_name);
//             printf("CURR FILE = %s\n", curr_file);
//             found_file = strncmp(file_name, curr_file, len) == 0;
//             if (found_file)
//             {
//             printf("FILE FOUND\n");
//               break;
//             }
//             directory_num++;
            
//           }
//         }
//         // FILE FOUND
//         if (found_file){
//             int offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
//             printf("ENTRIES_IN_FAT = %d\n", ENTRIES_IN_FAT);
//             printf("DIRECTORY NUM = %d\n", directory_num);
//             printf("BLOCK SIZE = %d\n", block_size);
//             printf("START BLOCK = %d\n", start_block);
//             printf("OFFSET = %X\n", offset);
//             printf("FAT OFFSET %d\n", fat[offset]);
//             // get fd based on name
//             Node* fd_node = get_fd_node_by_name(fd_table, file_name);
//             int fd_num = ((file_descriptor*)fd_node->data)->fd;
//             return fd_num;
//         } else{
//             f_touch(file_name);
//             Node* fd_node = get_fd_node_by_name(fd_table, file_name);
//             int fd_num = ((file_descriptor*)fd_node->data)->fd;
//             return fd_num;
//         }


//     return -1;
// }


//read function that returns number of bytes read
int f_read(int fd, int n, char *buf){
    int num_bytes = 0;
    if(fd == STDIN_FILENO){
        num_bytes = read(fd, buf, n);
    } else {
        Node* fd_node = get_fd_node_by_fd_num(fd_table, fd);
        if (fd_node == NULL){
            return -1;
        }
        // DO CHECK IF 1ST BLOCK IS EMPTY?
        file_descriptor* fd = (file_descriptor*)fd_node->data;
        num_bytes = read_from_fat(buf, fd, 256);
    }
    return num_bytes;
}


//write functin that returns number of bytes written
int f_write(int fd, const char *str, int n){
    char* to_write = malloc((strlen(str) + 1) * (sizeof(char)));
    strcpy(to_write, str);
    int num_bytes = 0;
    int block_size = 0;
    if(fd == STDOUT_FILENO){
        num_bytes = write(fd, str, n);
        return num_bytes;
    } else {
        Node* fd_node = get_fd_node_by_fd_num(fd_table, fd);
        if (fd_node == NULL){
            printf("COULDN'T FIND FD NODE: fd_node is NULL\n");
            return -1;
        }
        // CHECK IF 1ST BLOCK IS EMPTY?
        file_descriptor* fd = (file_descriptor*)fd_node->data;
        if (fd->start_block == 0xFFFF){

            block_size = fat[0] & 0x00FF;
            switch (block_size)
            {
            case 0:
            block_size = 256;
            break;
            case 1:
            block_size = 512;
            break;
            case 2:
            block_size = 1024;
            break;
            case 3:
            block_size = 2048;
            break;
            case 4:
            block_size = 4096;
            break;
            default:
            printf("Invalid configuration size. Try Again.\n");
            return -1;
            }

            int block_num = 0;
            int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
            int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
            int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
            while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
            {
            block_num++;
            }
            if (block_num == BLOCKS_IN_DATA)
            {
            printf("File system is full. Remove a file and try again.\n");
            return -1;
            }
            int first_block = block_num + 1;
            fd->start_block = first_block;
            printf("FIRST BLOCK = %d\n", first_block);
            // START BLOCK UPDATED
        }


        if (fd->perm == F_WRITE || fd->perm == F_APPEND) {
            if (fd->perm == F_APPEND) {
                // Optionally, you can seek to the end of the file for appending.
                f_lseek(fd->fd, 0, SEEK_END);
            }
            // FIX WRITE HERE
            int num_bytes = write_to_fat(to_write, fd, block_size);
            if (num_bytes == -1){
                printf("WRITE TO FAT FAILED\n");
                return -1;
            }
            // num_bytes = write(fd->fd, str, n);
            printf("NUM_BYTES WRITTEN = %d\n", num_bytes);
            fd->file_size += num_bytes;
            return num_bytes;
        } else {
            return -1;
        }
        return num_bytes;
    }
}


//close function that returns 0 when successfully closing a file
int f_close(int fd){
    if(fd == STDIN_FILENO || fd == STDOUT_FILENO){
        close(fd);
        return 0;
    } else {

        Node* fd_node = get_fd_node_by_fd_num(fd_table, fd);
        if (fd_node == NULL){
            return -1;
        }
        delete_node(fd_table, fd_node);
        return 0;
    }
}


//unlink function that deletes a file if there are no other process using it. Waits if multiple process are using the file.
int f_unlink(const char *fname){
          // Check that there's a file system mounted
      if (fat == NULL)
      {
        printf("No file system mounted. Try again.\n");
        return EXIT_FAILURE;
      }

      char *file_name = malloc((strlen(fname) + 1) * (sizeof(char)));
        strcpy(file_name, fname);

        // Get the block size and number of blocks in the fat region
        int block_size = fat[0] & 0x00FF;
        switch (block_size)
        {
        case 0:
          block_size = 256;
          break;
        case 1:
          block_size = 512;
          break;
        case 2:
          block_size = 1024;
          break;
        case 3:
          block_size = 2048;
          break;
        case 4:
          block_size = 4096;
          break;
        default:
          printf("Invalid configuration size. Try Again.\n");
          return EXIT_FAILURE;
        }

        // Search for the specified file starting in the root directory
        bool found_file = false;
        int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
        int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
        int start_block = 1;
        uint16_t root_dir_path = fat[start_block];
        int directory_num = 0;
        int max_directories_num = block_size / 64;
        while (root_dir_path != 0xFFFF)
        {
          while (directory_num < max_directories_num)
          {
            char curr_file[32];
            int i = 0;
            int j = 0;
            while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
                   fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
            {
              char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
              char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
              curr_file[j] = c1;
              curr_file[j + 1] = c2;
              i++;
              j += 2;
            }
            int len = strlen(file_name);
            found_file = strncmp(file_name, curr_file, len) == 0;
            if (found_file)
            {
              break;
            }
            directory_num++;
          }

          // If we've maxed out this directory, then move to the next listed directory
          if (directory_num != max_directories_num)
          {
            break;
          }
          else
          {
            start_block = root_dir_path;
            root_dir_path = fat[start_block];
          }
        }

        // Check the final directory to see if the file is there
        if (root_dir_path == 0xFFFF && !found_file)
        {
          directory_num = 0;
          while (directory_num < max_directories_num)
          {
            char curr_file[32];
            int i = 0;
            int j = 0;
            while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
                   fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
            {
              char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
              char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
              curr_file[j] = c1;
              curr_file[j + 1] = c2;
              i++;
              j += 2;
            }
            int len = strlen(file_name);
            found_file = strncmp(file_name, curr_file, len) == 0;
            if (found_file)
            {
              break;
            }
            directory_num++;
          }
        }

        // Check if file isn't found
        if (!found_file)
        {
          printf("'%s' not found in the directory.\n", file_name);
        }
        else
        {
          // Move to correct block in root directory and write name
          int offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
          fat[offset] = (fat[offset] & 0xFF00) | 0x0001;
        }
        int name = get_index(name_list, file_name);
        // Node* fd = NULL;
        while(name != -1) {
            Node* fd_node = get_fd_node_by_name(fd_table, file_name);
            f_close(*(int*)(fd_node->data));
            name = get_index(name_list, file_name);
        }
        return EXIT_SUCCESS;
}


//lseek function that return the current offset of the file.
off_t f_lseek(int fd, int offset, int whence){
    off_t off;
    if(fd == STDIN_FILENO){
        if(whence == F_SEEK_SET){
            off = lseek(fd, offset, SEEK_SET);
            if(off == -1){
                return -1;
            }
        } else if(whence == F_SEEK_CUR) {
            off = lseek(fd, offset, SEEK_CUR);
            if(off == -1){
                return -1;
            }
        } else if(whence == F_SEEK_END) {
            off = lseek(fd, offset, SEEK_END);
            if(off == -1){
                return -1;
            }
        } else {
            return -1;
        }
    } else {
        Node* fd_node = get_fd_node_by_fd_num(fd_table, fd);
        if (fd_node == NULL){
            return -1;
        }
        if(whence == F_SEEK_SET){
            off = lseek(fd, offset, SEEK_SET);
            if(off == -1){
                return -1;
            }
        } else if(whence == F_SEEK_CUR) {
            off = lseek(fd, offset, SEEK_CUR);
            if(off == -1){
                return -1;
            }
        } else if(whence == F_SEEK_END) {
            off = lseek(fd, offset, SEEK_END);
            if(off == -1){
                return -1;
            }
        } else {
            return -1;
        }
    }
    return off;
}



//lseek function that return the current offset of the file.
void f_ls(const char *filename){
// Get the block size and number of blocks in the fat region
        int block_size = fat[0] & 0x00FF;
        printf("Block size: %d\n", block_size);
        switch (block_size)
        {
        case 0:
            block_size = 256;
            break;
        case 1:
            block_size = 512;
            break;
        case 2:
            block_size = 1024;
            break;
        case 3:
            block_size = 2048;
            break;
        case 4:
            block_size = 4096;
            break;
        default:
            printf("Invalid configuration size. Try Again.\n");
            return;
        }

        // Open the file system
        
        int fd = open(filename, O_RDONLY);
        if (fd == -1)
        {
            printf("Invalid file name. Try again.\n");
            return;
        }

        // Search for the specified file starting in the root directory
        int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
        int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
        int start_block = 1;
        uint16_t root_dir_path = fat[start_block];
        int directory_num = 0;
        int max_directories_num = block_size / 64;
        while (root_dir_path != 0xFFFF)
        {
        while (directory_num < max_directories_num)
        {
            if (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] != 0 &&
                (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1)
            {
            // Move to first open block in root directory and read name
            off_t offset = lseek(fd, ENTRIES_IN_FAT * 2 + 64 * directory_num + block_size * (start_block - 1), SEEK_SET);
            char file_name[32];
            if (pread(fd, file_name, sizeof(file_name), offset) == -1)
            {
                printf("Error reading file name during ls. Try again.\n");
                return;
            }

            // Move to place to read file size
            offset = lseek(fd, 32, SEEK_CUR);
            uint32_t file_size[1];
            if (pread(fd, &file_size, sizeof(file_size), offset) == -1)
            {
                printf("Error reading file size during ls. Try again.\n");
                return;
            }

            // Move to place to read first block
            offset = lseek(fd, 4, SEEK_CUR);
            uint16_t firstBlock[1];
            if (pread(fd, firstBlock, sizeof(firstBlock), offset) == -1)
            {
                printf("Error reading first block during ls. Try again.\n");
                return;
            }

            // Move to place to read permissions
            offset = lseek(fd, 3, SEEK_CUR);
            uint8_t perm[1];
            char *perm_string;
            if (pread(fd, perm, sizeof(perm), offset) == -1)
            {
                printf("Error reading file perm during ls. Try again.\n");
                return;
            }
            switch (perm[0])
            {
            case 0x00:
                perm_string = "N/A";
                break;
            case 0x02:
                perm_string = "-w";
                break;
            case 0x04:
                perm_string = "-r";
                break;
            case 0x05:
                perm_string = "-rx";
                break;
            case 0x06:
                perm_string = "-rw";
                break;
            case 0x07:
                perm_string = "-rwx";
                break;
            }

            // Move to place to read time
            offset = lseek(fd, 1, SEEK_CUR);
            time_t mtime;
            if (pread(fd, &mtime, sizeof(mtime), offset) == -1)
            {
                printf("Error reading file time during ls. Try again.\n");
                return;
            }
            struct tm tm = *localtime(&mtime);

            // Print file to terminal
            if (file_size[0] == 0)
            {
                printf("%s %d %02d-%02d-%d %02d:%02d:%02d %s\n", perm_string, (unsigned)file_size[0], tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, file_name);
            }
            else
            {
                printf("%d %s %d %02d-%02d-%d %02d:%02d:%02d %s\n", (unsigned)firstBlock[0], perm_string, (unsigned)file_size[0], tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, file_name);
            }
            }
            directory_num++;
        }

        // If we've maxed out this directory, then move to the next listed directory
        if (directory_num == max_directories_num)
        {
            start_block = root_dir_path;
            root_dir_path = fat[start_block];
            directory_num = 0;
        }
        }

        if (root_dir_path == 0xFFFF)
        {
        directory_num = 0;
        while (directory_num < max_directories_num)
        {
            if ((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
                (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1)
            {
            // Move to first open block in root directory and read name
            off_t offset = lseek(fd, ENTRIES_IN_FAT * 2 + 64 * directory_num + block_size * (start_block - 1), SEEK_SET);
            char file_name[32];
            if (pread(fd, file_name, sizeof(file_name), offset) == -1)
            {
                printf("Error reading file name during ls. Try again.\n");
                return;
            }

            // Move to place to read file size
            offset = lseek(fd, 32, SEEK_CUR);
            uint32_t file_size[1];
            if (pread(fd, &file_size, sizeof(file_size), offset) == -1)
            {
                printf("Error reading file size during ls. Try again.\n");
                return;
            }

            // Move to place to read first block
            offset = lseek(fd, 4, SEEK_CUR);
            uint16_t firstBlock[1];
            if (pread(fd, firstBlock, sizeof(firstBlock), offset) == -1)
            {
                printf("Error reading first block during ls. Try again.\n");
                return;
            }

            // Move to place to read permissions
            offset = lseek(fd, 3, SEEK_CUR);
            uint8_t perm[1];
            char *perm_string;
            if (pread(fd, perm, sizeof(perm), offset) == -1)
            {
                printf("Error reading file perm during ls. Try again.\n");
                return;
            }
            switch (perm[0])
            {
            case 0x00:
                perm_string = "N/A";
                break;
            case 0x02:
                perm_string = "-w";
                break;
            case 0x04:
                perm_string = "-r";
                break;
            case 0x05:
                perm_string = "-rx";
                break;
            case 0x06:
                perm_string = "-rw";
                break;
            case 0x07:
                perm_string = "-rwx";
                break;
            }

            // Move to place to read time
            offset = lseek(fd, 1, SEEK_CUR);
            time_t mtime;
            if (pread(fd, &mtime, sizeof(mtime), offset) == -1)
            {
                printf("Error reading file time during ls. Try again.\n");
                return;
            }
            struct tm tm = *localtime(&mtime);

            // Print file to terminal
            if (file_size[0] == 0)
            {
                printf("%s %d %02d-%02d-%d %02d:%02d:%02d %s\n", perm_string, (unsigned)file_size[0], tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, file_name);
            }
            else
            {
                printf("%d %s %d %02d-%02d-%d %02d:%02d:%02d %s\n", (unsigned)firstBlock[0], perm_string, (unsigned)file_size[0], tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, file_name);
            }
            }
            directory_num++;
        }
    }
}

//helper function that creates empty files in write mode in the fat table
int f_touch(char* file_name){
    // Get the block size and number of blocks in the fat region
      int block_size = fat[0] & 0x00FF;
      switch (block_size)
      {
      case 0:
        block_size = 256;
        break;
      case 1:
        block_size = 512;
        break;
      case 2:
        block_size = 1024;
        break;
      case 3:
        block_size = 2048;
        break;
      case 4:
        block_size = 4096;
        break;
      default:
        printf("Invalid configuration size. Try Again.\n");
        return -1;
      }

      int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
      int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
      int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;

        // Make sure specified file doesn't already exist
        bool found_file = false;
        int start_block = 1;
        uint16_t root_dir_path = fat[start_block];
        int directory_num = 0;
        int max_directories_num = block_size / 64;
        while (root_dir_path != 0xFFFF)
        {
          while (directory_num < max_directories_num)
          {
            char curr_file[32];
            int i = 0;
            int j = 0;
            while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
                   fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
            {
              char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
              char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
              curr_file[j] = c1;
              curr_file[j + 1] = c2;
              i++;
              j += 2;
            }
            int len = strlen(file_name);
            found_file = strncmp(file_name, curr_file, len) == 0;
            if (found_file)
            {
              break;
            }
            directory_num++;
          }

          // If we've maxed out this directory, then move to the next listed directory
          if (directory_num != max_directories_num)
          {
            break;
          }
          else
          {
            start_block = root_dir_path;
            root_dir_path = fat[start_block];
          }
        }

        // Check the final directory to see if the file is there
        if (root_dir_path == 0xFFFF && !found_file)
        {
          directory_num = 0;
          while (directory_num < max_directories_num)
          {
            char curr_file[32];
            int i = 0;
            int j = 0;
            while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
                   fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
            {
              char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
              char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
              curr_file[j] = c1;
              curr_file[j + 1] = c2;
              i++;
              j += 2;
            }
            int len = strlen(file_name);
            found_file = strncmp(file_name, curr_file, len) == 0;
            if (found_file)
            {
              break;
            }
            directory_num++;
          }
        }

        // Check if file already exists
        if (found_file)
        {
          printf("'%s' already exists in the directory. Try again.\n", file_name);
          return -1;
        }

        // Scan to find open directories starting in original block
        while (root_dir_path != 0xFFFF)
        {
          while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
                  (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
                 directory_num < max_directories_num)
          {
            directory_num++;
          }

          // If we've maxed out this directory, then move to the next listed directory
          if (directory_num != max_directories_num)
          {
            break;
          }
          else
          {
            start_block = root_dir_path;
            root_dir_path = fat[start_block];
          }
        }

        // Check the final directory to see if there's an empty file
        if (root_dir_path == 0xFFFF)
        {
          directory_num = 0;
          while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
                  (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
                 directory_num < max_directories_num)
          {
            directory_num++;
          }
        }

        // If all directories in all listed blocks are full, make a new block
        if (directory_num == max_directories_num)
        {
          int block_num = 0;
          while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
          {
            block_num++;
          }
          if (block_num == BLOCKS_IN_DATA)
          {
            printf("File system is full. Remove a file and try again.\n");
            return -1;
          }

          // Once we've found the next open block, update FAT region
          block_num++;
          fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
          start_block = block_num;
          fat[block_num] = 0xFFFF;
          directory_num = 0;
        }

        // Move to first open block in root directory and write name
        int offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
        int i = 0;
        if (strlen(file_name) < 2)
        {
          uint16_t c = file_name[i] << 8;
          fat[offset + i] = c;
        }
        while (i < strlen(file_name))
        {
          char c1 = file_name[i];
          char c2 = file_name[i + 1];
          uint16_t c = c2 << 8 | c1;
          fat[offset + i / 2] = c;
          i += 2;
        }
        if (i == strlen(file_name) - 1)
        {
          char c1 = file_name[i];
          uint16_t c = c1 << 8 & 0xFF00;
          fat[offset + i / 2] = c;
        }
        // Fill in remainders with 0's
        while (fat[offset + i / 2] != 0 && i < 18)
        {
          fat[offset + i / 2] = 0;
          i += 2;
        }

        // Move to place to write first block
        offset += 18;
        int fd_offset = offset;
        fat[offset] = 0xFFFF;

        // Move to place to write type and permissions
        offset += 1;
        fat[offset] = 0x0601;

        // Move to place to write time
        offset += 1;
        time_t mtime = time(NULL);

        unsigned b0 = (mtime & 0x00000000000000ff);
        unsigned b1 = (mtime & 0x000000000000ff00);
        unsigned b2 = (mtime & 0x0000000000ff0000) >> 16u;
        unsigned b3 = (mtime & 0x00000000ff000000) >> 16u;
        unsigned b4 = (mtime & 0x000000ff00000000) >> 32u;
        unsigned b5 = (mtime & 0x0000ff0000000000) >> 32u;
        unsigned b6 = (mtime & 0x00ff000000000000) >> 48u;
        unsigned b7 = (mtime & 0xff00000000000000) >> 48u;

        fat[offset] = b0 | b1;
        fat[offset + 1] = b2 | b3;
        fat[offset + 2] = b4 | b5;
        fat[offset + 3] = b6 | b7;

        char* file_name_pointer = malloc((strlen(file_name) + 1) * (sizeof(char)));
        strcpy(file_name_pointer, file_name);

        file_descriptor* new_fd = fd_create(curr_fd, 0xFFFF, fd_offset, F_WRITE, file_name_pointer, 0);
        curr_fd++;
        insert_end(fd_table, new_fd);
        // insert_end(file_list, new_fd_num);
        // insert_end(mode_list, mode);
        // insert_end(name_list, file_name);
        printf("File '%s' created successfully.\n", file_name);
        return 0;
}

//wrapper function that rename src to dest
bool f_move(char** files){
    // Check that there's a file system mounted
      if (fat == NULL)
      {
        printf("No file system mounted. Try again.\n");
        return false;
      }

    char* source_name = files[0];
    char* dest_name = files[1];

      // Get the block size and number of blocks in the fat region
      int block_size = fat[0] & 0x00FF;
      switch (block_size)
      {
      case 0:
        block_size = 256;
        break;
      case 1:
        block_size = 512;
        break;
      case 2:
        block_size = 1024;
        break;
      case 3:
        block_size = 2048;
        break;
      case 4:
        block_size = 4096;
        break;
      default:
        printf("Invalid configuration size. Try Again.\n");
        return false;
      }

      // Search for the specified file starting in the root directory
      bool found_file = false;
      int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
      int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
      int start_block = 1;
      uint16_t root_dir_path = fat[start_block];
      int directory_num = 0;
      int max_directories_num = block_size / 64;
      while (root_dir_path != 0xFFFF)
      {
        while (directory_num < max_directories_num)
        {
          char file_name[32];
          int i = 0;
          int j = 0;
          while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
                 fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
          {
            char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
            char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
            file_name[j] = c1;
            file_name[j + 1] = c2;
            i++;
            j += 2;
          }
          int len = strlen(source_name);
          found_file = strncmp(file_name, source_name, len) == 0;
          if (found_file)
          {
            break;
          }
          directory_num++;
        }

        // If we've maxed out this directory, then move to the next listed directory
        if (directory_num != max_directories_num)
        {
          break;
        }
        else
        {
          start_block = root_dir_path;
          root_dir_path = fat[start_block];
        }
      }

      // Check the final directory to see if the file is there
      if (root_dir_path == 0xFFFF && !found_file)
      {
        directory_num = 0;
        while (directory_num < max_directories_num)
        {
          char file_name[32];
          int i = 0;
          int j = 0;
          while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
                 fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
          {
            char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
            char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
            file_name[j] = c1;
            file_name[j + 1] = c2;
            i++;
            j += 2;
          }
          int len = strlen(source_name);
          found_file = strncmp(file_name, source_name, len) == 0;
          if (found_file)
          {
            break;
          }
          directory_num++;
        }
      }

      // Check if file isn't found
      if (!found_file)
      {
        printf("'%s' not found in the directory. Try again.\n", source_name);
        return false;
      }

      // Move to correct block in root directory and write name
      int offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
      int i = 0;
      if (strlen(dest_name) < 2)
      {
        uint16_t c = dest_name[i] << 8;
        fat[offset + i] = c;
      }
      while (i < strlen(dest_name))
      {
        char c1 = dest_name[i];
        char c2 = dest_name[i + 1];
        uint16_t c = c2 << 8 | c1;
        fat[offset + i / 2] = c;
        i += 2;
      }
      if (i == strlen(dest_name) - 1)
      {
        char c1 = dest_name[i];
        uint16_t c = c1 << 8 & 0xFF00;
        fat[offset + i / 2] = c;
      }
      // Fill in remainders with 0's
      while (fat[offset + i / 2] != 0 && i < 16)
      {
        fat[offset + i / 2] = 0;
        i += 2;
      }

      // Move to place to write time
      offset += 20;
      time_t mtime = time(NULL);

      unsigned b0 = (mtime & 0x00000000000000ff);
      unsigned b1 = (mtime & 0x000000000000ff00);
      unsigned b2 = (mtime & 0x0000000000ff0000) >> 16u;
      unsigned b3 = (mtime & 0x00000000ff000000) >> 16u;
      unsigned b4 = (mtime & 0x000000ff00000000) >> 32u;
      unsigned b5 = (mtime & 0x0000ff0000000000) >> 32u;
      unsigned b6 = (mtime & 0x00ff000000000000) >> 48u;
      unsigned b7 = (mtime & 0xff00000000000000) >> 48u;

      fat[offset] = b0 | b1;
      fat[offset + 1] = b2 | b3;
      fat[offset + 2] = b4 | b5;
      fat[offset + 3] = b6 | b7;

      return true;
}

//wrapper function that changes the permissions of a file
void f_chmod(char** args) {
    // Check that there's a file system mounted
      if (fat == NULL)
      {
        printf("No file system mounted. Try again.\n");
        return;
      }

      char *new_permissions = args[0];

      char *file_name = args[1];

      

      // Check if new permission is adding, setting, or removing
      bool is_add = false;
      bool is_remove = false;
      bool is_set = false;
      switch (new_permissions[0])
      {
      case '+':
        is_add = true;
        break;
      case '=':
        is_set = true;
        break;
      case '-':
        is_remove = true;
        break;
      default:
        printf("Invalid permission. Try again.\n");
        return;
      }


      // Get the block size and number of blocks in the fat region
      int block_size = fat[0] & 0x00FF;
      switch (block_size)
      {
      case 0:
        block_size = 256;
        break;
      case 1:
        block_size = 512;
        break;
      case 2:
        block_size = 1024;
        break;
      case 3:
        block_size = 2048;
        break;
      case 4:
        block_size = 4096;
        break;
      default:
        printf("Invalid configuration size. Try Again.\n");
        return;
      }

      // Search for the specified file starting in the root directory
      bool found_file = false;
      int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
      int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
      int start_block = 1;
      uint16_t root_dir_path = fat[start_block];
      int directory_num = 0;
      int max_directories_num = block_size / 64;
      while (root_dir_path != 0xFFFF)
      {
        while (directory_num < max_directories_num)
        {
          char curr_file[32];
          int i = 0;
          int j = 0;
          while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
                 fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
          {
            char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
            char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
            curr_file[j] = c1;
            curr_file[j + 1] = c2;
            i++;
            j += 2;
          }
          int len = strlen(file_name);
          found_file = strncmp(file_name, curr_file, len) == 0;
          if (found_file)
          {
            break;
          }
          directory_num++;
        }

        // If we've maxed out this directory, then move to the next listed directory
        if (directory_num != max_directories_num)
        {
          break;
        }
        else
        {
          start_block = root_dir_path;
          root_dir_path = fat[start_block];
        }
      }

      // Check the final directory to see if the file is there
      if (root_dir_path == 0xFFFF && !found_file)
      {
        directory_num = 0;
        while (directory_num < max_directories_num)
        {
          char curr_file[32];
          int i = 0;
          int j = 0;
          while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
                 fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
          {
            char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
            char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
            curr_file[j] = c1;
            curr_file[j + 1] = c2;
            i++;
            j += 2;
          }
          int len = strlen(file_name);
          found_file = strncmp(file_name, curr_file, len) == 0;
          if (found_file)
          {
            break;
          }
          directory_num++;
        }
      }

      // Check if file isn't found
      if (!found_file)
      {
        printf("'%s' not found in the directory. Try again.\n", file_name);
        return;
      }

      // Move to correct block in root directory and write name
      int offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + 19;
      uint8_t curr_permissions = (fat[offset] & 0xFF00) >> 8;

      // Determine what updated permissions are
      new_permissions = new_permissions + 1;
      uint8_t new_permission_bytes = 0x00;
      if (is_set)
      {
        if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
        {
          new_permission_bytes = 0x02;
        }
        if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
        {
          new_permission_bytes = 0x04;
        }
        if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0)
        {
          new_permission_bytes = 0x05;
        }
        if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0)
        {
          new_permission_bytes = 0x06;
        }
        else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0)
        {
          new_permission_bytes = 0x07;
        }
        else
        {
          printf("Invalid permissions. Try again.\n");
          return;
        }
      }
      else if (is_add)
      {
        switch (curr_permissions)
        {

          // Current permissions: N/A
        case 0x00:
          if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x04;
          }
          else if (strncmp(new_permissions, "x", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x05;
          }
          else if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0 || strncmp(new_permissions, "wr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x06;
          }
          else if (strncmp(new_permissions, "wx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "rxw", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wxr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wrx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xwr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xrw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else
          {
            printf("Invalid permissions. Try again.\n");
            return;
          }
          break;

          // Current permissions: w
        case 0x02:
          if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x06;
          }
          else if (strncmp(new_permissions, "x", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0 || strncmp(new_permissions, "wr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x06;
          }
          else if (strncmp(new_permissions, "wx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "rxw", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wxr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wrx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xwr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xrw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else
          {
            printf("Invalid permissions. Try again.\n");
            return;
          }
          break;

          // Current permissions: r
        case 0x04:
          if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x06;
          }
          else if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x04;
          }
          else if (strncmp(new_permissions, "x", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x05;
          }
          else if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x05;
          }
          else if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0 || strncmp(new_permissions, "wr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x06;
          }
          else if (strncmp(new_permissions, "wx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "rxw", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wxr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wrx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xwr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xrw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else
          {
            printf("Invalid permissions. Try again.\n");
            return;
          }
          break;

          // Current permissions: rx
        case 0x05:
          if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x05;
          }
          else if (strncmp(new_permissions, "x", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x05;
          }
          else if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x05;
          }
          else if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0 || strncmp(new_permissions, "wr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else if (strncmp(new_permissions, "wx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "rxw", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wxr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wrx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xwr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xrw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else
          {
            printf("Invalid permissions. Try again.\n");
            return;
          }
          break;

          // Current permissions: rw
        case 0x06:
          if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x06;
          }
          else if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x06;
          }
          else if (strncmp(new_permissions, "x", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0 || strncmp(new_permissions, "wr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x06;
          }
          else if (strncmp(new_permissions, "wx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "rxw", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wxr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wrx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xwr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xrw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x07;
          }
          else
          {
            printf("Invalid permissions. Try again.\n");
            return;
          }
          break;

        case 0x07:
          new_permission_bytes = curr_permissions;
          break;
        }
      }
      else if (is_remove)
      {
        switch (curr_permissions)
        {

          // Current permissions: N/A
        case 0x00:
          new_permission_bytes = curr_permissions;
          break;

          // Current permissions: w
        case 0x02:
          if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "x", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0 || strncmp(new_permissions, "wr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "wx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "rxw", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wxr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wrx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xwr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xrw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else
          {
            printf("Invalid permissions. Try again.\n");
            return;
          }
          break;

          // Current permissions: r
        case 0x04:
          if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x04;
          }
          else if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "x", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x04;
          }
          else if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0 || strncmp(new_permissions, "wr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "wx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x04;
          }
          else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "rxw", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wxr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wrx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xwr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xrw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else
          {
            printf("Invalid permissions. Try again.\n");
            return;
          }
          break;

          // Current permissions: rx
        case 0x05:
          if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x05;
          }
          else if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "x", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x04;
          }
          else if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0 || strncmp(new_permissions, "wr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "wx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x04;
          }
          else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "rxw", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wxr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wrx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xwr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xrw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else
          {
            printf("Invalid permissions. Try again.\n");
            return;
          }
          break;

          // Current permissions: rw
        case 0x06:
          if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x04;
          }
          else if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "x", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x06;
          }
          else if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0 || strncmp(new_permissions, "wr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "wx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x04;
          }
          else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "rxw", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wxr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wrx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xwr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xrw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else
          {
            printf("Invalid permissions. Try again.\n");
            return;
          }
          break;

        case 0x07:
          if (strncmp(new_permissions, "w", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x05;
          }
          else if (strncmp(new_permissions, "r", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "x", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x06;
          }
          else if (strncmp(new_permissions, "rx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x02;
          }
          else if (strncmp(new_permissions, "rw", strlen(new_permissions)) == 0 || strncmp(new_permissions, "wr", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else if (strncmp(new_permissions, "wx", strlen(new_permissions)) == 0 || strncmp(new_permissions, "xw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x04;
          }
          else if (strncmp(new_permissions, "rwx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "rxw", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wxr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "wrx", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xwr", strlen(new_permissions)) == 0 ||
                   strncmp(new_permissions, "xrw", strlen(new_permissions)) == 0)
          {
            new_permission_bytes = 0x00;
          }
          else
          {
            printf("Invalid permissions. Try again.\n");
            return;
          }
          break;
        }
      }

      // Update permissions data in FAT table
      fat[offset] = (new_permission_bytes << 8) | (fat[offset] & 0x00FF);
    }

// void f_cp(char** files){
//     char *arg1 = files[0];
//       char *arg2 = files[1];

//       // cp -h SOURCE DEST (source on host to dest on fat)
//       if (strncmp(arg1, "-h", strlen(arg1)) == 0)
//       {
//         char *source_file = files[0];
//         char *dest_name = files[1];

//         // Open the file from the host system
//         FILE *source_fd = fopen(source_file, "r");
//         if (source_fd == NULL)
//         {
//           printf("Invalid source file name. Try again.\n");
//           return;
//         }

//         // Get the length of the file
//         long file_size;
//         fseek(source_fd, 0L, SEEK_END);
//         file_size = ftell(source_fd);
//         fseek(source_fd, 0L, SEEK_SET);

//         // Read data from host file
//         char *source_data = malloc(sizeof(uint8_t) * file_size + 1);
//         fgets(source_data, file_size + 1, source_fd);

//         // Get the block size and number of blocks in the fat region
//         int block_size = fat[0] & 0x00FF;
//         switch (block_size)
//         {
//         case 0:
//           block_size = 256;
//           break;
//         case 1:
//           block_size = 512;
//           break;
//         case 2:
//           block_size = 1024;
//           break;
//         case 3:
//           block_size = 2048;
//           break;
//         case 4:
//           block_size = 4096;
//           break;
//         default:
//           printf("Invalid configuration size. Try Again.\n");
//           return;
//         }

//         // Check if file already exists
//         int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
//         int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
//         int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
//         bool found_file = false;
//         int start_block = 1;
//         uint16_t root_dir_path = fat[start_block];
//         int directory_num = 0;
//         int max_directories_num = block_size / 64;
//         while (root_dir_path != 0xFFFF)
//         {
//           while (directory_num < max_directories_num)
//           {
//             char curr_file[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
//               curr_file[j] = c1;
//               curr_file[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(dest_name);
//             found_file = strncmp(dest_name, curr_file, len) == 0;
//             if (found_file)
//             {
//               break;
//             }
//             directory_num++;
//           }

//           // If we've maxed out this directory, then move to the next listed directory
//           if (directory_num != max_directories_num)
//           {
//             break;
//           }
//           else
//           {
//             start_block = root_dir_path;
//             root_dir_path = fat[start_block];
//           }
//         }

//         // Check the final directory to see if the file is there
//         if (root_dir_path == 0xFFFF && !found_file)
//         {
//           directory_num = 0;
//           while (directory_num < max_directories_num)
//           {
//             char curr_file[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
//               curr_file[j] = c1;
//               curr_file[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(dest_name);
//             found_file = strncmp(dest_name, curr_file, len) == 0;
//             if (found_file)
//             {
//               break;
//             }
//             directory_num++;
//           }
//         }

//         // Check if file already exists
//         if (!found_file)
//         {
//           // Scan to find open directories starting in original block
//           while (root_dir_path != 0xFFFF)
//           {
//             while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
//                     (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
//                    directory_num < max_directories_num)
//             {
//               directory_num++;
//             }

//             // If we've maxed out this directory, then move to the next listed directory
//             if (directory_num != max_directories_num)
//             {
//               break;
//             }
//             else
//             {
//               start_block = root_dir_path;
//               root_dir_path = fat[start_block];
//             }
//           }

//           // Check the final directory to see if there's an empty file
//           if (root_dir_path == 0xFFFF)
//           {
//             directory_num = 0;
//             while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
//                     (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
//                    directory_num < max_directories_num)
//             {
//               directory_num++;
//             }
//           }

//           // If all directories in all listed blocks are full, make a new block
//           if (directory_num == max_directories_num)
//           {
//             int block_num = 0;
//             while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//             {
//               block_num++;
//             }
//             if (block_num == BLOCKS_IN_DATA)
//             {
//               printf("File system is full. Remove a file and try again.\n");
//               continue;
//             }

//             // Once we've found the next open block, update FAT region
//             block_num++;
//             fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
//             start_block = block_num;
//             fat[block_num] = 0xFFFF;
//             directory_num = 0;
//           }
//         }

//         // Move to first open block in root directory and write name
//         int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
//         int i = 0;
//         if (strlen(dest_name) < 2)
//         {
//           uint16_t c = dest_name[i] << 8;
//           fat[file_offset + i] = c;
//         }
//         while (i < strlen(dest_name))
//         {
//           char c1 = dest_name[i];
//           char c2 = dest_name[i + 1];
//           uint16_t c = c2 << 8 | c1;
//           fat[file_offset + i / 2] = c;
//           i += 2;
//         }
//         if (i == strlen(dest_name) - 1)
//         {
//           char c1 = dest_name[i];
//           uint16_t c = c1 << 8 & 0xFF00;
//           fat[file_offset + i / 2] = c;
//         }
//         // Fill in remainders with 0's
//         while (fat[file_offset + i / 2] != 0 && i < 18)
//         {
//           fat[file_offset + i / 2] = 0;
//           i += 2;
//         }

//         // Move to place to write file size
//         file_offset += 16;
//         unsigned b0 = (file_size & 0xff);
//         unsigned b1 = (file_size & 0xff00);
//         unsigned b2 = (file_size & 0xff0000) >> 16u;
//         unsigned b3 = (file_size & 0xff000000) >> 16u;
//         fat[file_offset] = b0 | b1;
//         fat[file_offset + 1] = b2 | b3;

//         // Find first open block to write file data
//         int block_num = 0;
//         while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//         {
//           block_num++;
//         }
//         if (block_num == BLOCKS_IN_DATA)
//         {
//           printf("File system is full. Remove a file and try again.\n");
//           continue;
//         }
//         int first_block = block_num + 1;

//         // Move to place to write first block
//         file_offset += 2;
//         fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);

//         // Move to place to write type and permissions
//         file_offset += 1;
//         fat[file_offset] = 0x0601;

//         // Move to place to write time
//         file_offset += 1;
//         time_t mtime = time(NULL);

//         unsigned time_b0 = (mtime & 0x00000000000000ff);
//         unsigned time_b1 = (mtime & 0x000000000000ff00);
//         unsigned time_b2 = (mtime & 0x0000000000ff0000) >> 16u;
//         unsigned time_b3 = (mtime & 0x00000000ff000000) >> 16u;
//         unsigned time_b4 = (mtime & 0x000000ff00000000) >> 32u;
//         unsigned time_b5 = (mtime & 0x0000ff0000000000) >> 32u;
//         unsigned time_b6 = (mtime & 0x00ff000000000000) >> 48u;
//         unsigned time_b7 = (mtime & 0xff00000000000000) >> 48u;

//         fat[file_offset] = time_b0 | time_b1;
//         fat[file_offset + 1] = time_b2 | time_b3;
//         fat[file_offset + 2] = time_b4 | time_b5;
//         fat[file_offset + 3] = time_b6 | time_b7;

//         free(dest_name);

//         // Move to first open block in root directory and write data
//         block_num = first_block - 1;
//         int offset = ENTRIES_IN_FAT + block_size * block_num / 2;
//         i = 0;
//         int j = 0;
//         int curr_block = first_block;
//         while (i < block_size && j < strlen(source_data))
//         {
//           // Write data
//           if (strlen(source_data) < 2)
//           {
//             uint16_t c = source_data[i] << 8;
//             fat[offset + i] = c;
//           }
//           while (i < strlen(source_data))
//           {
//             char c1 = source_data[i];
//             char c2 = source_data[i + 1];
//             uint16_t c = c2 << 8 | c1;
//             fat[offset + i / 2] = c;
//             i += 2;
//           }
//           if (i == strlen(source_data) - 1)
//           {
//             char c1 = source_data[i];
//             uint16_t c = c1 << 8 & 0xFF00;
//             fat[offset + i / 2] = c;
//           }
//           j = i;

//           // Check if block has overflown. If so, move to next free block
//           if (i >= block_size && block_num < BLOCKS_IN_DATA)
//           {
//             while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//             {
//               block_num++;
//             }
//             if (block_num == BLOCKS_IN_DATA)
//             {
//               printf("File system is full. Remove a file and try again.\n");
//               continue;
//             }
//             i = 0;

//             // Update FAT region to contain new pointers
//             fat[curr_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
//             curr_block++;
//           }
//         }
//         // Update FAT region to contain new pointers
//         fat[curr_block] = 0xFFFF;
//       }

//       // cp SOURCE -h DEST (source on fat to dest on HOST)
//       else if (strncmp(arg2, "-h", strlen(arg1)) == 0)
//       {

//         char *source_name = arg1;
//         char *dest_name;

//         // Get the dest file name
//         token = strtok(NULL, " ");
//         if (token == NULL || strlen(token) < 1)
//         {
//           printf("Make sure to add a destination filename. Try again\n");
//           continue;
//         }
//         if (strlen(token) > 33)
//         {
//           printf("Source file name too long. Try again\n");
//           continue;
//         }
//         dest_name = malloc(sizeof(char) * (strlen(token) - 1));
//         strcpy(dest_name, token);
//         if (dest_name[strlen(token) - 1] == 0x0a)
//         {
//           dest_name[strlen(token) - 1] = '\0';
//         }

//         // Get the block size and number of blocks in the fat region
//         int block_size = fat[0] & 0x00FF;
//         switch (block_size)
//         {
//         case 0:
//           block_size = 256;
//           break;
//         case 1:
//           block_size = 512;
//           break;
//         case 2:
//           block_size = 1024;
//           break;
//         case 3:
//           block_size = 2048;
//           break;
//         case 4:
//           block_size = 4096;
//           break;
//         default:
//           printf("Invalid configuration size. Try Again.\n");
//           continue;
//         }

//         // Search for the specified file starting in the root directory
//         bool found_file = false;
//         int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
//         int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
//         int start_block = 1;
//         uint16_t root_dir_path = fat[start_block];
//         int directory_num = 0;
//         int max_directories_num = block_size / 64;
//         while (root_dir_path != 0xFFFF)
//         {
//           while (directory_num < max_directories_num)
//           {
//             char file_name[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
//               file_name[j] = c1;
//               file_name[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(source_name);
//             found_file = strncmp(file_name, source_name, len) == 0;
//             if (found_file)
//             {
//               break;
//             }
//             directory_num++;
//           }

//           // If we've maxed out this directory, then move to the next listed directory
//           if (directory_num != max_directories_num)
//           {
//             break;
//           }
//           else
//           {
//             start_block = root_dir_path;
//             root_dir_path = fat[start_block];
//           }
//         }

//         // Check the final directory to see if the file is there
//         if (root_dir_path == 0xFFFF && !found_file)
//         {
//           directory_num = 0;
//           while (directory_num < max_directories_num)
//           {
//             char file_name[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
//               file_name[j] = c1;
//               file_name[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(source_name);
//             found_file = strncmp(file_name, source_name, len) == 0;
//             if (found_file)
//             {
//               break;
//             }
//             directory_num++;
//           }
//         }

//         // Check if file isn't found
//         if (!found_file)
//         {
//           printf("'%s' not found in the directory. Try again.\n", source_name);
//           continue;
//         }

//         // Find the first block and file size of the data in the source file
//         int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + 16;
//         int file_size = 0;
//         unsigned b0 = (fat[file_offset] & 0xff00);
//         unsigned b1 = (fat[file_offset] & 0xff);
//         unsigned b2 = (fat[file_offset + 1] & 0xff00);
//         unsigned b3 = (fat[file_offset + 1] & 0xff);
//         file_size = ((b3 | b2) << 16) | (b1 | b0);
//         file_offset += 2;
//         int first_block = (fat[file_offset] & 0xFF00) | (fat[file_offset] & 0x00FF);

//         // Move to block with file data and read data
//         char file_data[file_size];
//         int offset = ENTRIES_IN_FAT + block_size * (first_block - 1) / 2;
//         start_block = first_block;
//         root_dir_path = fat[start_block];
//         int byte_counter = 0;
//         while (byte_counter < file_size)
//         {
//           char c1 = (fat[offset] & 0xff);
//           char c2 = (fat[offset] & 0xff00) >> 8;
//           file_data[byte_counter] = c1;
//           file_data[byte_counter + 1] = c2;
//           byte_counter += 2;
//           offset += 1;

//           // If we've maxed out this block, then move to the next listed block
//           if (root_dir_path != 0xFFFF && byte_counter == block_size)
//           {
//             start_block = root_dir_path;
//             root_dir_path = fat[start_block];
//           }
//         }

//         // Open destination file on the host system
//         int dest_fd = open(dest_name, O_CREAT | O_TRUNC | O_WRONLY);
//         if (dest_fd == -1)
//         {
//           printf("Invalid destination file name. Try again.\n");
//           continue;
//         }

//         // Copy file data from source on fat to dest file on host
//         write(dest_fd, file_data, file_size);
//       }

//       // cp SOURCE DEST (source on fat to dest on fat)
//       else
//       {
//         char *source_name = arg1;
//         char *dest_name = arg2;

//         // Get the block size and number of blocks in the fat region
//         int block_size = fat[0] & 0x00FF;
//         switch (block_size)
//         {
//         case 0:
//           block_size = 256;
//           break;
//         case 1:
//           block_size = 512;
//           break;
//         case 2:
//           block_size = 1024;
//           break;
//         case 3:
//           block_size = 2048;
//           break;
//         case 4:
//           block_size = 4096;
//           break;
//         default:
//           printf("Invalid configuration size. Try Again.\n");
//           continue;
//         }

//         // Search for the specified file starting in the root directory
//         bool found_file = false;
//         int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
//         int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
//         int start_block = 1;
//         uint16_t root_dir_path = fat[start_block];
//         int directory_num = 0;
//         int max_directories_num = block_size / 64;
//         while (root_dir_path != 0xFFFF)
//         {
//           while (directory_num < max_directories_num)
//           {
//             char file_name[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
//               file_name[j] = c1;
//               file_name[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(source_name);
//             found_file = strncmp(file_name, source_name, len) == 0;
//             if (found_file)
//             {
//               break;
//             }
//             directory_num++;
//           }

//           // If we've maxed out this directory, then move to the next listed directory
//           if (directory_num != max_directories_num)
//           {
//             break;
//           }
//           else
//           {
//             start_block = root_dir_path;
//             root_dir_path = fat[start_block];
//           }
//         }

//         // Check the final directory to see if the file is there
//         if (root_dir_path == 0xFFFF && !found_file)
//         {
//           directory_num = 0;
//           while (directory_num < max_directories_num)
//           {
//             char file_name[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
//               file_name[j] = c1;
//               file_name[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(source_name);
//             found_file = strncmp(file_name, source_name, len) == 0;
//             if (found_file)
//             {
//               break;
//             }
//             directory_num++;
//           }
//         }

//         // Check if file isn't found
//         if (!found_file)
//         {
//           printf("'%s' not found in the directory. Try again.\n", source_name);
//           continue;
//         }

//         // Find the first block and file size of the data in the source file
//         int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + 16;
//         int file_size = 0;
//         unsigned b0 = (fat[file_offset] & 0xff00);
//         unsigned b1 = (fat[file_offset] & 0xff);
//         unsigned b2 = (fat[file_offset + 1] & 0xff00);
//         unsigned b3 = (fat[file_offset + 1] & 0xff);
//         file_size = ((b3 | b2) << 16) | (b1 | b0);
//         file_offset += 2;
//         int first_block = (fat[file_offset] & 0xFF00) | (fat[file_offset] & 0x00FF);

//         // Move to block with file data and read data
//         char file_data[file_size];
//         int offset = ENTRIES_IN_FAT + block_size * (first_block - 1) / 2;
//         start_block = first_block;
//         root_dir_path = fat[start_block];
//         int byte_counter = 0;
//         while (byte_counter < file_size)
//         {
//           char c1 = (fat[offset] & 0xff);
//           char c2 = (fat[offset] & 0xff00) >> 8;
//           file_data[byte_counter] = c1;
//           file_data[byte_counter + 1] = c2;
//           byte_counter += 2;
//           offset += 1;

//           // If we've maxed out this block, then move to the next listed block
//           if (root_dir_path != 0xFFFF && byte_counter == block_size)
//           {
//             start_block = root_dir_path;
//             root_dir_path = fat[start_block];
//           }
//         }

//         // Check if destination file already exists
//         blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
//         ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
//         int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
//         found_file = false;
//         start_block = 1;
//         root_dir_path = fat[start_block];
//         directory_num = 0;
//         max_directories_num = block_size / 64;
//         while (root_dir_path != 0xFFFF)
//         {
//           while (directory_num < max_directories_num)
//           {
//             char curr_file[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
//               curr_file[j] = c1;
//               curr_file[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(dest_name);
//             found_file = strncmp(dest_name, curr_file, len) == 0;
//             if (found_file)
//             {
//               break;
//             }
//             directory_num++;
//           }

//           // If we've maxed out this directory, then move to the next listed directory
//           if (directory_num != max_directories_num)
//           {
//             break;
//           }
//           else
//           {
//             start_block = root_dir_path;
//             root_dir_path = fat[start_block];
//           }
//         }

//         // Check the final directory to see if the destination file is there
//         if (root_dir_path == 0xFFFF && !found_file)
//         {
//           directory_num = 0;
//           while (directory_num < max_directories_num)
//           {
//             char curr_file[32];
//             int i = 0;
//             int j = 0;
//             while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
//                    fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
//             {
//               char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
//               char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
//               curr_file[j] = c1;
//               curr_file[j + 1] = c2;
//               i++;
//               j += 2;
//             }
//             int len = strlen(dest_name);
//             found_file = strncmp(dest_name, curr_file, len) == 0;
//             if (found_file)
//             {
//               break;
//             }
//             directory_num++;
//           }
//         }

//         // Check if file already exists
//         if (!found_file)
//         {
//           // Scan to find open directories to place new dest file in starting in original block
//           while (root_dir_path != 0xFFFF)
//           {
//             while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
//                     (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
//                    directory_num < max_directories_num)
//             {
//               directory_num++;
//             }

//             // If we've maxed out this directory, then move to the next listed directory
//             if (directory_num != max_directories_num)
//             {
//               break;
//             }
//             else
//             {
//               start_block = root_dir_path;
//               root_dir_path = fat[start_block];
//             }
//           }

//           // Check the final directory to see if there's an empty directory slot
//           if (root_dir_path == 0xFFFF)
//           {
//             directory_num = 0;
//             while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
//                     (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
//                    directory_num < max_directories_num)
//             {
//               directory_num++;
//             }
//           }

//           // If all directories in all listed blocks are full, make a new block
//           if (directory_num == max_directories_num)
//           {
//             int block_num = 0;
//             while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//             {
//               block_num++;
//             }
//             if (block_num == BLOCKS_IN_DATA)
//             {
//               printf("File system is full. Remove a file and try again.\n");
//               continue;
//             }

//             // Once we've found the next open block, update FAT region
//             block_num++;
//             fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
//             start_block = block_num;
//             fat[block_num] = 0xFFFF;
//             directory_num = 0;
//           }
//         }

//         // Move to destination file and write name
//         file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
//         int i = 0;
//         if (strlen(dest_name) < 2)
//         {
//           uint16_t c = dest_name[i] << 8;
//           fat[file_offset + i] = c;
//         }
//         while (i < strlen(dest_name))
//         {
//           char c1 = dest_name[i];
//           char c2 = dest_name[i + 1];
//           uint16_t c = c2 << 8 | c1;
//           fat[file_offset + i / 2] = c;
//           i += 2;
//         }
//         if (i == strlen(dest_name) - 1)
//         {
//           char c1 = dest_name[i];
//           uint16_t c = c1 << 8 & 0xFF00;
//           fat[file_offset + i / 2] = c;
//         }
//         // Fill in remainders with 0's
//         while (fat[file_offset + i / 2] != 0 && i < 18)
//         {
//           fat[file_offset + i / 2] = 0;
//           i += 2;
//         }

//         // Move to place to write file size
//         file_offset += 16;
//         unsigned sizeb0 = (file_size & 0xff);
//         unsigned sizeb1 = (file_size & 0xff00);
//         unsigned sizeb2 = (file_size & 0xff0000) >> 16u;
//         unsigned sizeb3 = (file_size & 0xff000000) >> 16u;
//         fat[file_offset] = sizeb0 | sizeb1;
//         fat[file_offset + 1] = sizeb2 | sizeb3;

//         // Find first open block to write file data
//         int block_num = 0;
//         while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//         {
//           block_num++;
//         }
//         if (block_num == BLOCKS_IN_DATA)
//         {
//           printf("File system is full. Remove a file and try again.\n");
//           continue;
//         }
//         first_block = block_num + 1;

//         // Move to place to write first block
//         file_offset += 2;
//         fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);

//         // Move to place to write type and permissions
//         file_offset += 1;
//         fat[file_offset] = 0x0601;

//         // Move to place to write time
//         file_offset += 1;
//         time_t mtime = time(NULL);

//         unsigned time_b0 = (mtime & 0x00000000000000ff);
//         unsigned time_b1 = (mtime & 0x000000000000ff00);
//         unsigned time_b2 = (mtime & 0x0000000000ff0000) >> 16u;
//         unsigned time_b3 = (mtime & 0x00000000ff000000) >> 16u;
//         unsigned time_b4 = (mtime & 0x000000ff00000000) >> 32u;
//         unsigned time_b5 = (mtime & 0x0000ff0000000000) >> 32u;
//         unsigned time_b6 = (mtime & 0x00ff000000000000) >> 48u;
//         unsigned time_b7 = (mtime & 0xff00000000000000) >> 48u;

//         fat[file_offset] = time_b0 | time_b1;
//         fat[file_offset + 1] = time_b2 | time_b3;
//         fat[file_offset + 2] = time_b4 | time_b5;
//         fat[file_offset + 3] = time_b6 | time_b7;

//         free(dest_name);

//         // Move to first open block in root directory and write data
//         block_num = first_block - 1;
//         offset = ENTRIES_IN_FAT + block_size * block_num / 2;
//         i = 0;
//         int j = 0;
//         int curr_block = first_block;
//         while (i < block_size && j < strlen(file_data))
//         {
//           // Write data
//           if (strlen(file_data) < 2)
//           {
//             uint16_t c = file_data[i] << 8;
//             fat[offset + i] = c;
//           }
//           while (i < strlen(file_data))
//           {
//             char c1 = file_data[i];
//             char c2 = file_data[i + 1];
//             uint16_t c = c2 << 8 | c1;
//             fat[offset + i / 2] = c;
//             i += 2;
//           }
//           if (i == strlen(file_data) - 1)
//           {
//             char c1 = file_data[i];
//             uint16_t c = c1 << 8 & 0xFF00;
//             fat[offset + i / 2] = c;
//           }
//           j = i;

//           // Check if block has overflown. If so, move to next free block
//           if (i >= block_size && block_num < BLOCKS_IN_DATA)
//           {
//             while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//             {
//               block_num++;
//             }
//             if (block_num == BLOCKS_IN_DATA)
//             {
//               printf("File system is full. Remove a file and try again.\n");
//               continue;
//             }
//             i = 0;

//             // Update FAT region to contain new pointers
//             fat[curr_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
//             curr_block++;
//           }
//         }
//         // Update FAT region to contain new pointers
//         fat[curr_block] = 0xFFFF;
//       }
// }

// void f_cat(char** files){
//     char *file_name;
//         char *all_file_data = "\0";

//         // Get the block size and number of blocks in the fat region
//         int block_size = fat[0] & 0x00FF;
//         switch (block_size)
//         {
//         case 0:
//           block_size = 256;
//           break;
//         case 1:
//           block_size = 512;
//           break;
//         case 2:
//           block_size = 1024;
//           break;
//         case 3:
//           block_size = 2048;
//           break;
//         case 4:
//           block_size = 4096;
//           break;
//         default:
//           printf("Invalid configuration size. Try Again.\n");
//           return;
//         }

//             // Get the block size and number of blocks in the fat region
//             int block_size = fat[0] & 0x00FF;
//             switch (block_size)
//             {
//             case 0:
//               block_size = 256;
//               break;
//             case 1:
//               block_size = 512;
//               break;
//             case 2:
//               block_size = 1024;
//               break;
//             case 3:
//               block_size = 2048;
//               break;
//             case 4:
//               block_size = 4096;
//               break;
//             default:
//               printf("Invalid configuration size. Try Again.\n");
//               return;
//             }

//             // Check if destination file already exists
//             int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
//             int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
//             int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
//             int found_file = false;
//             int start_block = 1;
//             int root_dir_path = fat[start_block];
//             int directory_num = 0;
//             int max_directories_num = block_size / 64;
//             while (root_dir_path != 0xFFFF)
//             {
//               while (directory_num < max_directories_num)
//               {
//                 char curr_file[32];
//                 int i = 0;
//                 int j = 0;
//                 while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
//                        fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
//                 {
//                   char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
//                   char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
//                   curr_file[j] = c1;
//                   curr_file[j + 1] = c2;
//                   i++;
//                   j += 2;
//                 }
//                 int len = strlen(dest_name);
//                 found_file = strncmp(dest_name, curr_file, len) == 0;
//                 if (found_file)
//                 {
//                   break;
//                 }
//                 directory_num++;
//               }

//               // If we've maxed out this directory, then move to the next listed directory
//               if (directory_num != max_directories_num)
//               {
//                 break;
//               }
//               else
//               {
//                 start_block = root_dir_path;
//                 root_dir_path = fat[start_block];
//               }
//             }

//             // Check the final directory to see if the destination file is there
//             if (root_dir_path == 0xFFFF && !found_file)
//             {
//               directory_num = 0;
//               while (directory_num < max_directories_num)
//               {
//                 char curr_file[32];
//                 int i = 0;
//                 int j = 0;
//                 while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
//                        fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
//                 {
//                   char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
//                   char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
//                   curr_file[j] = c1;
//                   curr_file[j + 1] = c2;
//                   i++;
//                   j += 2;
//                 }
//                 int len = strlen(dest_name);
//                 found_file = strncmp(dest_name, curr_file, len) == 0;
//                 if (found_file)
//                 {
//                   break;
//                 }
//                 directory_num++;
//               }
//             }

//             // Check if file already exists
//             if (!found_file)
//             {
//               // Scan to find open directories to place new dest file in starting in original block
//               while (root_dir_path != 0xFFFF)
//               {
//                 while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
//                         (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
//                        directory_num < max_directories_num)
//                 {
//                   directory_num++;
//                 }

//                 // If we've maxed out this directory, then move to the next listed directory
//                 if (directory_num != max_directories_num)
//                 {
//                   break;
//                 }
//                 else
//                 {
//                   start_block = root_dir_path;
//                   root_dir_path = fat[start_block];
//                 }
//               }

//               // Check the final directory to see if there's an empty directory slot
//               if (root_dir_path == 0xFFFF)
//               {
//                 directory_num = 0;
//                 while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
//                         (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
//                        directory_num < max_directories_num)
//                 {
//                   directory_num++;
//                 }
//               }

//               // If all directories in all listed blocks are full, make a new block
//               if (directory_num == max_directories_num)
//               {
//                 int block_num = 0;
//                 while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//                 {
//                   block_num++;
//                 }
//                 if (block_num == BLOCKS_IN_DATA)
//                 {
//                   printf("File system is full. Remove a file and try again.\n");
//                   return;
//                 }

//                 // Once we've found the next open block, update FAT region
//                 block_num++;
//                 fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
//                 start_block = block_num;
//                 fat[block_num] = 0xFFFF;
//                 directory_num = 0;
//               }
//             }

//             // Move to destination file and write name
//             int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
//             int i = 0;
//             if (strlen(dest_name) < 2)
//             {
//               uint16_t c = dest_name[i] << 8;
//               fat[file_offset + i] = c;
//             }
//             while (i < strlen(dest_name))
//             {
//               char c1 = dest_name[i];
//               char c2 = dest_name[i + 1];
//               uint16_t c = c2 << 8 | c1;
//               fat[file_offset + i / 2] = c;
//               i += 2;
//             }
//             if (i == strlen(dest_name) - 1)
//             {
//               char c1 = dest_name[i];
//               uint16_t c = c1 << 8 & 0xFF00;
//               fat[file_offset + i / 2] = c;
//             }
//             // Fill in remainders with 0's
//             while (fat[file_offset + i / 2] != 0 && i < 18)
//             {
//               fat[file_offset + i / 2] = 0;
//               i += 2;
//             }

//             // Move to place to write file size
//             int file_size = strlen(all_file_data);
//             file_offset += 16;
//             unsigned sizeb0 = (file_size & 0xff);
//             unsigned sizeb1 = (file_size & 0xff00);
//             unsigned sizeb2 = (file_size & 0xff0000) >> 16u;
//             unsigned sizeb3 = (file_size & 0xff000000) >> 16u;
//             fat[file_offset] = sizeb0 | sizeb1;
//             fat[file_offset + 1] = sizeb2 | sizeb3;

//             // Move to place to read first block if it is defined
//             file_offset += 2;
//             int first_block = fat[file_offset];

//             // Find first open block to write file data
//             if (first_block == 0)
//             {
//               int block_num = 0;
//               while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//               {
//                 block_num++;
//               }
//               if (block_num == BLOCKS_IN_DATA)
//               {
//                 printf("File system is full. Remove a file and try again.\n");
//                 return;
//               }
//               first_block = block_num + 1;
//             }

//             // Move to place to write first block
//             fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);

//             // Move to place to write type and permissions
//             file_offset += 1;
//             fat[file_offset] = 0x0601;

//             // Move to place to write time
//             file_offset += 1;
//             time_t mtime = time(NULL);

//             unsigned time_b0 = (mtime & 0x00000000000000ff);
//             unsigned time_b1 = (mtime & 0x000000000000ff00);
//             unsigned time_b2 = (mtime & 0x0000000000ff0000) >> 16u;
//             unsigned time_b3 = (mtime & 0x00000000ff000000) >> 16u;
//             unsigned time_b4 = (mtime & 0x000000ff00000000) >> 32u;
//             unsigned time_b5 = (mtime & 0x0000ff0000000000) >> 32u;
//             unsigned time_b6 = (mtime & 0x00ff000000000000) >> 48u;
//             unsigned time_b7 = (mtime & 0xff00000000000000) >> 48u;

//             fat[file_offset] = time_b0 | time_b1;
//             fat[file_offset + 1] = time_b2 | time_b3;
//             fat[file_offset + 2] = time_b4 | time_b5;
//             fat[file_offset + 3] = time_b6 | time_b7;

//             free(dest_name);

//             // Move to first block in root directory and write data
//             int block_num = first_block - 1;
//             int offset = ENTRIES_IN_FAT + block_size * block_num / 2;
//             i = 0;
//             int j = 0;
//             int curr_block = first_block;
//             while (i < block_size && j < strlen(all_file_data))
//             {
//               // Write data
//               if (strlen(all_file_data) < 2)
//               {
//                 uint16_t c = all_file_data[i] << 8;
//                 fat[offset + i] = c;
//               }
//               while (i < strlen(all_file_data))
//               {
//                 char c1 = all_file_data[i];
//                 char c2 = all_file_data[i + 1];
//                 uint16_t c = c2 << 8 | c1;
//                 fat[offset + i / 2] = c;
//                 i += 2;
//               }
//               if (i == strlen(all_file_data) - 1)
//               {
//                 char c1 = all_file_data[i];
//                 uint16_t c = c1 << 8 & 0xFF00;
//                 fat[offset + i / 2] = c;
//               }
//               j = i;

//               // Check if block has overflown. If so, move to next free block
//               if (i >= block_size && block_num < BLOCKS_IN_DATA)
//               {
//                 while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//                 {
//                   block_num++;
//                 }
//                 if (block_num == BLOCKS_IN_DATA)
//                 {
//                   printf("File system is full. Remove a file and try again.\n");
//                   return;
//                 }
//                 i = 0;

//                 // Update FAT region to contain new pointers
//                 fat[curr_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
//                 curr_block++;
//               }
//             }

//             // Change any remaining pointers in FAT to be undefined if necessary
//             fat[curr_block] = 0xFFFF;
//             curr_block++;
//             while (fat[curr_block] != 0)
//             {
//               fat[curr_block] = 0x0000;
//               curr_block++;
//             }

//             break;
//           }

//           // check if we've reached -a
//           else if (strncmp(file_name, "-a", strlen(file_name)) == 0)
//           {
//             char *dest_name;
//             has_output = true;

//             // Get the dest file name
//             token = strtok(NULL, " ");
//             if (token == NULL || strlen(token) <= 1)
//             {
//               printf("Make sure to add a destination filename. Try again\n");
//               return;
//             }
//             if (strlen(token) > 33)
//             {
//               printf("Destination file name too long. Try again\n");
//               return;
//             }
//             dest_name = malloc(sizeof(char) * (strlen(token) - 1));
//             strcpy(dest_name, token);
//             if (dest_name[strlen(token) - 1] == 0x0a)
//             {
//               dest_name[strlen(token) - 1] = '\0';
//             }

//             // Get the block size and number of blocks in the fat region
//             int block_size = fat[0] & 0x00FF;
//             switch (block_size)
//             {
//             case 0:
//               block_size = 256;
//               break;
//             case 1:
//               block_size = 512;
//               break;
//             case 2:
//               block_size = 1024;
//               break;
//             case 3:
//               block_size = 2048;
//               break;
//             case 4:
//               block_size = 4096;
//               break;
//             default:
//               printf("Invalid configuration size. Try Again.\n");
//               return;
//             }

//             // Check if destination file already exists
//             int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
//             int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
//             int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
//             int found_file = false;
//             int start_block = 1;
//             int root_dir_path = fat[start_block];
//             int directory_num = 0;
//             int max_directories_num = block_size / 64;
//             while (root_dir_path != 0xFFFF)
//             {
//               while (directory_num < max_directories_num)
//               {
//                 char curr_file[32];
//                 int i = 0;
//                 int j = 0;
//                 while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
//                        fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
//                 {
//                   char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
//                   char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
//                   curr_file[j] = c1;
//                   curr_file[j + 1] = c2;
//                   i++;
//                   j += 2;
//                 }
//                 int len = strlen(dest_name);
//                 found_file = strncmp(dest_name, curr_file, len) == 0;
//                 if (found_file)
//                 {
//                   break;
//                 }
//                 directory_num++;
//               }

//               // If we've maxed out this directory, then move to the next listed directory
//               if (directory_num != max_directories_num)
//               {
//                 break;
//               }
//               else
//               {
//                 start_block = root_dir_path;
//                 root_dir_path = fat[start_block];
//               }
//             }

//             // Check the final directory to see if the destination file is there
//             if (root_dir_path == 0xFFFF && !found_file)
//             {
//               directory_num = 0;
//               while (directory_num < max_directories_num)
//               {
//                 char curr_file[32];
//                 int i = 0;
//                 int j = 0;
//                 while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
//                        fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
//                 {
//                   char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
//                   char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
//                   curr_file[j] = c1;
//                   curr_file[j + 1] = c2;
//                   i++;
//                   j += 2;
//                 }
//                 int len = strlen(dest_name);
//                 found_file = strncmp(dest_name, curr_file, len) == 0;
//                 if (found_file)
//                 {
//                   break;
//                 }
//                 directory_num++;
//               }
//             }

//             // Check if file already exists
//             if (!found_file)
//             {
//               // Scan to find open directories to place new dest file in starting in original block
//               while (root_dir_path != 0xFFFF)
//               {
//                 while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
//                         (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
//                        directory_num < max_directories_num)
//                 {
//                   directory_num++;
//                 }

//                 // If we've maxed out this directory, then move to the next listed directory
//                 if (directory_num != max_directories_num)
//                 {
//                   break;
//                 }
//                 else
//                 {
//                   start_block = root_dir_path;
//                   root_dir_path = fat[start_block];
//                 }
//               }

//               // Check the final directory to see if there's an empty directory slot
//               if (root_dir_path == 0xFFFF)
//               {
//                 directory_num = 0;
//                 while (((fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 0 &&
//                         (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2] & 0x00FF) != 1) &&
//                        directory_num < max_directories_num)
//                 {
//                   directory_num++;
//                 }
//               }

//               // If all directories in all listed blocks are full, make a new block
//               if (directory_num == max_directories_num)
//               {
//                 int block_num = 0;
//                 while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//                 {
//                   block_num++;
//                 }
//                 if (block_num == BLOCKS_IN_DATA)
//                 {
//                   printf("File system is full. Remove a file and try again.\n");
//                   return;
//                 }

//                 // Once we've found the next open block, update FAT region
//                 block_num++;
//                 fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
//                 start_block = block_num;
//                 fat[block_num] = 0xFFFF;
//                 directory_num = 0;
//               }
//             }

//             // Move to destination file and write name
//             int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
//             int i = 0;
//             if (strlen(dest_name) < 2)
//             {
//               uint16_t c = dest_name[i] << 8;
//               fat[file_offset + i] = c;
//             }
//             while (i < strlen(dest_name))
//             {
//               char c1 = dest_name[i];
//               char c2 = dest_name[i + 1];
//               uint16_t c = c2 << 8 | c1;
//               fat[file_offset + i / 2] = c;
//               i += 2;
//             }
//             if (i == strlen(dest_name) - 1)
//             {
//               char c1 = dest_name[i];
//               uint16_t c = c1 << 8 & 0xFF00;
//               fat[file_offset + i / 2] = c;
//             }
//             // Fill in remainders with 0's
//             while (fat[file_offset + i / 2] != 0 && i < 18)
//             {
//               fat[file_offset + i / 2] = 0;
//               i += 2;
//             }

//             // Find the first block and file size of the data in the source file
//             file_offset += 16;
//             int file_size = 0;
//             unsigned b0 = (fat[file_offset] & 0xff00);
//             unsigned b1 = (fat[file_offset] & 0xff);
//             unsigned b2 = (fat[file_offset + 1] & 0xff00);
//             unsigned b3 = (fat[file_offset + 1] & 0xff);
//             file_size = ((b3 | b2) << 16) | (b1 | b0);

//             // Write adjusted file size
//             int input_file_size = strlen(all_file_data);
//             unsigned sizeb0 = ((file_size + input_file_size) & 0xff);
//             unsigned sizeb1 = ((file_size + input_file_size) & 0xff00);
//             unsigned sizeb2 = ((file_size + input_file_size) & 0xff0000) >> 16u;
//             unsigned sizeb3 = ((file_size + input_file_size) & 0xff000000) >> 16u;
//             fat[file_offset] = sizeb0 | sizeb1;
//             fat[file_offset + 1] = sizeb2 | sizeb3;

//             // Move to place to read first block if it is defined
//             file_offset += 2;
//             int first_block = fat[file_offset];

//             // Find first open block to write file data
//             if (first_block == 0)
//             {
//               int block_num = 0;
//               while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//               {
//                 block_num++;
//               }
//               if (block_num == BLOCKS_IN_DATA)
//               {
//                 printf("File system is full. Remove a file and try again.\n");
//                 return;
//               }
//               first_block = block_num + 1;
//             }

//             // Move to place to write first block
//             fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);

//             // Move to place to write type and permissions
//             file_offset += 1;
//             fat[file_offset] = 0x0601;

//             // Move to place to write time
//             file_offset += 1;
//             time_t mtime = time(NULL);

//             unsigned time_b0 = (mtime & 0x00000000000000ff);
//             unsigned time_b1 = (mtime & 0x000000000000ff00);
//             unsigned time_b2 = (mtime & 0x0000000000ff0000) >> 16u;
//             unsigned time_b3 = (mtime & 0x00000000ff000000) >> 16u;
//             unsigned time_b4 = (mtime & 0x000000ff00000000) >> 32u;
//             unsigned time_b5 = (mtime & 0x0000ff0000000000) >> 32u;
//             unsigned time_b6 = (mtime & 0x00ff000000000000) >> 48u;
//             unsigned time_b7 = (mtime & 0xff00000000000000) >> 48u;

//             fat[file_offset] = time_b0 | time_b1;
//             fat[file_offset + 1] = time_b2 | time_b3;
//             fat[file_offset + 2] = time_b4 | time_b5;
//             fat[file_offset + 3] = time_b6 | time_b7;

//             free(dest_name);

//             // Move to first block in root directory and write data
//             int block_num = first_block - 1;
//             int offset = ENTRIES_IN_FAT + block_size * block_num / 2 + file_size / 2;
//             if (file_size % 2 == 1)
//             {
//               char c1 = all_file_data[0];
//               fat[offset] = c1 << 8 | (fat[offset] & 0xFF);
//               i = 1;
//               offset++;
//             }
//             else
//             {
//               i = 0;
//             }
//             int j = 0;
//             int curr_block = first_block;
//             while (i < block_size && j < strlen(all_file_data))
//             {
//               // Write data
//               if (strlen(all_file_data) < 2)
//               {
//                 uint16_t c = all_file_data[i] << 8;
//                 fat[offset + i] = c;
//               }
//               while (i < strlen(all_file_data))
//               {
//                 char c1 = all_file_data[i];
//                 char c2 = all_file_data[i + 1];
//                 uint16_t c = c2 << 8 | c1;
//                 fat[offset + i / 2] = c;
//                 i += 2;
//               }
//               if (i == strlen(all_file_data) - 1)
//               {
//                 char c1 = all_file_data[i];
//                 uint16_t c = c1 << 8 & 0xFF00;
//                 fat[offset + i / 2] = c;
//               }
//               j = i;

//               // Check if block has overflown. If so, move to next free block
//               if (i >= block_size && block_num < BLOCKS_IN_DATA)
//               {
//                 while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
//                 {
//                   block_num++;
//                 }
//                 if (block_num == BLOCKS_IN_DATA)
//                 {
//                   printf("File system is full. Remove a file and try again.\n");
//                   return;
//                 }
//                 i = 0;

//                 // Update FAT region to contain new pointers
//                 fat[curr_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
//                 curr_block++;
//               }
//             }

//             // Change any remaining pointers in FAT to be undefined if necessary
//             fat[curr_block] = 0xFFFF;
//             curr_block++;
//             while (fat[curr_block] != 0)
//             {
//               fat[curr_block] = 0x0000;
//               curr_block++;
//             }
//           }

//           // Continue concatenating file data
//           else
//           {
//             // Search for the specified file starting in the root directory
//             bool found_file = false;
//             int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
//             int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
//             int start_block = 1;
//             uint16_t root_dir_path = fat[start_block];
//             int directory_num = 0;
//             int max_directories_num = block_size / 64;
//             while (root_dir_path != 0xFFFF)
//             {
//               while (directory_num < max_directories_num)
//               {
//                 char curr_file[32];
//                 int i = 0;
//                 int j = 0;
//                 while (fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 0 &&
//                        fat[ENTRIES_IN_FAT + 32 * directory_num + i] != 1)
//                 {
//                   char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0x00FF;
//                   char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + i] & 0xFF00) >> 8;
//                   curr_file[j] = c1;
//                   curr_file[j + 1] = c2;
//                   i++;
//                   j += 2;
//                 }
//                 int len = strlen(file_name);
//                 found_file = strncmp(file_name, curr_file, len) == 0;
//                 if (found_file)
//                 {
//                   break;
//                 }
//                 directory_num++;
//               }

//               // If we've maxed out this directory, then move to the next listed directory
//               if (directory_num != max_directories_num)
//               {
//                 break;
//               }
//               else
//               {
//                 start_block = root_dir_path;
//                 root_dir_path = fat[start_block];
//               }
//             }

//             // Check the final directory to see if the file is there
//             if (root_dir_path == 0xFFFF && !found_file)
//             {
//               directory_num = 0;
//               while (directory_num < max_directories_num)
//               {
//                 char curr_file[32];
//                 int i = 0;
//                 int j = 0;
//                 while (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 0 &&
//                        fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] != 1)
//                 {
//                   char c1 = fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0x00FF;
//                   char c2 = (fat[ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + i] & 0xFF00) >> 8;
//                   curr_file[j] = c1;
//                   curr_file[j + 1] = c2;
//                   i++;
//                   j += 2;
//                 }
//                 int len = strlen(file_name);
//                 found_file = strncmp(curr_file, file_name, len) == 0;
//                 if (found_file)
//                 {
//                   break;
//                 }
//                 directory_num++;
//               }
//             }

//             // Check if file isn't found
//             if (!found_file)
//             {
//               printf("'%s' not found in the directory. Try again.\n", file_name);
//               free(file_name);
//               token = strtok(NULL, " ");
//               return;
//             }
//             else
//             {
//               // Find the first block and file size of the data in the source file
//               int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + 16;
//               int file_size = 0;
//               unsigned b0 = (fat[file_offset] & 0xff00);
//               unsigned b1 = (fat[file_offset] & 0xff);
//               unsigned b2 = (fat[file_offset + 1] & 0xff00);
//               unsigned b3 = (fat[file_offset + 1] & 0xff);
//               file_size = ((b3 | b2) << 16) | (b1 | b0);
//               file_offset += 2;
//               int first_block = (fat[file_offset] & 0xFF00) | (fat[file_offset] & 0x00FF);

//               // Move to block with file data and read data
//               char file_data[file_size];
//               int offset = ENTRIES_IN_FAT + block_size * (first_block - 1) / 2;
//               start_block = first_block;
//               root_dir_path = fat[start_block];
//               int byte_counter = 0;
//               while (byte_counter < file_size)
//               {
//                 char c1 = (fat[offset] & 0xff);
//                 char c2 = (fat[offset] & 0xff00) >> 8;
//                 file_data[byte_counter] = c1;
//                 file_data[byte_counter + 1] = c2;
//                 byte_counter += 2;
//                 offset += 1;

//                 // If we've maxed out this block, then move to the next listed block
//                 if (root_dir_path != 0xFFFF && byte_counter == block_size)
//                 {
//                   start_block = root_dir_path;
//                   root_dir_path = fat[start_block];
//                 }
//               }

//               char *temp = strcpy(malloc(sizeof(char) * strlen(all_file_data)), all_file_data);
//               all_file_data = malloc(sizeof(char) * strlen(file_data) + sizeof(char) * strlen(all_file_data));
//               strcpy(all_file_data, temp);
//               strcat(all_file_data, file_data);

//               free(file_name);
//               token = strtok(NULL, " ");
//             }
//           }
//         }

//         if (!has_output)
//         {
//           printf("%s\n", all_file_data);
//         }
// }



//mount function that mounts the filesystem
void mount(char* filename){
    // Open the file for the file system
    FILE *fp;
    fp = fopen(filename, "r+");
    if (fp == NULL)
    {
        printf("Invalid file name. Try again.\n");
    }

    // Get the length of the file
    fseek(fp, 0L, SEEK_END);
    long fat_length = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    // Load the FAT into memory
    fat = (uint16_t *) mmap(NULL, fat_length, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(fp), 0);
    // Get the block size and number of blocks in the fat region
    int block_size = fat[0] & 0x00FF;
    switch (block_size)
    {
    case 0:
    block_size = 256;
    break;
    case 1:
    block_size = 512;
    break;
    case 2:
    block_size = 1024;
    break;
    case 3:
    block_size = 2048;
    break;
    case 4:
    block_size = 4096;
    break;
    default:
    printf("Invalid configuration size. Try Again.\n");
    return;
    }

    BLOCKS_IN_FAT = (fat[0] & 0xFF00) >> 8;
    ENTRIES_IN_FAT = BLOCKS_IN_FAT * block_size / 2;
    BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;

    printf("FAT length: %ld\n", fat_length);
    printf("FAT[0]: %d\n", fat[0]);

    fd_table = create_list();

    file_list = create_list();
    mode_list = create_list();
    name_list = create_list();

    fclose(fp);
}
