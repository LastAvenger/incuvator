/* Gopher filesystem

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

#ifndef __GOPHERFS_H__
#define __GOPHERFS_H__

#include <stdlib.h>
#include <maptime.h>
#include <sys/stat.h>
#include <pthread.h>
#include <error.h>
#include <netdb.h>

#include <hurd/hurd_types.h>
#define GOPHER_SERVER_NAME "gopherfs"
#define GOPHER_SERVER_VERSION "0.1.2"

#include "debug.h"

extern volatile struct mapped_time_value *gopherfs_maptime;

/* gopherfs parameters. */
struct gopherfs_params
{
  char *server;        /* Server host name. */
  unsigned short port; /* Port number. */
  char *dir;           /* Root directory. */
};

#include "gopher.h"

/* private data per `struct node' */
struct netnode
{
  /* Gopher parameters (type, name, selector, server, port). */
  struct gopher_entry *e;

  /* The directory entry for this node. */
  struct node *dir;

  /* directory entries if this is a directory */
  struct node *ents;
  boolean_t noents;
  unsigned int num_ents;

  /* XXX cache reference ? */
};

/* The filesystem data type */
struct gopherfs
{
  struct node *root;
  /* stat infrmation */
  mode_t umask;
  uid_t uid;
  gid_t gid;
  ino_t next_inode;
  /* some kind of cache thingy */

  struct gopherfs_params *params;
};

/* global pointer to the filesystem */
extern struct gopherfs *gopherfs;

/* handle all initial parameter parsing */
error_t gopherfs_parse_args (int argc, char **argv, struct gopherfs_params *params);

/* fetch a directory node from the gopher server
   DIR should already be locked */
error_t fill_dirnode (struct node *dir);

/* free an instance of `struct netnode' */
void free_netnode (struct netnode *node);

/* make an instance of `struct node' in DIR with the parameters in ENTRY,
   return NULL on error */
struct node *gopherfs_make_node (struct gopher_entry *entry,
				 struct node *dir);

/* free an instance of `struct node' */
void free_node (struct node *node);

#endif /* __GOPHERFS_H__ */
