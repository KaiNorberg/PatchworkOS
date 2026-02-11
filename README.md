
# PatchworkOS

<div align="center">
    <a href="https://github.com/KaiNorberg/PatchworkOS/issues">
      <img src="https://img.shields.io/github/issues/KaiNorberg/PatchworkOS">
    </a>
    <a href="https://github.com/KaiNorberg/PatchworkOS/stargazers">
      <img src="https://img.shields.io/github/stars/KaiNorberg/PatchworkOS">
    </a>
    <a href="https://kainorberg.github.io/PatchworkOS/html/index.html">
      <img src="https://img.shields.io/badge/docs-Doxygen-blue">
    </a>
    <a href="https://github.com/KaiNorberg/PatchworkOS/blob/main/license">
      <img src="https://img.shields.io/github/license/KaiNorberg/PatchworkOS">
    </a>
    <a href="https://github.com/KaiNorberg/PatchworkOS/actions/workflows/test.yml">
        <img src="https://github.com/KaiNorberg/PatchworkOS/actions/workflows/test.yml/badge.svg" alt="Build and Test"/>
    </a>
</div>

<img src="meta/screenshots/desktop.png" alt="Desktop Screenshot" />

## Introduction

**PatchworkOS** is a non-POSIX operating system written from scratch in C and assembly that takes inspiration from many sources, such as an "everything is a file" philosophy from Plan9, asynchronous IRP based I/O from Windows NT and io_uring from Linux.

This is not a UNIX clone, it's intended to be a (hopefully) interesting experiment in operating system design while remaining approachable and educational. Sometimes this leads to bad results, and sometimes, with a bit of luck, good ones.

The goal is still to make a “real” operating system, one that runs on real hardware and has the performance one would expect from a modern operating system without jumping ahead to user space features or drivers, a floppy disk driver with a round-robin scheduler is not enough.

Will this project ever reach its goals? Probably not, but that’s not the point.

<table>
<tr>
<td width="50%" valign="top" align="center">
  <img src="meta/screenshots/stresstest.png" alt="Stresstest Screenshot" />
  <br>
  <i>Stress test showing ~100% utilization across 12 CPUs.</i>
</td>
<td width="50%" valign="top" align="center">
  <img src="meta/screenshots/doom.png" alt="Doom Screenshot" />
  <br>
  <i>DOOM running on PatchworkOS using a <a href="https://github.com/ozkl/doomgeneric">doomgeneric</a> port.</i>
</td>
</tr>
</table>

## Philosophy

There are three concepts that form the core of PatchworkOS, "everything is a file", asynchronous I/O and capability based security.

### Everything is a File

The "everything is a file" philosophy means that almost all kernel resources are exposed as files, where a file is defined as an object that can be interacted with "like a file", as in it can be opened, read, written, and closed.

> The file concept is distinct from a "regular file", which is a specific type of file that is stored on a disk and is what most people think of when they hear the word "file".

This can often result in unorthodox APIs that seem overcomplicated at first, but the goal is to provide a simple, consistent and most importantly composable interface for all kernel subsystems. The core argument is not that each individual API is better than its POSIX counterpart, but that they combine to form a system that is greater than the sum of its parts.

Plus its fun.

#### Process Creation

Let's use the `proc_create()` system call as an example of how this philosophy is applied. The `proc_create()` function is used to create new processes. It takes in three arguments:

- `const char** argv`: The argument vector, similar to POSIX systems except that the first argument is always the path to the executable.
- `proc_flags_t flags`: Flags controlling the creation of the new process, primarily what to inherit from the parent process.
- `proc_t* proc`: Output pointer for the child's identifier.

> The system call will also return a `status_t` value, as will all other system calls. The status value is a bitpacked value that can represent both errors and informational messages. All status values store what subsystem created them, the "kind" of error or message and a code describing what happened. See `<sys/status.h>`.

This system call is very minimal, but the flags allow enough control in most cases. For those other cases, the process can be created in a suspended state using the `PROC_SUSPENDED` flag and modified using its `/proc/<pid>/` directory.

For example, one could create a suspended process and then modify its environment variables by creating, reading or writing to files in its `/proc/<pid>/env` directory. One could also send `dup` commands to its `/proc/<pid>/ctl` file to modify its file descriptors. Finally, one could send the `start` command to its `/proc/<pid>/ctl` file to start the process.

Included below is a pseudocode example of how to use the `proc_create()` system call.

```c
const char* argv[] = {"/bin/sh", NULL};
status_t status = proc_create(argv, PROC_SUSPENDED, NULL);
if (IS_ERR(status))
{
    // Handle error
}

fd_t fd;
status = open(&fd, "/proc/123/ctl");
iowrite(fd, "dup 0 3", 7, 0, NULL); // Duplicate FD 3 to FD 0
iowrite(fd, "start", 5, 0, NULL);   // Start the process
```

The advantages of this approach are numerous, we avoid COW issues with `fork()`, weirdness with `vfork()`, system call bloat with `CreateProcess()`, and we get a very flexible and powerful process creation system that can use any of the other file based APIs to modify the child process. In exchange, the only real price we pay is overhead from additional context switches, string parsing and path traversals, how much this matters in practice is debatable.

#### Waiting for Processes

Let's take another example, say we wanted to wait on multiple processes with a `waitpid()` syscall. Since that is not possible, we would need a new system call. Which is not just confusing, but also inefficient.

Meanwhile, we just have a pollable `/proc/[pid]/wait` file that blocks until the process dies and returns the exit result, now any polling behavior that can be used on files can also be used while waiting on processes, including waiting on multiple processes at once, waiting on a keyboard and a process, waiting with a timeout, or any weird combination we can think of.

### Asynchronous I/O

There are two components to asynchronous I/O, the I/O Ring and I/O Request Packets.

The I/O Ring acts as the user-kernel space boundary and is made up of two queues mapped into user space. The first queue is the submission queue, which is used by the user to submit I/O requests to the kernel. The second queue is the completion queue, which is used by the kernel to notify the user of the completion of I/O requests. This system also features a register system, allowing I/O Requests to store the result of their operation to a virtual register, which another I/O Request can read from into their arguments, allowing for very complex operations to be performed asynchronously.

The I/O Request Packet is a self-contained structure that contains the information needed to perform an I/O operation. When the kernel receives a submission queue entry, it will parse it and create an I/O Request Packet from it. The I/O Request Packet will then be sent to the appropriate vnode (file system, device, etc.) for processing, once the I/O Request is completed, the kernel will write the result of the operation into the completion queue.

The combination of this system and our "everything is a file" philosophy means that since files are interacted with via asynchronous I/O and everything is a file, practically all operations can be asynchronous and dispatched via a I/O Ring.

### Security

In PatchworkOS, there are no Access Control Lists, user IDs or similar mechanisms. Instead, PatchworkOS uses a pseudo-capability security model based on per-process mountpoint namespaces and containerization. This means that there is no global filesystem view, each process has its own view of the filesystem defined by what has been mounted or bound into its namespace.

The namespace system allows for a composable, transparent and pseudo-capability security model. Processes can be given access to any combination of files and directories without needing hidden permission bits or similar mechanisms. Since everything is a file, this applies to practically everything in the system, including devices, IPC mechanisms, etc.

In most cases this is utilized by creating a process with an empty namespace, mounting a tmpfs instance as its root and then binding the necessary files and directories into the namespace.

### Standard Library

The standard library (libstd) is a superset of the ANSI C standard library, meaning that headers such as `<stdio.h>` and `<stdlib.h>` are included while POSIX headers such as `<unistd.h>` are not. Instead, the `sys` directory provides a set of PatchworkOS-specific headers such as `<sys/io.h>` and `<sys/proc.h>`.

Overall, an attempt is made to reuse and integrate our extensions cleanly without duplicating the ANSI sections of the standard library, for example the C11 `<threads.h>` header provides threading with `<sys/proc.h>` intentionally mirroring its API.

## Other Features

### Kernel

- Preemptive and tickless [EEVDF scheduler](https://kainorberg.github.io/PatchworkOS/html/d7/d85/group__kernel__sched.html) based upon the [original paper](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564) and implemented using an [Augmented Red-Black tree](https://kainorberg.github.io/PatchworkOS/html/da/d90/group__kernel__utils__rbtree.html) to achieve `O(log n)` worst case complexity. Providing a more approachable implementation of the scheduler used by the modern Linux kernel, but ours is obviously **a lot** less mature.
- Multithreading and Symmetric Multi Processing with fine-grained locking.
- Optimized memory management, featuring object caching and `O(1)` per page physical and virtual memory managers.
- File based IPC and driver abstractions.
- [Synchronization primitives](https://kainorberg.github.io/PatchworkOS/html/dd/d6b/group__kernel__sync.html) including Read-Copy-Update, mutexes, R/W locks, sequential locks, futex-inspired synchronization control objects and others.
- Highly [Modular design](#modules), even [SMP Bootstrapping](https://kainorberg.github.io/PatchworkOS/html/d3/d0a/group__modules__smp.html) is done in a module.

### File System

- Vnode and dentry based VFS with RCU traversal, hardlinks, symlinks, per-process namespaces, etc.
- Custom [Framebuffer BitMaP](https://github.com/KaiNorberg/fbmp) (.fbmp) image format, allows for faster loading by removing the need for parsing.
- Custom [Grayscale Raster Font](https://github.com/KaiNorberg/grf) (.grf) font format, allows for antialiasing and kerning without complex vector graphics.

### User Space

- Theming via [config files](https://github.com/KaiNorberg/PatchworkOS/blob/main/root/cfg).
- Capability based containerization security model using per-process mountpoint namespaces. See [Security](#security) for more info.
- Note that currently a heavy focus has been placed on the kernel and low-level stuff, so user space is quite small... for now.

*And much more...*

## Setup

```bash
# Install dependencies
sudo dnf install gcc make mtools qemu-system-x86 # For Fedora
sudo apt install build-essential mtools qemu-system-x86 # For Debian/Ubuntu

# Clone this repository, you can also use the green Code button at the top of the Github.
git clone https://github.com/KaiNorberg/PatchworkOS
cd PatchworkOS

# Build (creates PatchworkOS.img in bin/)
make all

# Run using QEMU
make run
```

---

## Doxygen Documentation

As one of the main goals of PatchworkOS is to be educational and approachable, the codebase is extensively documented with citations provided to any used sources when reasonable.

For more, check out the [documentation](https://kainorberg.github.io/PatchworkOS/html/index.html). Within the documentation checking the `topics` section in the sidebar is recommended.

## Modules

PatchworkOS uses a "modular" kernel design, meaning that instead of having one big kernel binary, the kernel is split into several smaller "modules" that can be loaded and unloaded at runtime.

This is highly convenient for development, but it also has practical advantages, for example, there is no need to load a driver for a device that is not attached to the system, saving memory.

### Make your own Module

Making a module is intended to be as straightforward as possible. For the sake of demonstration, we will create a simple "Hello, World!" module.

First, we create a new directory in `src/kernel/modules/` named `hello`, and inside that directory we create a `hello.c` file to which we write the following code:

```c
#include <kernel/module/module.h>
#include <kernel/log/log.h>

#include <stdint.h>

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        LOG_INFO("Hello, World!\n");
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Hello", "<author>", "A simple hello world module", "1.0", "MIT", "BOOT_ALWAYS");
```

An explanation of the code will be provided later.

Now we need to add the module to the build system. To do this, just copy an existing module's `.mk` file without making any modifications. For example, we can copy `src/modules/drivers/ps2/ps2.mk` to `src/modules/hello/hello.mk`. The build system will handle the rest, including copying the module to the final image.

Now, we can build and run PatchworkOS using `make all run`, or we could use `make all` and then flash the generated `bin/PatchworkOS.img` file to a USB drive.

Now to validate that the module is working, you can either watch the boot log and spot the `Hello, World!` message, or you could use `grep` on the `/dev/klog` file in the terminal program like so:

```bash
cat /dev/klog | grep "Hello, World!"
```

This should output something like:

```bash
[   0.747-00-I] Hello, World!
```

That's all, if this did not work, make sure you followed all the steps correctly. If there is still issues, feel free to open an issue.

### What can I do now?

Whatever you want. You can include any kernel header, or even headers from other modules, create your own modules and include their headers or anything else. There is no need to worry about linking, dependencies or exporting/importing symbols, the kernels module loader will handle all of it for you. Go nuts.

### Code Explanation

This code in the `hello.c` file does a few things. First, it includes the relevant kernel headers.

Second, it defines a `_module_procedure()` function. This function serves as the entry point for the module and will be called by the kernel to notify the module of events, for example the module being loaded or a device attached. On the load event, it will print using the kernels logging system `"Hello, World!"`, resulting in the message being readable from `/dev/klog`.

Finally, it defines the modules information. This information is, from left to right, the name of the module, the author of the module (that's you), a short description of the module, the module version, the license of the module, and finally a list of "device types", in this case just `BOOT_ALWAYS`, but more could be added by separating them with a semicolon (`;`).

The list of device types is what causes the kernel to actually load the module. We will avoid going into too much detail (you can check the documentation for that), but included is a brief explanation.

The module loader itself has no idea what these type strings actually are, but subsystems can specify that "a device of the type represented by this string is now available", the module loader can then load either one or all modules that have specified in their list of device types that it can handle the specified type. This means that any new subsystem, ACPI, USB, PCI, etc., can implement dynamic module loading using whatever types they want.

So what is `BOOT_ALWAYS`? It is the type of special device that the kernel will pretend to "attach" during boot. In this case, it simply causes our hello module to be loaded during boot.

For more information, check the [Module Documentation](https://kainorberg.github.io/PatchworkOS/html/dd/d41/group__kernel__module.html).

## ACPI (WIP)

PatchworkOS features a from-scratch ACPI implementation and AML parser, with the goal of being, at least by ACPI standards, easy to understand and educational. It is tested on the [Tested Configurations](#tested-configurations) below and against [ACPICA's](https://github.com/acpica/acpica) runtime test suite, but remains a work in progress (and probably always will be).

See [ACPI Documentation](https://kainorberg.github.io/PatchworkOS/html/d1/d39/group__modules__acpi.html) for a progress checklist.

See [ACPI specification Version 6.6](https://uefi.org/specs/ACPI/6.6/index.html) as the main reference.

### What is ACPI?

ACPI or Advanced Configuration and Power Interface is used for *a lot* of things in modern systems but mainly power management and device enumeration/configuration. It's not possible to go over everything here, instead a brief overview of the parts most likely to cause confusion while reading the code will be provided.

It consists of two main parts, the ACPI tables and AML bytecode. If you have completed a basic operating systems tutorial, you have probably seen the ACPI tables before, for example the RSDP, FADT, MADT, etc. These tables are static in memory data structures storing information about the system, they are very easy to parse but are limited in what they can express.

AML or ACPI Machine Language is a Turing complete "mini language", and the source of much frustration, that is used to express more complex data, primarily device configuration. This is needed as its impossible for any specification to account for every possible hardware configuration that exists currently, much less that may exist in the future. So instead of trying to design that, what if we could just have a small program generate whatever data we wanted dynamically? Well that's more or less what AML is.

### Device Configuration

To demonstrate how ACPI is used for device configuration, we will use the [PS/2 driver](https://kainorberg.github.io/PatchworkOS/html/d9/d70/group__modules__drivers__ps2.html) as an example.

If you have followed a basic operating systems tutorial, you have probably implemented a PS/2 keyboard driver at some point, and most likely you hardcoded the I/O ports `0x60` and `0x64` for data and commands respectively, and IRQ `1` for keyboard interrupts.

Using this hardcoded approach will work for the vast majority of systems, but, perhaps surprisingly, there is no standard that guarantees that these ports and IRQs will actually be used for PS/2 devices. It's just a silent agreement that pretty much all systems adhere to for legacy reasons.

But this is where the device configuration from AML comes in, it lets us query the system for the actual resources used by the PS/2 keyboard, so we don't have to rely on hardcoded values.

If you were to decompile the AML bytecode into its original ASL (ACPI Source Language), you might find something like this:

```asl
Device (KBD)
{
    Name (_HID, EisaId ("PNP0303") /* IBM Enhanced Keyboard (101/102-key, PS/2 Mouse) */)  // _HID: Hardware ID
    Name (_STA, 0x0F)  // _STA: Status
    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
    {
        IO (Decode16,
            0x0060,             // Range Minimum
            0x0060,             // Range Maximum
            0x01,               // Alignment
            0x01,               // Length
            )
        IO (Decode16,
            0x0064,             // Range Minimum
            0x0064,             // Range Maximum
            0x01,               // Alignment
            0x01,               // Length
            )
        IRQNoFlags ()
            {1}
    })
}
```

*Note that just like C compiles to assembly, ASL compiles to AML bytecode, which is what the OS actually parses.*

In the example ASL, we see a `Device` object representing a PS/2 keyboard. It has a hardware ID (`_HID`), which we can cross-reference with an [online database](https://uefi.org/PNP_ACPI_Registry) to confirm that it is indeed a PS/2 keyboard, a status (`_STA`), which is just a bit field indicating if the device is present, enabled, etc., and finally the current resource settings (`_CRS`), which is the thing we are really after.

The `_CRS` might look a bit complicated but focus on the `IO` and `IRQNoFlags` entries. Notice how they are specifying the I/O ports and IRQ used by the keyboard? Which in this case are indeed `0x60`, `0x64` and `1` respectively. So in this case the standard held true.

So how is this information used? During boot, the `_CRS` information of each device is parsed by the ACPI subsystem, it then queries the kernel for the needed resources, assigned them to each device and makes the final configuration available to drivers.

Then when the PS/2 driver is loaded, it gets told "you are handling a device with the name `\_SB_.PCI0.SF8_.KBD_` (which is just the full path to the device object in the ACPI namespace) and the type `PNP0303`", it can then query the ACPI subsystem for the resources assigned to that device, and use them instead of hardcoded values.

Having access to this information for all devices also allows us to avoid resource conflicts, making sure two devices are not trying to use the same IRQ(s) or I/O port(s).

Of course, it gets way, way worse than this, but hopefully this clarifies why the PS/2 driver and other drivers, might look a bit different from what you might be used to.

---

## Development

### Additional commands

```bash
# Clean build files
make clean

# Build with debug mode enabled
make all DEBUG=1

# Build with debug mode enabled and testing enabled (you will need to have iasl installed)
make all DEBUG=1 TESTING=1

# Debug using qemu with one cpu and GDB
make all run DEBUG=1 QEMU_CPUS=1 GDB=1

# Debug using qemu and exit on panic
make all run DEBUG=1 QEMU_EXIT_ON_PANIC=1

# Generate doxygen documentation
make doxygen

# Create compile commands file
make compile_commands
```

### Repo Structure

Source code can be found in the `src/` directory, with public API headers in the `include/` directory, private API headers are located alongside their respective source files.

```plain
.
├── meta              // Meta files including screenshots, doxygen, etc.
├── lib               // Third party files, for example doomgeneric.
├── root              // Files to copy to the root of the generated image.
└── <src|include>     // Source code and public API headers.
    ├── boot          // UEFI bootloader.
    ├── boxes         // Boxed applications.
    ├── kernel        // The kernel and its core subsystems.
    ├── libpatchwork  // The PatchworkOS system library, gui, etc.
    ├── libstd        // The C standard library.
    ├── modules       // Kernel modules, drivers, filesystems, etc.
    └── programs      // User space programs.
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

- **QEMU boot failure**: Check if you are using QEMU version 10.0.0, as that version has previously caused issues. These issues appear to be fixed currently however consider using version 9.2.3
- **Any other errors?**: If an error not listed here occurs or is not resolvable, please open an issue in the GitHub repository.

## Testing

Testing uses a GitHub action that compiles the project and runs it for some amount of time using QEMU with `DEBUG=1`, `TESTING=1` and `QEMU_EXIT_ON_PANIC=1` set. This will run some additional tests in the kernel (for example it will clone ACPICA and run all its runtime tests), and if QEMU has not crashed by the end of the allotted time, it is considered a success.

Note that the `QEMU_EXIT_ON_PANIC` flag will cause any failed test, assert or panic in the kernel to exit QEMU using their "-device isa-debug-exit" feature with a non-zero exit code, thus causing the GitHub action to fail.

### Tested Configurations

- QEMU emulator version 9.2.3 (qemu-9.2.3-1.fc42)
- Lenovo ThinkPad E495
- Ryzen 5 3600X | 32GB 3200MHZ Corsair Vengeance

Currently untested on Intel hardware (broke student, no access to hardware). Let me know if you have different hardware, and it runs (or doesn't) for you!

## Roadmap

### Current Work

- Continue refactoring the kernel to replace synchronous code with asynchronous code.
- Replace local sockets with 9P file servers.
- Completely redo user-space to use new async and 9P, as the kernel has simply outgrown user-space.

### Notable Future Plans

- Improve `share()` and `claim()` security by specifying a target PID when sharing.
- Port LUA and use it for dynamic system configuration.
- Driver support, for example USB.

### Known Limitations

- Currently limited to RAM disks only (Waiting for USB support).
- Only support for x86_64.

## Community

We use [GitHub Discussions](https://github.com/KaiNorberg/PatchworkOS/discussions) for announcements, progress updates and general discussions. As such, subscribing to the discussions or watching the repository is the best way to stay updated.

If you like PatchworkOS, consider starring the repository or donating via [GitHub Sponsors](https://github.com/sponsors/KaiNorberg) to support development.

### Contributing

Contributions are welcome! Anything from bug reports/fixes, performance improvements, new features, or even just fixing typos or adding documentation.

If you are unsure where to start, check the [Todo List](https://kainorberg.github.io/PatchworkOS/html/dd/da0/todo.html).

Check out the [contribution guidelines](CONTRIBUTING.md) to get started.

## License

Distributed under the MIT License. See [LICENSE](https://github.com/KaiNorberg/PatchworkOS/blob/main/LICENSE) for more information.

## Nostalgia

[The first Reddit post and image of PatchworkOS](https://www.reddit.com/r/osdev/comments/18gbsng/a_little_over_2_years_ago_i_posted_a_screenshot/) from back when getting to user space was a massive milestone and the kernel was supposed to be a UNIX-like microkernel.
