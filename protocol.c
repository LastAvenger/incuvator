/* netio - creates socket ports via the filesystem
   Copyright (C) 2001, 02 Free Software Foundation, Inc.
   Written by Moritz Schulte.
 
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
#include <hurd/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "netio.h"
#include "lib.h"
#include "node.h"

static error_t protocol_socket_open_TCP (struct node *node);
static error_t protocol_socket_open_UDP (struct node *node);

#define PROTOCOL_INIT(p) { PROTOCOL_ID_##p, PROTOCOL_NAME_##p, \
                           protocol_socket_open_##p }

static protocol_t protocols[] = { PROTOCOL_INIT (TCP),
				  PROTOCOL_INIT (UDP),
				  { 0, NULL, NULL } };

static error_t protocol_find_by_name (char *name, protocol_t **protocol);

/* Connect the node *NODE with style STYLE (SOCK_STREAM or
   SOCK_DGRAM).  Used by protocol_socket_open_{tcp,udp}.  Return 0 on
   success or an error code.  */
static error_t
protocol_socket_open_std (struct node *node, int style)
{
  extern pf_t socket_server;
  socket_t sock;
  addr_port_t aport;
  error_t err;
  struct sockaddr_in addr;
  struct hostent *hostinfo;

  hostinfo = gethostbyname (node->nn->host);
  if (hostinfo == NULL)
    {
      err = h_errno;
      goto out;
    }

  if ((err = socket_create (socket_server, style, 0, &sock)))
    goto out;
  addr.sin_family = AF_INET;
  addr.sin_addr = *(struct in_addr *) hostinfo->h_addr;
  addr.sin_port = htons (node->nn->port);
  if ((err = socket_create_address (sock, addr.sin_family,
				    (char *) &addr, sizeof (addr), &aport)))
    goto out;
  if ((err = socket_connect (sock, aport)))
    goto out;

  node->nn->sock = sock;
  node->nn->addr = aport;

  /* FIXME:  cleanup missing?  */

 out:
  return err;
}

/* Open a TCP socket for *NODE.  Return 0 on success or an error code.  */
static error_t
protocol_socket_open_TCP (struct node *node)
{
  return protocol_socket_open_std (node, SOCK_STREAM);
}

/* Open a UDP socket for *NODE.  Return 0 on success or an error code.  */
static error_t
protocol_socket_open_UDP (struct node *node)
{
  return protocol_socket_open_std (node, SOCK_DGRAM);
}

/* Store the protocol node for the protocol specified by NAME in
   *NODE.  Return 0 on success or ENOENT if the node could not be
   found.  */
error_t
protocol_find_node (char *name, struct node **node)
{
  struct node *np;
  error_t err = ENOENT;

  for (np = netfs_root_node->nn->entries;
       np && strcmp (np->nn->protocol->name, name);
       np = np->next);
  if (np)
    {
      *node = np;
      err = 0;
    }
  return err;
}

/* Register a protocol specified by ID and NAME, creating an according
   node.  Sockets for that protocol get opened by SOCKET_OPEN_FUNC.
   Return 0 on success or an error code.  */
error_t
protocol_register (char *name)
{
  error_t err;
  protocol_t *protocol;

  err = protocol_find_by_name (name, &protocol);
  if (err)
    return EOPNOTSUPP;		/* No protocol definition by that name
				   found.  */
  err = node_make_protocol_node (netfs_root_node, protocol);
  if (err)
    return err;

  return err;
}

/* Unregister the protocol specified by NAME, drop the reference to
   the protocol node.  Return 0 on success or ENOENT if that protocol
   could not be found.  */
error_t
protocol_unregister (char *name)
{
  struct node *np;
  error_t err;

  err = protocol_find_node (name, &np);
  if (err)
    return err;
  netfs_nrele (np);
  return 0;
}

/* Register the protocols - create according nodes.  Return 0 on
   success or an error code.  */
error_t
protocol_register_default (void)
{
  protocol_t *p;
  error_t err = 0;

  for (p = protocols; ! err && p->name; p++)
    err = protocol_register (p->name);

  return err;
}

/* Lookup the protocol information for NAME and store them in
   *PROTOCOL.  Return zero on success or ENOENT if the protocol for
   NAME could not be found.  */
static error_t
protocol_find_by_name (char *name, protocol_t **protocol)
{
  protocol_t *p;
  error_t err = 0;

  for (p = protocols; p->name && strcmp (p->name, name); p++);

  if (p->name)
    *protocol = p;
  else
    err = ENOENT;

  return err;
}
