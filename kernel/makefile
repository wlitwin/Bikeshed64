CC=gcc
CFLAGS=-m64 \
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
	   -ansi \
	   -pedantic \
	   -std=c99

AS=as
AS_FLAGS=--64

LD=ld
LD_FLAGS=-m elf64 --oformat=binary
LD_FLAGS_DBG=-m elf64

OUTPUT_DIR=bin
OBJ_DIR=obj

# Filled in by debug rule
DBG=

KERNEL_OBJECTS=$(shell find src/ -name '*.[cs]' -o | sed -e 's/^\(.*\.di\|.*\.[cs]\)$$/obj\/\1.o/g')
PRE_KERNEL = $(OBJ_DIR)/src/arch/x86_64/prekernel.s.o

all: kernel 

clean:
	/bin/rm -rf $(OBJ_DIR)/*
	/bin/rm -rf $(OUTPUT_DIR)/*

# TODO Separate the runtime compilation from the kernels compilation
$(OBJ_DIR)/%.c.o : %.c
	$(CC) $(DBG) $(CFLAGS) -c $^ -of$@

$(OBJ_DIR)/%.s.o : %.s
	mkdir -p $(shell dirname $@)
	$(AS) $(AS_FLAGS) $^ -o $@

.PHONY: kernel debug
debug: DBG += -g

debug: kernel

kernel: $(KERNEL_OBJECTS)
	$(LD) $(LD_FLAGS) -T ./kernel.ld $(PRE_KERNEL) $(filter-out $(PRE_KERNEL),$(KERNEL_OBJECTS)) -o $(OUTPUT_DIR)/kernel.b
	$(LD) $(LD_FLAGS_DBG) -T ./kernel.ld $(PRE_KERNEL) $(filter-out $(PRE_KERNEL),$(KERNEL_OBJECTS)) -o $(OUTPUT_DIR)/kernel.o
