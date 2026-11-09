/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* Raw Linux x86-64 syscall backend for the freestanding early-boot checker.
 * No libc or runtime library is needed before the CPU check completes. */
#include <asm/unistd.h>
#include <linux/fcntl.h>
#include <linux/mount.h>
#include <linux/reboot.h>

#include "cpuinfo.h"
#include "preflight.h"

static long syscall0(unsigned long number) {
        long result;

        __asm__ volatile("syscall" : "=a"(result) : "a"(number) : "rcx", "r11", "memory");
        return result;
}

static long syscall1(unsigned long number, unsigned long arg1) {
        long result;

        __asm__ volatile("syscall" : "=a"(result) : "a"(number), "D"(arg1) : "rcx", "r11", "memory");
        return result;
}

static long syscall2(unsigned long number, unsigned long arg1, unsigned long arg2) {
        long result;

        __asm__ volatile("syscall" : "=a"(result) : "a"(number), "D"(arg1), "S"(arg2) : "rcx", "r11", "memory");
        return result;
}

static long syscall3(unsigned long number, unsigned long arg1, unsigned long arg2, unsigned long arg3) {
        long result;

        __asm__ volatile("syscall"
                         : "=a"(result)
                         : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
                         : "rcx", "r11", "memory");
        return result;
}

static long syscall4(unsigned long number, unsigned long arg1, unsigned long arg2, unsigned long arg3,
                     unsigned long arg4) {
        register unsigned long r10 __asm__("r10") = arg4;
        long result;

        __asm__ volatile("syscall"
                         : "=a"(result)
                         : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
                         : "rcx", "r11", "memory");
        return result;
}

static long syscall5(unsigned long number, unsigned long arg1, unsigned long arg2, unsigned long arg3,
                     unsigned long arg4, unsigned long arg5) {
        register unsigned long r10 __asm__("r10") = arg4;
        register unsigned long r8 __asm__("r8") = arg5;
        long result;

        __asm__ volatile("syscall"
                         : "=a"(result)
                         : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8)
                         : "rcx", "r11", "memory");
        return result;
}

long isa_sys_read(int fd, void *buffer, unsigned long count) {
        return syscall3(__NR_read, (unsigned long) fd, (unsigned long) buffer, count);
}

long preflight_sys_write(int fd, const void *buffer, unsigned long count) {
        return syscall3(__NR_write, (unsigned long) fd, (unsigned long) buffer, count);
}

long isa_sys_open_readonly(const char *path) {
        return syscall4(__NR_openat, (unsigned long) AT_FDCWD, (unsigned long) path, O_RDONLY, 0);
}

long isa_sys_close(int fd) {
        return syscall1(__NR_close, (unsigned long) fd);
}

long preflight_sys_mkdir(const char *path, unsigned int mode) {
        return syscall2(__NR_mkdir, (unsigned long) path, mode);
}

long preflight_sys_mount_proc(const char *target) {
        return syscall5(__NR_mount, (unsigned long) "proc", (unsigned long) target, (unsigned long) "proc",
                        MS_NOSUID | MS_NODEV | MS_NOEXEC, 0);
}

long preflight_sys_umount(const char *target) {
        return syscall2(__NR_umount2, (unsigned long) target, 0);
}

long preflight_sys_execve(const char *path, char *const argv[], char *const envp[]) {
        return syscall3(__NR_execve, (unsigned long) path, (unsigned long) argv, (unsigned long) envp);
}

long preflight_sys_getpid(void) {
        return syscall0(__NR_getpid);
}

long preflight_sys_reboot(unsigned long command) {
        return syscall4(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, command, 0);
}

long preflight_sys_pause(void) {
        return syscall0(__NR_pause);
}

__attribute__((noreturn)) void preflight_sys_exit(int status) {
        for (;;)
                syscall1(__NR_exit_group, (unsigned long) status);
}
