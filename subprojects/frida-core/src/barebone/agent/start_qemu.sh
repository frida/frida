#!/bin/bash

QEMUAppleSilicon/build/qemu-system-aarch64 \
    -M t8030,memory-backend=ram,trustcache=iPhone11_8_iPhone12_1_14.0_18A5351d_Restore/Firmware/038-44135-124.dmg.trustcache,ticket=root_ticket.der,sep-fw=sep-firmware.n104.RELEASE.new.img4,sep-rom=AppleSEPROM-Cebu-B1,kaslr-off=true,usb-conn-type=unix,usb-conn-addr=/Users/oleavr/src/frida-barebone-ios/usb.socket \
    -object memory-backend-file,id=ram,size=4G,mem-path="/Volumes/RAM Disk/ios-dram",share=on \
    -kernel iPhone11_8_iPhone12_1_14.0_18A5351d_Restore/kernelcache.research.iphone12b \
    -dtb iPhone11_8_iPhone12_1_14.0_18A5351d_Restore/Firmware/all_flash/DeviceTree.n104ap.im4p \
    -append "tlto_us=-1 mtxspin=-1 agm-genuine=1 agm-authentic=1 agm-trusted=1 serial=3 launchd_unsecure_cache=1 wdt=-1" \
    -smp 7 \
    -drive file=sep_nvram,if=pflash,format=qcow2 \
    -drive file=sep_ssc,if=pflash,format=qcow2 \
    -drive file=nvme.1,format=qcow2,if=none,id=drive.1 -device nvme-ns,drive=drive.1,bus=nvme-bus.0,nsid=1,nstype=1,logical_block_size=4096,physical_block_size=4096 \
    -drive file=nvme.2,format=qcow2,if=none,id=drive.2 -device nvme-ns,drive=drive.2,bus=nvme-bus.0,nsid=2,nstype=2,logical_block_size=4096,physical_block_size=4096 \
    -drive file=nvme.3,format=qcow2,if=none,id=drive.3 -device nvme-ns,drive=drive.3,bus=nvme-bus.0,nsid=3,nstype=3,logical_block_size=4096,physical_block_size=4096 \
    -drive file=nvme.4,format=qcow2,if=none,id=drive.4 -device nvme-ns,drive=drive.4,bus=nvme-bus.0,nsid=4,nstype=4,logical_block_size=4096,physical_block_size=4096 \
    -drive file=nvram,if=none,format=qcow2,id=nvram -device apple-nvram,drive=nvram,bus=nvme-bus.0,nsid=5,nstype=5,id=nvram,logical_block_size=4096,physical_block_size=4096 \
    -drive file=nvme.6,format=qcow2,if=none,id=drive.6 -device nvme-ns,drive=drive.6,bus=nvme-bus.0,nsid=6,nstype=6,logical_block_size=4096,physical_block_size=4096 \
    -drive file=nvme.7,format=qcow2,if=none,id=drive.7 -device nvme-ns,drive=drive.7,bus=nvme-bus.0,nsid=7,nstype=8,logical_block_size=4096,physical_block_size=4096 \
    -initrd iPhone11_8_iPhone12_1_14.0_18A5351d_Restore/038-44135-124.dmg \
    -serial mon:stdio \
    -monitor tcp:127.0.0.1:9001,server,nowait \
    -gdb tcp::9000
