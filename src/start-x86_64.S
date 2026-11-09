/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <asm/unistd.h>

        .text
        .global _start
        .type _start, @function
_start:
        xorl %ebp, %ebp
        movq %rsp, %rdi
        andq $-16, %rsp
        call preflight_main

        /* preflight_main must not return. Avoid a PID 1 exit if it ever does. */
1:
        movl $__NR_pause, %eax
        syscall
        jmp 1b
        .size _start, .-_start

        .section .note.GNU-stack,"",@progbits
