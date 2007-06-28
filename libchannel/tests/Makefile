# Makefile for libchannel tests
#
#   Copyright (C) 2002, 2007 Free Software Foundation, Inc.
#
#   Reworked by Carl Fredrik Hammar <hammy.lite@gmail.com>
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

# XXX This Makefile is a hack since Makeconf doesn't handle tests yet.

CHECK_SRC := test-1.c

CHECK_OBJS := $(addsuffix .o,$(basename $(notdir $(CHECK_SRC))))
CHECK_PROGS := $(basename $(notdir $(CHECK_SRC))) \
	       $(addsuffix -static, $(basename $(CHECK_SRC)))

CFLAGS := -Wall -g -I.. -D_GNU_SOURCE
LDFLAGS := -L..
LDLIBS := -lchannel

%-static: %.o
	$(CC) -static $(CFLAGS) $(LDFLAGS) $< -o $@ $(LDLIBS)

check: ../libchannel.a ../libchannel.so $(CHECK_PROGS)
	export LD_LIBRARY_PATH=..;		\
	for i in $(CHECK_PROGS); do		\
	  echo -n Running $$i...\ ;		\
	  if ./$$i 2>&1 > $$i.out;		\
	  then					\
	    echo Success.;			\
	  else					\
	    echo Failure.;			\
	  fi					\
	done

clean:
	rm -f $(CHECK_OBJS) $(CHECK_PROGS) \
	  $(addsuffix .out,$(basename $(notdir $(CHECK_PROGS))))