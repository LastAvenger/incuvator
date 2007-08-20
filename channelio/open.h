/* Per-open information for channelio.

   Copyright (C) 1995, 1996, 2007 Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.ai.mit.edu>
   Reworked for channelio by Carl Fredrik Hammar <hammy.lite@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifndef __OPEN_H__
#define __OPEN_H__

#include <cthreads.h>
#include <hurd/channel.h>

/* Information about an open session.  */
struct open
{
  struct mutex lock;
  struct channel *channel;

  /* The current owner of the session.  For terminals, this affects
     controlling terminal behavior (see term_become_ctty).  For all
     objects this affects old-style async IO.  Negative values represent
     pgrps.  This has nothing to do with the owner of a file (as returned
     by io_stat, and as used for various permission checks by
     filesystems).  An owner of 0 indicates that there is no owner.  */
  pid_t owner;
};

/* Returns a new per-open structure in OPEN that wraps CHANNEL.
   Propagates any error.  */
error_t open_alloc (struct channel *channel, struct open **open);

/* Free OPEN and any resources it holds.  */
void open_free (struct open *open);

#endif /* __OPEN_H__ */
