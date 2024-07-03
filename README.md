# Patchwork OS [![License](https://img.shields.io/badge/licence-MIT-green)](https://github.com/KaiNorberg/PatchworkOS/blob/main/LICENSE)

**Keep in mind that PatchworkOS is currently in a very early stage of development.**

## Features

- Very simple Unix inspired architecture (Not Unix-like)
- Monolithic preemptive 64-bit kernel
- SMP (Symmetric Multiprocessing)
- Multithreading (Kernel Level Threads)
- O(1) scheduler
- Custom Standard Library
- Custom UEFI Bootloader
- SIMD
- ![Custom image format (.fbmp)](https://github.com/KaiNorberg/fbmp)
- More to be added...

## Limitations

- Ram disks only
- Only x86_64

## Tested Hardware Configurations

- Lenovo Thinkpad E495
- Ryzen 5 3600X | 32GB 3200MHZ Corsair Vengeance

Currently untested on Intel hardware.

## Setup

<ins>**1. Cloning**</ins>

To clone this repository, you can either use the ```Code``` button at the top left of the screen on GitHub, or if you have ![Git](https://git-scm.com/) installed, run the ```git clone --recursive https://github.com/KaiNorberg/PatchworkOS``` command.

<ins>**2. Building**</ins>

Before building Patchwork, ensure you have make, gcc, nasm and mtools installed, Linux is also recommended.

Once everything is installed, navigate to the cloned repository and run the ```make setup all``` command:

You should then find a ```PatchworkOS.img``` file in the newly created bin directory.

<ins>**3. Running**</ins>

There are three ways to run Patchwork.

1. **Create a Bootable USB:** Use a tool like [balenaEtcher](https://etcher.balena.io/) to create a bootable USB using the created .img file.
2. **Use QEMU:** Download [QEMU](https://www.qemu.org/) and use the ```make run``` command.
3. **Other Virtual Machine:** Run the created .img file in a virtual machine of your choice.

## Roadmap

The short term goal is to create a basic user-space shell, however the long term goal is to play DOOM.

## Documentation (WIP)

Documentation will eventually be found on the ![wiki](https://github.com/Kaj9296/PatchworkOS/wiki) page.

## Contributing

Patchwork is intended as a personal project, therefor contributions are unwanted. However if you find any bugs, issues or just have a suggestion for something i could do better, then feel free to open an issue. This might change in the future.
