#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

export PATH=/usr/local/bin/:$PATH

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} mrproper

    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} defconfig

    make -j4 ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} all

    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} modules

    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/sbin usr/lib
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
cd ${OUTDIR}/rootfs
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
TOOLCHAIN_DIR=/usr/local/bin/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu
cp ${TOOLCHAIN_DIR}/aarch64-none-linux-gnu/libc/lib/ld-linux-aarch64.so.1 lib
cp ${TOOLCHAIN_DIR}/aarch64-none-linux-gnu/libc/lib64/libm.so.6 lib64
cp ${TOOLCHAIN_DIR}/aarch64-none-linux-gnu/libc/lib64/libresolv.so.2 lib64
cp ${TOOLCHAIN_DIR}/aarch64-none-linux-gnu/libc/lib64/libc.so.6 lib64

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 1 5

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}
cd ${OUTDIR}/rootfs

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ${FINDER_APP_DIR}/finder.sh home
cp ${FINDER_APP_DIR}/finder-test.sh home
cp ${FINDER_APP_DIR}/writer home
cp ${FINDER_APP_DIR}/writer.sh home
mkdir home/conf
cp ${FINDER_APP_DIR}/conf/assignment.txt home/conf 
cp ${FINDER_APP_DIR}/conf/username.txt home/conf 
cp ${FINDER_APP_DIR}/autorun-qemu.sh home

# TODO: Chown the root directory
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}
gzip -f initramfs.cpio
