<!-- Copyright (C) 2026 Canonical Ltd -->
<!-- SPDX-License-Identifier: GPL-3.0-only -->

# isa-check and isa-preflight

Small tools for checking processor architecture variants:

- **isa-check** lists or probes compatible native-architecture variants using
  dpkg's `/usr/share/dpkg/varianttable` and the CPU flags reported in
  `/proc/cpuinfo`.
- **isa-preflight** is an initramfs entry point. For variant builds, this checks
  CPU requirements before chain-loading the next init program; non-variant
  builds simply chain-load unconditionally. Variant checking currently supports
  x86-64.

## Build and test

Requires a C compiler, Linux headers, libc development files, Meson 0.63 or
newer, Ninja, awk, binutils, bash and coreutils. Unit tests use the in-tree
variant table; a variant build needs dpkg's table or an explicit override.

```sh
meson setup build --prefix=/usr
meson compile -C build
meson test -C build --print-errorlogs
```

The default build produces the non-variant chainloader. To embed amd64v3
requirements, configure a separate build with `-Dvariant=amd64v3`. Use
`-Dvarianttable=PATH` to select a different variant table.

For Debian/Ubuntu packages, install the build dependencies listed in
[`debian/control`](debian/control), then run:

```sh
dpkg-buildpackage -us -uc
```

Set `DEB_HOST_ARCH_VARIANT=amd64v3` for a variant package build.

## Probe CPU support

On amd64, with proc already mounted:

```sh
./build/isa-check          # List compatible variants in table order
./build/isa-check amd64v3  # Probe one variant
```

Exit status is **0** for support, **1** for incompatibility, or **2** for an
input/read error. `--table PATH` overrides the runtime variant table.
The native architecture is obtained from `dpkg --print-architecture`. CPU
features are read from Linux's `flags` (x86) or `Features` (ARM) fields. New
variant-table rows using those feature names need no CLI rebuild. Architectures
without table entries return an empty list with status 1.

## Initramfs handoff

`isa-preflight` for variant builds is a freestanding (no libgcc, no glibc code
used either statically or dynamically) binary built using only baseline
instructions that checks the CPU features of the system match the variant it was
built for before executing `/next-init`.  For a non-variant build it is a
regular dynamic binary that just executes `/next-init`, to allow image
construction to be unconditional.

The variant requirements are read from dpkg's variant table at build time and
embedded in the binary, so variant packages must be rebuilt when the table's
requirements change.

The package installs a dracut module as `/usr/lib/dracut/modules.d/95preinit`.
Installing or removing `isa-preflight` therefore enables or disables the
integration without changing dracut itself.  Installation, upgrades, and removal
activate the `update-initramfs` trigger, which regenerates the newest kernel's
initramfs; images for other kernels, and images built separately, must be
regenerated explicitly.

The module runs after dracut's init providers, moves the original `/init` (a
regular script or a symlink such as `init -> usr/lib/systemd/systemd`) to
`/next-init`, and installs `isa-preflight` as `/init`.

With `squash-lib`, only the outer loader is wrapped; the inner init is
unchanged. Dracut installs the runtime dependencies of `isa-preflight` into the
same root as `/init`, including the outer loader's root when squash is used.

Booting with `rdinit=/next-init` bypasses the check.

Arguments and environment are preserved during handoff. The variant checker
mounts proc if needed and unmounts it again before the handoff. The successor
receives `argv[0]` unchanged (normally `/init`). Errors halt the system when
running as PID 1; otherwise the program pauses indefinitely.

## Installed-package tests

The `check` autopkgtest compares the installed CLI with an independent reading
of dpkg's table and `/proc/cpuinfo`. On amd64, the `boot` test uses dracut's own
test framework (`dracut-test`) to build initramfs images with the installed
module and boot a test root disk under QEMU TCG, so it needs neither KVM nor
matching host CPU features. It boots the installed checker with and without
`squash-lib`, and a production amd64v3 checker built from source against the
installed dpkg table, which must also halt with a diagnostic on a processor
without AVX2.

Run the tests against built packages in an Ubuntu testbed with `autopkgtest`.
The `boot` test needs root to read the packaged kernel; its dependencies include
`dracut-test` and the source build dependencies.

See [`isa-check(8)`](man/isa-check.8) and
[`isa-preflight(8)`](man/isa-preflight.8) for details.
