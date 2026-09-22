project('frida-swift', 'swift', 'c',
  version: run_command(find_program('python3'), files('tools' / 'detect-version.py'),
    capture: true,
    check: true).stdout().strip(),
  meson_version: '>=1.1.0',
)

llvm_archs = {
  'aarch64': 'arm64',
}
llvm_vendors = {
  'darwin': 'apple',
  'windows': 'pc',
}

host_os_family = host_machine.system()
host_arch = host_machine.cpu_family()

host_llvm_arch = llvm_archs.get(host_arch, host_arch)
host_llvm_vendor = llvm_vendors.get(host_os_family, 'unknown')
host_triplet = '-'.join([host_llvm_arch, host_llvm_vendor, host_machine.subsystem()])

cc = meson.get_compiler('c')

frida_core_dep = dependency('frida-core-1.0', default_options: [
  'frida_version=' + meson.project_version(),
])

python = import('python').find_installation()

subdir('Frida')
