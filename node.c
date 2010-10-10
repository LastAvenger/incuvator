/* Gopherfs node handling routines

   Copyright (C) 2002 James A. Morrison <ja2morri@uwaterloo.ca>
   Copyright (C) 2000 Igor Khavkine <igor@twu.net>
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

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "gopherfs.h"

#include <hurd/hurd_types.h>
#include <hurd/netfs.h>

/* free an instance of `struct netnode' */
void
free_netnode (struct netnode *nn)
{
  free (nn->e->name);
  free (nn->e->selector);
  free (nn->e->server);
  free (nn->e);
  for (struct node *nd = nn->ents; nd; nd = nd->next)
    free (nd);
  free (nn);
}

/* free an instance of `struct node' */
void
free_node (struct node *node)
{
  /* XXX possibly take care of cache references */
  free_netnode (node->nn);
  free (node);
}

/* Normalize filename: replace '/' by '-'. */
char *
normalize_filename (char *name)
{
  char *s = strdup (name);

  for (char *tmp = s; *tmp != '\0'; tmp++)
    {
      if (*tmp == '/')
	*tmp = '-';
    }

  return s;
}

/* make an instance of `struct netnode' with the specified parameters,
   return NULL on error */
static struct netnode *
gopherfs_make_netnode (struct gopher_entry *entry,
		       struct node *dir)
{
  struct netnode *nn;

  if (!entry)
    return NULL;

  nn = malloc (sizeof (struct netnode));
  if (!nn)
    return NULL;
  memset (nn, 0, sizeof (struct netnode));

  nn->dir = dir;
  nn->e = malloc (sizeof (struct gopher_entry));
  if (!nn->e)
    {
      free (nn);
      return NULL;
    }

  debug ("Creating entry %s (selector %s, server %s, port %hu)",
	 entry->name,
	 entry->selector, entry->server, entry->port);
  nn->e->name = normalize_filename (entry->name);
  nn->e->type = entry->type;
  nn->e->selector = strdup (entry->selector);
  nn->e->server = strdup (entry->server);
  nn->e->port = entry->port;

  /* XXX init cache references */

  if (!(nn->e->name && nn->e->selector && nn->e->server))
    { 
      /* We are allowed to free NULL pointers */
      free_netnode (nn);
      return NULL;
    }
  return nn;

}

/* make an instance of `struct node' in DIR with the parameters in ENTRY.
   return NULL on error */
struct node *
gopherfs_make_node (struct gopher_entry *entry,
		    struct node *dir)
{
  struct netnode *nn;
  struct node *nd;

  nn = gopherfs_make_netnode (entry, dir);
  if (!nn)
    return NULL;

  nd = netfs_make_node (nn);
  if (!nd)
    {
      free (nn);
      return NULL;
    }
  nd->next = NULL;
  nd->prevp = NULL;
  nd->owner = gopherfs->uid;

  /* XXX Hold a reference to the new dir's node.  */
  spin_lock (&netfs_node_refcnt_lock);
  nd->references++;
  spin_unlock (&netfs_node_refcnt_lock);

  /* fill in stat info for the node */
  nd->nn_stat.st_mode = (S_IRUSR | S_IRGRP | S_IROTH) & ~gopherfs->umask;
  nd->nn_stat.st_mode |= entry->type == GPHR_DIR ? S_IFDIR : S_IFREG;
  nd->nn_stat.st_nlink = 1;
  nd->nn_stat.st_uid = gopherfs->uid;
  nd->nn_stat.st_gid = gopherfs->gid;
  nd->nn_stat.st_rdev = 0;
  nd->nn_stat.st_size = 0;
  nd->nn_stat.st_blksize = 0;
  nd->nn_stat.st_blocks = 0;
  nd->nn_stat.st_ino = gopherfs->next_inode++;
  fshelp_touch (&nd->nn_stat, TOUCH_ATIME | TOUCH_MTIME | TOUCH_CTIME,
		gopherfs_maptime);

  return nd;
}
