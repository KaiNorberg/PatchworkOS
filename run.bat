set OSNAME=Electric_OS
set BUILDDIR=%0/../bin
set OVMFDIR=%0/../OVMFbin
set BUILDDIR=%BUILDDIR:"=%
set OVMFDIR=%OVMFDIR:"=%
qemu-system-x86_64 -machine q35 -drive file=%BUILDDIR%/%OSNAME%.img -m 1G -drive if=pflash,format=raw,unit=0,file=%OVMFDIR%/OVMF_CODE-pure-efi.fd,readonly=on -drive if=pflash,format=raw,unit=1,file=%OVMFDIR%/OVMF_VARS-pure-efi.fd -net none