/* netio - creates socket ports via the filesystem
   Copyright (C) 2002 Moritz Schulte <moritz@duesseldorf.ccc.de>
 
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

/* Connect the node *NODE with style STYLE (SOCK_STREAM or
   SOCK_DGRAM).  Used by protocol_socket_open_{tcp,udp}.  Return 0 on
   success or an error code.  */
error_t
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
error_t
protocol_socket_open_tcp (struct node *node)
{
  return protocol_socket_open_std (node, SOCK_STREAM);
}

/* Open a UDP socket for *NODE.  Return 0 on success or an error code.  */
error_t
protocol_socket_open_udp (struct node *node)
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
  for (np = netfs_root_node->nn->entries;
       np && strcmp (np->nn->protocol->name, name);
       np = np->next);
  if (! np)
    return ENOENT;
  *node = np;
  return 0;
}

/* Register a protocol specified by ID and NAME, creating an according
   node.  Sockets for that protocol get opened by SOCKET_OPEN_FUNC.
   Return 0 on success or an error code.  */
error_t
protocol_register (int id, char *name,
		   error_t (*socket_open_func) (struct node *node))
{
  error_t err;
  struct protocol *protocol;
  err = my_malloc (sizeof (struct protocol), (void **) &protocol);
  if (err)
    return err;
  protocol->name = strdup (name);
  if (! protocol->name)
    {
      free (protocol);
      return ENOMEM;
    }
  err = node_make_protocol_node (netfs_root_node, protocol);
  if (err)
    {
      free (protocol->name);
      free (protocol);
      return err;
    }
  protocol->id = id;
  protocol->socket_open = socket_open_func;
  return 0;
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
protocol_register_protocols (void)
{
  return (protocol_register (PROTOCOL_ID_TCP, "tcp",
			     protocol_socket_open_tcp)
	  || protocol_register (PROTOCOL_ID_UDP, "udp",
				protocol_socket_open_udp));
}
