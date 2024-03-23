%pragma once

%define SYSCALL_VECTOR 0x80

%define SYS_PROCESS_EXIT 0
%define SYS_SPAWN 1
%define SYS_SLEEP 2
%define SYS_ALLOCATE 3
%define SYS_KERNEL_ERRNO 4
%define SYS_TEST 5

%define SYSCALL_AMOUNT 6
