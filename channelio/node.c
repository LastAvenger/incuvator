/* Per-node information for channelio.

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

#include "node.h"

#include <assert.h>
#include <fcntl.h>
#include <string.h>

/* Initialize NODE.  */
void
node_init (struct node *node)
{
  memset (node, 0, sizeof (struct node));
  mutex_init (&node->lock);
}

/* Open the node and return a new per-open structure in OPEN.  Return
   error if one occurs.  */
error_t
node_open (struct node *node, int flags, struct open **open)
{
  struct open *new, **opens = node->opens;
  struct channel *channel;
  error_t err;

  mutex_lock (&node->hub->lock);
  err = channel_open (node->hub, flags, &channel);
  mutex_unlock (&node->hub->lock);
  if (err)
    return err;

  if ((flags & (CHANNEL_READONLY | CHANNEL_READONLY))
      != (channel->flags & (CHANNEL_READONLY | CHANNEL_READONLY)))
    /* The channel is more restricted than the open modes given (FLAGS are
       just converted open modes.)  XXX this should be reflected better in
       by the interface.  */
    {
      channel_close (channel);
      return EPERM;
    }

  err = open_alloc (channel, &new);
  if (err)
    return err;

  mutex_lock (&node->lock);
  opens = realloc (opens, (node->num_opens + 1) * sizeof (struct open *));
  if (opens == NULL)
    {
      mutex_unlock (&node->lock);
      open_free (new);
      return ENOMEM;
    }
  
  opens[node->num_opens] = new;
  node->opens = opens;
  node->num_opens++;
  mutex_unlock (&node->lock);

  *open = new;
  return 0;
}

/* Remove any resources use by OPEN in NODE, then free it.  */
void
node_close (struct node *node, struct open *open)
{
  struct open **opens = node->opens;
  int i;

  mutex_lock (&node->lock);
  for (i = 0; i < node->num_opens; i++)
    if (opens[i] == open)
      break;

  assert (opens[i] == open);
  node->num_opens--;
  opens[i] = opens[node->num_opens];

  open_free (open);
  opens = realloc (opens, node->num_opens * sizeof (struct open *));
  if (node->num_opens == 0)
    /* OPENS was freed.  */
    opens = NULL;

  node->opens = opens;
  mutex_unlock (&node->lock);
}

/* Return the effective flags of NODE by or-ing them with the flags of the
   underlying hub if it has been created.  */
int
node_flags (const struct node *node)
{
  int flags;

  if (node->hub == NULL)
    return node->flags;

  mutex_lock (&node->hub->lock);
  flags = node->hub->flags; 
  mutex_unlock (&node->hub->lock);

  return flags;
}

/* Return true if open modes in MODES are permitted when opening
   channels.  */
int
node_check_perms (const struct node *node, int modes)
{
  int flags = node_flags (node);
  return ! ((modes & O_WRITE) && (flags & CHANNEL_READONLY))
    || ! ((modes & O_READ) && (flags & CHANNEL_WRITEONLY));
}

/* Return the channel flags that corresponds to the open modes set in
   MODES.  */
int
modes_to_flags (int modes)
{
  if ((modes & O_READ) && !(modes & O_WRITE))
   return CHANNEL_READONLY;

  if ((modes & O_WRITE) && !(modes & O_READ))
    return CHANNEL_WRITEONLY;

  return 0;
}


/* Try to write out any pending writes from any channels open on NODE.
   Return any error that occurs.  */
error_t
node_sync (struct node *node)
{
  int i;

  for (i = 0; i < node->num_opens; i++)
    {
      error_t err = channel_flush (node->opens[node->num_opens]->channel);
      if (err)
	return err;
    }

  return 0;
}
