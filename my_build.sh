#!/bin/bash

# 1 prereq
. ./config.sh

git clone --depth=1 -b 2026.02.x git://git.busybox.net/buildroot
git clone --depth=1 https://github.com/hugsy/gef.git
git clone --depth=1 https://github.com/dwks/pagemap.git demo_races/pagemap
git clone --depth=1 -b v1.8 https://github.com/riscv-software-src/opensbi.git
cd opensbi; rm -rf .git; patch -p1 < ../opensbi.diff; cd ..
git clone --depth=1 -b v6.19 https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux.git
curl https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.19.14.tar.xz -O linux.tar.xz
tar -xf linux.tar.xz
mv linux-6.19.14 linux
cd linux; rm -rf .git; patch -p1 < ../linux.diff; cd ..

sed -i 's/CC = gcc/CC ?= gcc/' demo_races/pagemap/Makefile

mkdir -p output
mkdir -p $ROOTFS_OVERLAY

# 2 toolchain
cp buildroot_config buildroot/.config
cd buildroot
make sdk
cd ../output
rm -rf toolchain
tar xf ../buildroot/output/images/riscv64-buildroot-linux-gnu_sdk-buildroot.tar.gz
mv riscv64-buildroot-linux-gnu_sdk-buildroot toolchain
cd ..

# 3 linux prepare
cp linux_config linux/.config
cd linux
make ARCH=riscv CROSS_COMPILE=$CROSS_COMPILE modules_prepare
make ARCH=riscv CROSS_COMPILE=$CROSS_COMPILE -j$JOBS -s
cp arch/riscv/boot/Image $KERNEL_OUT
cd ..

# 4 opensbi
cd opensbi
git clean -fdx
make CROSS_COMPILE=$CROSS_COMPILE PLATFORM=generic FW_OPTIONS=0x2 BUILD_INFO=y V=1 -j$JOBS -s
cp build/platform/generic/firmware/fw_dynamic.bin $SBI_OUT
cp build/platform/generic/firmware/fw_dynamic.elf $SBI_ELF
cd ..

# 5 demos
mkdir -p ${ROOTFS_OVERLAY}/opt
${CROSS_COMPILE}gcc -O2 -g -fno-stack-protector demo_races/my_ecall.c -o $ROOTFS_OVERLAY/opt/my_ecall

# 6 rootfs
cp buildroot_config buildroot/.config
cd buildroot
make -s
cp ./output/images/rootfs.cpio $ROOTS_OUT
cd ..

# 7 run
qemu-system-riscv64 \
    -smp 4 \
    -machine virt \
    -cpu rv64,v=true,vlen=128,sscofpmf=true \
    -m 256m \
    -nographic \
    -bios $SBI_OUT \
    -kernel $KERNEL_OUT \
    -initrd $ROOTS_OUT \
    -append "console=ttyS0 earlycon root=/dev/ram0 rw init=/init" \
    -device virtio-net-device,netdev=net \
    -netdev user,id=net,hostfwd=tcp::2345-:2345,hostfwd=tcp::10022-:22 \
    -gdb tcp::1234 \
    -monitor telnet:127.0.0.1:1122,server,nowait \
    -serial mon:stdio \
    $1
