CC      = i686-elf-gcc
AS      = nasm
LD      = i686-elf-ld
CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra
LDFLAGS = -ffreestanding -O2 -nostdlib

BOOT_OBJ  = boot/boot.o

KERN_OBJ  = kernel/kernel.o \
			kernel/shell.o 

MM_OBJS   = kernel/mm/pmm.o \
			kernel/mm/heap.o

ARCH_OBJS = kernel/arch/gdt.o \
			kernel/arch/gdt_flush.o \
            kernel/arch/idt.o \
			kernel/arch/idt_load.o \
            kernel/arch/keyboard.o \
			kernel/arch/keyboard_asm.o \
			kernel/arch/pit.o \
			kernel/arch/pit_asm.o

CMD_OBJS  = kernel/commands/commands.o \
            kernel/commands/cmd_system.o \
            kernel/commands/cmd_wired.o \
            kernel/commands/cmd_mem.o \
            kernel/commands/cmd_signal.o

DRV_OBJS  = kernel/drivers/ata.o \
			kernel/drivers/rtc.o

FS_OBJS   = kernel/fs/signal_format.o \
            signal.o

ALL_OBJS  = $(BOOT_OBJ) $(KERN_OBJ) $(MM_OBJS) $(ARCH_OBJS) \
            $(CMD_OBJS) $(DRV_OBJS) $(FS_OBJS)

# signal partition - sda4, secondary bus slave (index=3)
SIGNAL_DEV  = /dev/sda4
SIGNAL_BLOCKS = 2621440

.PHONY: all clean run run-disk run-real deploy mkfs-signal

all: copeland_os-SEL.iso

boot/boot.o: boot/boot.asm
	$(AS) -f elf32 $< -o $@

signal.o: signal.c signal.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/shell.o: kernel/shell.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/commands/%.o: kernel/commands/%.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

kernel/mm/%.o: kernel/mm/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/arch/%.o: kernel/arch/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/arch/%.o: kernel/arch/%.asm
	$(AS) -f elf32 $< -o $@

kernel/drivers/%.o: kernel/drivers/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/fs/%.o: kernel/fs/%.c
	$(CC) $(CFLAGS) -I. -c $< -o $@


kernel.bin: $(ALL_OBJS)
	$(CC) -T kernel/linker.ld -o $@ $(LDFLAGS) $(ALL_OBJS) -lgcc

copeland_os-SEL.iso: kernel.bin
	cp kernel.bin iso/boot/kernel.bin
	grub-mkrescue -o $@ iso



# QEMU with signal.img (safe testing, no real disk though)
run: copeland_os-SEL.iso signal.img
	qemu-system-i386 $(QEMUFLAGS) \
		-cdrom copeland_os-SEL.iso \
		-drive file=signal.img,format=raw,if=ide,index=3,media=disk \
		-m 64M

# QEMU with sda4 partition
run-real: copeland_os-SEL.iso
	sudo -E qemu-system-i386 $(QEMUFLAGS) \
		-display sdl \
		-cdrom copeland_os-SEL.iso \
		-drive file=$(SIGNAL_DEV),format=raw,if=ide,index=3,media=disk \
		-m 64M


# create a blank test image (64MB(maybe))
signal.img:
	dd if=/dev/zero of=signal.img bs=1M count=64
	@echo "signal.img ready - $(SIGNAL_BLOCKS) blocks"
	@echo "in shell: mkfs 16384 0 copeland-sel"

# building the native mkfs tool for formatting real partitions from my arch
mkfs-signal: mkfs_signal.c signal.c kernel/fs/signal_format.c
	gcc -o mkfs_signal $^ -I. -std=gnu99 -Wall -include stdlib.h
	@echo "run: sudo ./mkfs_signal $(SIGNAL_DEV) $(SIGNAL_BLOCKS) copeland-sel"

deploy: kernel.bin
	sudo cp kernel.bin /boot/copeland.bin
	@echo "kernel deployed, reboot and select copeland_os-SEL from GRUB, {might not work}"

clean:
	rm -f $(ALL_OBJS) \
	      kernel.bin \
	      copeland_os-SEL.iso \
	      iso/boot/kernel.bin
