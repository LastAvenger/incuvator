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
  struct channel_hub *hub;
  void *class_hook;
};

struct channel_hub
{
  int flags;
  const struct channel_class *class;
  void *hook; /* For class use.  */
};

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

  /* Open CHANNEL and set FLAGS.  May be null.  See channel_open.  */
  error_t (*open) (struct channel *channel, int flags);

  /* Free any resources allocated for CHANNEL.  May be null.  See
     channel_close.  */
  void (*close) (struct channel *channel);

  /* Set any backend handled flags in HUB specified in FLAGS.  May be
     null.  See channel_set_hub_flags.  */
  error_t (*set_hub_flags) (struct channel_hub *hub, int flags);

  /* Clear any backend handled flags in HUB specified in FLAGS.  May be
     null.  See channel_clear_hub_flags.  */
  error_t (*clear_hub_flags) (struct channel_hub *hub, int flags);

  /* Free any class-specific resources allocated for HUB.  May be null.
     See channel_free_hub.  */
  void (*clear_hub) (struct channel_hub *hub);
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
				 | CHANNEL_HARD_WRITEONLY		\
				 | ~(CHANNEL_BACKEND_SPEC_BASE - 1))

/* Allocate a new channel of hub HUB, with FLAGS set, then return it in
   CHANNEL.  Return ENOMEM if memory for the hub couldn't be allocated.  */
error_t channel_alloc (struct channel_hub *hub, int flags,
		       struct channel **channel);

/* Free CHANNEL and any generic resources allocated for it.  */
void channel_free (struct channel *channel);

/* Allocate a new channel, open it, and return it in CHANNEL.  Uses
   HUB's open method and passes FLAGS to it, unless it's null.  */
error_t channel_open (struct channel_hub *hub, int flags,
		      struct channel **channel);

/* Call CHANNEL's close method, unless it's null, then free it
   (regardless.)  */
void channel_close (struct channel *channel);

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

/* Allocate a new hub of CLASS with FLAGS set, then return it in HUB.
   Return ENOMEM if memory for the hub couldn't be allocated.  */
error_t channel_alloc_hub (const struct channel_class *class,
			   int flags, struct channel_hub **hub);

/* If not null call method clear_hub to deallocate class-specific bits of
   HUB, then (regardless) free any generic resources used by it and
   itself.  */
void channel_free_hub (struct channel_hub *hub);

/* Set the flags FLAGS in HUB.  Remove any already set flags in FLAGS, if
   FLAGS then contain backend flags call set_hub_flags method with with
   FLAGS or if set_hub_flags is null return EINVAL.  Lastly generic flags
   get set.  */
error_t channel_set_hub_flags (struct channel_hub *hub, int flags);

/* Clear the flags FLAGS in HUB.  Remove any already cleared flags in
   FLAGS, if FLAGS then contain backend flags call clear_hub_flags method
   with with FLAGS or if clear_hub_flags is null return EINVAL.  Lastly
   generic flags get clear.  */
error_t channel_clear_hub_flags (struct channel_hub *hub, int flags);

#endif /* __CHANNEL_H__ */
