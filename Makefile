# netio - creates socket ports via the filesystem
# Copyright (C) 2001, 02 Moritz Schulte <moritz@duesseldorf.ccc.de>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or *
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA

CFLAGS = -g -Wall -D_GNU_SOURCE

netio: main.o netfs.o node.o lib.o protocol.o
	gcc $(CFLAGS) -lnetfs -lfshelp -liohelp -lthreads -lports \
	  -lihash -lshouldbeinlibc \
          -o netio main.o netfs.o node.o lib.o protocol.o

main.o: main.c netio.h lib.h node.h protocol.h
	gcc $(CFLAGS) -c main.c

netfs.o: netfs.c netio.h lib.h node.h protocol.h
	gcc $(CFLAGS) -c netfs.c

lib.o: lib.c lib.h
	gcc $(CFLAGS) -c lib.c

node.o: node.c netio.h lib.h protocol.h
	gcc $(CFLAGS) -c node.c

protocol.o: protocol.c netio.h lib.h node.h
	gcc $(CFLAGS) -c protocol.c

clean:
	rm -rf *.o netio

.PHONY: clean
