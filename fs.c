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

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <stdio.h>

#include "fs.h"

error_t
gopherfs_create (struct gopherfs_params *params)
{
  error_t err = 0;
  struct gopher_entry *root;

  gopherfs = malloc (sizeof (struct gopherfs));
  if (!gopherfs)
    return ENOMEM;

  gopherfs->umask = 0;
  gopherfs->uid = getuid();
  gopherfs->gid = getgid();
  gopherfs->next_inode = 0;

  root = malloc (sizeof (struct gopher_entry));
  if (!root)
    return ENOMEM;
  root->type = GPHR_DIR;
  root->name = "dir";
  root->selector = "";
  root->server = params->server;
  root->port = params->port;

  debug ("creating root node for %s:%hu%s", root->server, root->port);
  gopherfs->root = gopherfs_make_node (root, NULL);
  if (!gopherfs->root)
    err = ENOMEM;
  free (root);

  gopherfs->params = params;

  return err;
}

