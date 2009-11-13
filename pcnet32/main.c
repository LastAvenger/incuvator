#include <error.h>
#include <signal.h>
#include <fcntl.h>
#include <pciaccess.h>

#include <cthreads.h>
#include <mach.h>
#include <hurd/trivfs.h>
#include <hurd/ports.h>

#include "device_U.h"
#include "irq.h"
#include "netdevice.h"

struct port_bucket *port_bucket;
struct port_bucket *irq_port_bucket;
struct port_class *dev_class;
struct port_class *intr_class;

/* Trivfs hooks.  */
int trivfs_fstype = FSTYPE_MISC;
int trivfs_fsid = 0;
int trivfs_support_read = 0;
int trivfs_support_write = 0;
int trivfs_support_exec = 0;
int trivfs_allow_open = O_READ | O_WRITE;

struct port_class *trivfs_protid_portclasses[1];
struct port_class *trivfs_cntl_portclasses[1];
int trivfs_protid_nportclasses = 1;
int trivfs_cntl_nportclasses = 1;

/* Implementation of notify interface */
kern_return_t
do_mach_notify_port_deleted (mach_port_t notify,
			     mach_port_t name)
{
  return EOPNOTSUPP;
}

kern_return_t
do_mach_notify_msg_accepted (mach_port_t notify,
			     mach_port_t name)
{
  return EOPNOTSUPP;
}

kern_return_t
do_mach_notify_port_destroyed (mach_port_t notify,
			       mach_port_t port)
{
  return EOPNOTSUPP;
}

kern_return_t
do_mach_notify_no_senders (mach_port_t notify,
			   mach_port_mscount_t mscount)
{
  return ports_do_mach_notify_no_senders (notify, mscount);
}

kern_return_t
do_mach_notify_send_once (mach_port_t notify)
{
  return EOPNOTSUPP;
}

kern_return_t
do_mach_notify_dead_name (mach_port_t notify,
			  mach_port_t name)
{
  return EOPNOTSUPP;
}


void
trivfs_modify_stat (struct trivfs_protid *cred, io_statbuf_t *stat)
{
}

int notify_irq_server (mach_msg_header_t *inp, mach_msg_header_t *outp)
{
  extern void pcnet32_interrupt(int irq);
  extern struct device *ether_dev;
  mach_irq_notification_t *irq_header = (mach_irq_notification_t *) inp;

  if (inp->msgh_id != MACH_NOTIFY_IRQ)
    return 0;

  if (irq_header->irq == ether_dev->irq)
    pcnet32_interrupt (irq_header->irq);
//  ((mig_reply_header_t *) outp)->RetCode = MIG_NO_REPLY;
  return 1;
}

mach_port_t master_device;
mach_port_t priv_host;

void int_handler (int sig)
{
  error_t err = device_intr_notify (master_device, 2, 0, MACH_PORT_NULL,
				    MACH_MSG_TYPE_MAKE_SEND);
  if (err)
    error (2, err, "device_intr_notify");

  exit (0);
}

static int
demuxer (mach_msg_header_t *inp, mach_msg_header_t *outp)
{
  extern int device_server (mach_msg_header_t *, mach_msg_header_t *);
  extern int notify_server (mach_msg_header_t *, mach_msg_header_t *);
  return device_server (inp, outp) || notify_server (inp, outp)
    || trivfs_demuxer (inp, outp) || notify_irq_server (inp, outp);
}

boolean_t
is_master_device (mach_port_t port)
{
  struct port_info *pi = ports_lookup_port (port_bucket, port,
					    trivfs_protid_portclasses[0]);
  if (pi == NULL)
    return FALSE;
  
  ports_port_deref (pi);
  return TRUE;
}

error_t
trivfs_goaway (struct trivfs_control *fsys, int flags)
{
  int count;

  /* Stop new requests.  */
  ports_inhibit_class_rpcs (trivfs_cntl_portclasses[0]);
  ports_inhibit_class_rpcs (trivfs_protid_portclasses[0]);

  count = ports_count_class (trivfs_protid_portclasses[0]);

  if (count && !(flags & FSYS_GOAWAY_FORCE))
    {
      /* We won't go away, so start things going again...  */
      ports_enable_class (trivfs_protid_portclasses[0]);
      ports_resume_class_rpcs (trivfs_cntl_portclasses[0]);
      ports_resume_class_rpcs (trivfs_protid_portclasses[0]);
      return EBUSY;
    }

  mach_port_deallocate (mach_task_self (), master_device);
  pci_system_cleanup ();
  exit (0);
}

void dev_clean_routine (void *port)
{
  error_t err = device_intr_notify (master_device, 2, 0, MACH_PORT_NULL,
				    MACH_MSG_TYPE_MAKE_SEND);
  if (err)
    error (2, err, "device_intr_notify");
}

any_t
irq_receive_thread(any_t unused)
{
  int irq_demuxer (mach_msg_header_t *inp, mach_msg_header_t *outp)
    {
      return notify_irq_server (inp, outp);
    }
  ports_manage_port_operations_one_thread (irq_port_bucket, irq_demuxer, 0);
  return 0;
}

int
main ()
{
  extern void mach_device_init();
  extern any_t io_done_thread(any_t unused);
  extern boolean_t ioperm_ports ();
  extern int  pcnet32_probe(struct device *dev);
  mach_port_t bootstrap;
  error_t err;
  struct trivfs_control *fsys;

  task_get_bootstrap_port (mach_task_self (), &bootstrap);
  if (bootstrap == MACH_PORT_NULL)
    error (1, 0, "must be started as a translator");

  if (!ioperm_ports ())
    error (2, errno, "cannot set the port access permission for the keyboard");

  signal (SIGINT, int_handler);

  err = get_privileged_ports (&priv_host, &master_device);
  if (err)
    error (1, err, "get_privileged_ports");

  /* Initialize the port bucket and port classes. */
  port_bucket = ports_create_bucket ();
  irq_port_bucket = ports_create_bucket ();
  dev_class = ports_create_class (dev_clean_routine, 0);
  intr_class = ports_create_class (0, 0);
  trivfs_cntl_portclasses[0] = ports_create_class (trivfs_clean_cntl, 0);
  trivfs_protid_portclasses[0] = ports_create_class (trivfs_clean_protid, 0);

  err = pci_system_init ();
  if (err)
    error (2, err, "pci_system_init");

  err = pcnet32_probe (NULL);
  if (err)
    error (2, err, "pcnet32_probe");

  mach_device_init ();
  linux_net_emulation_init ();
  linux_kmem_init ();

  /* Reply to our parent.  */
  err = trivfs_startup (bootstrap, 0,
			trivfs_cntl_portclasses[0], port_bucket,
			trivfs_protid_portclasses[0], port_bucket, &fsys);
  mach_port_deallocate (mach_task_self (), bootstrap);
  if (err)
    error (1, err, "Contacting parent");

  cthread_detach (cthread_fork (io_done_thread, 0));
  cthread_detach (cthread_fork (irq_receive_thread, 0));

  /* Launch.  */
  do
    {
      ports_manage_port_operations_one_thread (port_bucket, demuxer, 0);
    } while (trivfs_goaway (fsys, 0));
  return 0;
}

