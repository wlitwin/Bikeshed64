CC=gcc
CFLAGS=-m64 \
	   -g \
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
	   -fno-builtin \
	   -std=c99 \
	   -DBIKESHED_X86_64 \
	   -I../kernel/src/
	   #-pedantic 

LD=ld
LD_FLAGS=-m elf_x86_64 -Tlinker.ld -z max-page-size=4096

AS=as
AS_FLAGS=--64 -am

OUTPUT_DIR=./bin
OBJ_DIR=./obj

USER_PROGS=$(shell ls -d src/*/ | sed -e 's/^src/bin/')

USER_OBJECTS=$(shell find src/ -name '*.[cs]' | sed -e 's/^\(.*\.[cs]\)$$/obj\/\1.o/g')

all: $(USER_OBJECTS) $(USER_PROGS)

$(OBJ_DIR)/src/ulib.c.o : src/ulib.c
	@echo " - Compiling " $^
	@mkdir -p $(shell dirname $@)
	@$(CC) $(CFLAGS) -Isrc/ -I../kernel/src/ -c $^ -o $@

$(OBJ_DIR)/%.c.o : %.c
	@echo " - Compiling" $^
	@mkdir -p $(shell dirname $@)
	@$(CC) $(CFLAGS) -I$(shell dirname $@)/  -c $^ -o $@

$(OBJ_DIR)/%.s.o : %.s
	@echo " - Assembling" $^
	@mkdir -p $(shell dirname $@)
	@$(AS) $(AS_FLAGS) $^ -o $@

bin/% : $(OBJ_DIR)/src/startup.s.o $(OBJ_DIR)/src/ulib.c.o obj/src/%/*.o
	$(LD) $(LD_FLAGS) obj/src/startup.s.o obj/src/ulib.c.o $(shell find obj/src/$(shell basename $@) -name "*.o") -o $@

clean:
	@echo Cleaning Programs
	/bin/rm -rf $(OBJ_DIR)/*
	/bin/rm -rf $(OUTPUT_DIR)/*
