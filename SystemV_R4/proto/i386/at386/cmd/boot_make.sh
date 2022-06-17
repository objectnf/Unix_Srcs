#	Copyright (c) 1990 UNIX System Laboratories, Inc.
#	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

:

#	Copyright (c) 1987, 1988 Microsoft Corporation
#	  All Rights Reserved

#	This Module contains Proprietary Information of Microsoft
#	Corporation and should be treated as Confidential.

#ident	"@(#)proto:i386/at386/cmd/boot_make.sh	1.3.1.1"

DRIVE_INFO=drive_info
PROTO=${1-:proto.tape}

FDRIVE=`cut -f1 ${DRIVE_INFO}`
BLKCYLS=`cut -f2 ${DRIVE_INFO}`
BLKS=`cut -f3 ${DRIVE_INFO}`
if [ `uname -r` = "4.0" ]
then VER4=-Fs5
fi

echo "Insert BOOTUNIX floppy 1 of 2 and press <RETURN>, s to skip or F not to format: \c"; read a
if [ "$a" != "s" ]
then
if [ "$a" != "F" ]
then
format -i2 /dev/rdsk/${FDRIVE}t
fi
sync
dd if=${ROOT}/etc/fboot of=/dev/rdsk/${FDRIVE}t bs=512 conv=sync
sync

mkfs ${VER4} -b 512 /dev/dsk/${FDRIVE} ${PROTO} 2 ${BLKCYLS}
labelit ${VER4} /dev/dsk/${FDRIVE} instal flop
mount ${VER4} /dev/dsk/${FDRIVE} /mnt
ln /mnt/sbin/sh /mnt/sbin/-sh
ln /mnt/sbin/sh /mnt/sbin/su
ln /mnt/sbin/sh /mnt/etc/sulogin
ln /mnt/dev/console /mnt/dev/vt00
ln /mnt/dev/dsk/0s1 /mnt/dev/root
rm -f /mnt/dev/swap
mknod /mnt/dev/swap b 21 1
ln /mnt/dev/dsk/f0q15dt /mnt/dev/fd0
ln /mnt/dev/rdsk/0s1 /mnt/dev/rroot
ln /mnt/dev/rdsk/0s2 /mnt/dev/rswap
ln /mnt/dev/rdsk/f0q15dt /mnt/dev/rfd0
ln /mnt/etc/emulator /mnt/etc/emulator.rel1
egrep -v "MREQMSG|DEFBOOTSTR" default.at386 > /mnt/etc/default/boot
echo "MREQMSG=WARNING: Your system does not have the required minimum 4 megabyte of memory." >> /mnt/etc/default/boot
echo "MREQMSG2=You must power down the machine, add memory, and begin the installation process again." >> /mnt/etc/default/boot
echo "AVAILKMEM=1024" >> /mnt/etc/default/boot
echo "rootdev=ramd(0,0)\nswapdev=ramd(1,0)\npipedev=ramd(0,0)" >> /mnt/etc/default/boot
ln /mnt/dev/tty00 /mnt/dev/term/000
ln /mnt/dev/tty01 /mnt/dev/term/001
touch /mnt/var/adm/utmp /mnt/var/adm/utmpx
ln /mnt/var/adm/utmp /mnt/etc/utmp
ln /mnt/var/adm/utmpx /mnt/etc/utmpx

echo y > /mnt/yes
>/mnt/etc/mnttab
date > .packagedate
install -f /mnt/etc -m 644 -u root -g sys .packagedate 

# set all files on boot floppy to official issue date.
find /mnt -print | xargs touch `cat ${ROOT}/etc/.installdate`

sync
df -t /mnt
sync
umount /dev/dsk/${FDRIVE}

fi

echo "Insert BOOTUNIX floppy 2 of 2 and press <RETURN>, s to skip or F not to format: \c"; read a
if [ "$a" != "s" ]
then
if [ "$a" != "F" ]
then
format -i2 /dev/rdsk/${FDRIVE}t
fi

mkfs ${VER4} /dev/dsk/${FDRIVE} ${2} 2 ${BLKCYLS}
labelit ${VER4} /dev/dsk/${FDRIVE} instal flop
mount ${VER4} /dev/dsk/${FDRIVE} /mnt
ln /mnt/usr/bin/mv /mnt/usr/bin/cp
ln /mnt/usr/bin/mv /mnt/usr/bin/ln
touch /mnt/LABEL.4.0.tp

# set all files on boot floppy to official issue date.
find /mnt -print | xargs touch `cat ${ROOT}/etc/.installdate`

sync
df -t /mnt
sync
umount /dev/dsk/${FDRIVE}
fi
