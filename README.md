# PatchworkOS

> **⚠ Warning**<br> Keep in mind that PatchworkOS is currently in a very early stage of development, and may have both known and unknown bugs.

![License](https://img.shields.io/badge/License-MIT-green) [![Build and Test](https://github.com/KaiNorberg/PatchworkOS/actions/workflows/test.yml/badge.svg)](https://github.com/KaiNorberg/PatchworkOS/actions/workflows/test.yml)

**Patchwork** is a 64-bit monolithic NON-POSIX operating system for the x86_64 architecture that rigorously follows a "everything is a file" philosophy. Built from scratch in C it takes many ideas from Unix, Plan9, DOS and others while simplifying them and sprinkling in some new ideas of its own.

## Screenshots

![Desktop Screenshot](meta/screenshots/desktop.png)
![Doom Screenshot](meta/screenshots/doom.png)

## Features

### Kernel

* Multithreading with a [constant-time scheduler](https://github.com/KaiNorberg/PatchworkOS/blob/main/src/kernel/sched/sched.h).
* Fully preemptive and tickless.
* Symmetric Multi Processing.
* Constant-time per page memory management, including both the physical and virtual memory managers.
* IPC including pipes, shared memory, sockets and Plan9 inspired "signals" called notes.
* Synchronization primitives including, mutexes, read-write locks+mutexes and a futex-like system call.
* SIMD.

### File System

* Linux-style VFS with dentry+inode caching, negative dentrys, mountpoints, hardlinks, etc.
* Strict adherence to "everything is a file".
* [Custom image format (.fbmp)](https://github.com/KaiNorberg/fbmp).
* [Custom font format (.grf)](https://github.com/KaiNorberg/grf).

### User space

* Custom C standard library and system libraries.
* Shared memory based window manager.
* Highly modular window manager, the taskbar, wallpaper and cursor are just windows, and can do anything a window can.
* Theming via [config files](https://github.com/KaiNorberg/PatchworkOS/blob/main/root/cfg).

### Performance

* Uses ~50-130 mb of ram with the desktop environment and a few applications running.
* Responsive desktop environment even att 100% CPU usage. DOOM is fully playable while running a stress test.
* Idle CPU usage with the desktop environment running and a few applications open is ~0.1% on a Lenovo ThinkPad E495.

And much more...

## Notable Differences with Unix

* Replaced `fork(), exec()` with `spawn()`.
* Single-User.
* Non POSIX standard library.
* Custom [shell utilities](#shell-utilities).

## Limitations

* Currently limited to RAM disks only.
* Only support for x86_64.

## Notable Future Plans

* Bootloader overhaul.
* Modular kernel.
* Shared libraries.
* Software interrupts for notes (signals).
* Lua port.
* Capability based security model (currently has no well-defined security model).

## Shell Utilities

Patchwork includes its own shell utilities designed around its [file flags](#file-flags) system. Included is a brief overview with some usage examples. For convenience the init program will create hardlinks for each shell utility to their unix equivalents, this can be configured in the [init cfg](https://github.com/KaiNorberg/PatchworkOS/tree/main/root/cfg/init-main.cfg).

### `open`

Opens a file path and then immediately closes it. Intended as a replacement for `touch`.

```bash
open file.txt:create:excl           # Creates the file.txt file only if it does not exist.
open mydir:create:dir               # Creates the mydir directory.
````

### `read`

Reads from stdin or provided files and outputs to stdout. Intended as a replacement for `cat`.

```bash
read file1.txt file2.txt            # Read the contents of file1.txt and file2.txt.
read < file.txt                     # Read the contents of file.txt.
read < file.txt > dest.txt:create   # Copy contents of file.txt to dest.txt and create it.
```

### `write`

Writes to stdout. Intended as a replacement for `echo`.

```bash
write "..." > file.txt              # Write to file.txt.
write "..." > file.txt:append       # Append to file.txt, makes ">>" unneeded.
```

### `dir`

Reads the contents of a directory to stdout. Intended as a replacement for `ls`.

```bash
dir mydir                           # Prints the contents of mydir.
dir mydir:recur                     # Recursively print the contents of mydir.
```

### `delete`

Deletes a file or directory. Intended as a replacement for `rm`, `unlink` and `rmdir`.

```bash
delete file.txt                     # Deletes file.txt.
delete mydir:recur                  # Recursively deletes mydir and its contents.
```

There are other utils available that work as expected, for example `stat` and `link`.

## Everything is a File

Patchwork strictly follows the "everything is a file" philosophy in a way similar to Plan9, this can often result in unorthodox APIs or could just straight up seem overly complicated, but it has its advantages. I will give some examples and then after I will explain why this is not a complete waste of time. Let's start with sockets.

### Sockets

In order to create a local seqpacket socket, you open the `/net/local/seqpacket` file, which will return a file that acts as the handle for your socket. Reading from this file will return the ID of your created socket so, for example, you can do

```c
    fd_t handle = open("/net/local/seqpacket");
    char id[32] = {0};
    read(handle, id, 32);
```

Note that when the handle is closed, the socket is also freed. The ID that the handle returns is the name of a directory that has been created in the "/net/local" directory, in which there are three files, these include:

  - `data` - used to send and retrieve data.
  - `ctl` - used to send commands.
  - `accept` - used to accept incoming connections.

So, for example, the sockets data file is located at `/net/local/[id]/data`. Note that only the process that created the socket or its children can open these files. Now say we want to make our socket into a server, we would then use the bind and listen commands, for example

```c
    fd_t ctl = openf("/net/local/%s/ctl", id);
    writef(ctl, "bind myserver");
    writef(ctl, "listen");
    close(ctl);
```

Note the use of `openf()` which allows us to open files via a formatted path and that we name our server `myserver`. If we wanted to accept a connection using our newly created server, we just open its accept file, like this

```c
    fd_t fd = openf("/net/local/%s/accept", id);
```

The returned file descriptor can be used to send and receive data, just like when calling `accept()` in for example Linux or other POSIX operating systems. This is practically true of the entire socket API, apart from using these weird files everything (should) work as expected. For the sake of completeness, if we wanted to connect to this server, we can do something like this

```c
    fd_t handle = open("/net/local/seqpacket");
    char id[32] = {0};
    read(handle, id, 32);

    fd_t ctl = openf("/net/local/%s/ctl", id);
    writef(ctl, "connect myserver");
    close(ctl);
```

### File Flags?

You may have noticed that, in the above section, the `open()` function does not take in a flags argument. This is because flags are part of the file path directly so if you wanted to create a non-blocking socket, you would use

```c
    fd_t handle = open("/net/local/seqpacket:nonblock");
```

Multiple flags are allowed, just seperate them with the `:` character, this means flags can be easily appended to a path using the `openf()` function. It is also possible to just specify the first letter of a flag, so instead of `:nonblock` you can use `:n`. Note that duplicate flags are ignored and that there are no read or write flags, all files are both read and write.

### The Why

So, finally, I can explain why I've decided to do this. It does seem overly complicated at first glance. There are three reasons in total.

The first is that I want Patchwork to be easy to expand upon. Normally, to just implement a single system call is quite a lot of work. You'd need to implement its behavior, create the system call handler, create a function for it in the standard library, and you'd need to make whatever software or shell utility to actually use that system call, that is a surprisingly large amount of work for just a single small system call. Meanwhile with this system, when something as significant as sockets were implemented the only thing that needed to be done was implementing the sockets, the rest of the operating system could remain unchanged.

The second reason is that it makes using the shell far more interesting, there is no need for special functions or any other magic keywords to for instance use sockets, all it takes is opening and reading from files.

Let's take an example. Say we wanted to implement `waitpid()`. First we need to implement the kernel behavior itself, then the appropriate system call, then add in handling for that system call in the standard library, then the actual function itself in the standard library and finally create some `waitpid` shell utility. That's a lot of work for something as simple as a waiting for a process to die, and it means a whole new API to learn. Instead, we can just add a `status` file to the process directory, which is only a handful lines of code, and we are done. Reading from the status file will block until the process dies and then read its exit status and can be used via `read()` or in the shell via `read /proc/[pid]/status`.

And of course the third and final reason is because I think it's fun, and honestly I think this kind of system is just kinda beautiful. There are downsides, of course, like the fact that these systems are less self documenting. But that is an argument for another time.

## Documentation

If you are still interested in knowing more, then you can check out the Doxygen generated [documentation](https://kainorberg.github.io/PatchworkOS/html/index.html).

## Directories

| Directory | Description |
| :-------- | :---------- |
| `include` | Public API |
| `src` | Source code |
| `root` | Files copied to the root directory of the generated .iso |
| `tools` | Build scripts (hacky alternative to cross-compiler) |
| `make` | Make files |
| `lib` | Third party dependencies |
| `meta` | Screenshots and repo metadata |

### Sections

  * **boot**: Minimal UEFI bootloader that collects system info and loads the kernel
  * **kernel**: The monolithic kernel handling everything from scheduling to IPC
  * **libstd**: C standard library extension with system call wrappers
  * **libpatchwork**: Higher-level library for windowing and user space services
  * **programs**: Shell utilities, services, and desktop applications

## Setup

### Requirements

  * **OS**: Linux (WSL might work, but I make no guarantees)
  * **Tools**: GCC, make, NASM, mtools, QEMU (optional)

### Build and Run

```bash
# Clone this repository, you can also use the green Code button at the top of the Github.
git clone --recursive [https://github.com/KaiNorberg/PatchworkOS](https://github.com/KaiNorberg/PatchworkOS)
cd PatchworkOS

# Build (creates PatchworkOS.img in bin/)
make all

# Run using QEMU
make run
```

### Grub Loopback

For frequent testing, it might be inconvenient to frequently flash to a USB. You can instead set up the `.img` file as a loopback device in GRUB.

Add this entry to the `/etc/grub.d/40_custom` file:

```bash
menuentry "Patchwork OS" {
        set root="[The grub identifer for the drive. Can be retrived using: sudo grub2-probe --target=drive /boot]"
        loopback loop0 /PatchworkOS.img # Might need to be modified based on your setup.
        set root=(loop0)
        chainloader /efi/boot/bootx64.efi
}
```

Regenerate grub configuration using `sudo grub2-mkconfig -o /boot/grub2/grub.cfg`.

Finally copy the generated `.img` file to your `/boot` directory, this can also be done with `make grub_loopback`.

You should now see a new entry in your GRUB boot menu allowing you to boot into the OS, like dual booting, but without the need to create a partition.

### Troubleshooting

  * **QEMU boot failure**: Check if you are using QEMU version 10.0.0, as that version is known to not work correctly, try using version 9.2.3.
  * **Any other errors?**: If an error not listed here occurs or is not resolvable, please open an issue in the GitHub.

## Testing

This repository uses a bit of a hacky way to do testing, we use a github action, as normal, that compiles the operating system then runs it using QEMU. QEMU is then allowed to run for one minute, the kernel will run some tests and then start as normal. If QEMU crashes or the kernel panicks then the test fails, if it is still running after one-minute we call it a success. Its an overly simple approach but gets the job done. A lot of the difficulty in performing testing comes from the inherent complexity of testing a OS, which also means that testing is currently very very limited in the kernel.

### Tested Configurations

  * QEMU emulator version 9.2.3 (qemu-9.2.3-1.fc42)
  * Lenovo ThinkPad E495
  * Ryzen 5 3600X | 32GB 3200MHZ Corsair Vengeance

Currently untested on Intel hardware. Let me know if you have different hardware, and it runs (or doesn't) for you!

## Contributing

If you find any bugs, issues or just have a suggestion for something I could do better, then feel free to open an issue or if you feel like it, you may submit a pull request!
