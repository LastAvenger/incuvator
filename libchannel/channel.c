/* Channel I/O

   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2001, 2002, 2003, 2007
     Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.org>
   Reworked for libchannel by Carl Fredrik Hammar <hammy.lite@gmail.com>

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111, USA. */

#include <malloc.h>

#include "channel.h"

/* Allocate a new channel structure with each fields initialized to
   the respective given parameter.  */
error_t
channel_create (const struct channel_class *class,
		int flags, struct channel **channel)
{
  struct channel *new = malloc (sizeof (struct channel));
  if (!new)
    return ENOMEM;

  new->flags = flags;
  new->class = class;

  *channel = new;

  return 0;
}

void
channel_free (struct channel *channel)
{
  if (channel->class->cleanup)
    (*channel->class->cleanup) (channel);

  free (channel);
}

/* Add FLAGS to CHANNEL's currently set flags.  */
error_t
channel_set_flags (struct channel *channel, int flags)
{
  error_t err = 0;
  int orig = channel->flags, new = flags & ~orig;

  if (new & CHANNEL_BACKEND_FLAGS)
    {
      if (channel->class->set_flags)
	err = (*channel->class->set_flags) (channel, new);
      else
	err = EINVAL;
    }

  if (! err)
    channel->flags |= (new & ~CHANNEL_BACKEND_FLAGS);

  return err;
}

/* Remove FLAGS from CHANNEL's currently set flags.  */
error_t
channel_clear_flags (struct channel *channel, int flags)
{
  error_t err = 0;
  int orig = channel->flags, kill = flags & orig;

  if (kill & CHANNEL_BACKEND_FLAGS)
    {
      if (channel->class->clear_flags)
	err = (*channel->class->clear_flags) (channel, kill);
      else
	err = EINVAL;
    }

  if (! err)
    channel->flags &= ~(kill & ~CHANNEL_BACKEND_FLAGS);

  return err;
}

/* Write LEN bytes from BUF to CHANNEL.  Return the amount written in
   AMOUNT.  */
error_t
channel_write (struct channel *channel, const void *buf,
	       size_t len, size_t *amount)
{
  if (channel->flags & CHANNEL_READONLY)
    return EROFS;

  return (*channel->class->write) (channel, buf, len, amount);
}

/* Read AMOUNT bytes from CHANNEL into BUF and LEN (using mach buffer
   return semantics).  */
error_t
channel_read (struct channel *channel, size_t amount,
	      void **buf, size_t *len)
{
  return (*channel->class->read) (channel, amount, buf, len);
}
