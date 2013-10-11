Bikeshed64
==========

64-bit version of Bikeshed OS

Build with `make` and run with `make qemu` if you have QEMU installed.
Note: Tetris is pretty unplayable in QEMU unless you change the time constants in `programs/src/init/tetris.c` `TICK_DURATION` and `TICKS_PER_SEC_DEFAULT`. The only problem is this makes it feel a little less responsive because input is only handled on a tick basis.

Features/Progress:
  - Shell with two commands!
    - help
	- tetris
  - Full 64-bit virtual memory
  - Ring 3 process(es)
  - Half finished scheduler
  - Some system calls
    - Fork
	- Exit
	- MSleep
	- Set priority
	- Key available
	- Get key
  - Almost finished Intel HDA sound driver
  - Fancy bootloader
  - ELF loader

Here's the shell:
![Shell](https://raw.github.com/wlitwin/Bikeshed64/master/shell.png)

Here's a picture of tetris:
![Tetris](https://raw.github.com/wlitwin/Bikeshed64/master/tetris.png)

