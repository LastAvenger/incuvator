#
#   Copyright (C) 1993, 1994, 1995, 1996 Free Software Foundation
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

dir := .
makemode := misc

ifndef srcdir
srcdir = .
endif

include $(srcdir)/Makeconf

lib-subdirs = libshouldbeinlibc libihash libiohelp libports libthreads \
	      libpager libfshelp libdiskfs libtrivfs libps \
	      libnetfs libpipe libstore libmom
prog-subdirs = auth boot exec fstests init \
	       proc term ufs utils sutils trans ufs-fsck \
	       devio ufs-utils ext2fs benchmarks pflocal defpager \
	       login nfs pfinet daemons
other-subdirs = hurd doc config release include
subdirs = $(lib-subdirs) $(prog-subdirs) $(other-subdirs)
subdirs-nodist =
working-prog-subdirs := $(filter-out \
			  $(patsubst %/,%,\
				 $(dir $(wildcard $(prog-subdirs:=/BROKEN)))),\
			  $(prog-subdirs))
DIST_FILES = COPYING Makeconf config.make.in configure.in configure \
	     hurd.boot build.mk.in build.mkcf.in SETUP \
	     README NEWS tasks INSTALL INSTALL-cross

all: $(addsuffix -all,$(lib-subdirs) $(working-prog-subdirs))

%-all:
	$(MAKE) -C $* all

%-lndist: $(top_srcdir)/hurd-snap
	$(MAKE) -C $* lndist no_deps=t

%-clean:
	$(MAKE) -C $* clean no_deps=t

%-relink:
	$(MAKE) -C $* relink

%-install:
	$(MAKE) -C $* install

%-install-headers:
	$(MAKE) -C $* install-headers

%-TAGS:
	$(MAKE) -C $* TAGS no_deps=t

$(srcdir)/hurd-snap:
	mkdir $(srcdir)/hurd-snap

ifeq ($(version),)
version:=$(shell date +%y%m%d)
dirname:=hurd-snap
else
dirname:=hurd
endif

dist: $(srcdir)/hurd-snap $(addsuffix -lndist,$(filter-out $(subdirs-nodist), $(subdirs))) lndist
	mv $(srcdir)/hurd-snap $(srcdir)/$(dirname)-$(version)
	tar cfz $(srcdir)/$(dirname)-$(version).tar.gz $(srcdir)/$(dirname)-$(version)
	rm -rf $(srcdir)/$(dirname)-$(version)

clean: $(addsuffix -clean,$(lib-subdirs)) $(addsuffix -clean,$(working-prog-subdirs)) clean-misc

relink: $(addsuffix -relink,$(prog-subdirs))

install: $(addsuffix -install,$(lib-subdirs) $(working-prog-subdirs) \
	   $(other-subdirs))

install-headers: $(addsuffix -install-headers,$(lib-subdirs) \
		$(working-prog-subdirs)\
		$(other-subdirs))

lndist: cp-linked-files

linked-files = install-sh config.guess config.sub mkinstalldirs
lf-inst = $(addprefix $(srcdir)/hurd-snap/,$(linked-files))
cp-linked-files: $(lf-inst)
$(lf-inst): $(srcdir)/hurd-snap/%: $(srcdir)/%
	cp $< $@

TAGS: $(addsuffix -TAGS,$(prog-subdirs) $(lib-subdirs))

.PHONY: clean-misc distclean
clean-misc:

distclean: clean
	rm -f config.make config.log config.status config.cache
ifneq (.,${srcdir})
	rm -f Makefile
endif
