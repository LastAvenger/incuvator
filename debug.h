/* Gopher filesystem

   Copyright (C) 2010 Manuel Menal <mmenal@hurdfr.org>
   This file is part of the Gopherfs translator.

   Gopherfs is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   Gopherfs is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

#define debug(format, args...) _debug ("%s: " format, __func__, ##args)

void debug_init (FILE *stream);
void _debug (const char *format, ...);

#endif /* __FS_H__ */

