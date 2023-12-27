# PatchworkOS [![License](https://img.shields.io/badge/licence-MIT-green)](https://github.com/Kaj9296/PatchworkOS/blob/main/LICENSE)

Patchwork is a hobbyist operating system written primarily in C.

***

**Keep in mind that Patchwork is currently in a very early stage of development and that these instructions may be incomplete.**

## Getting Started

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
  
## Contributing

Patchwork is intended to by my project, thus contributing major features or similar is not what I'm looking for. However, if you have the patience, you are welcome to try to fix any bugs or other issues you find. There are no strict guidelines beyond what's previously stated due to the small scale of the project.
