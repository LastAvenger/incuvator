/* The hurd io interface to channelio

   Copyright (C) 1995, 1996, 1997, 1999, 2000, 2002, 2007
     Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.org>
   Reworked for channelio by Carl Fredrik Hammar <hammy.lite@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#include <hurd/trivfs.h>
#include <stdio.h>
#include <fcntl.h>

#include "node.h"

/* Read data from an IO object.  If offset if -1, read from the object
   maintained file pointer.  If the object is not seekable, offset is
   ignored.  The amount desired to be read is in AMOUNT.  */
error_t
trivfs_S_io_read (struct trivfs_protid *cred,
		  mach_port_t reply, mach_msg_type_name_t reply_type,
		  char **data, mach_msg_type_number_t *data_len,
		  loff_t offs, mach_msg_type_number_t amount)
{
  struct open *open;
  error_t err;

  if (! cred)
    return EOPNOTSUPP;

  if (! (cred->po->openmodes & O_READ))
    return EBADF;

  open = cred->po->hook;
  mutex_lock (&open->lock);
  err = channel_read (open->channel, amount, (void **)data, data_len);
  mutex_unlock (&open->lock);

  return err;
}

/* Tell how much data can be read from the object without blocking for
   a "long time" (this should be the same meaning of "long time" used
   by the nonblocking flag.  */
error_t
trivfs_S_io_readable (struct trivfs_protid *cred,
		      mach_port_t reply, mach_msg_type_name_t reply_type,
		      mach_msg_type_number_t *amount)
{
  struct channel *channel;

  if (! cred)
    return EOPNOTSUPP;

  if (! (cred->po->openmodes & O_READ))
    return EBADF;

  channel = ((struct open *) cred->po->hook)->channel;
  if (channel->flags & CHANNEL_WRITEONLY)
    return EBADF;

  /* XXX either this, highest possible value or extend libchannel to
     haldle it.  */
  *amount = 0;
  return 0;
}

/* Write data to an IO object.  If offset is -1, write at the object
   maintained file pointer.  If the object is not seekable, offset is
   ignored.  The amount successfully written is returned in amount.  A
   given user should not have more than one outstanding io_write on an
   object at a time; servers implement congestion control by delaying
   responses to io_write.  Servers may drop data (returning ENOBUFS)
   if they recevie more than one write when not prepared for it.  */
error_t
trivfs_S_io_write (struct trivfs_protid *cred,
		   mach_port_t reply, mach_msg_type_name_t reply_type,
		   char *data, mach_msg_type_number_t data_len,
		   loff_t offs, mach_msg_type_number_t *amount)
{
  struct open *open;
  error_t err;

  if (! cred)
    return EOPNOTSUPP;

  if (! (cred->po->openmodes & O_WRITE))
    return EBADF;

  open = cred->po->hook;
  mutex_lock (&open->lock);
  err = channel_write (open->channel, data, data_len, amount);
  mutex_unlock (&open->lock);

  return err;
}

/* SELECT_TYPE is the bitwise OR of SELECT_READ, SELECT_WRITE, and SELECT_URG.
   Block until one of the indicated types of i/o can be done "quickly", and
   return the types that are then available.  */
error_t
trivfs_S_io_select (struct trivfs_protid *cred,
		    mach_port_t reply, mach_msg_type_name_t reply_type,
		    int *type)
{
  if (! cred)
    return EOPNOTSUPP;

  *type &= ~SELECT_URG;
  return 0;
}

/* Change current read/write offset.  */
error_t
trivfs_S_io_seek (struct trivfs_protid *cred,
		  mach_port_t reply, mach_msg_type_name_t reply_type,
		  off_t offs, int whence, off_t *new_offs)
{
  if (! cred)
    return EOPNOTSUPP;
  else
    return ESPIPE;
}

/* Truncate file.  */
error_t
trivfs_S_file_set_size (struct trivfs_protid *cred,
			mach_port_t reply, mach_msg_type_name_t reply_type,
			off_t size)
{
  if (! cred)
    return EOPNOTSUPP;
  else if (!(cred->po->openmodes & O_WRITE))
    return EBADF;
  else
    return 0;
}

/* These four routines modify the O_APPEND, O_ASYNC, O_FSYNC, and
   O_NONBLOCK bits for the IO object. In addition, io_get_openmodes
   will tell you which of O_READ, O_WRITE, and O_EXEC the object can
   be used for.  The O_ASYNC bit affects icky async I/O; good async
   I/O is done through io_async which is orthogonal to these calls. */

error_t
trivfs_S_io_get_openmodes (struct trivfs_protid *cred,
			   mach_port_t reply, mach_msg_type_name_t reply_type,
			   int *bits)
{
  if (! cred)
    return EOPNOTSUPP;

  *bits = cred->po->openmodes;
  return 0;
}

error_t
trivfs_S_io_set_all_openmodes (struct trivfs_protid *cred,
			       mach_port_t reply,
			       mach_msg_type_name_t reply_type,
			       int mode)
{
  if (! cred)
    return EOPNOTSUPP;
  else
    return 0;
}

error_t
trivfs_S_io_set_some_openmodes (struct trivfs_protid *cred,
				mach_port_t reply,
				mach_msg_type_name_t reply_type,
				int bits)
{
  if (! cred)
    return EOPNOTSUPP;
  else
    return 0;
}

error_t
trivfs_S_io_clear_some_openmodes (struct trivfs_protid *cred,
				  mach_port_t reply,
				  mach_msg_type_name_t reply_type,
				  int bits)
{
  if (! cred)
    return EOPNOTSUPP;
  else
    return 0;
}

/* Get/set the owner of the IO object.  For terminals, this affects
   controlling terminal behavior (see term_become_ctty).  For all
   objects this affects old-style async IO.  Negative values represent
   pgrps.  This has nothing to do with the owner of a file (as
   returned by io_stat, and as used for various permission checks by
   filesystems).  An owner of 0 indicates that there is no owner.  */
error_t
trivfs_S_io_get_owner (struct trivfs_protid *cred,
		       mach_port_t reply,
		       mach_msg_type_name_t reply_type,
		       pid_t *owner)
{
  struct open *open;

  if (! cred)
    return EOPNOTSUPP;

  open = cred->po->hook;
  mutex_lock (&open->lock);
  *owner = open->owner;
  mutex_unlock (&open->lock);

  return 0;
}

error_t
trivfs_S_io_mod_owner (struct trivfs_protid *cred,
		       mach_port_t reply, mach_msg_type_name_t reply_type,
		       pid_t owner)
{
  struct open *open;

  if (! cred)
    return EOPNOTSUPP;

  open = cred->po->hook;
  mutex_lock (&open->lock);
  open->owner = owner;
  mutex_unlock (&open->lock);

  return 0;
}

/* File syncing operations; these all do the same thing, sync the underlying
   device.  */

error_t
trivfs_S_file_sync (struct trivfs_protid *cred,
		    mach_port_t reply, mach_msg_type_name_t reply_type,
		    int wait, int omit_metadata)
{
  struct open *open;
  error_t err;

  if (! cred)
    return EOPNOTSUPP;

  open = cred->po->hook;
  mutex_lock (&open->lock);
  err = channel_flush (open->channel);
  mutex_unlock (&open->lock);

  return err;
}

error_t
trivfs_S_file_syncfs (struct trivfs_protid *cred,
		      mach_port_t reply, mach_msg_type_name_t reply_type,
		      int wait, int dochildren)
{
  struct open *open;
  error_t err;

  if (! cred)
    return EOPNOTSUPP;

  open = cred->po->hook;
  mutex_lock (&open->lock);
  err = channel_flush (open->channel);
  mutex_unlock (&open->lock);

  return err;
}

error_t
trivfs_S_file_get_channel_info (struct trivfs_protid *cred,
				mach_port_t reply,
				mach_msg_type_name_t reply_type,
				mach_port_t **ports,
				mach_msg_type_name_t *ports_type,
				mach_msg_type_number_t *num_ports,
				int **ints, mach_msg_type_number_t *num_ints,
				off_t **offsets,
				mach_msg_type_number_t *num_offsets,
				char **data, mach_msg_type_number_t *data_len)
{
  /* XXX implement.  */
  return EOPNOTSUPP;
}
