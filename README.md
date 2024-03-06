# Patchwork OS [![License](https://img.shields.io/badge/licence-MIT-green)](https://github.com/Kaj9296/PatchworkOS/blob/main/LICENSE)

**Keep in mind that PatchworkOS is currently in a very early stage of development.**

## Features

  - Monolithic Preemptive kernel
  - 64-bit
  - SMP (Symmetric Multiprocessing)
  - Custom UEFI bootloader
  - More to be added...

## Limitations

  - Only supports x86_64

## Setup

<ins>**1. Cloning (downloading) this repository**</ins>

To clone (download) this repository, you can use the ```Code``` button at the top left of the screen, or if you have git installed use the following command ```git clone --recursive https://github.com/Kaj9296/PatchworkOS```.

<ins>**2. Building Patchwork**</ins>

In order to build Patchwork you will need to use either Linux or if you insist WSL. You will also need to have Make, NASM and GCC installed, it is also possible to use clang by just editing the Makefile.

After everything is installed simply run ```make setup all```. You should then find a .img file in the bin directory.

<ins>**3. Running Patchwork**</ins>

There are three ways to run Patchwork.

1. Use a tool like [balenaEtcher](https://etcher.balena.io/) to create a bootable USB using the created .img file.
2. Download [QEMU](https://www.qemu.org/) on your Linux machine and use ```make run```.
3. Run the created .img file in a virtual machine of your choice.

## Roadmap

The short term goal is to create a basic user-space shell, however the long term goal is to play DOOM.

## Documentation (WIP)

Documentation will eventually be found on the ![wiki](https://github.com/Kaj9296/PatchworkOS/wiki) page.

## Contributing

Patchwork is intended as a personal project, therefor contributions to add features are unwanted. However if you find any bugs, issues or just have a suggestion for something i could do better, then feel free to open an issue.
