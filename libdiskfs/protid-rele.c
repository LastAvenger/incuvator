/* 
   Copyright (C) 1994 Free Software Foundation

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

/* Called when a protid CRED has no more references.  (Because references\
   to protids are maintained by the port management library, this is 
   installed in the clean routines list.)  The ports library will
   free the structure for us.  */
diskfs_protid_rele (void *arg)
{
  struct protid *cred = arg;

  if (cred->shared_object)
    mach_port_deallocate (mach_task_self (), cred->shared_object);
  if (cred->mapped)
    vm_deallocate (mach_task_self (), cred->mapped, vm_page_size);
  diskfs_peropen_rele (cred->po);
}

  
