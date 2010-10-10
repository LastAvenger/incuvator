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
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "gopherfs.h"

#include <hurd/hurd_types.h>
#include <hurd/netfs.h>

/* do a DNS lookup for NAME:PORT and store results in ENT and error in H_ERR */
error_t
lookup_host (char *name, unsigned short port, struct addrinfo **ai)
{
  /* XXX: cache host lookups. */
  struct addrinfo hints;
  char *service;
  error_t err;

  memset (&hints, 0, sizeof(hints));
  hints.ai_family = PF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_IP;
  hints.ai_flags = AI_NUMERICSERV;

  err = asprintf (&service, "%hu", port);
  if (err == -1)
    return ENOMEM;

  err = getaddrinfo (name, service, &hints, ai);

  free (service);

  return err;
}

/* open ENTRY and store the remote socket in *FD */
error_t
gopher_open (struct gopher_entry *entry, int *fd)
{
  struct addrinfo *addr;
  error_t err;
  ssize_t written;
  size_t towrite;
  
  *fd = socket (PF_INET, SOCK_STREAM, 0);
  if (*fd < 0)
    {
      debug ("failed to open socket (errno: %s)", strerror(errno));
      return errno;
    }

  err = lookup_host (entry->server, entry->port, &addr);
  if (err)
    {
      debug ("looking up host failed: %s", gai_strerror(err));
      return err;
    }

  /* XXX: cache host lookups. */
  do
    {
      debug ("connecting to %s:%hu...", 
	     inet_ntoa(((struct sockaddr_in *)addr->ai_addr)->sin_addr), 
	     ntohs(((struct sockaddr_in *)addr->ai_addr)->sin_port));

      err = connect (*fd, (struct sockaddr_in *) addr->ai_addr, addr->ai_addrlen);
      if (err)
	debug ("connect() failed! errno: %s", strerror(errno));

      addr = addr->ai_next;
    }
  while (addr && err);

  freeaddrinfo(addr);
  if (err)
    {
      close (*fd);
      return errno;
    }

  /* Write selector to *FD. */

  towrite = strlen (entry->selector);
  /* guard against EINTR failures */
  written = TEMP_FAILURE_RETRY (write (*fd, entry->selector, towrite));
  written += TEMP_FAILURE_RETRY (write (*fd, "\r\n", 2));
  if (written == -1 || written < (towrite + 2))
    return errno;

  return 0;
}

/* List all entries from ENTRY and store them in *MAP. */
error_t
gopher_list_entries (struct gopher_entry *entry, struct gopher_entry **map)
{
  FILE *sel;
  char *line = NULL;
  size_t line_len = 0;
  struct gopher_entry *prev = NULL;
  error_t err = 0;

  {
    int fd;
    err = gopher_open (entry, &fd);
    if (err)
	return err;

    sel = fdopen (fd, "r");
    if (!sel)
      {
	close (fd);
	return errno;
      }
  }
  
  while (getline (&line, &line_len, sel) >= 0)
    {
      /* Parse gopher entry. */
      struct gopher_entry *cur;
      char *tmp;

      debug ("%s", line);

      if (*line == '.' || err)
	break;

      cur = malloc (sizeof (struct gopher_entry));
      if (!cur)
	{
	 err = ENOMEM;
	 break;
	}

      cur->type = line[0];
      tmp = line + 1;
      cur->name = strdup(strsep (&tmp, "\t"));
      cur->selector = strdup(strsep (&tmp, "\t"));
      cur->server = strdup(strsep (&tmp, "\t"));
      cur->port = (unsigned short) atoi (strsep (&tmp, "\t"));

      if (!*map)
	/* First item. */
	*map = cur;
      else
	prev->next = cur;

      cur->prev = prev;
      cur->next = NULL;
      prev = cur;
    }

  free (line);
  fclose (sel);

  return err;
}
