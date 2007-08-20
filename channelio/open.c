/* Per-open information for channelio.

   Copyright (C) 2007 Free Software Foundation, Inc.

   Written by Carl Fredrik Hammar <hammy.lite@gmail.com>

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

#include "open.h"

/* Returns a new per-open structure in OPEN that wraps CHANNEL.
   Propagates any error.  */
error_t
open_alloc (struct channel *channel, struct open **open)
{
  struct open *new = malloc (sizeof (struct open));
  if (! new)
    return ENOMEM;
  
  mutex_init (&new->lock);
  new->channel = channel;
  new->owner = 0;

  channel->user_hook = new;

  *open = new;
  return 0;
}

/* Free OPEN and any resources it holds.  */
void
open_free (struct open *open)
{
  channel_close (open->channel);
  mutex_clear (&open->lock);
  free (open);
}
