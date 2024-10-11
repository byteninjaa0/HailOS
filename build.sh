#!/bin/bash

# Function to check the last command status and exit if it fails
check_status() {
    if [ $? -ne 0 ]; then
        echo "Error occurred in command: $1"
        exit 1
    fi
}

# Parse arguments to check if -r option is provided
RUN_IN_QEMU=false
while getopts "r" opt; do
    case $opt in
        r)
            RUN_IN_QEMU=true
            ;;
        *)
            echo "Usage: $0 [-r]"
            exit 1
            ;;
    esac
done

# Command 1: Compile kernel.c
echo "Running: i686-elf-gcc -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra"
i686-elf-gcc -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
check_status "i686-elf-gcc -c kernel.c"

# Command 2: Link object files to create the OS binary
echo "Running: i686-elf-gcc -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib boot.o kernel.o -lgcc"
i686-elf-gcc -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib boot.o kernel.o -lgcc
check_status "i686-elf-gcc -T linker.ld"

# Command 3: Check if the file is multiboot compatible
echo "Running: grub-file --is-x86-multiboot myos.bin"
grub-file --is-x86-multiboot myos.bin
check_status "grub-file --is-x86-multiboot myos.bin"

# Confirm multiboot
if grub-file --is-x86-multiboot myos.bin; then
    echo "Multiboot confirmed"
else
    echo "The file is not multiboot"
    exit 1
fi

# Command 4: Copy the OS binary to the ISO directory
echo "Running: cp myos.bin isodir/boot/myos.bin"
cp myos.bin isodir/boot/myos.bin
check_status "cp myos.bin isodir/boot/myos.bin"

# Command 5: Copy grub.cfg to the ISO directory
echo "Running: cp grub.cfg isodir/boot/grub/grub.cfg"
cp grub.cfg isodir/boot/grub/grub.cfg
check_status "cp grub.cfg isodir/boot/grub/grub.cfg"

# Command 6: Create the ISO image
echo "Running: grub-mkrescue -o myos.iso isodir"
grub-mkrescue -o myos.iso isodir
check_status "grub-mkrescue -o myos.iso isodir"

# Check if -r option is provided to run in QEMU
if [ "$RUN_IN_QEMU" = true ]; then
    echo "Running: qemu-system-i386 -cdrom myos.iso"
    qemu-system-i386 -cdrom myos.iso
    check_status "qemu-system-i386 -cdrom myos.iso"
fi

echo "All commands executed successfully!"
