import argparse
import subprocess
import os
import shutil      
from sys import platform

from termcolor import colored

def run_make_files(target):
    for foldername, subfolders, filenames in os.walk("./src/"):
        for filename in filenames:
            if filename.lower() == "makefile":
                print(colored("Making", "white"), colored(f"{foldername} {target}...", "white"))
                try:
                    subprocess.run(["make", "-s", target], cwd=foldername, check=True)
                except:
                    exit()

def copy_dir_to_img(imgPath, dirPath, dirImg):
    for foldername, subfolders, filenames in os.walk(dirPath):
        cleanPath = os.path.relpath(foldername, dirPath)
        cleanPath = os.path.join(dirImg, cleanPath)
        cleanPath = cleanPath.removesuffix("/.")
        
        if cleanPath != ".":
            subprocess.run(["mmd", "-i", imgPath, f"::{cleanPath}"], cwd=".", check=True)                 
        for filename in filenames:
            subprocess.run(["mcopy", "-i", imgPath, f"{foldername}/{filename}", f"::{cleanPath}"], cwd=".", check=True)   
            
                
def setup():
    print(colored("!====== RUNNING SETUP  ======!", "white"))
    if not os.path.exists("bin"):
        os.makedirs("bin")
    if not os.path.exists("build"):
        os.makedirs("build")
    
    subprocess.run(["make", "all"], cwd="vendor/gnu-efi", check=True)
    
    run_make_files("setup")

def clean():
    print(colored("!====== RUNNING CLEAN ======!", "white"))
    if os.path.exists("bin"):
        shutil.rmtree("bin")
    if os.path.exists("build"):
        shutil.rmtree("build")
    
    run_make_files("clean")

def build():
    print(colored("!====== RUNNING BUILD ======!", "white"))
    run_make_files("build")

def link():
    print(colored("!====== RUNNING LINK ======!", "white"))
    run_make_files("link")

def deploy():
    print(colored("!====== RUNNING DEPLOY ======!", "white"))
    subprocess.run(["dd", "if=/dev/zero", "of=bin/PatchworkOS.img", "bs=4096", "count=1024"], cwd=".", check=True)
    
    subprocess.run(["mkfs", "-t", "vfat", "bin/PatchworkOS.img"], cwd=".", check=True)
    
    copy_dir_to_img("bin/PatchworkOS.img", "root/", "")
    copy_dir_to_img("bin/PatchworkOS.img", "bin/efi", "efi")
    copy_dir_to_img("bin/PatchworkOS.img", "bin/kernel", "kernel")
    copy_dir_to_img("bin/PatchworkOS.img", "bin/programs/", "programs")
            
def run():
    subprocess.run([
    'qemu-system-x86_64',
    '-drive', 'file=bin/PatchworkOS.img',
    '-m', '1G',
    '-cpu', 'qemu64',
    '-drive', 'if=pflash,format=raw,unit=0,file=vendor/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on',
    '-drive', 'if=pflash,format=raw,unit=1,file=vendor/OVMFbin/OVMF_VARS-pure-efi.fd',
    '-net', 'none'], cwd=".", check=True)   

def all():
    build()
    link()
    deploy()
    
functionMap = {
    "clean": clean,
    "setup": setup,
    "build": build,
    "link": link,
    "deploy": deploy,
    "run": run,
    "all": all
}

parser = argparse.ArgumentParser(description="Patchwork build tools")
for name in functionMap.keys():
    parser.add_argument(f"-{name[0]}", f"--{name}", action="store_true", help=f"Call {name}")
args = parser.parse_args()

def main():
    for name, value in vars(args).items():
        if value:
            functionMap[name]()

if __name__ == "__main__":
    main()