/* 
   Copyright (C) 1996 Free Software Foundation, Inc.
   Written by Michael I. Bushnell, p/BSG.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include "netfs.h"

void
netfs_release_protid (void *arg)
{
  struct protid *user = arg;
  
  netfs_drop_credential (user->credential);
  if (user->shared_object)
    mach_port_deallocate (mach_task_self (), user->shared_object);
  if (user->mapped)
    vm_deallocate (mach_task_self (), (vm_address_t) user->mapped,
		   vm_page_size);
  netfs_release_peropen (user->po);
}
