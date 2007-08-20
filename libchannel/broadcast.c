/* Channel broadcaster.

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

#include <string.h>

struct broadcast_hook
{
  struct channel *channel; /* Channel to broadcast.  */

  size_t num_channels; /* Number of open channels.  */
  size_t amount;       /* Amount to read.  */

  /* Broadcasted when read has been made.  */
  struct condition read_cond;

  size_t wait_count; /* Number of channels waiting to read.  */ 
  size_t pending_count; /* Number of channels waiting to recieve.  */

  /* Result of read.  */
  error_t err;
  void *buf;
  size_t len;
};


static error_t
broadcast_read (struct channel *channel, size_t amount,
		void **buf, size_t *len)
{
  struct channel_hub *hub = channel->hub;
  struct broadcast_hook *hook = hub->hook;
  
  mutex_lock (&hub->lock);

  hook->wait_count++;
  if (hook->amount == 0 || amount < hook->amount)
    hook->amount = amount;

  if (hook->wait_count == hook->num_channels)
    {
      hook->err = channel_read (hook->channel, hook->amount,
				&hook->buf, &hook->len);
      hook->pending_count = hook->wait_count;
      hook->wait_count = 0;
      condition_broadcast (&hook->read_cond);
    }
  else
    condition_wait (&hook->read_cond, &hub->lock);

  hook->pending_count--;

  if (hook->err)
    return hook->err;
      
  if (hook->pending_count == 0)
    *buf = hook->buf;
  else
    memcpy (*buf, hook->buf, hook->amount);

  *len = hook->len;
  mutex_unlock (&hub->lock);

  return 0;
}


static error_t
broadcast_write (struct channel *channel, void *buf, size_t len,
		 size_t *amount)
{
  return EOPNOTSUPP;
}


static error_t
broadcast_open (struct channel *channel, int flags)
{
  struct broadcast_hook *hook = channel->hub->hook;
  error_t err = 0;

  if (hook->num_channels == 0)
    err = channel_open (channel->hub->children[0], flags, &hook->channel);

  if (! err)
    hook->num_channels++;

  return err;
}


static void
broadcast_close (struct channel *channel)
{
  struct broadcast_hook *hook = channel->hub->hook;

  mutex_lock (&channel->hub->lock);

  hook->num_channels--;
  if (hook->num_channels == 0)
    channel_close (hook->channel);

  mutex_unlock (&channel->hub->lock);
}


static void 
broadcast_clear_hub (struct channel_hub *hub)
{
  struct broadcast_hook *hook = hub->hook;
  condition_clear (&hook->read_cond);
  free (hook);
}


const struct channel_class
channel_broadcast_class = {
 name: "broadcast",
 read: broadcast_read,
 write: broadcast_write,
 open: broadcast_open,
 close: broadcast_close,
 create_hub: channel_create_broadcast_hub,
 clear_hub: broadcast_clear_hub
};
CHANNEL_STD_CLASS (broadcast);


error_t
channel_create_broadcast_hub (const char *name, int flags,
			      const struct channel_class *const *classes,
			      struct channel_hub **hub)
{
  struct channel_hub *new = 0, *child = 0;
  struct broadcast_hook *hook = 0;
  error_t err;

  flags |= CHANNEL_HARD_READONLY | CHANNEL_ENFORCED;

  err = channel_alloc_hub (&channel_broadcast_class, name, flags, &new);
  if (err)
    return err;

  err = channel_create_typed_hub (name, flags, classes, &child);
  if (err)
    {
      channel_free_hub (new);
      return err;
    }

  channel_set_children (new, &child, 1);
  if (err)
    {
      channel_free_hub (new);
      channel_free_hub (child);
      return err;
    }

  hook = new->hook = calloc (1, sizeof (struct broadcast_hook));
  if (!hook)
    {
      channel_free_hub (new);
      return ENOMEM;
    }

  condition_init (&hook->read_cond);
  
  *hub = new;
  return 0;
}
