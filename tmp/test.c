#include <mach.h>
#include <mach/notify.h>
#include <hurd.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int test_change_kernel_port ()
{
  task_t task = mach_task_self ();
  mach_port_t kernel_port = MACH_PORT_NULL;
  mach_port_t receive_port = mach_reply_port ();
  kern_return_t err;
  
  err = mach_port_insert_right (task, receive_port,
				receive_port, MACH_MSG_TYPE_MAKE_SEND);
  if (err)
    error (1, err, "mach_port_insert_right");
  err = task_set_kernel_port (task, receive_port);
  if (err)
    error (1, err, "task_set_kernel_port");
  err = task_get_kernel_port (task, &kernel_port);
  if (err)
    error (1, err, "task_get_kernel_port");
  printf ("task: %d, kernel port: %d, orig port: %d, receive port: %d\n",
	  (mach_task_self) (), kernel_port, task, receive_port);
  return 0;
}

int test_tasks_order ()
{
  mach_port_t *psets;
  mach_port_t priv_host;
  size_t npsets;
  int i;

  get_privileged_ports (&priv_host, NULL);
  host_processor_sets (mach_host_self (), &psets, &npsets);
  for (i = 0; i < npsets; i++)
    {
      mach_port_t psetpriv;
      mach_port_t *tasks;
      process_t proc;
      size_t ntasks;
      int j;

      proc = getproc ();
      host_processor_set_priv (priv_host, psets[i], &psetpriv);
      processor_set_tasks (psetpriv, &tasks, &ntasks);
      printf ("get %d tasks\n", ntasks);
      for (j = 0; j < ntasks; j++)
	{
	  pid_t pid;

	  printf ("task port (%d): %d\n", j, tasks[j]);
	  /* The kernel can deliver us an array with null slots in the
	     middle, e.g. if a task died during the call.  */
	  if (! MACH_PORT_VALID (tasks[j]))
	    continue;
	  proc_task2pid (proc, tasks[j], &pid);
	  printf ("task %d is valid, pid: %d\n", tasks[j], pid);

	  mach_port_deallocate (mach_task_self (), tasks[j]);
	}
      munmap (tasks, ntasks * sizeof (task_t));
      mach_port_deallocate (mach_task_self (), psetpriv);
      mach_port_deallocate (mach_task_self (), psets[i]);
    }
  munmap (psets, npsets * sizeof (mach_port_t));
  return 0;
}

int test_port_notification ()
{
  mach_port_t receive_port;
  mach_port_t foo;
  error_t err;

  err = mach_port_allocate (mach_task_self (), 
			    MACH_PORT_RIGHT_RECEIVE,
			    &receive_port);
  assert_perror (err);
  err = mach_port_request_notification (mach_task_self (), receive_port,
					MACH_NOTIFY_NO_SENDERS, 0,
					MACH_PORT_NULL,
					MACH_MSG_TYPE_MOVE_SEND_ONCE, &foo);
  assert_perror (err);
  return 0;
}

int main ()
{
  test_port_notification ();
}
