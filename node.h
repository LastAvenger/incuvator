/*---------------------------------------------------------------------------*/
/*node.h*/
/*---------------------------------------------------------------------------*/
/*Node management. Also see lnode.h*/
/*---------------------------------------------------------------------------*/
/*Based on the code of unionfs translator.*/
/*---------------------------------------------------------------------------*/
/*Copyright (C) 2001, 2002, 2005, 2008, 2009 Free Software Foundation,
  Inc.  Written by Sergiu Ivanov <unlimitedscolobb@gmail.com>.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or * (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.*/
/*---------------------------------------------------------------------------*/
#ifndef __NODE_H__
#define __NODE_H__

/*---------------------------------------------------------------------------*/
#include <error.h>
#include <sys/stat.h>
#include <hurd/netfs.h>
/*---------------------------------------------------------------------------*/
#include "lnode.h"
#include "trans.h"
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*--------Macros-------------------------------------------------------------*/
/*Checks whether the give node is the root of the proxy filesystem*/
#define NODE_IS_ROOT(n) (((n)->nn->lnode && (n)->nn->lnode->dir) ? (0) : (1))
/*---------------------------------------------------------------------------*/
/*Node flags*/
#define FLAG_NODE_ULFS_FIXED    0x00000001 /*this node should not be updated */
#define FLAG_NODE_INVALIDATE    0x00000002 /*this node must be updated */
#define FLAG_NODE_ULFS_UPTODATE	0x00000004 /*this node has just been updated */
/*---------------------------------------------------------------------------*/
/*Types of nodes */
#define NODE_TYPE_NORMAL	0
#define NODE_TYPE_PROXY		1
#define NODE_TYPE_SHADOW	2
/*---------------------------------------------------------------------------*/
/*The type of offset corresponding to the current platform*/
#ifdef __USE_FILE_OFFSET64
#	define OFFSET_T __off64_t
#else
#	define OFFSET_T __off_t
#endif /*__USE_FILE_OFFSET64*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*--------Types--------------------------------------------------------------*/
/*The user-defined node for libnetfs*/
struct netnode
{
  /*the reference to the corresponding light node */
  lnode_t *lnode;

  /*the flags associated with this node */
  int flags;

  /*the type of the current node */
  int type;

  /*a port to the underlying filesystem */
  file_t port;

  /*a reference to the element in the list of dynamic translators
    corresponding to the translator sitting on this node, in case this
    node is a shadow node */
  struct trans_el * dyntrans;

  /*the reference to the shadow node that is below the current shadow
    node in the dynamic translator stack */
  node_t * below;

  /*the neighbouring entries in the cache */
  node_t *ncache_prev, *ncache_next;
};				/*struct netnode */
/*---------------------------------------------------------------------------*/
typedef struct netnode netnode_t;
/*---------------------------------------------------------------------------*/
/*A list element containing directory entry*/
struct node_dirent
{
  /*the directory entry */
  struct dirent *dirent;

  /*the next element */
  struct node_dirent *next;
};				/*struct node_dirent */
/*---------------------------------------------------------------------------*/
typedef struct node_dirent node_dirent_t;
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*--------Global Variables---------------------------------------------------*/
/*The lock protecting the underlying filesystem*/
extern pthread_mutex_t ulfs_lock;
/*---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions----------------------------------------------------------*/
/*Derives a new node from `lnode` and adds a reference to `lnode`*/
error_t node_create (lnode_t * lnode, node_t ** node);
/*---------------------------------------------------------------------------*/
/*Derives a new proxy from `lnode`*/
error_t node_create_proxy (lnode_t * lnode, node_t ** node);
/*---------------------------------------------------------------------------*/
/*Creates a proxy (or a shadow) node for the supplied port*/
error_t node_create_from_port (mach_port_t port, node_t ** node);
/*---------------------------------------------------------------------------*/
/*Destroys the specified node and removes a light reference from the
  associated light node*/
void node_destroy (node_t * np);
/*---------------------------------------------------------------------------*/
/*Creates the root node and the corresponding lnode*/
error_t node_create_root (node_t ** root_node);
/*---------------------------------------------------------------------------*/
/*Initializes the port to the underlying filesystem for the root node*/
error_t node_init_root (node_t * node);
/*---------------------------------------------------------------------------*/
/*Frees a list of dirents*/
void node_entries_free (node_dirent_t * dirents);
/*---------------------------------------------------------------------------*/
/*Reads the directory entries from `node`, which must be locked*/
error_t node_entries_get (node_t * node, node_dirent_t ** dirents);
/*---------------------------------------------------------------------------*/
/*Makes sure that all ports to the underlying filesystem of `node` are
  up to date*/
error_t node_update (node_t * node);
/*---------------------------------------------------------------------------*/
/*Computes the size of the given directory*/
error_t node_get_size (node_t * dir, OFFSET_T * off);
/*---------------------------------------------------------------------------*/
/*Remove the file called `name` under `dir`*/
error_t node_unlink_file (node_t * dir, char *name);
/*---------------------------------------------------------------------------*/
/*Starts translator `trans` on the (shadow) node `np`, which should
  mirror the file `filename`, and returns the port `port` to the root
  of the translator opened as `flags.`*/
error_t
  node_set_translator
  (struct protid *diruser, node_t * np, char * trans, int flags,
   char * filename, mach_port_t * port);
/*---------------------------------------------------------------------------*/
/*Gets the port to the supplied node. */
error_t
  node_get_port
  (struct protid * diruser, node_t * np, int flags, mach_port_t * port);
/*---------------------------------------------------------------------------*/
/*Gets the send port right to the supplied node. */
error_t
  node_get_send_port
  (struct protid * diruser, node_t * np, int flags, mach_port_t * port);
/*---------------------------------------------------------------------------*/
#endif /*__NODE_H__*/
