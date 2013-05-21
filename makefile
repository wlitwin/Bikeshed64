OUTPUT_DIR=./bin

all: create_bin

OUTPUT_DIR=./bin
OBJ_DIR=./obj

clean:
	/bin/rm -rf $(OUTPUT_DIR)/*
	$(MAKE) -C boot/ clean
	$(MAKE) -C kernel/ clean

.PHONY: boot
boot:
	$(MAKE) -C boot/

.PHONY: utils
utils:
	$(MAKE) -C utils/

.PHONY: kernel
kernel:
	$(MAKE) -C kernel/

create_bin: boot kernel 
	./boot/bin/FancyCat 0x200000 ./kernel/bin/kernel.b 0x600000 ./fs/bin/myfs
	mv image.dat $(OUTPUT_DIR)/.
	cat ./boot/bin/bootloader.b $(OUTPUT_DIR)/image.dat > $(OUTPUT_DIR)/kernel.bin

emu: create_bin qemu

qemu: 
	qemu-system-i386 -s -m 1024 -cpu core2duo -drive file=$(OUTPUT_DIR)/kernel.bin,format=raw,cyls=200,heads=16,secs=63 -monitor stdio -serial /dev/pts/1 -net user -net nic,model=i82559er
