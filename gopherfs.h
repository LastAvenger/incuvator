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
#include <cthreads.h>
#include <maptime.h>
#include <sys/stat.h>
/* #include <pthreads.h> */
#include <error.h>
#include <netdb.h>

#include <hurd/hurd_types.h>

/* declaration of global config parapeters */
extern char *gopherfs_root_server;
extern unsigned short gopherfs_root_port;
extern char *gopherfs_server_dir;

extern int debug_flag;
extern volatile struct mapped_time_value *gopherfs_maptime;


/* handle all initial parameter parsing */
error_t gopherfs_parse_args (int argc, char **argv);

/* private data per `struct node' */
struct netnode
{
  char *name;
  char *selector;
  char *server;
  unsigned short port;
  enum
  {
    GPHR_FILE = '0',		/* Item is a file */
    GPHR_DIR = '1',		/* Item is a directory */
    GPHR_CSOPH = '2',		/* Item is a CSO phone-book server */
    GPHR_ERROR = '3',		/* Error */
    GPHR_BINHEX = '4',		/* Item is a BinHexed Macintosh file */
    GPHR_DOSBIN = '5',		/* Item is DOS binary archive of some sort */
    GPHR_UUENC = '6',		/* Item is a UNIX uuencoded file */
    GPHR_SEARCH = '7',		/* Item is an Index-Search server */
    GPHR_TELNET = '8',		/* Item points to a text-based telnet session */
    GPHR_BIN = '9'		/* Item is a binary file */
  }
  type;

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
};
/* global pointer to the filesystem */
extern struct gopherfs *gopherfs;

/* do a DNS lookup for NAME and store result in *ENT */
error_t lookup_host (char *name, struct hostent **ent);

/* store the remote socket in *FD after writing a gopher selector
   to it */
error_t open_selector (struct netnode *node, int *fd);

/* make an instance of `struct netnode' with the specified parameters,
   return NULL on error */
struct netnode *gopherfs_make_netnode (char type, char *name, char *selector,
				       char *server, unsigned short port);

/* fetch a directory node from the gopher server
   DIR should already be locked */
error_t fill_dirnode (struct netnode *dir);

/* free an instance of `struct netnode' */
void free_netnode (struct netnode *node);

/* make an instance of `struct node' with the specified parameters,
   return NULL on error */
struct node *gopherfs_make_node (char type, char *name, char *selector,
				 char *server, unsigned short port);

/* free an instance of `struct node' */
void free_node (struct node *node);

#endif /* __GOPHERFS_H__ */
