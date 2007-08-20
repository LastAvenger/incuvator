# Makefile for libchannel
#
#   Copyright (C) 1995, 1996, 1997, 2001, 2002, 2007
#     Free Software Foundation, Inc.
#
#   Written by Miles Bader <miles@gnu.org>
#   Reworked for libchannel by Carl Fredrik Hammar <hammy.lite@gmail.com>
#
#   This file is part of the GNU Hurd.
#
#   The GNU Hurd is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License as
#   published by the Free Software Foundation; either version 2, or (at
#   your option) any later version.
#
#   The GNU Hurd is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111, USA.

dir := libchannel
makemode := library

libname = libchannel
SRCS = channel.c hub.c argp.c control.c fetch.c kids.c file.c module.c \
       query.c typed.c $(channel-types:=.c)

channel-types = \
	      file \
	      broadcast \
	      tee \
	      module

libchannel.so-LDLIBS += -ldl
LCLHDRS=channel.h
installhdrs=channel.h

HURDLIBS = shouldbeinlibc
OBJS = $(SRCS:.c=.o) $(GUNZIP_OBJS) $(BUNZIP2_OBJS)

include ../Makeconf

module-CPPFLAGS = -D'CHANNEL_SONAME_SUFFIX=".so.$(hurd-version)"'

# You can use this rule to make a dynamically-loadable version of any
# of the modules.  We don't make any of these by default, since we
# just include all the standard channel types in libchannel.so itself.
libchannel_%.so.$(hurd-version): %_pic.o libchannel.so
	$(CC) -shared -Wl,-soname=$@ -o $@ \
	      $(rpath) $(CFLAGS) $(LDFLAGS) $(libchannel_$*.so-LDFLAGS) $^

# Each libchannel_TYPE.a is in fact an object file script so that
# `-lchannel_TYPE' just has the same effect as `-u
# channel_TYPE_class'.
$(channel-types:%=libchannel_%.a): libchannel_%.a: $(srcdir)/Makefile
	$(CC) -r -nostdlib -nostartfiles -x c /dev/null \
	      -o $@ -u channel_$*_class
cleantarg += $(channel-types:%=libchannel_%.a)

all: $(channel-types:%=libchannel_%.a)

install: $(channel-types:%=$(libdir)/libchannel_%.a)
$(channel-types:%=$(libdir)/libchannel_%.a): $(libdir)/%: %
	$(INSTALL_DATA) $< $@
