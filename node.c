/* netio - creates socket ports via the filesystem
   Copyright (C) 2001, 02 Moritz Schulte <moritz@duesseldorf.ccc.de>
 
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
   USA */

#include <hurd/netfs.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "netio.h"
#include "lib.h"

/* Create a new node in the directory *DIR (if DIR is nonzero) and
   store it in *NODE.  If CONNECT is true (in which case *DIR has to
   be a valid directory node), attach the new node to the list of
   directory entries of *DIR.  Return 0 on success or an error code.  */
error_t
node_make_new (struct node *dir, int connect, struct node **node)
{
  struct netnode *netnode;
  error_t err;

  err = my_malloc (sizeof (struct netnode), (void **) &netnode);
  if (err)
    return err;
  *node = netfs_make_node (netnode);
  if (! *node)
    {
      free (netnode);
      return ENOMEM;
    }
  (*node)->nn->flags = 0;
  (*node)->nn->host = 0;
  (*node)->nn->port = 0;
  (*node)->nn->protocol = 0;
  (*node)->nn->sock = MACH_PORT_NULL;
  (*node)->nn->addr = MACH_PORT_NULL;
  (*node)->nn->connected = 0;
  (*node)->nn->entries = 0;
  (*node)->nn->dir = dir;
  if (dir)
    netfs_nref (dir);

  if (connect)
    {
      (*node)->next = dir->nn->entries;
      (*node)->prevp = &dir->nn->entries;
      if ((*node)->next)
	(*node)->next->prevp = &(*node)->next;
      dir->nn->entries = (*node);
    }
  else
    {
      (*node)->next = 0;
      (*node)->prevp = 0;
    }
  return err;
}

/* Destroy the node and release a reference to the parent directory.  */
error_t
node_destroy (struct node *np)
{
  if (np->nn->flags & PORT_NODE)
    {
      if (np->nn->connected)
	socket_shutdown (np->nn->sock, 2);
    }
  else if (np->nn->flags & HOST_NODE)
    free (np->nn->host);
  else if (np->nn->flags & PROTOCOL_NODE)
    {
      free (np->nn->protocol);
      *np->prevp = np->next;
      if (np->next)
	np->next->prevp = np->prevp;
    }
  assert (np->nn->dir);
  spin_unlock (&netfs_node_refcnt_lock); /* FIXME?  Is this locking
                                            okay?  */
  netfs_nput (np->nn->dir);
  spin_lock (&netfs_node_refcnt_lock);
  free (np->nn);
  free (np);
  return 0;
}

/* Create a new protocol node for *PROTOCOL in the directory *DIR.
   Return 0 on success or an error code.  */
error_t
node_make_protocol_node (struct node *dir, struct protocol *protocol)
{
  struct node *node;
  error_t err;
  err = node_make_new (dir, 1, &node);
  if (err)
    return err;
  node->nn->flags |= PROTOCOL_NODE;
  node->nn->protocol = protocol;
  node->nn_stat.st_mode = S_IFDIR
    | S_IRUSR | S_IXUSR 
    | S_IRGRP | S_IXGRP
    | S_IROTH | S_IXOTH;
  node->nn_stat.st_ino = protocol->id;
  node->nn_stat.st_dev = netfs_root_node->nn_stat.st_dev;
  node->nn_stat.st_uid = netfs_root_node->nn_stat.st_uid;
  node->nn_stat.st_gid = netfs_root_node->nn_stat.st_gid;
  node->nn_stat.st_size = 0; /* ?  */
  node->nn_stat.st_blocks = 0;
  node->nn_stat.st_blksize = 0;
  fshelp_touch (&node->nn_stat, TOUCH_ATIME | TOUCH_CTIME | TOUCH_MTIME,
		netio_maptime);
  return err;
}

/* Create a new host node for HOST in the directory *DIR and store it
   in *NODE.  Return 0 on success or an error code.  */
error_t
node_make_host_node (struct iouser *user, struct node *dir, char *host,
		     struct node **node)
{
  struct node *np;
  error_t err;
  err = node_make_new (dir, 0, &np);
  if (err)
    return err;
  np->nn->host = strdup (host);
  if (! np->nn->host)
    {
      netfs_nrele (np);
      err = ENOMEM;
      goto out;
    }
  np->nn->flags |= HOST_NODE;
  np->nn->protocol = dir->nn->protocol;
  np->nn_stat.st_mode = S_IFDIR | S_IRUSR | S_IXUSR ;
  np->nn_stat.st_ino = 1; /* ?  */
  np->nn_stat.st_dev = netfs_root_node->nn_stat.st_dev;
  np->nn_stat.st_uid = *user->uids->ids;
  np->nn_stat.st_gid = *user->gids->ids;
  np->nn_stat.st_size = 0; /* ?  */
  np->nn_stat.st_blocks = 0;
  np->nn_stat.st_blksize = 0;
  fshelp_touch (&np->nn_stat, TOUCH_ATIME | TOUCH_CTIME | TOUCH_MTIME,
		netio_maptime);
  *node = np;

 out:
  return err;
}

/* Create a new port node for PORT in the directory *DIR and store it
   in *NODE.  Return 0 on success or an error code.  */
error_t
node_make_port_node (struct iouser *user, struct node *dir,
		     char *port, struct node **node)
{
  struct node *np;
  error_t err;
  err = node_make_new (dir, 0, &np);
  if (err)
    return err;
  np->nn->flags |= PORT_NODE;
  np->nn->host = dir->nn->host;
  np->nn->protocol = dir->nn->protocol;
  np->nn->port = (uint16_t) strtol (port, 0, 10);
  np->nn->flags |= HOST_NODE;
  np->nn->protocol = dir->nn->protocol;
  np->nn_stat.st_mode = S_IFSOCK | S_IRUSR | S_IWUSR ;
  np->nn_stat.st_ino = 1; /* ? */
  np->nn_stat.st_dev = netfs_root_node->nn_stat.st_dev;
  np->nn_stat.st_uid = *user->uids->ids;
  np->nn_stat.st_gid = *user->gids->ids;
  np->nn_stat.st_size = 0; /* ?  */
  np->nn_stat.st_blocks = 0;
  np->nn_stat.st_blksize = 0;
  fshelp_touch (&np->nn_stat, TOUCH_ATIME | TOUCH_CTIME | TOUCH_MTIME,
		netio_maptime);
  *node = np;
  return err;
}

/* Create the root node.  Return 0 on success or an error code.  */
error_t
node_make_root_node (struct node **node)
{
  struct node *np;
  error_t err;
  err = node_make_new (0, 0, &np);
  if (err)
    goto out;
  np->nn->flags |= ROOT_NODE;
  *node = np;
 out:
  return err;
}

/* Connect the socket of the node *NP.  Return 0 on success or an
   error code.  */
error_t
node_connect_socket (struct node *np)
{
  error_t err;
  err = (*np->nn->protocol->socket_open) (np);
  if (! err)
    np->nn->connected = 1;
  return err;
}
