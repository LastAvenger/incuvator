/* Creating a channel hub from a file name.

   Copyright (C) 1996, 1997, 1998, 2001, 2002, 2007
     Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.org>
   Reworked for libchannel by Carl Fredrik Hammar <hammy.lite@gmail.com>

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111, USA. */

#include <fcntl.h>
#include <hurd.h>

#include "channel.h"

/* Open the file NAME and return a new hub in HUB, which is either a copy
   of the file's underlying hub or a hub using it through file io, unless
   CHANNEL_NO_FILEIO flag is given.  The class of HUB is found in CLASSES
   or CHANNEL_STD_CLASSES, if CLASSES is null.  FLAGS is set with
   channel_set_hub_flags.  Keeps the SOURCE reference, which may be closed
   using channel_close_hub_source.  */
error_t
channel_create_query_hub (const char *name, int flags,
			  const struct channel_class *const *classes,
			  struct channel_hub **hub)
{
  error_t err;
  file_t node = file_name_lookup (name, O_READ, 0);

  if (node == MACH_PORT_NULL)
    return errno;

  err = channel_fetch_hub (node, flags, classes, hub);
  if (! err)
    return 0;

  mach_port_deallocate (mach_task_self (), node);

  if (flags & CHANNEL_NO_FILEIO)
    return err;

  /* Try making a hub that does file io to NODE.  */
  return channel_create_file_hub (name, flags, classes, hub);
}

const struct channel_class
channel_query_class = { name: "query", create_hub: channel_create_query_hub };
CHANNEL_STD_CLASS (query);
