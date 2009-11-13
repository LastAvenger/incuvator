# Copyright (C) 2009 Free Software Foundation, Inc.
# This file is part of the GNU Hurd.
#
# The GNU Hurd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# The GNU Hurd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the GNU Hurd; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

dir := pcnet32
makemode := server

SRCS = pcnet32.c net_init.c deviceUser.c machUser.c irq.c net.c main.c \
       ds_routines.c queue.c device_replyUser.c deviceServer.c \
       notifyServer.c kmem.c
LCLHDRS = 
HURDLIBS = threads ports fshelp shouldbeinlibc trivfs
target = pcnet32
OBJS = $(SRCS:.c=.o) $(MIGSTUBS)

include ../Makeconf

LDFLAGS += -lpciaccess
