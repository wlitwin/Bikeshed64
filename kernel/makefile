CC=gcc
CFLAGS=-m64 \
	   -Wwrite-strings \
	   -mcmodel=large \
	   -ffreestanding \
	   -nostdlib \
	   -mno-red-zone \
	   -mno-mmx \
	   -mno-sse \
	   -mno-sse2 \
	   -mno-sse3 \
	   -mno-3dnow \
	   -Wall \
	   -Wextra \
	   -Wformat \
	   -pedantic \
	   -fno-builtin \
	   -std=c99 \
	   -Isrc/ \
	   -DBIKESHED_X86_64

AS=as
AS_FLAGS=--64 -am

LD=ld
LD_FLAGS=-m elf_x86_64 --oformat=binary
LD_FLAGS_DBG=-m elf_x86_64

OUTPUT_DIR=bin
OBJ_DIR=obj

# Filled in by debug rule
DBG=

KERNEL_OBJECTS=$(shell find src/ -name '*.[cs]' | sed -e 's/^\(.*\.[cs]\)$$/obj\/\1.o/g')
PRE_KERNEL = $(OBJ_DIR)/src/arch/x86_64/prekernel.s.o

all: kernel 

clean:
	@echo Cleaning Kernel
	@/bin/rm -rf $(OBJ_DIR)/*
	@/bin/rm -rf $(OUTPUT_DIR)/*

# TODO Separate the runtime compilation from the kernels compilation
$(OBJ_DIR)/%.c.o : %.c
	@echo " - Compiling" $^
	@mkdir -p $(shell dirname $@)
	@$(CC) $(DBG) $(CFLAGS) -c $^ -o $@

$(OBJ_DIR)/%.s.o : %.s
	@echo " - Assembling" $^
	@mkdir -p $(shell dirname $@)
	@$(AS) $(AS_FLAGS) $^ -o $@

.PHONY: kernel debug
debug: DBG += -g

debug: kernel

print_args:
	@echo
	@echo Compiler: $(CC)
	@echo CFLAGS: $(CFLAGS)
	@echo
	@echo Assembler: $(AS)
	@echo AFLAGS: $(AS_FLAGS)
	@echo
	@echo Linker: $(LD)
	@echo LFLAGS: $(LD_FLAGS)
	@echo

kernel: print_args $(KERNEL_OBJECTS)
	@echo
	@echo ----------------------------
	@echo Creating Kernel Binary Image
	@echo ----------------------------
	@$(LD) $(LD_FLAGS) -T ./kernel.ld $(PRE_KERNEL) $(filter-out $(PRE_KERNEL),$(KERNEL_OBJECTS)) -o $(OUTPUT_DIR)/kernel.b
	@echo ---------------------------
	@echo Creating Kernel Debug Image
	@echo ---------------------------
	@$(LD) $(LD_FLAGS_DBG) -T ./kernel.ld $(PRE_KERNEL) $(filter-out $(PRE_KERNEL),$(KERNEL_OBJECTS)) -o $(OUTPUT_DIR)/kernel.o
