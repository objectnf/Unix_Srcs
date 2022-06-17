#	Copyright (c) 1990 UNIX System Laboratories, Inc.
#	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.


#	Portions Copyright(c) 1988, Sun Microsystems, Inc.
#	All Rights Reserved.

#ident	"@(#)compress:compress.mk	1.6.3.1"

#	Makefile for compress

ROOT =

INSDIR = $(ROOT)/usr/bin

INC = $(ROOT)/usr/include

LDFLAGS = -s $(SHLIBS)

CFLAGS = -O -I$(INC) 

INS = install

STRIP = strip

SIZE = size

#top#
# Generated by makefile 1.47

MAKEFILE = compress.mk

MAINS = compress uncompress zcat

OBJECTS =  compress.o

SOURCES =  compress.c

ALL:		$(MAINS)

compress: compress.o 
	$(CC) $(CFLAGS) -o compress compress.o $(LDFLAGS)

uncompress:	compress
		rm -f uncompress
		ln compress uncompress

zcat:		compress
		rm -f zcat
		ln compress zcat

compress.o:	 $(INC)/signal.h $(INC)/stdio.h \
		 $(INC)/ctype.h $(INC)/sys/types.h \
		 $(INC)/sys/stat.h $(INC)/sys/ioctl.h \
		 $(INC)/unistd.h

GLOBALINCS = $(INC)/signal.h $(INC)/stdio.h $(INC)/ctype.h \
	$(INC)/types.h $(INC)/sys/stat.h $(INC)/sys/ioctl.h \
		 $(INC)/unistd.h

clean:
	rm -f $(OBJECTS)

clobber:
	rm -f $(OBJECTS) $(MAINS)

newmakefile:
	makefile -m -f $(MAKEFILE)  -s INC $(INC)
#bottom#

all : ALL

install: all
	$(INS) -f $(INSDIR) -m 0555 -u bin -g bin compress
	rm -f $(INSDIR)/uncompress $(INSDIR)/zcat
	ln $(INSDIR)/compress $(INSDIR)/uncompress
	ln $(INSDIR)/compress $(INSDIR)/zcat

size: ALL
	$(SIZE) $(MAINS)

strip: ALL
	$(STRIP) $(MAINS)

#	These targets are useful but optional

partslist:
	@echo $(MAKEFILE) $(SOURCES) $(LOCALINCS)  |  tr ' ' '\012'  |  sort

productdir:
	@echo $(INSDIR) | tr ' ' '\012' | sort

product:
	@echo $(MAINS)  |  tr ' ' '\012'  | \
	sed 's;^;$(INSDIR)/;'

srcaudit:
	@fileaudit $(MAKEFILE) $(LOCALINCS) $(SOURCES) -o $(OBJECTS) $(MAINS)