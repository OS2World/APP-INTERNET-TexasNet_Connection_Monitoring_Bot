# Makefile for MrsBot
#   Written by: Hendrix <jimi@texas.net>
#

# Pick your operating system below... uncomment it, comment the others
#OS = SUNOS             # Tested
#OS = SOLARIS          # Tested
#OS = ULTRIX           # Tested
#OS = OSF1             # Untested
#OS = AIX              # Tested (Phisher)
#OS = LINUX            # Tested
#OS = BSDI             # Tested
#OS = HPUX             # Tested
#OS = IRIX             # Untested
#OS = FREEBSD          # Untested
#OS = NEXT	       # Tested (Twazard)
OS = OS/2

# If you like gcc better than cc, change the line below.
CC = gcc

# Any other defines you need, add below.
DEFINES =

# -O for optimization.  Can't debug.  Use -g for debuggable code.
CFLAGS = -O2 -Zsysv-signals

# Any libraries that need to be linked in (-lresolv for example)
# should go here.
LIBS = -lsocket -lcurses -lbtermcap

###### CHANGES BELOW HERE ONLY IF YOU KNOW WHAT YOU'RE DOING ######


OBJECTS = serverif.o stdcmds.o wild.o bothunt.o userlist.o
SOURCES = serverif.c stdcmds.c wild.c bothunt.c userlist.c


all: tcm
clean: 
	rm -f *.o tcm core tcm.core

.c.o:
	${CC} ${CFLAGS} ${DEFINES} -c $<

hpux_link:
	${CC} ${CFLAGS} -o tcm ${OBJECTS} ${LIBS} -lM

solaris_link:
	${CC} ${CFLAGS} -o tcm ${OBJECTS} ${LIBS} -lnsl -lsocket


next_link:
	${CC} ${CFLAGS} -o tcm ${OBJECTS} ${LIBS}

os2_link:
	${CC} ${CFLAGS} -o tcm.exe ${OBJECTS} ${LIBS}
	emxbind -s tcm.exe

link:
	${CC} ${CFLAGS} -o tcm ${OBJECTS} ${LIBS}

tcm: HEN_${OS}

# Yes, this REALLY sucks.  But gmake does NOT recognize either the
# wildcard char (%) or the := to add additional libraries to LIBS
# so we're left the the lowest common denominator: "solaris_link",
# "hpux_link", and this whole group of HEN_* targets.
HEN_SUNOS: ${OBJECTS} link
HEN_SOLARIS: ${OBJECTS} solaris_link
HEN_ULTRIX: ${OBJECTS} link
HEN_OSF1: ${OBJECTS} link
HEN_AIX: ${OBJECTS} link
HEN_LINUX: ${OBJECTS} link
HEN_BSDI: ${OBJECTS} link
HEN_HPUX: ${OBJECTS} hpux_link
HEN_IRIX: ${OBJECTS} link
HEN_FREEBSD: ${OBJECTS} link
HEN_NEXT: ${OBJECTS} strdup.o next_link
HEN_OS/2: ${OBJECTS} os2_link

serverif.o: serverif.c config.h
stdcmds.o: stdcmds.c config.h
wild.o: wild.c
userlist.o: userlist.c
bothunt.o: bothunt.c config.h
#
# only necessary for NEXT
#
strdup.o: strdup.c