OUTPUT_DIR=./bin

all: create_bin

OUTPUT_DIR=./bin
OBJ_DIR=./obj

clean:
	@echo Cleaning $(OUTPUT_DIR)
	@/bin/rm -rf $(OUTPUT_DIR)/*
	@$(MAKE) --no-print-directory -C  boot/ clean
	@$(MAKE) --no-print-directory -C kernel/ clean

.PHONY: boot
boot:
	@$(MAKE) --no-print-directory -C boot/

.PHONY: utils
utils:
	@$(MAKE) --no-print-directory -C utils/

.PHONY: kernel
kernel:
	@$(MAKE) --no-print-directory -C kernel/

create_bin: boot kernel 
	@echo --------------------
	@echo Creating Final Image
	@echo --------------------
	@./boot/bin/FancyCat 0x200000 ./kernel/bin/kernel.b 
	@mv image.dat $(OUTPUT_DIR)/.
	@cat ./boot/bin/bootloader.b $(OUTPUT_DIR)/image.dat > $(OUTPUT_DIR)/kernel.bin
	@echo --------------------
	@echo Done

emu: create_bin qemu

qemu: 
	qemu-system-x86_64 -s -m 2048 -cpu core2duo -drive file=$(OUTPUT_DIR)/kernel.bin,format=raw,cyls=200,heads=16,secs=63 -monitor stdio -serial /dev/pts/1 -net user -net nic,model=i82559er

qemu2: 
	sudo qemu-system-x86_64 -s -m 1024 -cpu core2duo -drive file=/dev/sdb,format=raw,cyls=200,heads=16,secs=63 -monitor stdio -serial /dev/pts/1 -net user -net nic,model=i82559er

usb: clean create_bin
	sudo dd if=bin/kernel.bin of=/dev/sdb
