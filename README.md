# PatchworkOS

> **⚠ Warning**<br> Keep in mind that PatchworkOS is currently in a very early stage of development, and may have both known and unknown bugs.

![License](https://img.shields.io/badge/License-MIT-green) [![Build and Test](https://github.com/KaiNorberg/PatchworkOS/actions/workflows/test.yml/badge.svg)](https://github.com/KaiNorberg/PatchworkOS/actions/workflows/test.yml)

**Patchwork** is a 64 bit monolithic hobbyist OS built from scratch in C for the x86_64 architecture. It's intended as an easy-to-modify toy-like NON-POSIX OS that takes many ideas from Unix, Plan9, DOS and other places while simplifying them and removing some fat. Made entirely for fun.

## Screenshots

![Desktop Screenshot](meta/screenshots/desktop.png)
![Doom Screenshot](meta/screenshots/doom.png)

## Quick Start

```bash
git clone --recursive https://github.com/KaiNorberg/PatchworkOS
cd PatchworkOS
make all
make run  # Requires QEMU
```

See [Setup](#Setup) for more.

---

## Monolithic Preemptive 64-bit Fully Tickless Kernel with SMP

Patchwork has full SMP (Symmetric Multi Processing) support, and is fully preemptive, meaning that the scheduler is allowed to preempt a running thread even while that thread is in kernel space, resulting in significant improvements to latency.

Additionally, it is completely tickless. Many kernels rely on timer interrupts that occur at a regular rate to allow the kernel more opportunities to schedule, these timer interrupts are usually called ticks, it's also common for the kernel to be given a chance to schedule at the end of a system call. This system is quite inefficient and not often used in modern kernels, instead of their timers having a fixed regular rate, they can change the amount of time between each timer interrupt this is referred to as a kernel being tickless.

Say we know that the current thread's time slice expires in 100 ms and a thread needs to unblock in 50 ms, and that's all that needs to happen, then we can just set a timer for the lowest of the two values (50 ms) and perform the needed work when the timer arrives, instead of constantly checking if work needs to be done at regular intervals using ticks. This means that in a tickless kernel when there is no work to be done there is truly nothing happening a "true" 0% CPU usage, unlike in a tick based kernel where even when idle the CPU is still performing checks in its regular timer interrupt. A tickless kernel can also have faster system calls as they no longer have to provide an opportunity for the kernel to schedule, the scheduling will always happen exactly when needed due to the dynamic timers. There are also additional considerations for SMP, like how to notify an idle CPU of available threads, but I have a tendency to ramble, so we will leave it there.

## *Mostly* Constant-Time Memory Management

Memory management in the kernel used by Patchwork is designed to run in constant time *per page*, from the physical memory manager to the virtual memory manager.

### Physical Memory manager

When user space wants to allocate memory, it is first allocated from the PMM (Physical Memory Manager), which uses a simple constant-time free stack for most allocations. There is also a non-constant time bitmap allocator for more special allocations that need specific alignment or where the allocated address needs to be within a range of addresses, but this is almost never used. In short, all allocations that matter run in constant-time.

### Virtual Memory Manager

Then after being allocated from the PMM the VMM (Virtual Memory Manager) maps the memory to the processes address space, this also runs in constant time. It does this by embedding metadata directly into the page table structure itself. Since page tables are essentially just arrays, accessing any element runs in constant time, allowing virtual memory operations to also be performed in constant-time, with the asterisk that it's constant time *per page*. Mapping n pages is still O(n). Note that if user space does not specify a desired address for the mapping then the kernel needs to find a free region for the mapping, this is done by storing the previously mapped address and mapping the new memory above it, or iterating from there until a free region is found. In short, the VMM runs in constant-time for all allocations and mapping, unless the address space is fragmented because then finding a free region will require iterating to find a free region.

### Callbacks

For more advanced concepts like shared memory there is a system for mapping memory with a callback, such that the callback will be called when all the pages have been unmapped or the address space is freed (the process is killed). This is used to for example to implement reference counting in a separate shared memory system. Updating these callbacks is also constant-time per page, but the callback system will scale with O(n) where n is the amount of callbacks within an unmapped region (not the entire address space), note that updating a callback is as simple as decrementing a number storing the amount of pages mapped with the callback, so this is hardly a big concern.

### An Example

The callback system is also a good example to use to explain how metadata is stored in the page tables. In x86_64, each table has a set of bits within each entry that is available for use by the kernel to store whatever it wants. This is where we put our metadata, in the case of the callback system we reserve 8 of the bits to store a "callback ID". This is the index into an array of callback structures stored by each address space, each of these structures stores for example the amount of pages associated with the callback and the callback function. When we unmap pages we count how many pages within the region we are unmapping have each used callback ID, then we simply decrement the amount of pages associated with each callback by the amount of pages found in the region with its callback ID, when that number reaches 0, we call the callback, note that all lookups are done via an array to avoid lookups. There are also additional optimizations like using a bitmap to store which callback IDs are currently being used. The system's big limitation is that we are limited in the number of IDs we can have as we have a limited amount of bits in each PML entry.

Finally, the kernel uses a combination of slab allocators and direct virtual memory allocation to allocate memory for itself. So in short, yes there are a few asterisks, but in the most common cases of expanding a process's user heap, mapping to an arbitrary address or similar or allocating memory for a kernel structure, memory management will always be constant-time per page.

## Other Features

- Kernel level multithreading with a [constant-time scheduler](https://github.com/KaiNorberg/PatchworkOS/blob/main/src/kernel/sched/sched.h), supporting dynamic priorities, dynamic time slices, and more.
- Custom C standard library and system libraries.
- SIMD.
- [Custom image format (.fbmp)](https://github.com/KaiNorberg/fbmp).
- [Custom font format (.grf)](https://github.com/KaiNorberg/grf).
- Strict adherence to "everything is a file".
- IPC including pipes, shared memory, sockets and plan9 inspired "signals" called notes.
- And much more...

## Notable Differences with Unix

- Multiroot file system, with labels not letters ```home:/usr/bin```.
- Replaced ```fork(), exec()``` with ```spawn()```.
- Single-User.
- Non POSIX standard library.

## Limitations

- Currently limited to RAM disks only, might remain that way for quite some time because being able to run the OS on real hardware is very important to me meaning that simple I/O devices like floppy disks can't be used as I do not live in the 80s, instead the optimal plan would be to allow for USB storage devices something I am currently not willing/able to work on.
- Only support for x86_64.

## Notable Short Term Future Plans

- Software interrupts for notes (signals).
- Lua port.
- Capability based security model (currently has no well-defined security model).

---

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

The first is that I want Patchwork to be easy to expand upon, for that sake I want its interfaces to be highly generalized. Normally, to just implement a single system call is quite a lot of work. You'd need to implement its behavior, register the system call handler, then you'd need to create it in the standard library, and you'd need to make whatever software to actually use that system call, that is a surprisingly large amount of stuff that needs to be changed just for a single small system call. Meanwhile with this system, when sockets were implemented the only thing that needed to be done was implementing the sockets, the rest of the operating system could remain the same.

The second reason is that it makes using the shell far more interesting, there is no need for special functions or any other magic keywords to for instance use sockets, all it takes is opening and reading from files.

Let's take an example of these first two reasons. Say we wanted to implement the ability to wait for a process to die via a normal system. First we need to implement the kernel behavior to do that, then the appropriate system call, then add in handling for that system call in the standard library, then the actual function itself in the standard library and finally probably create some program that could be used in the shell. That's a lot of work for something as simple as a waiting for a process to die. Meanwhile, if waiting for a processes death is done via just writing to that processes "ctl" file then it's as simple as adding a "wait" action to it and calling it a day, you can now easily use that behavior via the standard library and via the shell by something like ```echo wait > sys:/proc/[pid]/ctl``` without any additional work.

And of course the third and final reason is because I think it's fun, and honestly I think this kind of system is just kinda beautiful due to just how generalized and how strictly it follows the idea that "everything is a file". There are downsides, of course, like the fact that these systems are less self documenting. But that is an argument for another time.

## A Big Taste (Documentation)

If you are still interested in knowing more, then you can check out the Doxygen generated [documentation](https://kainorberg.github.io/PatchworkOS/html/index.html).

---

## Directories

| Name                                                                    | Description                                                                         |
| :---------------------------------------------------------------------  | :---------------------------------------------------------------------------------- |
| [include](https://github.com/KaiNorberg/PatchworkOS/tree/main/include)  | Public API.                                                                         |
| [lib](https://github.com/KaiNorberg/PatchworkOS/tree/main/lib)          | Third party stuff, for example OVMF-bin for QEMU.                                   |
| [make](https://github.com/KaiNorberg/PatchworkOS/tree/main/make)        | Lots of make files.                                                                 |
| [meta](https://github.com/KaiNorberg/PatchworkOS/tree/main/meta)        | Meta files for this repo, for example screenshots.                                  |
| [root](https://github.com/KaiNorberg/PatchworkOS/tree/main/root)        | Stores files that will be copied to the root directory of the generated .iso.       |
| [src](https://github.com/KaiNorberg/PatchworkOS/tree/main/src)          | Source code.                                                                        |
| [tools](https://github.com/KaiNorberg/PatchworkOS/tree/main/tools)      | Stores scripts that we use as a hacky alternative to compiling a cross-compiler.    |

## Sections

The project is split into various sections which both the include and src directories are also split into, below is a description of each section.

### bootloader

The UEFI bootloader is intended to be as small as possible and get out of the way as quickly as possible. It is responsible for collecting system info such as the GOP frame buffer and loading the ram disk, finally it loads the kernel directly into the higher half. The address space after the bootloader is done will have all physical memory identity mapped and the kernel mapped. It uses memory type EFI_RESERVED to store the kernel.

### kernel

The monolithic kernel is responsible for pretty much everything. Handles scheduling, hardware, virtual file system, IPC, SMP, etc. The kernel is fully preemptive and multithreaded.

### libpatchwork

The libpatchwork library is best thought of as a wrapper around user space services and systems, for example it handles windowing via the [Desktop Window Manager](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/programs/dwm)  and provides access to system configuration via a series of configuration files stored at ```home:/cfg```.

### libstd

The libstd library is an extension of the C standard library, in the same way that something like Linux uses the POSIX extension to the C standard library. It contains the expected headers, string.h, stdlib.h etc., along with a few borrowed from POSIX like strings.h and then a bunch of extensions located in the [sys](https://github.com/KaiNorberg/PatchworkOS/tree/main/include/libstd/sys) directory. For instance, `sys/io.h` contains wrappers around the io system calls. The way I think of libstd is that its a wrapper around the kernel and its system calls, while libpatchwork is a wrapper around user space. The kernel and bootloader also has its own version of this library, containing for example memcpy, malloc, printf and similar functions to reduce code duplication while writing the OS. The seperation between the user space, kernel and bootloader versions of the library is handled by giving each platform having its own directory within the [platform](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/libstd/platform) directory.

### programs

Finally Patchwork has a series of programs designed for it, including shell utilities like [cat](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/programs/cat) and [echo](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/programs/echo), services like the [Desktop Window Manager](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/programs/dwm) and desktop apps like the [terminal](https://github.com/KaiNorberg/PatchworkOS/tree/main/src/programs/terminal).

---

## Setup

### Requirements

- **OS:** Linux, WSL appears to work, but I make no guarantees
- **Tools:** GCC, make, NASM, mtools, QEMU (optional)

### Build and Run

```bash
# Clone this repository, you can also use the green Code button at the top of the Github.
git clone --recursive https://github.com/KaiNorberg/PatchworkOS
cd PatchworkOS

# Build (creates PatchworkOS.img in bin/)
make all

# Run using QEMU
make run
```

### Alternative Runtimes

- **Real Hardware:** Flash `PatchworkOS.img` to USB with tools like [balenaEtcher](https://etcher.balena.io/)
- **Other VMs:** Import the `.img` file into VirtualBox, VMware, etc.

### Troubleshooting

- **QEMU boot failure** Avoid QEMU 10.0.0 try using version 9.2.3.
- **Build fails?** Ensure all dependencies are installed.
- **Any other errors?** If an error not listed here occurs or is not resolvable, please open an issue in the GitHub.

## Testing

This repository uses a bit of a hacky way to do testing, we use a github action, as normal, that compiles the operating system then runs it using QEMU, QEMU is then allowed to run for one minute, the kernel will run some tests and then start as normal. If QEMU crashes* then the test fails, if it is still running after one-minute we call it a success. Its an overly simple approach but gets the job done. A lot of the difficulty in performing testing comes from the inherent complexity of testing a OS, which also means that testing is currently very very limited in the kernel.

\* QEMU will crash if a kernel panic occurs due to the use of QEMU's isa-debug-exit in the kernel when make is called with DEBUG=1.

### Tested Configurations

- QEMU emulator version 9.2.3 (qemu-9.2.3-1.fc42)
- Lenovo ThinkPad E495
- Ryzen 5 3600X | 32GB 3200MHZ Corsair Vengeance

Currently untested on Intel hardware. Let me know if you have different hardware, and it runs (or doesn't) for you!

---

## Contributing

If you find any bugs, issues or just have a suggestion for something I could do better, then feel free to open an issue or if you feel like it, you may submit a pull request.
