local disableSimdFlags = {
    "-mno-mmx", "-mno-3dnow", "-mno-80387", "-mno-sse", "-mno-sse2", "-mno-sse3", "-mno-ssse3", "-mno-sse4"
}

local programs = {}
local function create_program_target(program)
    target(program)
        set_kind("binary")
        add_deps("stdlib")
        add_files("src/programs/" .. program .. "/**.c")
        add_includedirs("include/stdlib")
    table.insert(programs, program)
end

add_rules("mode.debug", "mode.release")
set_license("GPL-3.0")

set_languages("gnu11")
set_optimize("fastest")
set_toolchains("nasm")
add_cxflags("-Wall", "-Wextra", "-Werror", "-Wno-deprecated-pragma", "-Wno-unused-function", "-Wno-unused-variable",
    "-Wno-ignored-qualifiers", "-Wno-unused-parameter", "-Wno-unused-but-set-variable", "-Wno-implicit-fallthrough",
    "-Wno-deprecated-non-prototype", "-fno-stack-protector", "-ffreestanding", "-nostdlib")
add_ldflags("-nostdlib")

target("gnu-efi")
    set_kind("phony")
    on_build(function(target)
        os.vrunv("make -C lib/gnu-efi", {})
    end)
    on_clean(function(target)
        os.vrunv("make -C lib/gnu-efi clean", {})
    end)

target("bootloader")
    add_cxflags(disableSimdFlags, "-fPIC", "-fno-stack-check", "-fshort-wchar", "-mno-red-zone")
    set_kind("shared")
    add_deps("gnu-efi")
    add_files("src/bootloader/**.c")
    add_files("src/stdlib/string.c")
    add_defines("__EMBED__", "__BOOTLOADER__", "GNU_EFI_USE_MS_ABI")
    add_includedirs("include", "include/stdlib", "src/bootloader", "lib/gnu-efi/inc")
    on_link(function(target)
        os.mkdir(path.directory(target:targetfile()))
        os.exec(
            "ld -shared -Bsymbolic -Llib/gnu-efi/x86_64/lib -Llib/gnu-efi/x86_64/gnuefi -Tlib/gnu-efi/gnuefi/elf_x86_64_efi.lds lib/gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o %s -o %s -lgnuefi -lefi",
            table.concat(target:objectfiles(), " "), target:targetfile())
    end)

target("bootx64")
    set_kind("binary")
    add_deps("bootloader")
    set_filename("bootx64.efi")
    on_link(function(target)
        import("core.project.project")
        os.exec(
            "objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 %s %s",
            project.target("bootloader"):targetfile(), target:targetfile())
    end)

target("kernel")
    add_cxflags(disableSimdFlags, "-fno-pic", "-mcmodel=large", "-fno-stack-check", "-mno-red-zone", "-fno-stack-protector", "-fomit-frame-pointer")
    set_kind("binary")
    add_files("src/kernel/**.c")
    add_files("src/kernel/**.s")
    add_files("src/stdlib/**.c")
    add_files("src/stdlib/**.s")
    add_files("src/kernel/linker.ld")
    add_defines("__EMBED__")
    add_includedirs("include", "include/stdlib", "src/kernel")

target("stdlib")
    set_kind("static")
    add_files("src/stdlib/**.c")
    add_files("src/stdlib/**.s")
    add_includedirs("include", "include/stdlib", "src/stdlib")

create_program_target("cursor")
create_program_target("shell")
create_program_target("taskbar")
create_program_target("calculator")

target("deploy")
    set_kind("phony")
    add_deps("bootloader", "kernel", "stdlib", programs)
    on_build(function()
        import("core.project.project")

        os.exec("mkdir -p bin")
        os.exec("dd if=/dev/zero of=bin/PatchworkOS.img bs=1M count=64")
        os.exec("mkfs.vfat -F 32 -n \"PATCHWORKOS\" bin/PatchworkOS.img")
        os.exec("mmd -i bin/PatchworkOS.img ::/efi")
        os.exec("mmd -i bin/PatchworkOS.img ::/efi/boot")
        os.exec("mmd -i bin/PatchworkOS.img ::/boot")
        os.exec("mmd -i bin/PatchworkOS.img ::/bin")
        os.exec("mcopy -i bin/PatchworkOS.img -s root/usr ::/")
        os.exec("mcopy -i bin/PatchworkOS.img -s root/startup.nsh ::/")
        os.exec("mmd -i bin/PatchworkOS.img ::/usr/licence")
        os.exec("mcopy -i bin/PatchworkOS.img -s COPYING ::/usr/licence")
        os.exec("mcopy -i bin/PatchworkOS.img -s LICENSE ::/usr/licence")
        os.exec(string.format("mcopy -i bin/PatchworkOS.img -s %s ::/efi/boot", project.target("bootx64"):targetfile()))
        os.exec(string.format("mcopy -i bin/PatchworkOS.img -s %s ::/boot", project.target("kernel"):targetfile()))
        for _,program in pairs(programs) do
            os.exec(string.format("mcopy -i bin/PatchworkOS.img -s %s ::/bin", project.target(program):targetfile()))
        end

        os.exec("rm -rf bin/temp")
    end)
    on_clean(function()
        os.exec("rm -rf bin")
    end)

task("format")
    set_menu {
        usage = "xmake format",
        description = "Format all files using clang-format.",
        options = {}
    }
    on_run(function()
        local files = os.iorun("find src -name '*.c' -or -name '*.h'")
        for file in string.gmatch(files, "[^\n]+") do
            os.exec("clang-format -style=file -i %s", file)
        end
    end)

task("run")
    set_menu {
        usage = "xmake run",
        description = "Run PatchworkOS in qemu.",
        options = {}
    }
    on_run(function()
        os.exec([[qemu-system-x86_64
        -M q35
        -display sdl
        -drive file=bin/PatchworkOS.img
        -m 1G
        -smp 6
        -serial stdio
        -no-shutdown -no-reboot
        -drive if=pflash,format=raw,unit=0,file=lib/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on
        -drive if=pflash,format=raw,unit=1,file=lib/OVMFbin/OVMF_VARS-pure-efi.fd
        -net none]])
    end)

task("run_debug")
    set_menu {
        usage = "xmake run_debug",
        description = "Run PatchworkOS in qemu with debug info.",
        options = {}
    }
    on_run(function()
        os.exec([[qemu-system-x86_64
        -M q35
        -display sdl
        -drive file=bin/PatchworkOS.img
        -m 1G
        -smp 6
        -serial stdio
        -d int
        -no-shutdown -no-reboot
        -drive if=pflash,format=raw,unit=0,file=lib/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on
        -drive if=pflash,format=raw,unit=1,file=lib/OVMFbin/OVMF_VARS-pure-efi.fd
        -net none]])
    end)
