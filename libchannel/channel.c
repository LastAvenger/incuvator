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

/* Allocate a new channel of class CLASS, with FLAGS set (using
   channel_set_flags,) that is returned in CHANNEL.  Return ENOMEM if
   memory for channel couldn't be allocated.  */
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

/* If not null call method cleanup to deallocate class-specific bits of
   CHANNEL, then free it (regardless) and any generic resources used by
   it.  */
void
channel_free (struct channel *channel)
{
  if (channel->class->cleanup)
    (*channel->class->cleanup) (channel);

  free (channel);
}

/* Set the flags FLAGS in CHANNEL.  Remove any already set flags in FLAGS,
   if FLAGS contain any backend flags call set_flags method or if
   set_flags is null return EINVAL.  Lastly generic flags get set.  */
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

/* Clear the flags FLAGS in CHANNEL.  Remove any already cleared flags in
   FLAGS, if FLAGS contain any backend flags call clear_flags method or if
   clear_flags is null return EINVAL.  Lastly generic flags get
   cleared.  */
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

/* Reads at most AMOUNT bytes from CHANNEL into BUF and LEN with the usual
   return buf semantics.  Block until data is available and return 0 bytes
   on EOF.  If channel is write-only return EPERM, otherwise forward call
   to CHANNEL's read method.  */
error_t
channel_read (struct channel *channel, size_t amount,
	      void **buf, size_t *len)
{
  if (channel->flags & CHANNEL_WRITEONLY)
    return EPERM;

  return (*channel->class->read) (channel, amount, buf, len);
}

/* Write LEN bytes of BUF to CHANNEL, AMOUNT is set to the amount actually
   witten.  Block until data can be written.  If channel is read-only
   return EPERM, otherwise forward call to CHANNEL's write method.  */
error_t
channel_write (struct channel *channel, const void *buf,
	       size_t len, size_t *amount)
{
  if (channel->flags & CHANNEL_READONLY)
    return EPERM;

  return (*channel->class->write) (channel, buf, len, amount);
}
