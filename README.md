1.

- Nigel Newby - newbyn
- Fernando Castaneda - fercas
- Omar Ameen - omario
- Jonathan Wong - jonwong

2.
<img width="245" alt="Screenshot 2023-12-05 at 6 34 41 PM" src="https://github.com/CIS548/23fa-cis3800-pennos-39/assets/82334955/f58cb76c-e2dc-4cf2-8d53-a77ff9f8d79d">



3. N/A

4. 
- Run "make -B" in root directory
- Run `bin/pennfat` then create a new filsystem through `mkfs newfs 1 0` 
- In a separate terminal, run `bin/pennos newfs`, which will mount newfs for our OS 

5. All functionality for our scheduler are complete. We were able to implement nearly all functionality for our file system and most functionality for our shell and integration between the kernel and file system.

6. Our code is broken up into kernel, file system, util, and shell modules. The kernel contains the code for the scheduler, including a kernel.c file that handles kernel level functions and user.c for user level functions. The fat system is contained in it's own directory, and likewise with the shell. Our util folder contains helper functions and things of the like.

The file system works in it's own file as a shell that reads user input and uses it to manipulate the file system. All of the logic is consolidated in the file, which is broken up based on what the command was entered by the user. Each block for each command contains the code that is necessary for handling the execution of that command. We reused this code when integrating our file system with the shell.

For the Kernel and Scheduler, we created a global PCB linked list that keeps track of all existing processes, as well as 3 linked list meant for processes of different priorities. In scheduler.c, we have a linked list with a 9-6-4 ratio of -1, 0, and 1s respectively and we loop through this list to get our next process to schedule to run, which helps achieve the desired priority queue ratio. By default, the Shell is the foreground process and all other processes are background jobs. A job is only the foreground process is the shell calls blocking wait on it. Any signals sent by user input through signal handlers are always sent to the foreground process.
