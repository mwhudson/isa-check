/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef ISA_PREFLIGHT_H
#define ISA_PREFLIGHT_H

/* Shared by both isa-preflight builds: the dracut module moves the original
 * /init here before installing isa-preflight in its place. */
#ifndef NEXT_INIT_PATH
#define NEXT_INIT_PATH "/next-init"
#endif

#ifndef PROC_DIR
#define PROC_DIR "/proc"
#endif

/* Init operations supplied by the freestanding x86-64 backend. Each returns a
 * negative errno value on failure. */
long preflight_sys_write(int fd, const void *buffer, unsigned long count);
long preflight_sys_mkdir(const char *path, unsigned int mode);
long preflight_sys_mount_proc(const char *target);
long preflight_sys_umount(const char *target);
long preflight_sys_execve(const char *path, char *const argv[], char *const envp[]);
long preflight_sys_getpid(void);
long preflight_sys_reboot(unsigned long command);
long preflight_sys_pause(void);
__attribute__((noreturn)) void preflight_sys_exit(int status);

#endif
