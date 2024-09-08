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
#define BUF_SIZE 1024
#define PROMPT "penn-fat > "

bool isCommand(char *command, char *input)
{
  return strncmp(command, input, strlen(command)) == 0;
}

int main()
{
  uint16_t *fat = NULL;
  int fat_length = -1;
  char *fat_name = NULL;

  while (true)
  {
    // Display prompt
    printf("%s", PROMPT);

    // Get user input
    char *line = NULL;
    size_t input_length = 0;
    getline(&line, &input_length, stdin);

    if (isCommand("mkfs", line))
    {
      char *FS_NAME;
      int BLOCKS_IN_FAT;
      int BLOCK_SIZE_CONFIG;
      int BLOCK_SIZE_IN_BYTES;

      char *token = strtok(line, " ");

      // Get the file name
      token = strtok(NULL, " ");
      if (token == NULL)
      {
        printf("Make sure to add a filename. Try again\n");
        continue;
      }
      FS_NAME = malloc(sizeof(char) * strlen(token));
      strcpy(FS_NAME, token);

      // Get the number of blocks in fat
      token = strtok(NULL, " ");
      if (token == NULL)
      {
        printf("Make sure to add the number of blocks in FAT. Try again\n");
        continue;
      }
      BLOCKS_IN_FAT = atoi(token);
      if (BLOCKS_IN_FAT > 32 || BLOCKS_IN_FAT < 1)
      {
        printf("Invalid number of blocks. Try Again.\n");
        continue;
      }

      // Get the number for the block size config
      token = strtok(NULL, " ");
      if (token == NULL)
      {
        printf("Make sure to add a number for the block size. Try again\n");
        continue;
      }
      BLOCK_SIZE_CONFIG = atoi(token);
      switch (BLOCK_SIZE_CONFIG)
      {
      case 0:
        BLOCK_SIZE_IN_BYTES = 256;
        break;
      case 1:
        BLOCK_SIZE_IN_BYTES = 512;
        break;
      case 2:
        BLOCK_SIZE_IN_BYTES = 1024;
        break;
      case 3:
        BLOCK_SIZE_IN_BYTES = 2048;
        break;
      case 4:
        BLOCK_SIZE_IN_BYTES = 4096;
        break;
      default:
        printf("Invalid configuration size. Try Again.\n");
        continue;
      }

      // Check for invalid arguments
      token = strtok(NULL, " ");
      if (token != NULL)
      {
        printf("Invalid number of arguments. Try again\n");
        continue;
      }

      // Create the file for the file system
      int fd = open(FS_NAME, O_CREAT | O_TRUNC | O_WRONLY);

      // Define variables used for determining size of file for file system
      int ENTRIES_IN_FAT = BLOCKS_IN_FAT * BLOCK_SIZE_IN_BYTES / 2;
      int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
      int FS_SIZE = ENTRIES_IN_FAT * 2 + BLOCKS_IN_DATA * BLOCK_SIZE_IN_BYTES;

      // Check if maxfs: if so we should remove one extra block as xFFFE is largest possible block in data region
      if (BLOCKS_IN_FAT == 32 && BLOCK_SIZE_CONFIG == 4)
      {
        FS_SIZE -= 4096;
      }

      // Make file system the correct size
      ftruncate(fd, FS_SIZE);

      // Initialize the first two blocks of the FAT table
      char buf[12] = {BLOCK_SIZE_CONFIG, BLOCKS_IN_FAT, 0xFF, 0xFF};
      if (write(fd, buf, 12) == -1)
      {
        printf("Error initializing file system.\n");
        return EXIT_FAILURE;
      }

      free(FS_NAME);
      close(fd);
    }
    else if (isCommand("mount", line))
    {
      char *FS_NAME;

      char *token = strtok(line, " ");

      // Get the file name
      token = strtok(NULL, " ");
      if (token == NULL || strlen(token) <= 1)
      {
        printf("Make sure to add a filename. Try again\n");
        continue;
      }
      FS_NAME = malloc(sizeof(char) * (strlen(token) - 1));
      strcpy(FS_NAME, token);
      FS_NAME[strlen(token) - 1] = '\0';

      // Check for invalid arguments
      token = strtok(NULL, " ");
      if (token != NULL)
      {
        printf("Invalid number of arguments. Try again\n");
        continue;
      }

      // Open the file for the file system
      FILE *fp;
      fp = fopen(FS_NAME, "r+");
      if (fp == NULL)
      {
        printf("Invalid file name. Try again.\n");
        continue;
      }

      // Get the length of the file
      fseek(fp, 0L, SEEK_END);
      fat_length = ftell(fp);
      fseek(fp, 0L, SEEK_SET);

      // Load the FAT into memory
      fat = mmap(NULL, fat_length, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(fp), 0);

      // Store the name of the mounted FAT table
      fat_name = malloc(sizeof(char) * strlen(FS_NAME));
      strcpy(fat_name, FS_NAME);

      free(FS_NAME);
      fclose(fp);
    }
    else if (isCommand("unmount", line))
    {
      if (fat == NULL || fat_length == -1)
      {
        printf("No file system currently mounted. Try again.\n");
        continue;
      }
      if (munmap(fat, fat_length) != 0)
      {
        printf("Failed to unmount. Try again.\n");
        continue;
      }
      else
      {
        fat = NULL;
        fat_length = -1;
        free(fat_name);
      }
    }
    else if (isCommand("touch", line))
    {
      // Check that there's a file system mounted
      if (fat == NULL)
      {
        printf("No file system mounted. Try again.\n");
        continue;
      }

      char *file_name;

      char *token = strtok(line, " ");
      token = strtok(NULL, " ");
      if (token == NULL || strlen(token) <= 1)
      {
        printf("Make sure to add a filename. Try again\n");
        continue;
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
        continue;
      }

      int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
      int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
      int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;

      while (token != NULL)
      {
        // Get the file name
        if (token == NULL)
        {
          continue;
        }
        if (strlen(token) <= 1)
        {
          printf("Make sure to add a filename. Try again\n");
          continue;
        }
        if (strlen(token) > 33)
        {
          printf("File name too long. Try again\n");
          continue;
        }
        file_name = malloc(sizeof(char) * (strlen(token) - 1));
        strcpy(file_name, token);
        if (file_name[strlen(token) - 1] == 0x0a)
        {
          file_name[strlen(token) - 1] = '\0';
        }

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
          free(file_name);
          token = strtok(NULL, " ");
          continue;
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
            continue;
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
          uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
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

        free(file_name);
        token = strtok(NULL, " ");
      }
    }
    else if (isCommand("mv", line))
    {
      // Check that there's a file system mounted
      if (fat == NULL)
      {
        printf("No file system mounted. Try again.\n");
        continue;
      }

      char *source_name;
      char *dest_name;

      // Get the source file name
      char *token = strtok(line, " ");
      token = strtok(NULL, " ");
      if (token == NULL || strlen(token) < 1)
      {
        printf("Make sure to add a source filename. Try again\n");
        continue;
      }
      if (strlen(token) > 33)
      {
        printf("Source file name too long. Try again\n");
        continue;
      }
      source_name = malloc(sizeof(char) * (strlen(token) - 1));
      strcpy(source_name, token);
      if (source_name[strlen(token) - 1] == 0x0a)
      {
        source_name[strlen(token) - 1] = '\0';
      }

      // Get the dest file name
      token = strtok(NULL, " ");
      if (token == NULL || strlen(token) <= 1)
      {
        printf("Make sure to add a destination filename. Try again\n");
        continue;
      }
      if (strlen(token) > 33)
      {
        printf("Destination file name too long. Try again\n");
        continue;
      }
      dest_name = malloc(sizeof(char) * (strlen(token) - 1));
      strcpy(dest_name, token);
      if (dest_name[strlen(token) - 1] == 0x0a)
      {
        dest_name[strlen(token) - 1] = '\0';
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
        continue;
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
          int len = strlen(dest_name);
          found_file = strncmp(dest_name, curr_file, len) == 0;
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
          int len = strlen(dest_name);
          found_file = strncmp(dest_name, curr_file, len) == 0;
          if (found_file)
          {
            break;
          }
          directory_num++;
        }
      }

      // Check if file isn't found
      if (found_file)
      {
        // Move to correct block in root directory and write name
        int offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
        fat[offset] = (fat[offset] & 0xFF00) | 0x0001;

        // Find the first block and file size of the data in the source file
        offset += 16;
        int file_size = 0;
        unsigned b0 = (fat[offset] & 0xff00);
        unsigned b1 = (fat[offset] & 0xff);
        unsigned b2 = (fat[offset + 1] & 0xff00);
        unsigned b3 = (fat[offset + 1] & 0xff);
        file_size = ((b3 | b2) << 16) | (b1 | b0);

        // Move to place to read first block if it is defined
        offset += 2;
        int first_block = fat[offset];
        fat[offset] = 0x0000;

        // Delete all file data
        if (file_size != 0)
        {
          int i = 0;
          int j = 0;
          int curr_block = first_block;
          int block_num = first_block - 1;
          offset = ENTRIES_IN_FAT + block_size * block_num / 2;
          int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
          int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
          int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
          while (i < block_size && j < file_size)
          {
            // Delete data
            while (j < file_size && i < block_size)
            {
              fat[offset + i / 2] = 0x0000;
              i += 2;
              j += 2;
            }

            // Check if block has overflown. If so, move to next free block
            if (i >= block_size && block_num < BLOCKS_IN_DATA && j < file_size)
            {
              fat[curr_block] = 0x0000;
              curr_block++;
              block_num = curr_block - 1;
              offset = ENTRIES_IN_FAT + block_size * block_num / 2;
              i = 0;
            }
          }

          while (fat[curr_block] != 0xFFFF)
          {
            fat[curr_block] = 0x0000;
            curr_block++;
          }
          fat[curr_block] = 0x0000;
        }
      }

      // Search for the specified file starting in the root directory
      found_file = false;
      blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
      ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
      start_block = 1;
      root_dir_path = fat[start_block];
      directory_num = 0;
      max_directories_num = block_size / 64;
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
        continue;
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
        uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
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

      free(source_name);
      free(dest_name);
    }
    else if (isCommand("rm", line))
    {
      // Check that there's a file system mounted
      if (fat == NULL)
      {
        printf("No file system mounted. Try again.\n");
        continue;
      }

      char *file_name;

      char *token = strtok(line, " ");
      token = strtok(NULL, " ");
      if (token == NULL || strlen(token) <= 1)
      {
        printf("Make sure to add a filename. Try again\n");
        continue;
      }

      while (token != NULL)
      {
        // Get the file name
        if (token == NULL)
        {
          continue;
        }
        if (strlen(token) <= 1)
        {
          printf("Make sure to add a filename. Try again\n");
          continue;
        }
        if (strlen(token) > 33)
        {
          printf("File name too long. Try again\n");
          continue;
        }
        file_name = malloc(sizeof(char) * (strlen(token) - 1));
        strcpy(file_name, token);
        if (file_name[strlen(token) - 1] == 0x0a)
        {
          file_name[strlen(token) - 1] = '\0';
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
          continue;
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

          // Find the first block and file size of the data in the source file
          offset += 16;
          int file_size = 0;
          unsigned b0 = (fat[offset] & 0xff00);
          unsigned b1 = (fat[offset] & 0xff);
          unsigned b2 = (fat[offset + 1] & 0xff00);
          unsigned b3 = (fat[offset + 1] & 0xff);
          file_size = ((b3 | b2) << 16) | (b1 | b0);

          // Move to place to read first block if it is defined
          offset += 2;
          int first_block = fat[offset];
          fat[offset] = 0x0000;

          // Delete all file data
          if (file_size != 0)
          {
            int i = 0;
            int j = 0;
            int curr_block = first_block;
            int block_num = first_block - 1;
            offset = ENTRIES_IN_FAT + block_size * block_num / 2;
            int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
            int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
            int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
            while (i < block_size && j < file_size)
            {
              // Delete data
              while (j < file_size && i < block_size)
              {
                fat[offset + i / 2] = 0x0000;
                i += 2;
                j += 2;
              }

              // Check if block has overflown. If so, move to next free block
              if (i >= block_size && block_num < BLOCKS_IN_DATA && j < file_size)
              {
                fat[curr_block] = 0x0000;
                curr_block++;
                block_num = curr_block - 1;
                offset = ENTRIES_IN_FAT + block_size * block_num / 2;
                i = 0;
              }
            }

            while (fat[curr_block] != 0xFFFF)
            {
              fat[curr_block] = 0x0000;
              curr_block++;
            }
            fat[curr_block] = 0x0000;
          }
        }

        free(file_name);
        token = strtok(NULL, " ");
      }
    }
    else if (isCommand("cat", line))
    {
      // Check that there's a file system mounted
      if (fat == NULL)
      {
        printf("No file system mounted. Try again.\n");
        continue;
      }

      char *arg1;

      // Get the first argument (either FILE, -w, or -a)
      char *token = strtok(line, " ");
      token = strtok(NULL, " ");
      if (token == NULL || strlen(token) < 1)
      {
        printf("Invalid arguments. Try again with one of the following formats:\ncat FILE ... [ -w OUTPUT_FILE ]\ncat FILE ... [ -a OUTPUT_FILE ]\ncat -w OUTPUT_FILE\ncat -a OUTPUT_FILE");
        continue;
      }
      arg1 = malloc(sizeof(char) * (strlen(token) - 1));
      strcpy(arg1, token);
      if (arg1[strlen(token) - 1] == 0x0a)
      {
        arg1[strlen(token) - 1] = '\0';
      }

      // cat -w OUTPUT_FILE
      if (strncmp(arg1, "-w", strlen(arg1)) == 0)
      {
        char *dest_name;

        // Get the dest file name
        token = strtok(NULL, " ");
        if (token == NULL || strlen(token) <= 1)
        {
          printf("Make sure to add a destination filename. Try again\n");
          continue;
        }
        if (strlen(token) > 33)
        {
          printf("Destination file name too long. Try again\n");
          continue;
        }
        dest_name = malloc(sizeof(char) * (strlen(token) - 1));
        strcpy(dest_name, token);
        if (dest_name[strlen(token) - 1] == 0x0a)
        {
          dest_name[strlen(token) - 1] = '\0';
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
          continue;
        }

        // Get user input
        char *file_data = NULL;
        size_t file_size_t = 0;
        getline(&file_data, &file_size_t, stdin);
        int file_size = strlen(file_data);

        // Check if destination file already exists
        int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
        int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
        int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
        int found_file = false;
        int start_block = 1;
        int root_dir_path = fat[start_block];
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
            int len = strlen(dest_name);
            found_file = strncmp(dest_name, curr_file, len) == 0;
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

        // Check the final directory to see if the destination file is there
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
            int len = strlen(dest_name);
            found_file = strncmp(dest_name, curr_file, len) == 0;
            if (found_file)
            {
              break;
            }
            directory_num++;
          }
        }

        // Check if file already exists
        if (!found_file)
        {
          // Scan to find open directories to place new dest file in starting in original block
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

          // Check the final directory to see if there's an empty directory slot
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
              continue;
            }

            // Once we've found the next open block, update FAT region
            block_num++;
            fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
            start_block = block_num;
            fat[block_num] = 0xFFFF;
            directory_num = 0;
          }
        }

        // Move to destination file and write name
        int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
        int i = 0;
        if (strlen(dest_name) < 2)
        {
          uint16_t c = dest_name[i] << 8;
          fat[file_offset + i] = c;
        }
        while (i < strlen(dest_name))
        {
          char c1 = dest_name[i];
          char c2 = dest_name[i + 1];
          uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
          fat[file_offset + i / 2] = c;
          i += 2;
        }
        if (i == strlen(dest_name) - 1)
        {
          char c1 = dest_name[i];
          uint16_t c = c1 << 8 & 0xFF00;
          fat[file_offset + i / 2] = c;
        }
        // Fill in remainders with 0's
        while (fat[file_offset + i / 2] != 0 && i < 18)
        {
          fat[file_offset + i / 2] = 0;
          i += 2;
        }

        // Move to place to write file size
        file_offset += 16;
        unsigned sizeb0 = (file_size & 0xff);
        unsigned sizeb1 = (file_size & 0xff00);
        unsigned sizeb2 = (file_size & 0xff0000) >> 16u;
        unsigned sizeb3 = (file_size & 0xff000000) >> 16u;
        fat[file_offset] = sizeb0 | sizeb1;
        fat[file_offset + 1] = sizeb2 | sizeb3;

        // Move to place to read first block if it is defined
        file_offset += 2;
        int first_block = fat[file_offset];

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
            continue;
          }
          first_block = block_num + 1;
        }

        // Move to place to write first block
        fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);

        // Move to place to write type and permissions
        file_offset += 1;
        fat[file_offset] = 0x0601;

        // Move to place to write time
        file_offset += 1;
        time_t mtime = time(NULL);

        unsigned time_b0 = (mtime & 0x00000000000000ff);
        unsigned time_b1 = (mtime & 0x000000000000ff00);
        unsigned time_b2 = (mtime & 0x0000000000ff0000) >> 16u;
        unsigned time_b3 = (mtime & 0x00000000ff000000) >> 16u;
        unsigned time_b4 = (mtime & 0x000000ff00000000) >> 32u;
        unsigned time_b5 = (mtime & 0x0000ff0000000000) >> 32u;
        unsigned time_b6 = (mtime & 0x00ff000000000000) >> 48u;
        unsigned time_b7 = (mtime & 0xff00000000000000) >> 48u;

        fat[file_offset] = time_b0 | time_b1;
        fat[file_offset + 1] = time_b2 | time_b3;
        fat[file_offset + 2] = time_b4 | time_b5;
        fat[file_offset + 3] = time_b6 | time_b7;

        free(dest_name);

        // Move to first block in root directory and write data
        int block_num = first_block - 1;
        int offset = ENTRIES_IN_FAT + block_size * block_num / 2;
        i = 0;
        int j = 0;
        int curr_block = first_block;
        while (i < block_size && j < strlen(file_data))
        {
          // Write data
          if (strlen(file_data) < 2)
          {
            uint16_t c = file_data[i] << 8;
            fat[offset + i] = c;
          }
          while (i < strlen(file_data))
          {
            char c1 = file_data[i];
            char c2 = file_data[i + 1];
            uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
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
          if (i > block_size && block_num < BLOCKS_IN_DATA)
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

        // Change any remaining pointers in FAT to be undefined if necessary and restore zeroes
        if (i % 2 == 1)
        {
          fat[offset + i / 2] = (fat[offset] & 0xFF);
          i++;
          offset++;
        }
        for (int k = 0; k < block_size; k++)
        {
          fat[offset + i / 2] = 0x0000;
          i++;
        }
        fat[curr_block] = 0xFFFF;
        curr_block++;
        while (fat[curr_block] != 0xFFFF)
        {
          for (int k = 0; k < block_size; k++)
          {
            fat[offset + i / 2] = 0x0000;
            i++;
          }
          fat[curr_block] = 0x0000;
          curr_block++;
        }
        fat[curr_block] = 0x0000;
        for (int k = 0; k < block_size; k++)
        {
          fat[offset + i / 2] = 0x0000;
          i++;
        }
      }

      // cat -a OUTPUT_FILE
      else if (strncmp(arg1, "-a", strlen(arg1)) == 0)
      {
        char *dest_name;

        // Get the dest file name
        token = strtok(NULL, " ");
        if (token == NULL || strlen(token) <= 1)
        {
          printf("Make sure to add a destination filename. Try again\n");
          continue;
        }
        if (strlen(token) > 33)
        {
          printf("Destination file name too long. Try again\n");
          continue;
        }
        dest_name = malloc(sizeof(char) * (strlen(token) - 1));
        strcpy(dest_name, token);
        if (dest_name[strlen(token) - 1] == 0x0a)
        {
          dest_name[strlen(token) - 1] = '\0';
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
          continue;
        }

        // Get user input
        char *file_data = NULL;
        size_t file_size_t = 0;
        getline(&file_data, &file_size_t, stdin);
        int input_file_size = strlen(file_data);

        // Check if destination file already exists
        int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
        int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
        int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
        int found_file = false;
        int start_block = 1;
        int root_dir_path = fat[start_block];
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
            int len = strlen(dest_name);
            found_file = strncmp(dest_name, curr_file, len) == 0;
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

        // Check the final directory to see if the destination file is there
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
            int len = strlen(dest_name);
            found_file = strncmp(dest_name, curr_file, len) == 0;
            if (found_file)
            {
              break;
            }
            directory_num++;
          }
        }

        // Check if file already exists
        if (!found_file)
        {
          // Scan to find open directories to place new dest file in starting in original block
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

          // Check the final directory to see if there's an empty directory slot
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
              continue;
            }

            // Once we've found the next open block, update FAT region
            block_num++;
            fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
            start_block = block_num;
            fat[block_num] = 0xFFFF;
            directory_num = 0;
          }
        }

        // Move to destination file and write name
        int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
        int i = 0;
        if (strlen(dest_name) < 2)
        {
          uint16_t c = dest_name[i] << 8;
          fat[file_offset + i] = c;
        }
        while (i < strlen(dest_name))
        {
          char c1 = dest_name[i];
          char c2 = dest_name[i + 1];
          uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
          fat[file_offset + i / 2] = c;
          i += 2;
        }
        if (i == strlen(dest_name) - 1)
        {
          char c1 = dest_name[i];
          uint16_t c = c1 << 8 & 0xFF00;
          fat[file_offset + i / 2] = c;
        }
        // Fill in remainders with 0's
        while (fat[file_offset + i / 2] != 0 && i < 18)
        {
          fat[file_offset + i / 2] = 0;
          i += 2;
        }

        // Find the first block and file size of the data in the source file
        file_offset += 16;
        int file_size = 0;
        unsigned b0 = (fat[file_offset] & 0xff00);
        unsigned b1 = (fat[file_offset] & 0xff);
        unsigned b2 = (fat[file_offset + 1] & 0xff00);
        unsigned b3 = (fat[file_offset + 1] & 0xff);
        file_size = ((b3 | b2) << 16) | (b1 | b0);

        // Write adjusted file size
        unsigned sizeb0 = ((file_size + input_file_size) & 0xff);
        unsigned sizeb1 = ((file_size + input_file_size) & 0xff00);
        unsigned sizeb2 = ((file_size + input_file_size) & 0xff0000) >> 16u;
        unsigned sizeb3 = ((file_size + input_file_size) & 0xff000000) >> 16u;
        fat[file_offset] = sizeb0 | sizeb1;
        fat[file_offset + 1] = sizeb2 | sizeb3;

        // Move to place to read first block if it is defined
        file_offset += 2;
        int first_block = fat[file_offset];

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
            continue;
          }
          first_block = block_num + 1;
        }

        // Move to place to write first block
        fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);

        // Move to place to write type and permissions
        file_offset += 1;
        fat[file_offset] = 0x0601;

        // Move to place to write time
        file_offset += 1;
        time_t mtime = time(NULL);

        unsigned time_b0 = (mtime & 0x00000000000000ff);
        unsigned time_b1 = (mtime & 0x000000000000ff00);
        unsigned time_b2 = (mtime & 0x0000000000ff0000) >> 16u;
        unsigned time_b3 = (mtime & 0x00000000ff000000) >> 16u;
        unsigned time_b4 = (mtime & 0x000000ff00000000) >> 32u;
        unsigned time_b5 = (mtime & 0x0000ff0000000000) >> 32u;
        unsigned time_b6 = (mtime & 0x00ff000000000000) >> 48u;
        unsigned time_b7 = (mtime & 0xff00000000000000) >> 48u;

        fat[file_offset] = time_b0 | time_b1;
        fat[file_offset + 1] = time_b2 | time_b3;
        fat[file_offset + 2] = time_b4 | time_b5;
        fat[file_offset + 3] = time_b6 | time_b7;

        free(dest_name);

        // Move to first block in root directory and write data
        int block_num = first_block - 1;
        int offset = ENTRIES_IN_FAT + block_size * block_num / 2 + file_size / 2;
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
          }
          while (i < strlen(file_data))
          {
            char c1 = file_data[i];
            char c2 = file_data[i + 1];
            uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
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
          if (i > block_size && block_num < BLOCKS_IN_DATA)
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
      }

      // cat FILE ... [ -w OUTPUT_FILE ] or cat FILE ... [ -a OUTPUT_FILE ]
      else
      {
        char *file_name;
        char *all_file_data = "\0";

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
          continue;
        }

        bool has_output = false;
        while (token != NULL)
        {
          // Get the file name
          if (token == NULL)
          {
            continue;
          }
          if (strlen(token) <= 1)
          {
            printf("Make sure to add a filename. Try again\n");
            continue;
          }
          if (strlen(token) > 33)
          {
            printf("File name too long. Try again\n");
            continue;
          }
          file_name = malloc(sizeof(char) * (strlen(token) - 1));
          strcpy(file_name, token);
          if (file_name[strlen(token) - 1] == 0x0a)
          {
            file_name[strlen(token) - 1] = '\0';
          }

          // check if we've reached -w
          if (strncmp(file_name, "-w", strlen(file_name)) == 0)
          {
            char *dest_name;
            has_output = true;

            // Get the dest file name
            token = strtok(NULL, " ");
            if (token == NULL || strlen(token) <= 1)
            {
              printf("Make sure to add a destination filename. Try again\n");
              continue;
            }
            if (strlen(token) > 33)
            {
              printf("Destination file name too long. Try again\n");
              continue;
            }
            dest_name = malloc(sizeof(char) * (strlen(token) - 1));
            strcpy(dest_name, token);
            if (dest_name[strlen(token) - 1] == 0x0a)
            {
              dest_name[strlen(token) - 1] = '\0';
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
              continue;
            }

            // Check if destination file already exists
            int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
            int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
            int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
            int found_file = false;
            int start_block = 1;
            int root_dir_path = fat[start_block];
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
                int len = strlen(dest_name);
                found_file = strncmp(dest_name, curr_file, len) == 0;
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

            // Check the final directory to see if the destination file is there
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
                int len = strlen(dest_name);
                found_file = strncmp(dest_name, curr_file, len) == 0;
                if (found_file)
                {
                  break;
                }
                directory_num++;
              }
            }

            // Check if file already exists
            if (!found_file)
            {
              // Scan to find open directories to place new dest file in starting in original block
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

              // Check the final directory to see if there's an empty directory slot
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
                  continue;
                }

                // Once we've found the next open block, update FAT region
                block_num++;
                fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
                start_block = block_num;
                fat[block_num] = 0xFFFF;
                directory_num = 0;
              }
            }

            // Move to destination file and write name
            int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
            int i = 0;
            if (strlen(dest_name) < 2)
            {
              uint16_t c = dest_name[i] << 8;
              fat[file_offset + i] = c;
            }
            while (i < strlen(dest_name))
            {
              char c1 = dest_name[i];
              char c2 = dest_name[i + 1];
              uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
              fat[file_offset + i / 2] = c;
              i += 2;
            }
            if (i == strlen(dest_name) - 1)
            {
              char c1 = dest_name[i];
              uint16_t c = c1 << 8 & 0xFF00;
              fat[file_offset + i / 2] = c;
            }
            // Fill in remainders with 0's
            while (fat[file_offset + i / 2] != 0 && i < 18)
            {
              fat[file_offset + i / 2] = 0;
              i += 2;
            }

            // Move to place to write file size
            int file_size = strlen(all_file_data);
            file_offset += 16;
            unsigned sizeb0 = (file_size & 0xff);
            unsigned sizeb1 = (file_size & 0xff00);
            unsigned sizeb2 = (file_size & 0xff0000) >> 16u;
            unsigned sizeb3 = (file_size & 0xff000000) >> 16u;
            fat[file_offset] = sizeb0 | sizeb1;
            fat[file_offset + 1] = sizeb2 | sizeb3;

            // Move to place to read first block if it is defined
            file_offset += 2;
            int first_block = fat[file_offset];

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
                continue;
              }
              first_block = block_num + 1;
            }

            // Move to place to write first block
            fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);

            // Move to place to write type and permissions
            file_offset += 1;
            fat[file_offset] = 0x0601;

            // Move to place to write time
            file_offset += 1;
            time_t mtime = time(NULL);

            unsigned time_b0 = (mtime & 0x00000000000000ff);
            unsigned time_b1 = (mtime & 0x000000000000ff00);
            unsigned time_b2 = (mtime & 0x0000000000ff0000) >> 16u;
            unsigned time_b3 = (mtime & 0x00000000ff000000) >> 16u;
            unsigned time_b4 = (mtime & 0x000000ff00000000) >> 32u;
            unsigned time_b5 = (mtime & 0x0000ff0000000000) >> 32u;
            unsigned time_b6 = (mtime & 0x00ff000000000000) >> 48u;
            unsigned time_b7 = (mtime & 0xff00000000000000) >> 48u;

            fat[file_offset] = time_b0 | time_b1;
            fat[file_offset + 1] = time_b2 | time_b3;
            fat[file_offset + 2] = time_b4 | time_b5;
            fat[file_offset + 3] = time_b6 | time_b7;

            free(dest_name);

            // Move to first block in root directory and write data
            int block_num = first_block - 1;
            int offset = ENTRIES_IN_FAT + block_size * block_num / 2;
            i = 0;
            int j = 0;
            int curr_block = first_block;
            while (i < block_size && j < strlen(all_file_data))
            {
              // Write data
              if (strlen(all_file_data) < 2)
              {
                uint16_t c = all_file_data[i] << 8;
                fat[offset + i] = c;
              }
              while (i < strlen(all_file_data))
              {
                char c1 = all_file_data[i];
                char c2 = all_file_data[i + 1];
                uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
                fat[offset + i / 2] = c;
                i += 2;
              }
              if (i == strlen(all_file_data) - 1)
              {
                char c1 = all_file_data[i];
                uint16_t c = c1 << 8 & 0xFF00;
                fat[offset + i / 2] = c;
              }
              j = i;

              // Check if block has overflown. If so, move to next free block
              if (i > block_size && block_num < BLOCKS_IN_DATA)
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
            while (fat[curr_block] != 0xFFFF)
            {
              fat[curr_block] = 0x0000;
              curr_block++;
            }
            fat[curr_block] = 0x0000;

            break;
          }

          // check if we've reached -a
          else if (strncmp(file_name, "-a", strlen(file_name)) == 0)
          {
            char *dest_name;
            has_output = true;

            // Get the dest file name
            token = strtok(NULL, " ");
            if (token == NULL || strlen(token) <= 1)
            {
              printf("Make sure to add a destination filename. Try again\n");
              continue;
            }
            if (strlen(token) > 33)
            {
              printf("Destination file name too long. Try again\n");
              continue;
            }
            dest_name = malloc(sizeof(char) * (strlen(token) - 1));
            strcpy(dest_name, token);
            if (dest_name[strlen(token) - 1] == 0x0a)
            {
              dest_name[strlen(token) - 1] = '\0';
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
              continue;
            }

            // Check if destination file already exists
            int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
            int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
            int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
            int found_file = false;
            int start_block = 1;
            int root_dir_path = fat[start_block];
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
                int len = strlen(dest_name);
                found_file = strncmp(dest_name, curr_file, len) == 0;
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

            // Check the final directory to see if the destination file is there
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
                int len = strlen(dest_name);
                found_file = strncmp(dest_name, curr_file, len) == 0;
                if (found_file)
                {
                  break;
                }
                directory_num++;
              }
            }

            // Check if file already exists
            if (!found_file)
            {
              // Scan to find open directories to place new dest file in starting in original block
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

              // Check the final directory to see if there's an empty directory slot
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
                  continue;
                }

                // Once we've found the next open block, update FAT region
                block_num++;
                fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
                start_block = block_num;
                fat[block_num] = 0xFFFF;
                directory_num = 0;
              }
            }

            // Move to destination file and write name
            int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
            int i = 0;
            if (strlen(dest_name) < 2)
            {
              uint16_t c = dest_name[i] << 8;
              fat[file_offset + i] = c;
            }
            while (i < strlen(dest_name))
            {
              char c1 = dest_name[i];
              char c2 = dest_name[i + 1];
              uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
              fat[file_offset + i / 2] = c;
              i += 2;
            }
            if (i == strlen(dest_name) - 1)
            {
              char c1 = dest_name[i];
              uint16_t c = c1 << 8 & 0xFF00;
              fat[file_offset + i / 2] = c;
            }
            // Fill in remainders with 0's
            while (fat[file_offset + i / 2] != 0 && i < 18)
            {
              fat[file_offset + i / 2] = 0;
              i += 2;
            }

            // Find the first block and file size of the data in the source file
            file_offset += 16;
            int file_size = 0;
            unsigned b0 = (fat[file_offset] & 0xff00);
            unsigned b1 = (fat[file_offset] & 0xff);
            unsigned b2 = (fat[file_offset + 1] & 0xff00);
            unsigned b3 = (fat[file_offset + 1] & 0xff);
            file_size = ((b3 | b2) << 16) | (b1 | b0);

            // Write adjusted file size
            int input_file_size = strlen(all_file_data);
            unsigned sizeb0 = ((file_size + input_file_size) & 0xff);
            unsigned sizeb1 = ((file_size + input_file_size) & 0xff00);
            unsigned sizeb2 = ((file_size + input_file_size) & 0xff0000) >> 16u;
            unsigned sizeb3 = ((file_size + input_file_size) & 0xff000000) >> 16u;
            fat[file_offset] = sizeb0 | sizeb1;
            fat[file_offset + 1] = sizeb2 | sizeb3;

            // Move to place to read first block if it is defined
            file_offset += 2;
            int first_block = fat[file_offset];

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
                continue;
              }
              first_block = block_num + 1;
            }

            // Move to place to write first block
            fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);

            // Move to place to write type and permissions
            file_offset += 1;
            fat[file_offset] = 0x0601;

            // Move to place to write time
            file_offset += 1;
            time_t mtime = time(NULL);

            unsigned time_b0 = (mtime & 0x00000000000000ff);
            unsigned time_b1 = (mtime & 0x000000000000ff00);
            unsigned time_b2 = (mtime & 0x0000000000ff0000) >> 16u;
            unsigned time_b3 = (mtime & 0x00000000ff000000) >> 16u;
            unsigned time_b4 = (mtime & 0x000000ff00000000) >> 32u;
            unsigned time_b5 = (mtime & 0x0000ff0000000000) >> 32u;
            unsigned time_b6 = (mtime & 0x00ff000000000000) >> 48u;
            unsigned time_b7 = (mtime & 0xff00000000000000) >> 48u;

            fat[file_offset] = time_b0 | time_b1;
            fat[file_offset + 1] = time_b2 | time_b3;
            fat[file_offset + 2] = time_b4 | time_b5;
            fat[file_offset + 3] = time_b6 | time_b7;

            free(dest_name);

            // Move to first block in root directory and write data
            int block_num = first_block - 1;
            int offset = ENTRIES_IN_FAT + block_size * block_num / 2 + file_size / 2;
            char c1 = all_file_data[0];
            fat[offset] = c1 << 8 | (fat[offset] & 0xFF);
            i = 1;
            offset++;
            int j = 1;
            int curr_block = first_block;
            while (i < block_size && j < (file_size + input_file_size))
            {
              // Write data
              if (strlen(all_file_data) < 2)
              {
                uint16_t c = all_file_data[j] << 8;
                fat[offset + i] = c;
              }
              while (i < block_size && j < (file_size + input_file_size) && fat[offset + i / 2] == 0)
              {
                char c1 = all_file_data[j];
                char c2 = all_file_data[j + 1];
                uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
                fat[offset + i / 2] = c;
                i += 2;
                j += 2;
              }
              if (i == strlen(all_file_data) - 1)
              {
                char c1 = all_file_data[j];
                uint16_t c = c1 << 8 & 0xFF00;
                fat[offset + i / 2] = c;
              }

              // Check if block has overflown. If so, move to next free block
              if (j < (file_size + input_file_size) && block_num < BLOCKS_IN_DATA)
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
                offset = ENTRIES_IN_FAT + block_size * block_num / 2;

                // Update FAT region to contain new pointers
                fat[curr_block] = ((block_num + 1) & 0x00FF) | ((block_num + 1) & 0xFF00);
                curr_block += 2;
              }
            }

            // Change any remaining pointers in FAT to be undefined if necessary
            fat[curr_block] = 0xFFFF;
            curr_block++;
            break;
          }

          // Continue concatenating file data
          else
          {
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
                found_file = strncmp(curr_file, file_name, len) == 0;
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
              free(file_name);
              token = strtok(NULL, " ");
              continue;
            }
            else
            {
              // Find the first block and file size of the data in the source file
              int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + 16;
              int file_size = 0;
              unsigned b0 = (fat[file_offset] & 0xff00);
              unsigned b1 = (fat[file_offset] & 0xff);
              unsigned b2 = (fat[file_offset + 1] & 0xff00);
              unsigned b3 = (fat[file_offset + 1] & 0xff);
              file_size = ((b3 | b2) << 16) | (b1 | b0);
              file_offset += 2;
              int first_block = (fat[file_offset] & 0xFF00) | (fat[file_offset] & 0x00FF);

              // Move to block with file data and read data
              char file_data[file_size];
              int offset = ENTRIES_IN_FAT + block_size * (first_block - 1) / 2;
              start_block = first_block;
              root_dir_path = fat[start_block];
              int byte_counter = 0;
              while (byte_counter < file_size)
              {
                char c1 = (fat[offset] & 0xff);
                char c2 = (fat[offset] & 0xff00) >> 8;
                file_data[byte_counter] = c1;
                file_data[byte_counter + 1] = c2;
                byte_counter += 2;
                offset += 1;

                // If we've maxed out this block, then move to the next listed block
                if (root_dir_path != 0xFFFF && byte_counter == block_size)
                {
                  start_block = root_dir_path;
                  root_dir_path = fat[start_block];
                }
              }

              char *temp = strcpy(malloc(sizeof(char) * strlen(all_file_data)), all_file_data);
              all_file_data = malloc(sizeof(char) * strlen(file_data) + sizeof(char) * strlen(all_file_data));
              strcpy(all_file_data, temp);
              strcat(all_file_data, file_data);

              free(file_name);
              token = strtok(NULL, " ");
            }
          }
        }

        if (!has_output)
        {
          printf("%s\n", all_file_data);
        }
      }
    }
    else if (isCommand("cp", line))
    {
      // Check that there's a file system mounted
      if (fat == NULL)
      {
        printf("No file system mounted. Try again.\n");
        continue;
      }

      char *arg1;
      char *arg2;

      // Get the first argument (either -h or SOURCE)
      char *token = strtok(line, " ");
      token = strtok(NULL, " ");
      if (token == NULL || strlen(token) < 1)
      {
        printf("Invalid arguments. Try again with one of the following formats:\ncp [ -h ] SOURCE DEST\ncp SOURCE -h DEST\n");
        continue;
      }
      arg1 = malloc(sizeof(char) * (strlen(token) - 1));
      strcpy(arg1, token);
      if (arg1[strlen(token) - 1] == 0x0a)
      {
        arg1[strlen(token) - 1] = '\0';
      }

      // Get the second argument (one of -h, SOURCE, or DEST)
      token = strtok(NULL, " ");
      if (token == NULL || strlen(token) <= 1)
      {
        printf("Invalid arguments. Try again with one of the following formats:\ncp [ -h ] SOURCE DEST\ncp SOURCE -h DEST\n");
        continue;
      }
      arg2 = malloc(sizeof(char) * (strlen(token) - 1));
      strcpy(arg2, token);
      if (arg2[strlen(token) - 1] == 0x0a)
      {
        arg2[strlen(token) - 1] = '\0';
      }

      // cp -h SOURCE DEST (source on host to dest on fat)
      if (strncmp(arg1, "-h", strlen(arg1)) == 0)
      {
        char *source_file = arg2;
        char *dest_name;

        // Get the dest file name
        token = strtok(NULL, " ");
        if (token == NULL || strlen(token) <= 1)
        {
          printf("Make sure to add a destination filename. Try again\n");
          continue;
        }
        if (strlen(token) > 33)
        {
          printf("Destination file name too long. Try again\n");
          continue;
        }
        dest_name = malloc(sizeof(char) * (strlen(token) - 1));
        strcpy(dest_name, token);
        if (dest_name[strlen(token) - 1] == 0x0a)
        {
          dest_name[strlen(token) - 1] = '\0';
        }

        // Open the file from the host system
        FILE *source_fd = fopen(source_file, "r");
        if (source_fd == NULL)
        {
          printf("Invalid source file name. Try again.\n");
          continue;
        }

        // Get the length of the file
        long file_size;
        fseek(source_fd, 0L, SEEK_END);
        file_size = ftell(source_fd);
        fseek(source_fd, 0L, SEEK_SET);

        // Read data from host file
        char *source_data = malloc(sizeof(uint8_t) * file_size + 1);
        fread(source_data, sizeof(char), file_size + 1, source_fd);

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
          continue;
        }

        // Check if file already exists
        int blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
        int ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
        int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
        bool found_file = false;
        int start_block = 1;
        uint16_t root_dir_path = fat[1];
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
            int len = strlen(dest_name);
            found_file = strncmp(dest_name, curr_file, len) == 0;
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
            int len = strlen(dest_name);
            found_file = strncmp(dest_name, curr_file, len) == 0;
            if (found_file)
            {
              break;
            }
            directory_num++;
          }
        }

        // Check if file already exists
        if (!found_file)
        {
          // Scan to find open directories starting in original block
          start_block = 1;
          root_dir_path = fat[1];
          directory_num = 0;
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
              continue;
            }

            // Once we've found the next open block, update FAT region
            block_num++;
            fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
            start_block = block_num;
            fat[block_num] = 0xFFFF;
            directory_num = 0;
          }
        }

        // Move to first open block in root directory and write name
        int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
        int i = 0;
        if (strlen(dest_name) < 2)
        {
          uint16_t c = dest_name[i] << 8;
          fat[file_offset + i] = c;
        }
        while (i < strlen(dest_name))
        {
          char c1 = dest_name[i];
          char c2 = dest_name[i + 1];
          uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
          fat[file_offset + i / 2] = c;
          i += 2;
        }
        if (i == strlen(dest_name) - 1)
        {
          char c1 = dest_name[i];
          uint16_t c = c1 << 8 & 0xFF00;
          fat[file_offset + i / 2] = c;
        }
        // Fill in remainders with 0's
        while (fat[file_offset + i / 2] != 0 && i < 18)
        {
          fat[file_offset + i / 2] = 0;
          i += 2;
        }

        // Move to place to write file size
        file_offset += 16;
        unsigned b0 = (file_size & 0xff);
        unsigned b1 = (file_size & 0xff00);
        unsigned b2 = (file_size & 0xff0000) >> 16u;
        unsigned b3 = (file_size & 0xff000000) >> 16u;
        fat[file_offset] = b0 | b1;
        fat[file_offset + 1] = b2 | b3;

        // Find first open block to write file data
        int block_num = 0;
        while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
        {
          block_num++;
        }
        if (block_num == BLOCKS_IN_DATA)
        {
          printf("File system is full. Remove a file and try again.\n");
          continue;
        }
        int first_block = block_num + 1;

        // Move to place to write first block
        file_offset += 2;
        fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);

        // Move to place to write type and permissions
        file_offset += 1;
        fat[file_offset] = 0x0601;

        // Move to place to write time
        file_offset += 1;
        time_t mtime = time(NULL);

        unsigned time_b0 = (mtime & 0x00000000000000ff);
        unsigned time_b1 = (mtime & 0x000000000000ff00);
        unsigned time_b2 = (mtime & 0x0000000000ff0000) >> 16u;
        unsigned time_b3 = (mtime & 0x00000000ff000000) >> 16u;
        unsigned time_b4 = (mtime & 0x000000ff00000000) >> 32u;
        unsigned time_b5 = (mtime & 0x0000ff0000000000) >> 32u;
        unsigned time_b6 = (mtime & 0x00ff000000000000) >> 48u;
        unsigned time_b7 = (mtime & 0xff00000000000000) >> 48u;

        fat[file_offset] = time_b0 | time_b1;
        fat[file_offset + 1] = time_b2 | time_b3;
        fat[file_offset + 2] = time_b4 | time_b5;
        fat[file_offset + 3] = time_b6 | time_b7;

        free(dest_name);

        // Move to first open block in root directory and write data
        block_num = first_block - 1;
        int offset = ENTRIES_IN_FAT + block_size * block_num / 2;
        i = 0;
        int j = 0;
        int curr_block = first_block;
        while (j < block_size && i < strlen(source_data))
        {
          // Write data
          if (strlen(source_data) < 2)
          {
            uint16_t c = source_data[i] << 8;
            fat[offset + i] = c;
          }
          while (i < strlen(source_data) && j < block_size)
          {
            char c1 = source_data[i];
            char c2 = source_data[i + 1];
            uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
            fat[offset + i / 2] = c;
            i += 2;
            j += 2;
          }
          if (i == strlen(source_data) - 1)
          {
            char c1 = source_data[i];
            uint16_t c = c1 << 8 & 0xFF00;
            fat[offset + i / 2] = c;
          }
          j = i;

          // Check if block has overflown. If so, move to next free block
          if (j >= block_size && block_num < BLOCKS_IN_DATA && i < strlen(source_data))
          {
            block_num++;
            while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
            {
              block_num++;
            }
            if (block_num == BLOCKS_IN_DATA)
            {
              printf("File system is full. Remove a file and try again.\n");
              continue;
            }
            j = 0;

            // Update FAT region to contain new pointers
            fat[curr_block] = ((block_num + 1) & 0x00FF) | ((block_num + 1) & 0xFF00);
            curr_block++;
          }
        }
        // Update FAT region to contain new pointers
        fat[curr_block] = 0xFFFF;
      }

      // cp SOURCE -h DEST (source on fat to dest on HOST)
      else if (strncmp(arg2, "-h", strlen(arg1)) == 0)
      {

        char *source_name = arg1;
        char *dest_name;

        // Get the dest file name
        token = strtok(NULL, " ");
        if (token == NULL || strlen(token) < 1)
        {
          printf("Make sure to add a destination filename. Try again\n");
          continue;
        }
        if (strlen(token) > 33)
        {
          printf("Source file name too long. Try again\n");
          continue;
        }
        dest_name = malloc(sizeof(char) * (strlen(token) - 1));
        strcpy(dest_name, token);
        if (dest_name[strlen(token) - 1] == 0x0a)
        {
          dest_name[strlen(token) - 1] = '\0';
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
          continue;
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
          continue;
        }

        // Find the first block and file size of the data in the source file
        int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + 16;
        int file_size = 0;
        unsigned b0 = (fat[file_offset] & 0xff00);
        unsigned b1 = (fat[file_offset] & 0xff);
        unsigned b2 = (fat[file_offset + 1] & 0xff00);
        unsigned b3 = (fat[file_offset + 1] & 0xff);
        file_size = ((b3 | b2) << 16) | (b1 | b0);
        file_offset += 2;
        int first_block = (fat[file_offset] & 0xFF00) | (fat[file_offset] & 0x00FF);

        // Move to block with file data and read data
        char file_data[file_size];
        int offset = ENTRIES_IN_FAT + block_size * (first_block - 1) / 2;
        start_block = first_block;
        root_dir_path = fat[start_block];
        int byte_counter = 0;
        while (byte_counter < file_size)
        {
          char c1 = (fat[offset] & 0xff);
          char c2 = (fat[offset] & 0xff00) >> 8;
          file_data[byte_counter] = c1;
          file_data[byte_counter + 1] = c2;
          byte_counter += 2;
          offset += 1;

          // If we've maxed out this block, then move to the next listed block
          if (root_dir_path != 0xFFFF && byte_counter == block_size)
          {
            start_block = root_dir_path;
            root_dir_path = fat[start_block];
          }
        }

        // Open destination file on the host system
        int dest_fd = open(dest_name, O_CREAT | O_TRUNC | O_WRONLY);
        if (dest_fd == -1)
        {
          printf("Invalid destination file name. Try again.\n");
          continue;
        }

        // Copy file data from source on fat to dest file on host
        write(dest_fd, file_data, file_size);
      }

      // cp SOURCE DEST (source on fat to dest on fat)
      else
      {
        char *source_name = arg1;
        char *dest_name = arg2;

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
          continue;
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
          continue;
        }

        // Find the first block and file size of the data in the source file
        int file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2 + 16;
        int file_size = 0;
        unsigned b0 = (fat[file_offset] & 0xff00);
        unsigned b1 = (fat[file_offset] & 0xff);
        unsigned b2 = (fat[file_offset + 1] & 0xff00);
        unsigned b3 = (fat[file_offset + 1] & 0xff);
        file_size = ((b3 | b2) << 16) | (b1 | b0);
        file_offset += 2;
        int first_block = (fat[file_offset] & 0xFF00) | (fat[file_offset] & 0x00FF);

        // Move to block with file data and read data
        char file_data[file_size];
        int offset = ENTRIES_IN_FAT + block_size * (first_block - 1) / 2;
        start_block = first_block;
        root_dir_path = fat[start_block];
        int byte_counter = 0;
        while (byte_counter < file_size)
        {
          char c1 = (fat[offset] & 0xff);
          char c2 = (fat[offset] & 0xff00) >> 8;
          file_data[byte_counter] = c1;
          file_data[byte_counter + 1] = c2;
          byte_counter += 2;
          offset += 1;

          // If we've maxed out this block, then move to the next listed block
          if (root_dir_path != 0xFFFF && byte_counter == block_size)
          {
            start_block = root_dir_path;
            root_dir_path = fat[start_block];
          }
        }

        // Check if destination file already exists
        blocks_in_FAT = (fat[0] & 0xFF00) >> 8;
        ENTRIES_IN_FAT = blocks_in_FAT * block_size / 2;
        int BLOCKS_IN_DATA = ENTRIES_IN_FAT - 1;
        found_file = false;
        start_block = 1;
        root_dir_path = fat[start_block];
        directory_num = 0;
        max_directories_num = block_size / 64;
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
            int len = strlen(dest_name);
            found_file = strncmp(dest_name, curr_file, len) == 0;
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

        // Check the final directory to see if the destination file is there
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
            int len = strlen(dest_name);
            found_file = strncmp(dest_name, curr_file, len) == 0;
            if (found_file)
            {
              break;
            }
            directory_num++;
          }
        }

        // Check if file already exists
        if (!found_file)
        {
          // Scan to find open directories to place new dest file in starting in original block
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

          // Check the final directory to see if there's an empty directory slot
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
              continue;
            }

            // Once we've found the next open block, update FAT region
            block_num++;
            fat[start_block] = (block_num & 0x00FF) | (block_num & 0xFF00);
            start_block = block_num;
            fat[block_num] = 0xFFFF;
            directory_num = 0;
          }
        }

        // Move to destination file and write name
        file_offset = ENTRIES_IN_FAT + 32 * directory_num + block_size * (start_block - 1) / 2;
        int i = 0;
        if (strlen(dest_name) < 2)
        {
          uint16_t c = dest_name[i] << 8;
          fat[file_offset + i] = c;
        }
        while (i < strlen(dest_name))
        {
          char c1 = dest_name[i];
          char c2 = dest_name[i + 1];
          uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
          fat[file_offset + i / 2] = c;
          i += 2;
        }
        if (i == strlen(dest_name) - 1)
        {
          char c1 = dest_name[i];
          uint16_t c = c1 << 8 & 0xFF00;
          fat[file_offset + i / 2] = c;
        }
        // Fill in remainders with 0's
        while (fat[file_offset + i / 2] != 0 && i < 18)
        {
          fat[file_offset + i / 2] = 0;
          i += 2;
        }

        // Move to place to write file size
        file_offset += 16;
        unsigned sizeb0 = (file_size & 0xff);
        unsigned sizeb1 = (file_size & 0xff00);
        unsigned sizeb2 = (file_size & 0xff0000) >> 16u;
        unsigned sizeb3 = (file_size & 0xff000000) >> 16u;
        fat[file_offset] = sizeb0 | sizeb1;
        fat[file_offset + 1] = sizeb2 | sizeb3;

        // Find first open block to write file data
        int block_num = 0;
        while (fat[ENTRIES_IN_FAT + block_size * block_num / 2] != 0 && block_num < BLOCKS_IN_DATA)
        {
          block_num++;
        }
        if (block_num == BLOCKS_IN_DATA)
        {
          printf("File system is full. Remove a file and try again.\n");
          continue;
        }
        first_block = block_num + 1;

        // Move to place to write first block
        file_offset += 2;
        fat[file_offset] = (first_block & 0x00FF) | (first_block & 0xFF00);

        // Move to place to write type and permissions
        file_offset += 1;
        fat[file_offset] = 0x0601;

        // Move to place to write time
        file_offset += 1;
        time_t mtime = time(NULL);

        unsigned time_b0 = (mtime & 0x00000000000000ff);
        unsigned time_b1 = (mtime & 0x000000000000ff00);
        unsigned time_b2 = (mtime & 0x0000000000ff0000) >> 16u;
        unsigned time_b3 = (mtime & 0x00000000ff000000) >> 16u;
        unsigned time_b4 = (mtime & 0x000000ff00000000) >> 32u;
        unsigned time_b5 = (mtime & 0x0000ff0000000000) >> 32u;
        unsigned time_b6 = (mtime & 0x00ff000000000000) >> 48u;
        unsigned time_b7 = (mtime & 0xff00000000000000) >> 48u;

        fat[file_offset] = time_b0 | time_b1;
        fat[file_offset + 1] = time_b2 | time_b3;
        fat[file_offset + 2] = time_b4 | time_b5;
        fat[file_offset + 3] = time_b6 | time_b7;

        free(dest_name);

        // Move to first open block in root directory and write data
        block_num = first_block - 1;
        offset = ENTRIES_IN_FAT + block_size * block_num / 2;
        i = 0;
        int j = 0;
        int curr_block = first_block;
        while (i < block_size && j < strlen(file_data))
        {
          // Write data
          if (strlen(file_data) < 2)
          {
            uint16_t c = file_data[i] << 8;
            fat[offset + i] = c;
          }
          while (i < strlen(file_data))
          {
            char c1 = file_data[i];
            char c2 = file_data[i + 1];
            uint16_t c = (c2 & 0xff) << 8 | (c1 & 0xff);
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
        // Update FAT region to contain new pointers
        fat[curr_block] = 0xFFFF;
      }
    }
    else if (isCommand("ls", line))
    {
      /* NEED TO UPDATE SO THAT WE ITERATE THROUGH DIRECTORY BLOCKS AND DONT INCLUDE FILE BLOCKS*/
      // Check that there's a file system mounted
      if (fat == NULL)
      {
        printf("No file system mounted. Try again.\n");
        continue;
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
        continue;
      }

      // Open the file system
      int fd = open(fat_name, O_RDONLY);
      if (fd == -1)
      {
        printf("Invalid file name. Try again.\n");
        continue;
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
              return EXIT_FAILURE;
            }

            // Move to place to read file size
            offset = lseek(fd, 32, SEEK_CUR);
            uint32_t file_size[1];
            if (pread(fd, &file_size, sizeof(file_size), offset) == -1)
            {
              printf("Error reading file size during ls. Try again.\n");
              return EXIT_FAILURE;
            }

            // Move to place to read first block
            offset = lseek(fd, 4, SEEK_CUR);
            uint16_t firstBlock[1];
            if (pread(fd, firstBlock, sizeof(firstBlock), offset) == -1)
            {
              printf("Error reading first block during ls. Try again.\n");
              return EXIT_FAILURE;
            }

            // Move to place to read permissions
            offset = lseek(fd, 3, SEEK_CUR);
            uint8_t perm[1];
            char *perm_string;
            if (pread(fd, perm, sizeof(perm), offset) == -1)
            {
              printf("Error reading file perm during ls. Try again.\n");
              return EXIT_FAILURE;
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
              return EXIT_FAILURE;
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
              return EXIT_FAILURE;
            }

            // Move to place to read file size
            offset = lseek(fd, 32, SEEK_CUR);
            uint32_t file_size[1];
            if (pread(fd, &file_size, sizeof(file_size), offset) == -1)
            {
              printf("Error reading file size during ls. Try again.\n");
              return EXIT_FAILURE;
            }

            // Move to place to read first block
            offset = lseek(fd, 4, SEEK_CUR);
            uint16_t firstBlock[1];
            if (pread(fd, firstBlock, sizeof(firstBlock), offset) == -1)
            {
              printf("Error reading first block during ls. Try again.\n");
              return EXIT_FAILURE;
            }

            // Move to place to read permissions
            offset = lseek(fd, 3, SEEK_CUR);
            uint8_t perm[1];
            char *perm_string;
            if (pread(fd, perm, sizeof(perm), offset) == -1)
            {
              printf("Error reading file perm during ls. Try again.\n");
              return EXIT_FAILURE;
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
              return EXIT_FAILURE;
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
    else if (isCommand("chmod", line))
    {
      // Check that there's a file system mounted
      if (fat == NULL)
      {
        printf("No file system mounted. Try again.\n");
        continue;
      }

      char *new_permissions;
      char *file_name;

      // Get the new permission
      char *token = strtok(line, " ");
      token = strtok(NULL, " ");
      if (token == NULL || strlen(token) < 1)
      {
        printf("Make sure to add a new permission. Try again\n");
        continue;
      }
      new_permissions = malloc(sizeof(char) * (strlen(token) - 1));
      strcpy(new_permissions, token);
      if (new_permissions[strlen(token) - 1] == 0x0a)
      {
        new_permissions[strlen(token) - 1] = '\0';
      }

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
        continue;
      }

      // Get the file name
      token = strtok(NULL, " ");
      if (token == NULL || strlen(token) <= 1)
      {
        printf("Make sure to add a filename. Try again\n");
        continue;
      }
      if (strlen(token) > 33)
      {
        printf("File name too long. Try again\n");
        continue;
      }
      file_name = malloc(sizeof(char) * (strlen(token) - 1));
      strcpy(file_name, token);
      if (file_name[strlen(token) - 1] == 0x0a)
      {
        file_name[strlen(token) - 1] = '\0';
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
        continue;
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
        continue;
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
          continue;
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
            continue;
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
            continue;
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
            continue;
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
            continue;
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
            continue;
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
            continue;
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
            continue;
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
            continue;
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
            continue;
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
            continue;
          }
          break;
        }
      }

      // Update permissions data in FAT table
      fat[offset] = (new_permission_bytes << 8) | (fat[offset] & 0x00FF);
    }
    else if (isCommand("exit", line))
    {
      break;
    }
    else
    {
      printf("Invalid command. Try again.\n");
    }
  }
}
