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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hurd.h>
#include <hurd/socket.h>
#include <hurd/paths.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <error.h>

#include "lib.h"

/* Try to alloc SIZE bytes into *MEM.  Return ENOMEM if malloc()
    failed.  */
error_t
my_malloc (size_t size, void **mem)
{
  *mem = malloc (size);
  if (! *mem)
    return ENOMEM;
  return 0;
}

/* Open a port to the socket server for the protocol family number NO
   and store it in *SOCK.  Return 0 on success or an error code.  */
error_t
open_socket_server (int no, pf_t *sock)
{
  char *path;
  error_t err;
  pf_t port;
  err = asprintf (&path, "%s/%i", _SERVERS_SOCKET, no);
  if (err < 0)
    return ENOMEM;
  err = 0;
  port = file_name_lookup (path, 0, 0);
  free (path);
  if (port == MACH_PORT_NULL)
    err = errno;
  else
    *sock = port;
  return err;
}
