/* Generic channel hub functions.

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
#include <malloc.h>

/* Allocate a new hub of CLASS with FLAGS set, then return it in HUB.
   Return ENOMEM if memory for the hub couldn't be allocated.  */
error_t
channel_alloc_hub (const struct channel_class *class,
		   const char *name, int flags,
		   struct channel_hub **hub)
{
  error_t err;
  struct channel_hub *new = malloc (sizeof (struct channel_hub));
  if (! new)
    return ENOMEM;

  if (flags & CHANNEL_HARD_READONLY)
    flags |= CHANNEL_READONLY;

  if (flags & CHANNEL_HARD_WRITEONLY)
    flags |= CHANNEL_WRITEONLY;

  pthread_mutex_init (&new->lock, NULL);
  new->name = 0;
  new->flags = flags;
  new->hook = 0;
  new->children = 0;
  new->num_children = 0;

  new->class = class;

  err = channel_set_hub_name (new, name);
  if (err)
    free (new);

  if (! err)
    *hub = new;

  return 0;
}

/* If not null call method clear_hub to deallocate class-specific bits of
   HUB, then (regardless) free any generic resources used by it and
   itself.  */
void
channel_free_hub (struct channel_hub *hub)
{
  int i;

  if (hub->class->clear_hub)
    (*hub->class->clear_hub) (hub);

  pthread_mutex_destroy (&hub->lock);
  free (hub->name);

  for (i = 0; i < hub->num_children; i++)
    channel_free_hub (hub->children[i]);

  free (hub);
}

/* Set name of HUB to copy of NAME.  Return ENOMEM if no memory if
   available.  */
error_t
channel_set_hub_name (struct channel_hub *hub, const char *name)
{
  char *copy = name ? strdup (name) : 0;
  if (name && !copy)
    return ENOMEM;

  if (hub->name)
    free (hub->name);

  hub->name = copy;
  return 0;
}


/* Set the flags FLAGS in HUB.  Remove any already set flags in FLAGS, if
   FLAGS then contain backend flags call set_hub_flags method with with
   FLAGS or if set_hub_flags is null return EINVAL.  Lastly generic flags
   get set.  */
error_t
channel_set_hub_flags (struct channel_hub *hub, int flags)
{
  int orig = hub->flags, new = flags & ~orig;
  error_t err = 0;

  if (new & CHANNEL_BACKEND_FLAGS)
    {
      if (hub->class->set_hub_flags)
	err = (*hub->class->set_hub_flags) (hub, new);
      else
	err = EINVAL;
    }

  if (! err)
    hub->flags |= (new & ~CHANNEL_BACKEND_FLAGS);

  return err;
}

/* Clear the flags FLAGS in HUB.  Remove any already cleared flags in
   FLAGS, if FLAGS then contain backend flags call clear_hub_flags method
   with with FLAGS or if clear_hub_flags is null return EINVAL.  Lastly
   generic flags get clear.  */
error_t
channel_clear_hub_flags (struct channel_hub *hub, int flags)
{
  int orig = hub->flags, kill = flags & orig;
  error_t err = 0;

  if (kill & CHANNEL_BACKEND_FLAGS)
    {
      if (hub->class->clear_hub_flags)
	err = (*hub->class->clear_hub_flags) (hub, kill);
      else
	err = EINVAL;
    }

  if (! err)
    hub->flags &= ~(kill & ~CHANNEL_BACKEND_FLAGS);

  return err;
}
