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

#include <stdio.h>
#include <stdarg.h>

static FILE *debug_stream = NULL;

void
debug_init (FILE *stream)
{
  setbuf (stream, NULL);
  debug_stream = stream;
}

void
_debug (const char *format, ...)
{
  if (!debug_stream)
    return;

  va_list args;
  fprintf (debug_stream, "D: ");
  va_start (args, format);
  vfprintf (debug_stream, format, args);
  va_end (args);
  fprintf (debug_stream, "\n");
}

