# PatchworkOS [![License](https://img.shields.io/badge/licence-MIT-green)](https://github.com/Kaj9296/PatchworkOS/blob/main/LICENSE)

**Keep in mind that Patchwork is currently in a very early stage of development.**

## What is Patchwork?

Patchwork is an attempt to make an operating system single-handed in a reasonable amount of time. This of course requires a rather simple and improvised OS design, which is why Patchwork uses a simplified UNIX inspired design, however it is not UNIX-like. Note that in this case, simple means "containing few parts" or "easily understood" not merely "low quality".

The simple design means that Patchwork may be useful for those wishing to learn about how operating systems function, or for simply playing around with an existing code base. 

## Features

  - Multicore 64bit monolithic kernel
  - Custom UEFI bootloader
  - More to be added...

## Limitations

  - Only supports x86_64

## Setup

<ins>**1. Cloning (downloading) this repository**</ins>

To clone (download) this repository, you can use the ```Code``` button at the top left of the screen, or if you have git installed use the following command ```git clone --recursive https://github.com/Kaj9296/PatchworkOS```.

<ins>**2. Building Patchwork**</ins>

In order to build Patchwork you will need to use either Linux or WSL. You will also need to have Make, mtools, NASM and GCC installed, it is also possible to use clang by just editing the Makefile.

After everything is installed simply run ```make setup all```. You should then find a .img file in the bin directory.

<ins>**3. Running Patchwork**</ins>

There are three ways to run Patchwork.

1. Use a tool like [balenaEtcher](https://etcher.balena.io/) to create a bootable USB using the created .img file.
2. Download [QEMU](https://www.qemu.org/) on your Linux machine and use ```make run```.
3. Run the created .img file in a virtual machine of your choice.

## Roadmap

The short term goal is to create a basic terminal/shell, however the current long term goal is to play DOOM.

## Documentation (WIP)

Documentation will eventually be found on the ![wiki](https://github.com/Kaj9296/PatchworkOS/wiki) page.

## Contributing

Patchwork is intended as a personal project, therefor contributions to add features are unwanted. However if you find any bugs, issues or just have a suggestion for something i could do better, then feel free to open an issue.
