# Copyright (C) 2009, 2010 Free Software Foundation, Inc.
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


noinst_LIBRARIES =
MOSTLYCLEANFILES =


AM_CPPFLAGS =
AM_CFLAGS =

bin_PROGRAMS =


AM_CPPFLAGS += \
  -include config.h

AM_CFLAGS += \
  -Wall


# MIG Setup.

# MIGCOM.
MIGCOM = $(MIG) -n -cc cat - /dev/null

# Makerules.mig: how to do some MIG-related things.
include Makerules.mig.am


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


# Helper, awaiting a proper solution.
%.server.defs.c:
	rm -f $@
	echo \
	  > $@ \
	  '#include <$*.defs>' 

# Server stubs.
nodist_lib_dep_tr_for_defs_a_SOURCES += \
	mach/notify.server.defs.c
nodist_nsmux_SOURCES = \
	mach/notify.server.h \
	mach/notify.server.c \
	mach/notify.server.msgids


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
