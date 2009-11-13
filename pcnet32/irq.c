#include <error.h>

#include <mach.h>
#include <hurd.h>

#include "netdevice.h"
#include "device_U.h"
#include "irq.h"

extern mach_port_t master_device;

/*
 * Install the irq in the kernel.
 */
int
request_irq (struct linux_device *dev,
	     void (*handler) (int), unsigned long flags)
{
  return device_intr_notify (master_device, dev->irq, dev->dev_id,
			     ports_get_right (dev), MACH_MSG_TYPE_MAKE_SEND);
}

/*
 * Deallocate an irq.
 */
void
free_irq (struct linux_device *dev)
{
  error_t err;
  err = device_intr_notify (master_device, dev->irq, dev->dev_id,
			    MACH_PORT_NULL, MACH_MSG_TYPE_MAKE_SEND);
  if (err)
    error (0, err, "device_intr_notify");
}
