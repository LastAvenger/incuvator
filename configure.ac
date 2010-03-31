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


m4_include([package.m4])
AC_INIT(
  [AC_PACKAGE_NAME],
  [AC_PACKAGE_VERSION],
  [AC_PACKAGE_BUGREPORT])

AC_CONFIG_SRCDIR([nsmux.c])

AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE(
  [foreign] dnl For now.
  [-Wall]
  [no-define]
  [subdir-objects])


AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_RANLIB


AC_CHECK_TOOL([MIG], [mig], [mig])


AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
