# Copyright (C) 2026 Canonical Ltd
# SPDX-License-Identifier: GPL-3.0-only

project('isa-check', 'c',
        version: '0.1',
        license: 'GPL-3.0-only',
        meson_version: '>=0.63.0',
        default_options: ['c_std=gnu11', 'warning_level=2', 'b_pie=true'])

variant = get_option('variant')
varianttable = get_option('varianttable')
x86_64 = host_machine.cpu_family() == 'x86_64'
awk = find_program('awk')

required_flags = ''
if variant != ''
    result = run_command(awk, '-v', 'variant=' + variant,
                         '$1 == variant { print $3 }', varianttable, check: false)
    if result.returncode() != 0 or result.stdout().strip() == ''
        error('Variant ' + variant + ' is not present in ' + varianttable + ': ' + result.stderr().strip())
    endif
    required_flags = result.stdout().strip()
    if not x86_64
        error('Variant builds are not supported on ' + host_machine.cpu_family())
    endif
endif

noop_link_args = ['-static', '-no-pie']
variant_c_args = [
    '-Os', '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
    '-fno-pic', '-fno-pie', '-fno-lto', '-fno-asynchronous-unwind-tables',
    '-fno-unwind-tables', '-fcf-protection=none', '-march=x86-64',
    '-mgeneral-regs-only',
]
variant_link_args = ['-nostdlib', '-static', '-no-pie', '-Wl,-z,noexecstack']
variant_sources = [
    'src/preflight.c',
    'src/cpuinfo.c',
    'src/x86_64.c',
    'src/start-x86_64.S',
]
check_sources = ['src/check.c', 'src/cpuinfo.c', 'src/libc.c']

if variant == ''
    preflight = executable('isa-preflight', 'src/noop.c',
                           pie: false,
                           link_args: noop_link_args,
                           install: true,
                           install_dir: 'lib/isa-preflight')
else
    variant_defines = [
        '-DPREFLIGHT_VARIANT="' + variant + '"',
        '-DPREFLIGHT_FLAGS="' + required_flags + '"',
    ]
    preflight = executable('isa-preflight', variant_sources,
                           pie: false,
                           c_args: variant_c_args + variant_defines,
                           link_args: variant_link_args,
                           install: true,
                           install_dir: 'lib/isa-preflight')
endif

install_man('man/isa-preflight.8')

check_static = find_program('tests/check-static.sh')
run_tests = find_program('tests/run.sh')

base_test_root = meson.current_build_dir() / 'tests/base'
base_test = executable('isa-preflight-base-test', 'src/noop.c',
                       pie: false,
                       c_args: ['-DCONFIG_DIR="@0@"'.format(base_test_root / 'config'),
                                '-DPREFLIGHT_TEST_NO_REBOOT'],
                       link_args: noop_link_args)

test('static-production', check_static,
     args: [preflight.full_path()], depends: [preflight])
test('static-base', check_static,
     args: [base_test.full_path()], depends: [base_test])
test('run-base', run_tests,
     args: [base_test.full_path(), base_test_root, '', 'yes'],
     depends: [base_test], timeout: 30)

if x86_64
    test_variant = 'amd64v3'
    result = run_command(awk, '-v', 'variant=' + test_variant,
                         '$1 == variant { print $3 }', varianttable, check: false)
    if result.returncode() != 0 or result.stdout().strip() == ''
        error('Test variant ' + test_variant + ' is not present in ' + varianttable + ': ' + result.stderr().strip())
    endif
    test_flags = result.stdout().strip()
    variant_test_root = meson.current_build_dir() / 'tests/variant'
    variant_test_defines = [
        '-DPREFLIGHT_TEST_NO_REBOOT',
        '-DCONFIG_DIR="@0@"'.format(variant_test_root / 'config'),
        '-DCPUINFO_PATH="@0@"'.format(variant_test_root / 'cpuinfo'),
        '-DPREFLIGHT_VARIANT="' + test_variant + '"',
        '-DPREFLIGHT_FLAGS="' + test_flags + '"',
    ]
    variant_test = executable('isa-preflight-variant-test', variant_sources,
                              pie: false,
                              c_args: variant_c_args + variant_test_defines,
                              link_args: variant_link_args)
    test('static-variant', check_static,
         args: [variant_test.full_path()], depends: [variant_test])
    test('run-variant', run_tests,
         args: [variant_test.full_path(), variant_test_root, test_flags, 'no'],
         depends: [variant_test], timeout: 30)

    executable('isa-check', check_sources,
               install: true, install_dir: 'bin')
    install_man('man/isa-check.8')

    check_test_root = meson.current_build_dir() / 'tests/check'
    check_test = executable('isa-check-test', check_sources,
                            c_args: ['-DCPUINFO_PATH="@0@"'.format(check_test_root / 'cpuinfo')])
    run_check = find_program('tests/run-check.sh')
    test('run-check', run_check,
         args: [check_test.full_path(), check_test_root, meson.current_source_dir() / 'tests/varianttable'],
         depends: [check_test], timeout: 30)
endif

summary({
    'Variant': variant == '' ? 'base (no-op)' : variant,
    'Variant table': varianttable,
    'Host architecture': host_machine.cpu_family(),
}, section: 'Configuration')
