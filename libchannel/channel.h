/* Channel I/O

   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2001, 2002, 2004, 2005, 2007
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

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <mach.h>
#include <hurd/hurd_types.h>

struct channel
{
  int flags;
  const struct channel_class *class;
  void *hook;
};

/* Flags implemented by generic channel code.  */
#define CHANNEL_READONLY   0x1 /* No writing allowed. */
#define CHANNEL_WRITEONLY  0x2 /* No reading allowed. */
#define CHANNEL_GENERIC_FLAGS (CHANNEL_READONLY | CHANNEL_WRITEONLY)

/* Flags implemented by each backend.  */
#define CHANNEL_HARD_READONLY     0x010 /* Can't be made writable.  */
#define CHANNEL_HARD_WRITEONLY    0x020 /* Can't be made readable.  */

#define CHANNEL_BACKEND_SPEC_BASE 0x100 /* Here up are backend-specific */
#define CHANNEL_BACKEND_FLAGS	(CHANNEL_HARD_READONLY			\
				 | CHANNEL_HARD_WRITEONLY
				 | ~(CHANNEL_BACKEND_SPEC_BASE - 1))

struct channel_class
{
  /* Name of the class.  */
  const char *name;

  /* Read at most AMOUNT bytes from CHANNEL into BUF and LEN with the
     usual return buf semantics.  Blocks until data is available or return
     0 bytes on EOF.  May not be null.  See channel_read.  */
  error_t (*read) (struct channel *channel,
		   size_t amount, void **buf, size_t *len);

  /* Write LEN bytes from BUF to CHANNEL, AMOUNT is set to the amount
     actually witten.  Should block until data can be written.  May not be
     null.  See channel_write.  */
  error_t (*write) (struct channel *channel,
		    const void *buf, size_t len, size_t *amount);
 
  /* Set any backend handled flags in CHANNEL specified in FLAGS.  May be
     null.  See channel_set_flags.  */
  error_t (*set_flags) (struct channel *channel, int flags);

  /* Clear any backend handled flags in CHANNEL specified in FLAGS.  May
     be null.  See channel_clear_flags.  */
  error_t (*clear_flags) (struct channel *channel, int flags);

  /* Free any class-specific resources allocated for CHANNEL.  May be
     null. */
  void (*cleanup) (struct channel *channel);
};

/* Allocate a new channel of class CLASS, with FLAGS set (using
   channel_set_flags,) that is returned in CHANNEL.  Return ENOMEM if
   memory for channel couldn't be allocated.  */
error_t channel_create (const struct channel_class *class,
			int flags, struct channel **channel);

/* If not null call method cleanup to deallocate class-specific bits of
   CHANNEL, then free it (regardless) and any generic resources used by
   it.  */
void channel_free (struct channel *channel);

/* Set the flags FLAGS in CHANNEL.  Remove any already set flags in FLAGS,
   if FLAGS contain any backend flags call set_flags method or if
   set_flags is null return EINVAL.  Lastly generic flags get set.  */
error_t channel_set_flags (struct channel *channel, int flags);

/* Clear the flags FLAGS in CHANNEL.  Remove any already cleared flags in
   FLAGS, if FLAGS contain any backend flags call clear_flags method or if
   clear_flags is null return EINVAL.  Lastly generic flags get
   cleared.  */
error_t channel_clear_flags (struct channel *channel, int flags);

/* Reads at most AMOUNT bytes from CHANNEL into BUF and LEN with the usual
   return buf semantics.  Block until data is available and return 0 bytes
   on EOF.  If channel is write-only return EPERM, otherwise forward call
   to CHANNEL's read method.  */
error_t channel_read (struct channel *channel, size_t amount,
		      void **buf, size_t *len);

/* Write LEN bytes of BUF to CHANNEL, AMOUNT is set to the amount actually
   witten.  Block until data can be written.  If channel is read-only
   return EPERM, otherwise forward call to CHANNEL's write method.  */
error_t channel_write (struct channel *channel, const void *buf,
		       size_t len, size_t *amount);

#endif /* __CHANNEL_H__ */
