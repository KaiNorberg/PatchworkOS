# PatchworkOS [![License](https://img.shields.io/badge/licence-MIT-green)](https://github.com/Kaj9296/PatchworkOS/blob/main/LICENSE)

Patchwork is a hobbyist operating system written primarily in C.

***

**Keep in mind that Patchwork is currently in a very early stage of development and that these instructions may be incomplete.**

## Getting Started

<ins>**1. Cloning (downloading) this repository**</ins>

To clone (download) this repository, you can use the ```Code``` button at the top left of the screen, or if you have git installed use the following command ```git clone --recursive https://github.com/Kaj9296/PatchworkOS```.

<ins>**2. Building Patchwork**</ins>

In order to build Patchwork you will need to use Linux, tho using WSL is also an alternative. After you have made sure you have Make installed on your preferred system all you need to do is run the ```make all``` command. You will then find a .img file in the bin directory.

<ins>**3. Running Patchwork**</ins>

There are three ways to run Patchwork.

1. Use a tool like [Rufus](https://rufus.ie/en/) to create a bootable USB using the created .img file.
2. Download [QEMU](https://www.qemu.org/) on your Windows machine and then use the [run.bat](https://github.com/Kaj9296/PatchworkOS/blob/main/run.bat) file.
2. Download [QEMU](https://www.qemu.org/) on your Linux machine and then use the  ```make run``` command.

## Contributing

Patchwork is open to contributions. There are currently no strict guidelines for contributing. If the project grows significantly in the future, a more standardized process for contributions may be implemented.
