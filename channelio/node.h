/* Per-node information for channelio.

   Copyright (C) 1995, 1996, 1997, 1999, 2000, 2001, 2007
     Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.org>
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

#ifndef __NODE_H__
#define __NODE_H__

#include "open.h"

/* Information for a file node that has an underlying channel hub.  */
struct node
{
  /* This lock protects `hub', `owner', `opens' and `num_opens'.  The
     other members never change after creation.  */
  struct mutex lock;

  /* The argument specification that we use to create the hub.  */
  struct channel_parsed *channel_name;

  /* The channel hub from which we obtain channels.  This is null until
     the first channel is to be opened.  */
  struct channel_hub *hub;

  int flags;
  dev_t rdev; /* A unixy device number for st_rdev.  */

  /* All currently open sessions.  */
  struct open **opens;
  int num_opens;
};

/* Initialize NODE.  */
void node_init (struct node *node);

/* Open the node and return a new per-open structure in OPEN.  Return
   error if one occurs.  */
error_t node_open (struct node *node, int flags, struct open **open);

/* Remove any resources use by OPEN in NODE, then free it.  */
void node_close (struct node *node, struct open *open);

/* Return the effective flags of NODE by or-ing them with the flags of the
   underlying hub if it has been created.  */
int node_flags (const struct node *node);

/* Return true if open modes in MODES are permitted when opening
   channels.  */
int node_check_perms (const struct node *node, int modes);

/* Return the channel flags that corresponds to the open modes set in
   MODES.  */
int modes_to_flags (int modes);

/* Try to write out any pending writes from any channels open on NODE.
   Return any error that occurs.  */
error_t node_sync (struct node *node);

#endif /* __NODE_H__ */
