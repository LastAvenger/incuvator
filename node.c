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

/* make an instance of `struct netnode' with the specified parameters,
   return NULL on error */
struct netnode *
gopherfs_make_netnode (char type, char *name, char *selector,
		       char *server, unsigned short port)
{
  struct netnode *nn;
  int err;

  nn = (struct netnode *) malloc (sizeof (struct netnode));
  if (!nn)
    return NULL;
  memset (nn, 0, sizeof (struct netnode));
  nn->type = type;
  nn->name = strdup (name);
  if (! nn->name)
    err = 1;
  nn->selector = strdup (selector);
  if (! nn->selector)
    err = 2;
  nn->server = strdup (server);
  if (! nn->server)
    err = 3;
  nn->port = port;
  nn->ents = NULL;
  nn->noents = FALSE;
  /* XXX init cache references */

  switch (err)
    {
    case 4:
      free (nn->server);
    case 3:
      free (nn->selector);
    case 2:
      free (nn->name);
    case 1:
      free (nn);
    case 0:
      return NULL;
    default:
      return nn;
    }

}

/* free an instance of `struct netnode' */
void
free_netnode (struct netnode *node)
{
  struct node *nd;

  free (node->server);
  free (node->selector);
  free (node->name);
  for (nd = node->ents; nd; nd = nd->next)
    free (nd);
  free (node);
}

/* make an instance of `struct node' with the specified parameters,
   return NULL on error */
struct node *
gopherfs_make_node (char type, char *name, char *selector,
		    char *server, unsigned short port)
{
  struct netnode *nn;
  struct node *nd;

  nn = gopherfs_make_netnode (type, name, selector, server, port);
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
  nd->nn_stat.st_mode |= type == GPHR_DIR ? S_IFDIR : S_IFREG;
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

/* free an instance of `struct node' */
void
free_node (struct node *node)
{
  /* XXX possibly take care of cache references */
  free_netnode (node->nn);
  free (node);
}
