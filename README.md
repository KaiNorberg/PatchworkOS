
# PatchworkOS ![](https://img.shields.io/badge/License-MIT-green)

> **⚠ Warning**<br> Keep in mind that PatchworkOS is currently in a very early stage of development, and may have both known and unknown bugs.

**Patchwork** is a 64 bit monolithic hobbyist OS built from scratch in C for the x86_64 architecture, it is intended as an easy-to-modify toy-like Unix-inspired OS (not Unix-like) it takes many ideas from Unix, Plan9, DOS and other places while simplifying them and removing some of the fat. Made entirely for fun.

## Screenshots

<img src="meta/screenshots/desktop.png">
<img src="meta/screenshots/doom.png"">

## Features

- Monolithic preemptive 64-bit SMP kernel.
- Kernel level multithreading via a O(1) scheduler.
- Custom C standard library and system libraries.
- SIMD.
- [Custom image format (.fbmp)](https://github.com/KaiNorberg/fbmp).
- [Custom font format (.grf)](https://github.com/KaiNorberg/grf).
- Strict adherence to "everything is a file".
- IPC including pipes, shared memory, sockets and plan9 inspired "signals" called notes.

## Notable Differences with Unix

- Multiroot file system, with labels not letters ```home:/usr/fonts```
- Replaced ```fork(), exec()``` with ```spawn()```
- Single-User
- Non POSIX standard library

## Limitations

- Ram disks only
- Only x86_64

## A Small Taste

Patchwork strictly follows the "everything is a file" philosophy in a way similar to Plan9, this can often result in unorthodox APIs or could just straight up seem overly complicated, but it has its advantages. I will give some examples and then after I will explain why this is not a complete waste of time. Let's start with sockets.

### Sockets

In order to create a local socket, you open the ```sys:/net/local/new``` file, which will return a file that acts as the handle for your socket. Reading from this file will return the ID of your created socket so, for example, you can do
```c
    fd_t handle = open("sys:/net/local/new");
    char id[32];
    read(handle, id, 32);
```
Note that when the handle is closed, the socket is also freed. The ID that the handle returns is the name of a directory that has been created in the "sys:/net/local" directory, in which there are three files, these include:
- ```data``` - used to send and retrieve data.
- ```ctl``` - used to send commands.
- ```accept``` - used to accept incoming connections.

So, for example, the sockets data file is located at ```sys:/net/local/[id]/data```. Note that only the process that created the socket or its children can open these files. Now say we want to make our socket into a server, we would then use the bind and listen commands, for example
```c
    fd_t ctl = openf("sys:/net/local/%s/ctl", id);
    writef(ctl, "bind myserver");
    writef(ctl, "listen");
    close(ctl);
```
Note the use of openf() which allows us to open files via a formatted path and that we name our server myserver. If we wanted to accept a connection using our newly created server, we just open its accept file, like this
```c
    fd_t fd = openf("sys:/net/local/%s/accept", id);
```
The returned file descriptor can be used to send and receive data, just like when calling accept() in for example Linux or other POSIX operating systems. This is practically true of the entire socket API, apart from using these weird files everything (should) work as expected. For the sake of completeness, if we wanted to connect to this server, we can do something like this
```c
    fd_t handle = open("sys:/net/local/new");
    char id[32];
    read(handle, id, 32);

    fd_t ctl = openf("sys:/net/local/%s/ctl", id);
    writef(ctl, "connect myserver");
    close(ctl);
```

### File Flags?

You may have noticed that, in the above section, the open() function does not take in a flags argument or anything similar. This is because flags are part of the file path directly so if you wanted to create a non-blocking socket, you would use
```c
    fd_t handle = open("sys:/net/local/new?nonblock");
```
Multiple flags can be separated with the ```&``` character, like an internet link. However, there are no read and/or write flags, all files are both read and write.

### The Why

So, finally, I can explain why I've decided to do this. It does seem overly complicated at first glance. There are three reasons in total.

The first is that I want Patchwork to be easy to expand upon, for that sake I want its interfaces to be highly generalized. Normally to just implement a single system call is quite a lot of work. You'd need to implement its behavior, register the system call handler, then you'd need to create it in the standard library, and you'd need to make whatever software to actually use that system call, that is a surprisingly large amount of stuff that needs to be changed just for a single small system call. Meanwhile with this system, when sockets were implemented the only thing that needed to be done was implementing the sockets, the rest of the operating system could remain the same.

The second reason is that it makes using the shell far more interesting, there is no need for special functions or any other magic keywords to for instance use sockets, all it takes is opening and reading from files.

Let's take an example of these first two reasons. Say we wanted to implement the ability to wait for a process to die via a normal system. First we need to implement the kernel behavior to do that, then the appropriate system call, then add in handling for that system call in the standard library, then the actual function itself in the standard library and finally probably create some program that could be used in the shell. That's a lot of work for something as simple as a waiting for a process to die. Meanwhile, if waiting for a processes death is done via just writing to that processes "ctl" file then it's as simple as adding a "wait" action to it and calling it a day, you can now easily use that behavior via the standard library and via the shell by something like ```echo wait > sys:/proc/[pid]/ctl``` without any additional work.

And of course the third and final reason is because I think it's fun, and honestly I think this kind of system is just kinda beautiful due to just how generalized and how strictly it follows the idea that "everything is a file". There are downsides, of course, like the fact that these systems are less self documenting. But that is an argument for another time.

## Directories

| Name                                                                    | Description                                                                         |
| :---------------------------------------------------------------------  | :---------------------------------------------------------------------------------- |
| [include](https://github.com/KaiNorberg/PatchworkOS/tree/main/include)  | Public API.                                                                         |
| [lib](https://github.com/KaiNorberg/PatchworkOS/tree/main/lib)          | Third party stuff, for example OVMF-bin for QEMU.                                   |
| [make](https://github.com/KaiNorberg/PatchworkOS/tree/main/make)        | Lots of make files.                                                                 |
| [meta](https://github.com/KaiNorberg/PatchworkOS/tree/main/meta)        | Meta files for this repo, for example screenshots.                                  |
| [root](https://github.com/KaiNorberg/PatchworkOS/tree/main/root)        | Stores files that will be copied to the root directory of the generated .iso.          |
| [src](https://github.com/KaiNorberg/PatchworkOS/tree/main/src)          | Source code.                                                                        |
| [tools](https://github.com/KaiNorberg/PatchworkOS/tree/main/tools)      | Stores scripts that we use as a hacky alternative to compiling a cross-compiler.    |

## Sections

The project is split into various sections which both the include and src directories are also split into, below is a description of each section.

### bootloader

The UEFI bootloader is intended to be as small as possible and get out of the way as quickly as possible. It is responsible for collecting system info such as the GOP frame buffer and loading the ram disk, finally it loads the kernel directly into the higher half. The address space after the bootloader is done will have all physical memory identity mapped and the kernel mapped. It uses memory type EFI_RESERVED to store the kernel.

### kernel

The monolithic kernel is responsible for pretty much everything. Handles scheduling, hardware, virtual file system, IPC, SMP, etc. The kernel is fully premptive and multithreaded.

### libpatchwork

The libpatchwork library is best thought of as a wrapper around user space services and systems, for example it handles windowing via the [Desktop Window Manager](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/programs/dwm)  and provides access to system configuration via a series of configuration files stored at ```home:/cfg```.

### libstd

The libstd library is an extension of the C standard library, in the same way that something like Linux uses the POSIX extension to the C standard library. It contains the expected headers, string.h, stdlib.h etc., along with a few borrowed from POSIX like strings.h and then a bunch of extensions located in the [sys](https://github.com/KaiNorberg/PatchworkOS/tree/main/include/libstd/sys) directory. For instance, `sys/io.h` contains wrappers around the io system calls. The way I think of libstd is that its a wrapper around the kernel and its system calls, while libpatchwork is a wrapper around user space. The kernel and bootloader also has its own version of this library, containing for example memcpy, malloc, printf and similar functions to reduce code duplication while writing the OS. The seperation between the user space, kernel and bootloader versions of the library is handled by giving each platform having its own directory within the [platform](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/libstd/platform) directory.

### programs

Finally Patchwork has a series of programs designed for it, including shell utilities like [cat](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/programs/cat) and [echo](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/programs/echo), services like the [Desktop Window Manager](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/programs/dwm) and desktop apps like the [terminal](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/programs/terminal).

## Setup

<ins>**1. Cloning**</ins>

To clone this repository, you can either use the ```Code``` button at the top left of the screen on GitHub, or if you have [git](https://git-scm.com/) installed, run the ```git clone --recursive https://github.com/KaiNorberg/PatchworkOS``` command.

<ins>**2. Building**</ins>

Before building Patchwork, ensure you have make, GCC and NASM installed, you will need to use Linux, other systems like WSL *might* work, but i make to guarantees.

Once everything is installed, navigate to the cloned repository and run the ```make all``` command. You should then find a ```PatchworkOS.img``` file in the newly created bin directory.

<ins>**3. Running**</ins>

There are three ways to run Patchwork.

1. **Create a Bootable USB:** Use a tool like [balenaEtcher](https://etcher.balena.io/) to create a bootable USB using the created .img file.
2. **Use QEMU:** Download [QEMU](https://www.qemu.org/) and use the ```make run``` command.
3. **Other Virtual Machine:** Run the created .img file in a virtual machine of your choice.

## Tested Hardware Configurations

- Lenovo ThinkPad E495
- Ryzen 5 3600X | 32GB 3200MHZ Corsair Vengeance

Currently untested on Intel hardware. Let me know if you have different hardware, and it runs (or doesn't) for you!

## Contributing

If you find any bugs, issues or just have a suggestion for something I could do better, then feel free to open an issue or if you feel like it, you may submit a pull request.
