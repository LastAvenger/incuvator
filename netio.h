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

#include <hurd/socket.h>
#include <stdint.h>
#include <maptime.h>

#define NETIO_VERSION "0.1"

#define OPENONLY_STATE_MODES (O_CREAT|O_EXCL|O_NOLINK|O_NOTRANS|O_NONBLOCK)

#define PROTOCOL_ID_TCP 0x00000001
#define PROTOCOL_ID_UDP 0x00000002

struct protocol
{
  char *name;
  int id;
  error_t (*socket_open)  (struct node *np);
};

#define ROOT_NODE     0x00000001
#define PROTOCOL_NODE 0x00000002
#define HOST_NODE     0x00000004
#define PORT_NODE     0x00000008

struct netnode
{
  unsigned short int flags; /* Either ROOT_NODE, PROTOCOL_NODE,
			       HOST_NODE or PORT_NODE.  */
  char *host;
  uint16_t port;
  struct protocol *protocol;
  socket_t sock;
  addr_port_t addr;
  unsigned short int connected;
  struct node *entries;
  struct node *dir;
};

extern volatile struct mapped_time_value *netio_maptime;
