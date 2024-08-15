#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u
# PS4='+ ' # Optional: Customize the prefix shown before each echoed command
# set -x

PATCH=`pwd`/e33a814e772cdc36436c8c188d8c42d019fda639.patch

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

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
    git apply ${PATCH}
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir rootfs
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make -j$(nproc) distclean
    make -j$(nproc) defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} 
make -j$(nproc) CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
interpreter=$(${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter" | awk -F: '{print $2}' | xargs | sed 's/.$//')
libraries=$(${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library" | awk -F'[' '{print $2}' | awk -F']' '{print $1}')

# TODO: Add library dependencies to rootfs
sysroot=$(${CROSS_COMPILE}gcc -print-sysroot)

if [ -n "$interpreter" ]; then
    cp ${sysroot}${interpreter} ${OUTDIR}/rootfs/lib/
fi

for lib in $libraries; do
    libpath=$(find ${sysroot} -name $lib)
    if [ -n "$libpath" ]; then
        cp $libpath ${OUTDIR}/rootfs/lib64/
    else
        echo "Library $lib not found!"
    fi
done

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make -j$(nproc) clean
make -j$(nproc) CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home/
sed -i 's|\.\./conf/assignment\.txt|conf/assignment.txt|g' ${OUTDIR}/rootfs/home/finder-test.sh
sed -i '/make/d' ${OUTDIR}/rootfs/home/finder-test.sh
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home/
cp -r ${FINDER_APP_DIR}/conf/ ${OUTDIR}/rootfs/home/

# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
sudo find . | sudo cpio -H newc -o | gzip > ${OUTDIR}/initramfs.cpio.gz