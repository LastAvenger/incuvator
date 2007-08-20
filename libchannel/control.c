/* Function for handling channel control RPCs.

   Copyright (C) 2007 Free Software Foundation, Inc.

   Written by Carl Fredrik Hammar <hammy.lite@gmail.com>

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

/* This demuxer will get the channel IN is intended for using
   channel_begin_using_channel and call channel_end_using_channel, then
   call the channel's control_demuxer method.  Return true, unless channel
   or its method is null or if the method couldn't handle IN.  */
int
channel_control_demuxer (mach_msg_header_t *in, mach_msg_header_t *out)
{
  int (*demuxer) (mach_msg_header_t *, mach_msg_header_t *);
  struct channel *channel =
    channel_begin_using_channel (in->msgh_local_port);

  if (! channel)
    return 0;

  demuxer = channel->hub->class->control_demuxer;
  channel_end_using_channel (channel);

  if (! demuxer)
    return 0;

  return (*demuxer) (in, out);
}

/* Find and return the channel associated with PORT or return null if
   there is no such channel.  This function should be overridden by the
   translator using libchannel, as the default implementation will
   always return null.  */
struct channel *
channel_begin_using_channel (mach_port_t port)
{
  return 0;
}

/* Reverses any side-effects of using channel_begin_using_channel and
   should also be implemented by the translator using libchannel, as
   the default implementation does nothing.  */
void
channel_end_using_channel (struct channel *channel)
{
}
