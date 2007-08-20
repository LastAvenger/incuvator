/* Channel tee.

   Copyright (C) 2007 Free Software Foundation, Inc.

   Written by Carl Fredrik Hammar <hammy.lite@gmail.com>

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

#include "channel.h"


static error_t
tee_read (struct channel *channel, size_t amount, void **buf, size_t *len)
{
  return EOPNOTSUPP;
}


static error_t
tee_write (struct channel *channel, void *buf, size_t len, size_t *amount)
{
  struct channel **ch, **channels = channel->class_hook;
  error_t err;
  size_t n, m;

  if (! channels[0])
    return 0;

  err = channel_write (channels[0], buf, len, amount);
  if (err)
    return err;

  for (ch = channels + 1; *ch; ch++)
    for (n = 0; n < *amount; n += m)
      {
	err = channel_write (*ch, buf, len, &m);
	if (err)
	  {
	    *amount = 0;
	    return err;
	  }
      }
  
  return 0;
}


static error_t
tee_flush (struct channel *channel)
{
  struct channel **ch, **channels = channel->class_hook;
  error_t err = 0;

  for (ch = channels; *ch && !err; ch++)
    channel_flush (*ch);

  return err;
}



static error_t
tee_open (struct channel *channel, int flags)
{
  error_t err = 0;
  int i;
  struct channel **channels = malloc (sizeof (struct channel *)
				      * (channel->hub->num_children + 1));
  if (! channels)
    return ENOMEM;

  for (i = 0; i < channel->hub->num_children && !err; i++)
    err = channel_open (channel->hub->children[i], flags, &channels[i]);

  channels[channel->hub->num_children] = 0;
  channel->class_hook = channels;

  return err;
}


static void
tee_close (struct channel *channel)
{
  free (channel->class_hook);
}


const struct channel_class
channel_tee_class = {
 name: "tee",
 read: tee_read,
 write: tee_write,
 flush: tee_flush,
 open: tee_open,
 close: tee_close,
 create_hub: channel_create_tee_hub,
};
CHANNEL_STD_CLASS (tee);


error_t
channel_create_tee_hub (const char *name, int flags,
			const struct channel_class *const *classes,
			struct channel_hub **hub)
{
  struct channel_hub *new;
  error_t err;

  flags |= CHANNEL_HARD_WRITEONLY | CHANNEL_ENFORCED;

  err = channel_alloc_hub (&channel_tee_class, name, flags, &new);
  if (err)
    return err;

  err = channel_create_hub_children (name, flags, classes,
				     &new->children, &new->num_children);

  if (! err)
    *hub = new;
  else
    channel_free_hub (new);

  return err;
}
