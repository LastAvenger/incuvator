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

error_t protocol_register_protocols (void);
error_t protocol_register (int id, char *name,
			   error_t (*socket_open_func)  (struct node *node));
error_t protocol_unregister (char *name);;
error_t protocol_find_node (char *name, struct node **node);
