@echo -off
mode 80 25

cls
if exist .\efi\boot\bootx64.efi then
 .\efi\boot\bootx64.efi
 goto END
endif

if exist fs0:\efi\boot\bootx64.efi then
 fs0:
 echo Found bootloader on fs0:
 efi\boot\bootx64.efi
 goto END
endif

if exist fs1:\efi\boot\bootx64.efi then
 fs1:
 echo Found bootloader on fs1:
 efi\boot\bootx64.efi
 goto END
endif

if exist fs2:\efi\boot\bootx64.efi then
 fs2:
 echo Found bootloader on fs2:
 efi\boot\bootx64.efi
 goto END
endif

if exist fs3:\efi\boot\bootx64.efi then
 fs3:
 echo Found bootloader on fs3:
 efi\boot\bootx64.efi
 goto END
endif

if exist fs4:\efi\boot\bootx64.efi then
 fs4:
 echo Found bootloader on fs4:
 efi\boot\bootx64.efi
 goto END
endif

if exist fs5:\efi\boot\bootx64.efi then
 fs5:
 echo Found bootloader on fs5:
 efi\boot\bootx64.efi
 goto END
endif

if exist fs6:\efi\boot\bootx64.efi then
 fs6:
 echo Found bootloader on fs6:
 efi\boot\bootx64.efi
 goto END
endif

if exist fs7:\efi\boot\bootx64.efi then
 fs7:
 echo Found bootloader on fs7:
 efi\boot\bootx64.efi
 goto END
endif

 echo "Unable to find bootloader".
 
:END
