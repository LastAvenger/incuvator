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

#include <stdio.h>
#include <assert.h>

#include <mach.h>
#include <hurd.h>
#include <hurd/ports.h>

#include "util.h"
#include "list.h"
#include "mach_proxy.h"

extern mach_port_t privileged_host_port;
extern int call_num;

/* Set task priority. */
kern_return_t
S_task_priority (mach_port_t task, int priority, boolean_t change_threads)
{
  struct task_info *task_pi;
  error_t err;

  int local_call_num = call_num++;
  debug ("num: %d", local_call_num);
  task_pi = ports_lookup_port (port_bucket, task, task_portclass);
  if (task_pi == NULL)
    {
      debug ("num: %d, %s", local_call_num, strerror (EOPNOTSUPP));
      return EOPNOTSUPP;
    }
  err = task_priority (task_pi->task_port, priority, change_threads);
  debug ("num: %d, %s", local_call_num, strerror (err));
  ports_port_deref (task_pi);
  return err;
}

/* Routine processor_set_tasks */
kern_return_t
S_processor_set_tasks (mach_port_t processor_set, task_array_t *task_list,
		       mach_msg_type_number_t *task_listCnt)
{
  error_t err = 0;
  mach_port_t *subhurd_tasks = NULL;
  int size = 0;
  /* no pseudo task port is created for the kernel task. */
  int num = 0;
  int local_call_num = call_num++;
  debug ("num: %d", local_call_num);
  int tot_nbtasks = ports_count_class (task_portclass) + num;

  size = tot_nbtasks * sizeof (mach_port_t);
  err = vm_allocate (mach_task_self (),
		     (vm_address_t *) (void *) &subhurd_tasks,
		     size, 1);
  if (err) 
    goto out;

  int get_pseudo_task_port (struct task_info *task_pi)
    {
      assert (num < tot_nbtasks);
      subhurd_tasks[num++] = ports_get_right (task_pi);
      return 0;
    }

  foreach_task (get_pseudo_task_port);
  assert (num == tot_nbtasks);
  *task_list = subhurd_tasks;
  *task_listCnt = tot_nbtasks;

out:
  debug ("num: %d, get %d tasks: %s",
	 local_call_num, tot_nbtasks, strerror (err));
  /* I enable the class here,
   * so no pseudo task port can be created when I count the number of tasks. */
  ports_enable_class (task_portclass);
  /* The array will be deallocated after it is sent,
   * but the task ports in it don't need to,
   * because I only call ports_get_right()
   * and the reference count isn't increased. */
  return err;
}

/* Get control port for a processor set. */
kern_return_t
S_host_processor_set_priv (mach_port_t host_priv, mach_port_t set_name,
			   mach_port_t *set, mach_msg_type_name_t *setPoly)
{
  extern struct port_class *other_portclass;
  struct port_info *pi;
  kern_return_t ret = 0;

  int local_call_num = call_num++;
  debug ("num: %d", local_call_num);
  // TODO create a port for each processor set
  // I should create the port for the processor set only once.
  ret = ports_create_port (other_portclass, port_bucket,
			   sizeof (*pi), &pi);
  if (ret)
    {
      debug ("num: %d, %s", local_call_num, strerror (ret));
      return ret;
    }
  *set = ports_get_right (pi);
  *setPoly = MACH_MSG_TYPE_MAKE_SEND;
  ports_port_deref (pi);
  debug ("num: %d, %s", local_call_num, strerror (ret));
  return ret;
}

/* Routine host_reboot */
kern_return_t
S_host_reboot (mach_port_t host_priv, int options)
{
  debug ("");
  assert (0);
  // TODO
  return EOPNOTSUPP;
}

/* Routine vm_wire */
kern_return_t
S_vm_wire (mach_port_t host_priv, mach_port_t task,
	   vm_address_t address, vm_size_t size, vm_prot_t access)
{
  debug ("");
  assert (0);
  // TODO
  return EOPNOTSUPP;
}

/* Routine thread_wire */
kern_return_t
S_thread_wire (mach_port_t host_priv, mach_port_t thread, boolean_t wired)
{
  debug ("");
  assert (0);
  // TODO
  return EOPNOTSUPP;
}

//////////the request to the host isn't forwarded by the proxy//////////

/* Routine host_processor_sets */
kern_return_t
S_host_processor_sets (mach_port_t host,
		       processor_set_name_array_t *processor_sets,
		       mach_msg_type_number_t *processor_setsCnt)
{
  debug ("");
  assert (0);
  // the request to the host isn't forwarded.
  return EOPNOTSUPP;
}

/* Routine host_get_time */
kern_return_t
S_host_get_time (mach_port_t host, time_value_t *current_time)
{
  debug ("");
  assert (0);
  // the request to the host isn't forwarded.
  return EOPNOTSUPP;
}

/* Routine host_info */
kern_return_t
S_host_info (mach_port_t host, int flavor, host_info_t host_info_out,
	     mach_msg_type_number_t *host_info_outCnt)
{
  debug ("");
  assert (0);
  // the request to the host isn't forwarded.
  return EOPNOTSUPP;
}

/* Get string describing current kernel version. */
kern_return_t
S_host_kernel_version (mach_port_t host, kernel_version_t kernel_version)
{
  debug ("");
  assert (0);
  // the proxy doesn't forward the request to the host port.
  return EOPNOTSUPP;
}

///////////////////the RPCs not used by Hurd//////////////////////

/* Get list of processors on this host. */
kern_return_t
S_host_processors (mach_port_t host_priv,
		   processor_array_t *processor_list,
		   mach_msg_type_number_t *processor_listCnt)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Start processor. */
kern_return_t
S_processor_start (mach_port_t processor)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Exit processor -- may not be restartable. */
kern_return_t
S_processor_exit (mach_port_t processor)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Get default processor set for host. */
kern_return_t
S_processor_set_default (mach_port_t host, mach_port_t *default_set)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/*
 * Create new processor set.  Returns real port for manipulations,
 *      and name port for obtaining information.
 */
kern_return_t
S_processor_set_create (mach_port_t host, mach_port_t *new_set,
			mach_port_t *new_name)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Destroy processor set. */
kern_return_t
S_processor_set_destroy (mach_port_t set)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Assign processor to processor set. */
kern_return_t
S_processor_assign (mach_port_t processor, mach_port_t new_set, boolean_t wait)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Get current assignment for thread. */
kern_return_t
S_processor_get_assignment (mach_port_t processor, mach_port_t *assigned_set)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Assign task to processor set. */
kern_return_t
S_task_assign (mach_port_t task, mach_port_t new_set, boolean_t assign_threads)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Assign task to default set. */
kern_return_t
S_task_assign_default (mach_port_t task, boolean_t assign_threads)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Get current assignment for task. */
kern_return_t
S_task_get_assignment (mach_port_t task, mach_port_t *assigned_set)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Set max priority for processor_set. */
kern_return_t
S_processor_set_max_priority (mach_port_t processor_set, int max_priority,
			      boolean_t change_threads)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Routine processor_set_policy_enable */
kern_return_t
S_processor_set_policy_enable (mach_port_t processor_set, int policy)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Routine processor_set_policy_disable */
kern_return_t
S_processor_set_policy_disable (mach_port_t processor_set, int policy,
				boolean_t change_threads)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Routine processor_set_threads */
kern_return_t
S_processor_set_threads (mach_port_t processor_set,
			 thread_array_t *thread_list,
			 mach_msg_type_number_t *thread_listCnt)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Routine host_set_time */
kern_return_t
S_host_set_time (mach_port_t host_priv, time_value_t new_time)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Routine host_adjust_time */
kern_return_t
S_host_adjust_time (mach_port_t host_priv, time_value_t new_adjustment,
		    time_value_t *old_adjustment)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Routine processor_info */
kern_return_t
S_processor_info (mach_port_t processor, int flavor, mach_port_t *host,
		  processor_info_t processor_info_out,
		  mach_msg_type_number_t *processor_info_outCnt)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Routine processor_set_info */
kern_return_t
S_processor_set_info (mach_port_t set_name, int flavor, mach_port_t *host,
		      processor_set_info_t info_out,
		      mach_msg_type_number_t *info_outCnt)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Routine processor_control */
kern_return_t
S_processor_control (mach_port_t processor, processor_info_t processor_cmd,
		     mach_msg_type_number_t processor_cmdCnt)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

/* Routine host_get_boot_info */
kern_return_t
S_host_get_boot_info (mach_port_t host_priv, kernel_boot_info_t boot_info)
{
  debug ("");
  assert (0);
  // Hurd currently doesn't use it.
  return EOPNOTSUPP;
}

///////////////////it's not a proxy for thread requests///////////////////

/* Assign thread to processor set. */
kern_return_t
S_thread_assign (mach_port_t thread, mach_port_t new_set)
{
  debug ("");
  assert (0);
  return EOPNOTSUPP;
}

/* Assign thread to default set. */
kern_return_t
S_thread_assign_default (mach_port_t thread)
{
  debug ("");
  assert (0);
  return EOPNOTSUPP;
}

/* Get current assignment for thread. */
kern_return_t
S_thread_get_assignment (mach_port_t thread, mach_port_t *assigned_set)
{
  debug ("");
  assert (0);
  return EOPNOTSUPP;
}

/* Set priority for thread. */
kern_return_t S_thread_priority (mach_port_t thread, int priority,
				 boolean_t set_max)
{
  debug ("");
  assert (0);
  return thread_priority (thread, priority, set_max);
}

/* Set max priority for thread. */
kern_return_t S_thread_max_priority (mach_port_t thread,
				     mach_port_t processor_set,
				     int max_priority)
{
  debug ("");
  assert (0);
  return thread_max_priority (thread, processor_set, max_priority);
}

/* Routine thread_depress_abort */
kern_return_t S_thread_depress_abort (mach_port_t thread)
{
  debug ("");
  assert (0);
  return EOPNOTSUPP;
}

/* Set policy for thread */
kern_return_t S_thread_policy (mach_port_t thread, int policy, int data)
{
  debug ("");
  assert (0);
  return EOPNOTSUPP;
}

////////////////////don't support obsolete requests///////////////////////

/* Routine yyy_host_info */
/* obsolete */
kern_return_t
S_yyy_host_info (mach_port_t host, int flavor, host_info_t host_info_out,
		 mach_msg_type_number_t *host_info_outCnt)
{
  debug ("");
  assert (0);
  return EOPNOTSUPP;
}

/* Routine yyy_processor_info */
/* obsolete */
kern_return_t
S_yyy_processor_info (mach_port_t processor, int flavor, mach_port_t *host,
		      processor_info_t processor_info_out,
		      mach_msg_type_number_t *processor_info_outCnt)
{
  debug ("");
  assert (0);
  return EOPNOTSUPP;
}

/* Routine yyy_processor_control */
/* obsolete */
kern_return_t
S_yyy_processor_control (mach_port_t processor,
			 processor_info_t processor_cmd,
			 mach_msg_type_number_t processor_cmdCnt)
{
  debug ("");
  assert (0);
  return EOPNOTSUPP;
}

/*
 * Get rights to default processor set for host.
 * Replaced by host_processor_set_priv.
 */
kern_return_t
S_xxx_processor_set_default_priv (mach_port_t host, mach_port_t *default_set)
{
  debug ("");
  assert (0);
  return EOPNOTSUPP;
}

/* Routine yyy_processor_set_info */
/* obsolete */
kern_return_t
S_yyy_processor_set_info (mach_port_t set_name, int flavor,
			  mach_port_t *host, processor_set_info_t info_out,
			  mach_msg_type_number_t *info_outCnt)
{
  debug ("");
  assert (0);
  return EOPNOTSUPP;
}
