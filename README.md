# copeland_os-SEL

> *"No matter where you go, everyone is connected."*

A bare-metal x86 operating system built from scratch, in the spirit of
Copeland OS and TempleOS. SEL stands for Serial Experiments Lain, a
show that made me fall in love with computers.

This is not a Linux distro. There is no POSIX. There is no libc.
Just a bootloader, a kernel, and the Wired, for now..

---

## Status | current v = 'v0.0.1' 

`v0.0.1` boots, VGA text output, The Wired says hello.

---

## Philosophy

Copeland was Apple's abandoned OS project from the 90s, ambitious,
ahead of its time, never shipped. Terry Davis built TempleOS alone over
a decade and made something singular. copeland_os-SEL will follow in those
footsteps; one person, from scratch, for the love of it.

The aesthetic comes from Serial Experiments Lain. The Wired is real.


---

## how it install, v = 'v0.0.1'

right now i am on arch linux, and my testing of installing will be limited, however, i have a small guide for v0.0.1

you need a cross-compiler targeting the bare metal x86, this is what you will need:

i686-elf-gcc          
i686-elf-ld        
nasm
qemu-system-i386
grub (grub-mkrescue)
xorriso


after that;

sudo pacman -S base-devel nasm qemu-system-x86 grub xorriso mtools

yay -S i686-elf-gcc-bin i686-elf-binutils

(% note, if i686-elf-gcc or i686-elf-ld aren't working, then cry, and install with -bin worked for me. %)


then the cool step; 

make                          

make run                        

if all is well it will boot, if not check you have gtk as a display option with qemu,


---

## License

GNU GPL v2. See [LICENSE](LICENSE).

Built by [@rafdog1222](https://github.com/rafdog1222).
Dedicated to Terry Davis, and the Wired.


---


## How to contact me... 

What!? you really want to talk to me? .... if, no... maybe.....  um....
I am active, a lot, heh, but if you really want to talk, I have a discord in my other repo's readme, good bye, see you everywhere...
