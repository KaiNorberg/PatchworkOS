# PatchworkOS [![License](https://img.shields.io/badge/licence-MIT-green)](https://github.com/Kaj9296/PatchworkOS/blob/main/LICENSE)

Patchwork is a hobbyist OS built in C for the x86_64 architecture.

***

**Keep in mind that Patchwork is currently in a very early stage of development and that these instructions may be incomplete.**

## Setup

<ins>**1. Cloning (downloading) this repository**</ins>

To clone (download) this repository, you can use the ```Code``` button at the top left of the screen, or if you have git installed use the following command ```git clone --recursive https://github.com/Kaj9296/PatchworkOS```.

<ins>**2. Building Patchwork**</ins>

In order to build Patchwork you will need to use either Linux or WSL. You will also need to have Make, Python, mtools, Nasm and gcc installed.

After you have chosen your preferred system run the ```python build.py --setup --all``` command. You will then find a .img file in the bin directory.

<ins>**3. Running Patchwork**</ins>

There are four ways to run Patchwork.

1. Use a tool like [Rufus](https://rufus.ie/en/) to create a bootable USB using the created .img file.
2. Download [QEMU](https://www.qemu.org/) on your Windows machine and then use the [run.bat](https://github.com/Kaj9296/PatchworkOS/blob/main/run.bat) file.
3. Download [QEMU](https://www.qemu.org/) on your Linux machine and then use the  ```python build.py --run``` command.
4. Run the created .img file in a virtual machine of your choice.

## Roadmap

The current long term goal is to create a desktop environment and to play DOOM.

## Documentation (WIP)

Documentation will eventually be found on the ![wiki](https://github.com/Kaj9296/PatchworkOS/wiki) page.

## Contributing

Patchwork is intended as a personal project, therefor contributions to add features are unwanted, however contributions to fix bugs or address issues are welcome. There are no strict guidelines due to the small scale of the project.
