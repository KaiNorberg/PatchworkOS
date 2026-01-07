.code64
.intel_syntax noprefix

.global sched_idle_loop
.type sched_idle_loop, @function

.text

sched_idle_loop:
    sti
    hlt
    jmp sched_idle_loop
