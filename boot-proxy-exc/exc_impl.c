/* 
   Copyright (C) 2009 Free Software Foundation, Inc.
   Written by Zheng Da.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the GNU Hurd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* This file implements the server-side RPC functions of mach_host. */

#include <mach.h>
#include <hurd.h>
#include <assert.h>

#include "util.h"
#include "mach_proxy.h"

kern_return_t
catch_exception_raise (mach_port_t exception_port, mach_port_t thread,
		       mach_port_t task, integer_t exception,
		       integer_t code, integer_t subcode)
{
  error_t err;
  struct task_info *fault_task_pi = NULL;
  debug ("");

  int match_task (struct task_info *task_pi)
    {
      if (task_pi->task_port == task)
	{
	  fault_task_pi = task_pi;
	  ports_port_ref (fault_task_pi);
	  return 1;
	}
      return 0;
    }
  foreach_task (match_task);
  
  if (fault_task_pi == NULL)
    {
      fault_task_pi = ports_lookup_port (port_bucket, task, task_portclass);
      if (fault_task_pi)
	info ("find the task: %d from libports", task);
    }

  if (fault_task_pi == NULL)
    {
      info ("cannot find the task: %d", task);
      return EINVAL;
    }

  err = catch_exception_raise (fault_task_pi->user_exc_port,
			       thread, fault_task_pi->task.port_right,
			       exception, code, subcode);
  ports_port_deref (fault_task_pi);
  return err;
}
