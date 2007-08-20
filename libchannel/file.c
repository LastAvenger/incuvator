/* File channel backend.

   Copyright (C) 1995, 1996, 1997, 1998, 2001, 2002, 2007
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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <hurd.h>

#include <hurd/io.h>
#include <hurd/fs.h>

#include "channel.h"

static error_t
file_read (struct channel *channel, size_t amount, void **buf, size_t *len)
{
  return io_read ((mach_port_t)channel->class_hook,
		  (char **)buf, len, -1, amount);
}

static error_t
file_write (struct channel *channel, void *buf, size_t len,
	    size_t *amount)
{
  return io_write ((mach_port_t)channel->class_hook, buf, len, -1, amount);
}

static error_t
file_flush (struct channel *channel)
{
  return file_sync ((mach_port_t)channel->class_hook, 0, 1);
}

static error_t
file_open (struct channel *channel, int flags)
{
  struct channel_hub *hub = channel->hub;
  file_t file;
  error_t err;

  if (flags & CHANNEL_READONLY)
    file = file_name_lookup (hub->name, O_READ, 0);
  else if (flags & CHANNEL_WRITEONLY)
    file = file_name_lookup (hub->name, O_WRITE, 0);
  else
    {
      file = file_name_lookup (hub->name, O_RDWR, 0);
      if (file == MACH_PORT_NULL
	  && (errno == EACCES || errno == EROFS))
	{
	  file = file_name_lookup (hub->name, O_READ, 0);
	  if (file != MACH_PORT_NULL)
	    flags |= CHANNEL_READONLY;
	}

      if (file == MACH_PORT_NULL
	  && (errno == EACCES || errno == EROFS))
	{
	  file = file_name_lookup (hub->name, O_WRITE, 0);
	  if (file != MACH_PORT_NULL)
	    flags |= CHANNEL_WRITEONLY;
	}
    }

  err = file == MACH_PORT_NULL ? errno : 0;
  if (err)
    return err;

  channel->flags = flags;
  channel->class_hook = (void*) file;
  return 0;
}

static void
file_close (struct channel *channel)
{
  mach_port_deallocate (mach_task_self (),
			(mach_port_t)channel->class_hook);
}

/* XXX implement control_demuxer.  */


const struct channel_class
channel_file_class =
{
  name: "file",
  read: file_read,
  write: file_write,
  flush: file_flush,
  open: file_open, 
  close: file_close,
  create_hub: channel_create_file_hub
};
CHANNEL_STD_CLASS (file);

/* Create and return in HUB a hub that creates channels that does file i/o
   with the file specified by NAME.  */
error_t
channel_create_file_hub (const char *name, int flags,
			 const struct channel_class *const *classes,
			 struct channel_hub **hub)
{
  return channel_alloc_hub (&channel_file_class, name, flags, hub);
}
