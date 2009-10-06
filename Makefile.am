# Copyright (C) 2009 Free Software Foundation, Inc.
# 
# This file is part of nsmux.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any later
# version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.


AM_CPPFLAGS =
AM_CFLAGS =

bin_PROGRAMS =


AM_CPPFLAGS += \
  -include config.h

AM_CFLAGS += \
  -Wall


bin_PROGRAMS += \
  nsmux


nsmux_SOURCES = \
  nsmux.c \
  node.c \
  lnode.c \
  ncache.c \
  options.c \
  lib.c \
  magic.c \
  trans.c

nsmux_CPPFLAGS = \
  -DDEBUG \
  $(AM_CPPFLAGS)

nsmux_LDADD = \
  -lnetfs \
  -lfshelp \
  -liohelp \
  -lthreads \
  -lports \
  -lihash \
  -lshouldbeinlibc
