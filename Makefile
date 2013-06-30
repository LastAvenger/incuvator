CFLAGS=-D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -g -O2 -Wall
LDLIBS=-lnetfs -lfshelp -liohelp -lports -lshouldbeinlibc
all: socketio
