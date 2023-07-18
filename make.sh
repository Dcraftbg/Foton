#!/bin/bash
#=================================================================================
# Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
#=================================================================================

# external resources initialization
git submodule update --init
(cd limine && make > /dev/null 2>&1)

# for clear view
clear

# remove all obsolete files, which could interference
rm -rf build system && mkdir -p build/iso system
rm -f bx_enh_dbg.ini	# just to make clean directory, if you executed bochs.sh

# we use clang, as no cross-compiler needed, include std.h header as default for all
C="clang -include std.h"
LD="ld.lld"

# default optimization -O2
#if

# default configuration of clang for kernel making
CFLAGS="-O2 -march=x86-64 -mtune=generic -m64 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-3dnow"
LDFLAGS="-nostdlib -static -no-dynamic-linker"

# build kernel file
${C} -c kernel/init.c -o build/kernel.o ${CFLAGS} || exit 1;
${LD} build/kernel.o -o build/kernel -T tools/linker.kernel ${LDFLAGS} || exit 1;

# copy kernel file and limine files onto destined iso folder
gzip build/kernel
cp build/kernel.gz tools/limine.cfg limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin build/iso

#===============================================================================
lib=""	# include list of libraries

for library in color font terminal; do
	# build
	${C} -c -fpic library/${library}.c -o build/${library}.o ${CFLAGS} || exit 1

	# convert to shared
	${C} -shared build/${library}.o -o system/lib${library}.so ${CFLAGS} -Wl,--as-needed,-T./tools/linker.library -Lsystem/ ${lib} || exit 1

	# update libraries list
	lib="${lib} -l${library}"
done
#===============================================================================

# prepare virtual file system with content of all available software, libraries, files
${C} tools/vfs.c -o build/vfs
./build/vfs system && gzip -fk build/system.vfs && mv build/system.vfs.gz build/iso/system.gz

# convert iso directory to iso file
xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label build/iso -o build/foton.iso > /dev/null 2>&1

# install bootloader limine inside created
limine/limine bios-install build/foton.iso > /dev/null 2>&1
