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

#include <errno.h>
#include <hurd/netfs.h>
#include <hurd/iohelp.h>

error_t node_make_new (struct node *dir, int connect, struct node **node);
error_t node_make_protocol_node (struct node *dir,
				 struct protocol *protocol);
error_t node_make_host_node (struct iouser *user, struct node *dir,
			     char *host, struct node **node);
error_t node_make_port_node (struct iouser *user, struct node *dir,
			     char *port, struct node **node);
error_t node_make_root_node (struct node **the_node);
error_t node_connect_socket (struct node *np);
error_t node_destroy (struct node *np);

