# How to use:
# make bin/pennos
# make bin/pennfat
# make bin/pennshell
# make cleanpennos
# make cleanpennfat
# make cleanutil
# make cleanshell
# make clean (cleans all)

KERNEL = pennos
PENNFAT = pennfat
SHELLBIN = pennshell

CC = clang
CFLAGS = -Wall -Werror -g
LDFLAGS = # Add linker flags if needed
RM = rm -f # Define the RM variable

# Source directories
FATDIR = src/FAT
KERNELDIR = src/kernel
UTILDIR = src/util
SHELLDIR = src/shell

# Object directories
OBJFATDIR = obj/FAT
OBJKERNELDIR = obj/kernel
OBJUTILDIR = obj/util
OBJSHELLDIR = obj/shell

# Binary directory
BINDIR = bin

# Ensure the obj and bin directories exist
$(shell mkdir -p $(OBJFATDIR) $(OBJKERNELDIR) $(OBJUTILDIR) $(BINDIR) $(OBJSHELLDIR))

# Source files
FATSRCS = $(wildcard $(FATDIR)/*.c)
KERNELSRCS = $(wildcard $(KERNELDIR)/*.c)
UTILSRCS = $(wildcard $(UTILDIR)/*.c)
SHELLSRCS = $(wildcard $(SHELLDIR)/*.c)

# Object files
FATOBJS = $(patsubst $(FATDIR)/%.c, $(OBJFATDIR)/%.o, $(FATSRCS))
KERNELOBJS = $(patsubst $(KERNELDIR)/%.c, $(OBJKERNELDIR)/%.o, $(KERNELSRCS))
UTILOBJS = $(patsubst $(UTILDIR)/%.c, $(OBJUTILDIR)/%.o, $(UTILSRCS))
SHELLOBJS = $(patsubst $(SHELLDIR)/%.c, $(OBJSHELLDIR)/%.o, $(SHELLSRCS))

# Header files
FATHEADERS = $(wildcard $(FATDIR)/*.h)
KERNELHEADERS = $(wildcard $(KERNELDIR)/*.h)
UTILHEADERS = $(wildcard $(UTILDIR)/*.h)
SHELLHEADERS = $(wildcard $(SHELLDIR)/*.h)

# Rules to make the executables
$(BINDIR)/$(KERNEL): $(KERNELOBJS) $(UTILOBJS) $(SHELLOBJS) $(OBJSHELLDIR)/parser.o $(OBJFATDIR)/system-calls.o $(OBJFATDIR)/file_descriptor.o
	$(CC) $(LDFLAGS) -o $@ $^

$(BINDIR)/$(PENNFAT): $(FATOBJS) $(OBJUTILDIR)/linked_list.o $(OBJUTILDIR)/errno.o
	$(CC) $(LDFLAGS) -o $@ $^

$(BINDIR)/$(SHELLBIN): $(SHELLOBJS) $(UTILOBJS) $(KERNELOBJS) $(OBJSHELLDIR)/parser.o
	$(CC) $(LDFLAGS) -o $@ $^



# Phony rules for cleaning up
.PHONY: all clean cleanpennfat cleanpennos cleanutil cleanshell

all: $(BINDIR)/$(KERNEL) $(BINDIR)/$(PENNFAT) $(BINDIR)/$(SHELLBIN)

cleanpennfat:
	$(RM) -r $(OBJFATDIR)/* $(BINDIR)/$(PENNFAT)

cleanpennos:
	$(RM) -r $(OBJKERNELDIR)/* $(BINDIR)/$(KERNEL)

cleanutil:
	$(RM) -r $(OBJUTILDIR)/*

cleanshell:
	$(RM) $(filter-out $(OBJSHELLDIR)/parser.o,$(wildcard $(OBJSHELLDIR)/*.o))

clean: cleanpennfat cleanpennos cleanutil cleanshell
	$(RM) -r $(BINDIR)/*

# Pattern rules to build object files from source files
$(OBJFATDIR)/%.o: $(FATDIR)/%.c $(FATHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJKERNELDIR)/%.o: $(KERNELDIR)/%.c $(KERNELHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJUTILDIR)/%.o: $(UTILDIR)/%.c $(UTILHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJSHELLDIR)/%.o: $(SHELLDIR)/%.c $(SHELLHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@
