/* Gopher protocol routines

   Copyright (C) 2002 James A. Morrison <ja2morri@uwaterloo.ca>
   Copyright (C) 2000 Igor Khavkine <igor@twu.net>
   This file is part of Gopherfs.

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

#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "gopherfs.h"

#include <hurd/hurd_types.h>
#include <hurd/netfs.h>

/* do a DNS lookup for NAME and store result in *ENT */
error_t
lookup_host (char *name, struct hostent **ent)
{
  error_t err;
  struct hostent hentbuf;
  int herr = 0;
  char *tmpbuf;
  size_t tmpbuflen = 512;

  tmpbuf = (char *) malloc (tmpbuflen);
  if (!tmpbuf)
    return ENOMEM;

  /* XXX: use getaddrinfo */
  while ((err = gethostbyname_r (name, &hentbuf, tmpbuf,
				 tmpbuflen, ent, &herr)) == ERANGE)
    {
      tmpbuflen *= 2;
      tmpbuf = (char *) realloc (tmpbuf, tmpbuflen);
      if (!tmpbuf)
	return ENOMEM;
    }
  free (tmpbuf);

  if (!herr)
    return EINVAL;

  return 0;
}

/* store the remote socket in *FD after writing a gopher selector
   to it */
error_t
open_selector (struct netnode * node, int *fd)
{
  error_t err;
  struct hostent *server_ent;
  struct sockaddr_in server;
  ssize_t written;
  size_t towrite;

  err = lookup_host (node->server, &server_ent);
  if (!err)
    return err;
  if (debug_flag)
    fprintf (stderr, "trying to open %s:%d/%s\n", node->server,
	     node->port, node->selector);

  server.sin_family = AF_INET;
  server.sin_port = htons (node->port);
  server.sin_addr = *(struct in_addr *) server_ent->h_addr;

  *fd = socket (PF_INET, SOCK_STREAM, 0);
  if (*fd == -1)
    return errno;

  err = connect (*fd, (struct sockaddr *) &server, sizeof (server));
  if (err == -1)
    return errno;

  towrite = strlen (node->selector);
  /* guard against EINTR failures */
  written = TEMP_FAILURE_RETRY (write (*fd, node->selector, towrite));
  written += TEMP_FAILURE_RETRY (write (*fd, "\r\n", 2));
  if (written == -1 || written < (towrite + 2))
    return errno;

  return 0;
}

/* fetch a directory node from the gopher server
   DIR should already be locked */
error_t
fill_dirnode (struct netnode * dir)
{
  error_t err = 0;
  FILE *sel;
  int sel_fd;
  char *line;
  size_t line_len;
  struct node *nd, **prevp;

  err = open_selector (dir, &sel_fd);
  if (err)
    return err;
  if (debug_flag)
    fprintf (stderr, "filling out dir %s\n", dir->name);
  errno = 0;
  sel = fdopen (sel_fd, "r");
  if (!sel)
    {
      close (sel_fd);
      return errno;
    }

  dir->noents = TRUE;
  dir->num_ents = 0;
  prevp = &dir->ents;
  line = NULL;
  line_len = 0;
  while (getline (&line, &line_len, sel) >= 0)
    {
      char type, *name, *selector, *server;
      unsigned short port;
      char *tok, *endtok;

      if (debug_flag)
	fprintf (stderr, "%s\n", line);
      if (*line == '.' || err)
	break;

      /* parse the gopher node description */
      type = *line;
      endtok = line + 1;
      name = strsep (&endtok, "\t");
      selector = strsep (&endtok, "\t");
      server = strsep (&endtok, "\t");
      port = (unsigned short) atoi (strsep (&endtok, "\t"));

      nd = gopherfs_make_node (type, name, selector, server, port);
      if (!nd)
	{
	  err = ENOMEM;
	  break;
	}
      *prevp = nd;
      nd->prevp = prevp;
      prevp = &nd->next;

      dir->num_ents++;
      if (dir->noents)
	dir->noents = FALSE;
    }
  free (line);

  if (err)
    {
      fclose (sel);
      return err;
    }

  return 0;
}
