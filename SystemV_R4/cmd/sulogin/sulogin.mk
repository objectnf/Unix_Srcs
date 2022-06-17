#	Copyright (c) 1990 UNIX System Laboratories, Inc.
#	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.


#ident	"@(#)sulogin:sulogin.mk	1.3.5.1"
#	Copyright (c) 1987, 1988 Microsoft Corporation
#	  All Rights Reserved

#	This Module contains Proprietary Information of Microsoft
#	Corporation and should be treated as Confidential.


ROOT =

#	Where MAINS are to be installed.
INSDIR = $(ROOT)/etc

INS = "install"

INC = $(ROOT)/usr/include
INCSYS = $(ROOT)/usr/include

CONS = -DSECURITY
CFLAGS = $(CONS) -O -I$(INC) -I$(INCSYS)

#	Common Libraries not found in /lib or /usr/lib.
COMLIB = 

#	Common Libraries and -l<lib> flags.
LDFLAGS = -s $(COMLIB) -lcmd -lcrypt_i

STRIP = strip

#top#
# Generated by makefile 1.43    Mon Jan 14 09:39:34 EST 1985

MAKEFILE = sulogin.mk


MAINS = sulogin

OBJECTS =  sulogin.o

SOURCES =  sulogin.c

ALL:		$(MAINS)

sulogin:		sulogin.o	
	$(CC) $(CFLAGS)  -o sulogin  sulogin.o   $(LDFLAGS) $(ROOTLIBS)


sulogin.o:	 $(INCSYS)/sys/types.h $(INC)/utmp.h	\
		 $(INC)/signal.h	$(INCSYS)/sys/signal.h \
		 $(INC)/pwd.h $(INC)/stdio.h \
		 $(INCSYS)/sys/stat.h $(INC)/dirent.h $(INCSYS)/sys/dirent.h \
		 $(INCSYS)/sys/utsname.h $(INCSYS)/sys/param.h \
		 $(INC)/errno.h $(INCSYS)/sys/errno.h \
		 $(INC)/fcntl.h 

GLOBALINCS = $(INC)/errno.h $(INC)/fcntl.h $(INC)/pwd.h \
	$(INC)/signal.h $(INC)/stdio.h $(INC)/dirent.h $(INCSYS)/sys/dirent.h \
	$(INCSYS)/sys/errno.h $(INCSYS)/sys/param.h \
	$(INCSYS)/sys/signal.h $(INCSYS)/sys/stat.h \
	$(INCSYS)/sys/types.h $(INCSYS)/sys/utsname.h \
	$(INC)/utmp.h 


clean:
	rm -f $(OBJECTS)
	
clobber:	
	rm -f $(OBJECTS) $(MAINS)

newmakefile:
	makefile -m -f $(MAKEFILE) -s INC $(INC)
#bottom#

save:
	cd $(INSDIR); set -x; for m in $(MAINS); do  cp $$m OLD$$m; done

restore:
	cd $(INSDIR); set -x; for m in $(MAINS); do; cp OLD$$m $$m; done

install:	$(MAINS) $(INSDIR)
	$(INS) -f $(INSDIR) -m 04555 -u root -g bin $(MAINS)

strip:
	$(STRIP) $(MAINS)

remove:
	cd $(INSDIR);  rm -f $(MAINS)

$(INSDIR):
	mkdir $(INSDIR);  chmod 755 $(INSDIR);  chown bin $(INSDIR)

partslist:
	@echo $(MAKEFILE) $(LOCALINCS) $(SOURCES)  |  tr ' ' '\012'  |  sort

product:
	@echo $(MAINS)  |  tr ' ' '\012'  | \
		sed -e 's;^;$(INSDIR)/;' -e 's;//*;/;g'

productdir:
	@echo $(INSDIR)

srcaudit:	# will not report missing nor present object or product files.
	@fileaudit $(MAKEFILE) $(LOCALINCS) $(SOURCES) -o $(OBJECTS) $(MAINS)
