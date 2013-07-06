OUTPUT_DIR=./bin

all: export QEMU=-DQEMU
all: create_bin

OUTPUT_DIR=./bin
OBJ_DIR=./obj

clean:
	@echo Cleaning $(OUTPUT_DIR)
	@/bin/rm -rf $(OUTPUT_DIR)/*
	@$(MAKE) --no-print-directory -C  boot/ clean
	@$(MAKE) --no-print-directory -C kernel/ clean
	@$(MAKE) --no-print-directory -C programs/ clean

.PHONY: boot
boot:
	@$(MAKE) --no-print-directory -C boot/

.PHONY: utils
utils:
	@$(MAKE) --no-print-directory -C utils/

.PHONY: kernel
kernel:
	@echo ----------------
	@echo Compiling Kernel
	@echo ----------------
	@$(MAKE) --no-print-directory -C kernel/

.PHONY: programs
programs:
	@echo -----------------------
	@echo Compiling User Programs
	@echo -----------------------
	@$(MAKE) --no-print-directory -C programs/

create_bin: boot programs kernel 
	@echo --------------------
	@echo Creating Final Image
	@echo --------------------
	@./boot/bin/FancyCat 0x200000 ./kernel/bin/kernel.b ./programs/bin/init
	@mv image.dat $(OUTPUT_DIR)/.
	@cat ./boot/bin/bootloader.b $(OUTPUT_DIR)/image.dat > $(OUTPUT_DIR)/kernel.bin
	@echo --------------------
	@echo Done

vmdk: create_bin
	qemu-img convert -O vmdk $(OUTPUT_DIR)/kernel.bin $(OUTPUT_DIR)/kernel.vmdk

emu: create_bin qemu
#clean create_bin 
qemu: 
	~/usr/local/bin/qemu-system-x86_64 -s -m 512 -cpu core2duo -drive file=$(OUTPUT_DIR)/kernel.bin,format=raw,cyls=200,heads=16,secs=63 -monitor stdio -serial /dev/pts/2 -net user -net nic,model=i82559er -d int,cpu_reset -soundhw hda

qemu2: 
	sudo qemu-system-x86_64 -s -m 512 -cpu core2duo -drive file=/dev/sdb,format=raw,cyls=200,heads=16,secs=63 -monitor stdio -serial /dev/pts/2 -net user -net nic,model=i82559er -soundhw hda 

usb: clean create_bin
	sudo dd if=bin/kernel.bin of=/dev/sdb
