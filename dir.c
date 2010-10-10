/* Gopher filesystem

   Copyright (C) 2010 Manuel Menal <mmenal@hurdfr.org>
   This file is part of the Gopherfs translator.

   Gopherfs is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   Gopherfs is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include <errno.h>

#include "gopherfs.h"

#include <hurd/hurd_types.h>
#include <hurd/netfs.h>

/* fetch a directory node from the gopher server
   DIR should already be locked */
error_t
fill_dirnode (struct node *dir)
{
  error_t err = 0;
  struct node *nd, **prevp;
  struct gopher_entry *map = NULL;

  /* Get entries from DIR. */
  err = gopher_list_entries (dir->nn->e, &map);
  if (err)
    return err;

  dir->nn->noents = TRUE;
  dir->nn->num_ents = 0;

  /* Fill DIR. */
  prevp = &dir->nn->ents;
  for (struct gopher_entry *ent = map; ent; ent = ent->next)
    {
      /* Don't make node from informational messages: they don't even have a valid selector. */
      if (ent->type == GPHR_INFO)
	continue;

      nd = gopherfs_make_node (ent, dir);
      if (!nd)
	{
	  err = ENOMEM;
	  break;
	}

      *prevp = nd;
      nd->prevp = prevp;
      prevp = &nd->next;

      dir->nn->num_ents++;
      if (dir->nn->noents)
	dir->nn->noents = FALSE;
    }

  return err;
}
