CC=gcc
C_FLAGS=-Wall -Werror -Wextra -pedantic -std=gnu99

AS=as
AS_FLAGS=--32 -n32

LD=ld
LD_FLAGS=-m elf_i386 --gc-sections --oformat=binary
LD_FLAGS_DBG=-m elf_i386 --gc-sections

OUTPUT_DIR=./bin
OBJ_DIR=./obj

# Filled in by debug rule
DBG=

all: bootloader fancycat

#obj/%.S.o : %.S

clean:
	@echo Cleaning Boot
	@/bin/rm -rf $(OBJ_DIR)/*
	@/bin/rm -rf $(OUTPUT_DIR)/*

#bootloader: $(OUTPUT_DIR)/bootloader.b

fancycat: $(OUTPUT_DIR)/FancyCat

bootloader:
	@echo Creating Bootloader
	@$(AS) $(AS_FLAGS) bootloader.s -o $(OBJ_DIR)/bootloader.o
	@$(LD) $(LD_FLAGS) $(OBJ_DIR)/bootloader.o -Tbootloader.ld -o $(OUTPUT_DIR)/bootloader.b


$(OUTPUT_DIR)/FancyCat:
	@echo Creating FancyCat
	@$(CC) $(C_FLAGS) FancyCat.c -o $(OUTPUT_DIR)/FancyCat
