# Makefile for gopherfs
# 
#   Copyright (C) 2002 James A. Morrison
#   Copyright (C) 1997, 2000 Free Software Foundation, Inc.
#
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License as
#   published by the Free Software Foundation; either version 2, or (at
#   your option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

dir := gopherfs
makemode := server

target = gopherfs

CC = gcc
CFLAGS = -Wall -g -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -std=gnu99
INCLUDES = -I.
SRCS = gopherfs.c args.c netfs.c gopher.c node.c fs.c debug.c dir.c
LCLHDRS = gopherfs.h fs.h debug.h

OBJS = $(SRCS:.c=.o)
HURDLIBS = -lnetfs -lfshelp -liohelp -lports

INSTALL = install

all: $(target)

$(target): $(OBJS)
	$(CC) $(CFLAGS) -o $(target) $(OBJS) $(HURDLIBS)

%.o: %.c $(LCLHDRS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

clean:
	-rm -f *.o $(target)

install:
	$(INSTALL) -d $(DESTDIR)/hurd
	$(INSTALL) $(target) $(DESTDIR)/hurd/
