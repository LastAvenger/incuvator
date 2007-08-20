/* Fetch channel hub via RPC.

   Copyright (C) 1995, 1996, 1997, 2001, 2007
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

#include "channel.h"

/* Return a new hub in HUB, which is a copy of the hub underlying SOURCE.
   The class of hub is found in CLASSES or CHANNEL_STD_CLASSES, if CLASSES
   is null.  FLAGS is set with channel_set_hub_flags.  Keeps the SOURCE
   reference, which may be closed using channel_close_hub_source.  */
error_t
channel_fetch_hub (file_t source, int flags,
		   const struct channel_class *const *classes,
		   struct channel_hub **hub)
{
  /* XXX implement by copying store_create.  */
  return EOPNOTSUPP;
}
