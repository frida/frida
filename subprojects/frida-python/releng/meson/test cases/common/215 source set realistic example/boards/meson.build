specific.add(when: 'TARGET_ARM',         if_true: files('arm/arm.cc', 'arm/arm32.cc'))
specific.add(when: 'TARGET_AARCH64',     if_true: files('arm/arm.cc', 'arm/aarch64.cc'))
specific.add(when: 'CONFIG_VIRT',        if_true: files('arm/virt.cc'))
specific.add(when: 'CONFIG_XLNX_ZCU102', if_true: files('arm/xlnx_zcu102.cc'))
specific.add(when: 'CONFIG_VERSATILEPB', if_true: files('arm/versatilepb.cc'))

specific.add(when: 'TARGET_X86',         if_true: files('x86/pc.cc'))
