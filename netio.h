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

#include <hurd/socket.h>
#include <stdint.h>
#include <maptime.h>

#define NETIO_VERSION "0.3cvs"

/* The supported protocols.  */
#define PROTOCOL_ID_TCP 0x00000001
#define PROTOCOL_ID_UDP 0x00000002
#define PROTOCOL_NAME_TCP "tcp"
#define PROTOCOL_NAME_UDP "udp"

/* Type for functions, which connect a node socket.  */
typedef error_t (*socket_open_func_t) (struct node *node);

/* One of these items per supported protocol.  */
typedef struct protocol
{
  int id;
  char *name;
  socket_open_func_t socket_open_func;
} protocol_t;

/* The different kinds of nodes managed by netio.  */
#define ROOT_NODE     0x00000001
#define PROTOCOL_NODE 0x00000002
#define HOST_NODE     0x00000004
#define PORT_NODE     0x00000008

struct netnode
{
  unsigned short int flags; /* Either ROOT_NODE, PROTOCOL_NODE,
			       HOST_NODE or PORT_NODE.  */
  char *host;			/* The host to connect to.  */
  uint16_t port;		/* The port to connect to.  */
  struct protocol *protocol;	/* The protocol information.  */
  socket_t sock;		/* Our socket port.  */
  addr_port_t addr;		/* Our socket address port.  */
  unsigned short int connected;	/* non-zero, if the socket is
				   connected.  */
  struct node *entries;		/* The entries in this directory, only
				   useful for the root node.  */
  struct node *dir;		/* The parent directory.  */
};

/* Used for touching the [acm]times of our nodes.  */
extern volatile struct mapped_time_value *netio_maptime;
