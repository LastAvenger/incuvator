/* Generic channel functions.

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

/* Allocate a new channel of hub HUB, with FLAGS set, then return it in
   CHANNEL.  Return ENOMEM if memory for the hub couldn't be allocated.  */
error_t
channel_alloc (struct channel_hub *hub, int flags,
               struct channel **channel)
{
  struct channel *new = malloc (sizeof (struct channel));
  if (!new)
    return ENOMEM;

  new->flags = flags;
  new->hub = hub;
  new->class_hook = 0;
  new->user_hook = 0;

  *channel = new;

  return 0;
}

/* Free CHANNEL and any generic resources allocated for it.  */
void
channel_free (struct channel *channel)
{
  free (channel);
}

/* Allocate a new channel, open it, and return it in CHANNEL.  Uses
   HUB's open method and passes FLAGS to it, unless it's null.  */
error_t
channel_open (struct channel_hub *hub, int flags,
	      struct channel **channel)
{
  error_t err;

  flags |= hub->flags & (CHANNEL_READONLY | CHANNEL_WRITEONLY);
  err = channel_alloc (hub, flags, channel);
  if (err)
    return err;

  if (hub->class->open)
    err = (*hub->class->open) (*channel, flags);

  return err;
}

/* Call CHANNEL's close method, unless it's null, then free it
   (regardless.)  */
void
channel_close (struct channel *channel)
{
  if (channel->hub->class->close)
    (*channel->hub->class->close) (channel);

  channel_free (channel);
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
      if (channel->hub->class->set_flags)
	err = (*channel->hub->class->set_flags) (channel, new);
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
      if (channel->hub->class->clear_flags)
	err = (*channel->hub->class->clear_flags) (channel, kill);
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

  return (*channel->hub->class->read) (channel, amount, buf, len);
}

/* Write LEN bytes of BUF to CHANNEL, AMOUNT is set to the amount actually
   witten.  Block until data can be written.  If channel is read-only
   return EPERM, otherwise forward call to CHANNEL's write method.  */
error_t
channel_write (struct channel *channel, void *buf, size_t len,
	       size_t *amount)
{
  if (channel->flags & CHANNEL_READONLY)
    return EPERM;

  return (*channel->hub->class->write) (channel, buf, len, amount);
}

/* Write out any pending data held by CHANNEL in buffers, by forwarding
   call to flush method, unless it's null.  */
error_t
channel_flush (struct channel *channel)
{
  if (channel->hub->class->flush)
    return (*channel->hub->class->flush) (channel);
  else
    return 0;
}
