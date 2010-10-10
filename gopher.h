/* Gopher filesystem

   Copyright (C) 2010 Manuel Menal <mmenal@hurdfr.org>
   This file is part of the Gopherfs translator.

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

#ifndef __GOPHER_H__
#define __GOPHER_H__

#include "gopherfs.h"

struct gopher_entry
{
  enum
    {
      GPHR_FILE = '0',		/* Item is a file */
      GPHR_DIR = '1',		/* Item is a directory */
      GPHR_CSOPH = '2',		/* Item is a CSO phone-book server */
      GPHR_ERROR = '3',		/* Error */
      GPHR_BINHEX = '4',	/* Item is a BinHexed Macintosh file */
      GPHR_DOSBIN = '5',	/* Item is DOS binary archive of some sort */
      GPHR_UUENC = '6',		/* Item is a UNIX uuencoded file */
      GPHR_SEARCH = '7',	/* Item is an Index-Search server */
      GPHR_TELNET = '8',        /* Item points to a text-based telnet session */
      GPHR_BIN = '9',	        /* Item is a binary file */
      GPHR_GIF = 'g',           /* Item is a GIF image */
      GPHR_HTML = 'h',          /* Item is a HTML file */
      GPHR_INFO = 'i'           /* Item is an informational message */
  } type;

  char *name;
  char *selector;
  char *server;
  unsigned short port;

  struct gopher_entry *prev, *next;
};

/* Do a DNS lookup for NAME:PORT and store result in PARAMS and error in H_ERR */
error_t lookup_host (char *name, unsigned short port, struct addrinfo **addr);

/* List all entries from ENTRY and store them in *MAP. */
error_t gopher_list_entries (struct gopher_entry *entry, struct gopher_entry **map);

/* open ENTRY and store the remote socket in *FD */
error_t gopher_open (struct gopher_entry *entry, int *fd);

#endif /* __GOPHER_H__ */

