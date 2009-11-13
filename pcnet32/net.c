/*
 * Linux network driver support.
 *
 * Copyright (C) 1996 The University of Utah and the Computer Systems
 * Laboratory at the University of Utah (CSL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *      Author: Shantanu Goel, University of Utah CSL
 */

/*
 * INET               An implementation of the TCP/IP protocol suite for the LINUX
 *              operating system.  INET is implemented using the  BSD Socket
 *              interface as the means of communication with the user level.
 *
 *              Ethernet-type device handling.
 *
 * Version:     @(#)eth.c       1.0.7   05/25/93
 *
 * Authors:     Ross Biro, <bir7@leland.Stanford.Edu>
 *              Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *              Mark Evans, <evansmp@uhura.aston.ac.uk>
 *              Florian  La Roche, <rzsfl@rz.uni-sb.de>
 *              Alan Cox, <gw4pts@gw4pts.ampr.org>
 * 
 * Fixes:
 *              Mr Linux        : Arp problems
 *              Alan Cox        : Generic queue tidyup (very tiny here)
 *              Alan Cox        : eth_header ntohs should be htons
 *              Alan Cox        : eth_rebuild_header missing an htons and
 *                                minor other things.
 *              Tegge           : Arp bug fixes. 
 *              Florian         : Removed many unnecessary functions, code cleanup
 *                                and changes for new arp and skbuff.
 *              Alan Cox        : Redid header building to reflect new format.
 *              Alan Cox        : ARP only when compiled with CONFIG_INET
 *              Greg Page       : 802.2 and SNAP stuff.
 *              Alan Cox        : MAC layer pointers/new format.
 *              Paul Gortmaker  : eth_copy_and_sum shouldn't csum padding.
 *              Alan Cox        : Protect against forwarding explosions with
 *                                older network drivers and IFF_ALLMULTI
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <error.h>
#include <arpa/inet.h>

#include "mach_U.h"

#include <mach.h>
#include <hurd.h>

#define MACH_INCLUDE

#include "vm_param.h"
#include "netdevice.h"
#include "device_reply_U.h"
#include "if_ether.h"
#include "dev_hdr.h"
#include "if.h"
#include "util.h"

#define ether_header ethhdr

extern int linux_intr_pri;
extern struct port_bucket *port_bucket;
extern struct port_class *dev_class;

/* One of these is associated with each instance of a device.  */
struct net_data
{
  struct port_info port;	/* device port */
//  struct ifnet ifnet;		/* Mach ifnet structure (needed for filters) */
  struct emul_device device;		/* generic device structure */
  mach_port_t delivery_port;
  struct linux_device *dev;	/* Linux network device structure */
};

/* List of sk_buffs waiting to be freed.  */
static struct sk_buff_head skb_done_list;

/* Forward declarations.  */

extern struct device_emulation_ops linux_net_emulation_ops;

static int print_packet_size = 0;

mach_msg_type_t header_type = 
{
  MACH_MSG_TYPE_BYTE,
  8,
  NET_HDW_HDR_MAX,
  TRUE,
  FALSE,
  FALSE,
  0
};

mach_msg_type_t packet_type = 
{
  MACH_MSG_TYPE_BYTE,	/* name */
  8,			/* size */
  0,			/* number */
  TRUE,			/* inline */
  FALSE,			/* longform */
  FALSE			/* deallocate */
};

/* Linux kernel network support routines.  */

/* Requeue packet SKB for transmission after the interface DEV
   has timed out.  The priority of the packet is PRI.
   In Mach, we simply drop the packet like the native drivers.  */
void
dev_queue_xmit (struct sk_buff *skb, struct linux_device *dev, int pri)
{
  dev_kfree_skb (skb, FREE_WRITE);
}

/* Close the device DEV.  */
int
dev_close (struct linux_device *dev)
{
  return 0;
}

/* Network software interrupt handler.  */
//void
//net_bh (void)
//{
//  int len;
//  struct sk_buff *skb;
//  struct linux_device *dev;
//
//  /* Start transmission on interfaces.  */
//  for (dev = dev_base; dev; dev = dev->next)
//    {
//      if (dev->base_addr && dev->base_addr != 0xffe0)
//	while (1)
//	  {
//	    skb = skb_dequeue (&dev->buffs[0]);
//	    if (skb)
//	      {
//		len = skb->len;
//		if ((*dev->hard_start_xmit) (skb, dev))
//		  {
//		    skb_queue_head (&dev->buffs[0], skb);
//		    mark_bh (NET_BH);
//		    break;
//		  }
//		else if (print_packet_size)
//		  printf ("net_bh: length %d\n", len);
//	      }
//	    else
//	      break;
//	  }
//    }
//}

/* Free all sk_buffs on the done list.
   This routine is called by the iodone thread in ds_routines.c.  */
void
free_skbuffs ()
{
  struct sk_buff *skb;

  while (1)
    {
      skb = skb_dequeue (&skb_done_list);
      if (skb)
	{
//	  if (skb->copy)
//	    {
//	      vm_map_copy_discard (skb->copy);
//	      skb->copy = NULL;
//	    }
	  if (MACH_PORT_VALID (skb->reply))
	    {
	      ds_device_write_reply (skb->reply, skb->reply_type, 0, skb->len);
	      skb->reply = MACH_PORT_NULL;
	    }
	  dev_kfree_skb (skb, FREE_WRITE);
	}
      else
	break;
    }
}

/* Allocate an sk_buff with SIZE bytes of data space.  */
struct sk_buff *
alloc_skb (unsigned int size, int priority)
{
  return dev_alloc_skb (size);
}

/* Free SKB.  */
void
kfree_skb (struct sk_buff *skb, int priority)
{
  dev_kfree_skb (skb, priority);
}

/* Allocate an sk_buff with SIZE bytes of data space.  */
struct sk_buff *
dev_alloc_skb (unsigned int size)
{
  struct sk_buff *skb;
  int len = size;

  size = (size + 15) & ~15;
  size += sizeof (struct sk_buff);

  // TODO the max packet size is 1 page,
  // so I don't need to record the size that will be used in deallocation.
  if (size > PAGE_SIZE)
    {
      fprintf (stderr,
	       "WARNING! fail to allocate a packet of %d bytes\n", size);
      return NULL;
    }

  /* XXX: In Mach, a sk_buff is located at the head,
     while it's located at the tail in Linux.  */
  skb = (struct sk_buff *) linux_kmalloc (size, 0);
  if (skb == NULL)
    {
      debug ("fails to allocate memory for the packet.");
      return NULL;
    }

  skb->dev = NULL;
  skb->reply = MACH_PORT_NULL;
//  skb->copy = NULL;
  skb->len = 0;
  skb->prev = skb->next = NULL;
  skb->list = NULL;
  skb->data = ((unsigned char *) skb) + sizeof (struct sk_buff);
  skb->tail = skb->data;
  skb->head = skb->data;
  skb->end = skb->data + len;

  return skb;
}

/* Free the sk_buff SKB.  */
void
dev_kfree_skb (struct sk_buff *skb, int mode)
{
//  unsigned flags;
  extern void wakeup_io_done_thread ();

  /* Queue sk_buff on done list if there is a
     page list attached or we need to send a reply.
     Wakeup the iodone thread to process the list.  */
  if (/*skb->copy ||*/ MACH_PORT_VALID (skb->reply))
    {
      skb_queue_tail (&skb_done_list, skb);
//      save_flags (flags);
      wakeup_io_done_thread ();
//      restore_flags (flags);
      return;
    }
  linux_kfree (skb);
}
/*
 * Deliver the message to all right pfinet servers that
 * connects to the virtual network interface.
 */
int
deliver_msg(mach_port_t dest, struct net_rcv_msg *msg)
{
  mach_msg_return_t err;

  msg->msg_hdr.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, 0);
  /* remember message sizes must be rounded up */
  msg->msg_hdr.msgh_local_port = MACH_PORT_NULL;
  msg->msg_hdr.msgh_kind = MACH_MSGH_KIND_NORMAL;
  msg->msg_hdr.msgh_id = NET_RCV_MSG_ID;

  msg->msg_hdr.msgh_remote_port = dest;
  err = mach_msg ((mach_msg_header_t *)msg,
		  MACH_SEND_MSG|MACH_SEND_TIMEOUT,
		  msg->msg_hdr.msgh_size, 0, MACH_PORT_NULL,
		  0, MACH_PORT_NULL);
  if (err != MACH_MSG_SUCCESS)
    {
      mach_port_deallocate(mach_task_self (),
			   ((mach_msg_header_t *)msg)->msgh_remote_port);
//      error (0, err, "mach_msg");
      return err;
    }

  return MACH_MSG_SUCCESS;
}

/* Accept packet SKB received on an interface.  */
void
netif_rx (struct sk_buff *skb)
{
  int pack_size;
  net_rcv_msg_t net_msg;
  struct ether_header *eh;
  struct packet_header *ph;
  struct linux_device *dev = skb->dev;

  assert (skb != NULL);

  if (print_packet_size)
    printf ("netif_rx: length %ld\n", skb->len);

  /* Allocate a kernel message buffer.  */
  net_msg = malloc (sizeof (*net_msg));
  if (!net_msg)
    {
      dev_kfree_skb (skb, FREE_READ);
      return;
    }

  pack_size = skb->len - sizeof (struct ethhdr);
  /* remember message sizes must be rounded up */
  net_msg->msg_hdr.msgh_size = (((mach_msg_size_t) (sizeof(struct net_rcv_msg)
					       - NET_RCV_MAX + pack_size)) + 3) & ~3;

  /* Copy packet into message buffer.  */
  eh = (struct ether_header *) (net_msg->header);
  ph = (struct packet_header *) (net_msg->packet);
  memcpy (eh, skb->data, sizeof (struct ether_header));
  /* packet is prefixed with a struct packet_header,
     see include/device/net_status.h.  */
  memcpy (ph + 1, skb->data + sizeof (struct ether_header), pack_size);
  ph->type = eh->h_proto;
  ph->length = pack_size + sizeof (struct packet_header);

  dev_kfree_skb (skb, FREE_READ);

  net_msg->sent = FALSE; /* Mark packet as received.  */

  net_msg->header_type = header_type;
  net_msg->packet_type = packet_type;
  net_msg->net_rcv_msg_packet_count = ph->length;
  deliver_msg (dev->net_data->delivery_port, net_msg);
  free (net_msg);
}

/* Mach device interface routines.  */

/* Return a send right associated with network device ND.  */
static mach_port_t
dev_to_port (void *nd)
{
  return (nd
	  ? ports_get_send_right (nd)
	  : MACH_PORT_NULL);
}

      
/*    
 *     * Initialize send and receive queues on an interface.
 *      */   
//void if_init_queues(ifp)
//     register struct ifnet *ifp;
//{     
//  IFQ_INIT(&ifp->if_snd);
//  queue_init(&ifp->if_rcv_port_list);
//  queue_init(&ifp->if_snd_port_list);
//  simple_lock_init(&ifp->if_rcv_port_list_lock);
//  simple_lock_init(&ifp->if_snd_port_list_lock);
//}


static io_return_t
device_open (mach_port_t reply_port, mach_msg_type_name_t reply_port_type,
	     dev_mode_t mode, char *name, device_t *devp)
{
  io_return_t err = D_SUCCESS;
//  struct ifnet *ifp;
  struct linux_device *dev;
  struct net_data *nd;

  /* Search for the device.  */
  for (dev = dev_base; dev; dev = dev->next)
    {
      if (dev->base_addr
	  && dev->base_addr != 0xffe0
	  && !strcmp (name, dev->name))
	break;
    }
  if (!dev)
    return D_NO_SUCH_DEVICE;

  /* Allocate and initialize device data if this is the first open.  */
  nd = dev->net_data;
  if (!nd)
    {
      err = ports_create_port (dev_class, port_bucket,
			       sizeof (*nd), &nd);
      if (err)
	goto out;
	
      dev->net_data = nd;
      nd->dev = dev;
      nd->device.emul_data = nd;
      nd->device.emul_ops = &linux_net_emulation_ops;
//      ipc_kobject_set (nd->port, (ipc_kobject_t) & nd->device, IKOT_DEVICE);
//      notify = ipc_port_make_sonce (nd->port);
//      ip_lock (nd->port);
//      ipc_port_nsrequest (nd->port, 1, notify, &notify);
//      assert (notify == IP_NULL);

//      ifp = &nd->ifnet;
//      ifp->if_unit = dev->name[strlen (dev->name) - 1] - '0';
//      ifp->if_flags = IFF_UP | IFF_RUNNING;
//      ifp->if_mtu = dev->mtu;
//      ifp->if_header_size = dev->hard_header_len;
//      ifp->if_header_format = dev->type;
//      ifp->if_address_size = dev->addr_len;
//      ifp->if_address = dev->dev_addr;
//      if_init_queues (ifp);

      if (dev->open)
	{
//	  linux_intr_pri = SPL6;
	  if ((*dev->open) (dev))
	    err = D_NO_SUCH_DEVICE;
	}

    out:
      if (err)
	{
	  if (nd)
	    {
	      ports_destroy_right (nd);
	      nd = NULL;
	      dev->net_data = NULL;
	    }
	}
      else
	{
	  /* IPv6 heavily relies on multicasting (especially router and
	     neighbor solicits and advertisements), so enable reception of
	     those multicast packets by setting `LINUX_IFF_ALLMULTI'.  */
	  dev->flags |= LINUX_IFF_UP | LINUX_IFF_RUNNING | LINUX_IFF_ALLMULTI;
	  skb_queue_head_init (&dev->buffs[0]);

	  if (dev->set_multicast_list)
	    dev->set_multicast_list (dev);
	}
      if (MACH_PORT_VALID (reply_port))
	ds_device_open_reply (reply_port, reply_port_type,
			      err, dev_to_port (nd));
      return MIG_NO_REPLY;
    }

  *devp = ports_get_right (nd);
  return D_SUCCESS;
}

static io_return_t
device_write (void *d, mach_port_t reply_port,
	      mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	      recnum_t bn, io_buf_ptr_t data, unsigned int count,
	      int *bytes_written)
{
//  unsigned char *p;
  int amt, skblen;
//  io_return_t err = 0;
  struct net_data *nd = d;
  struct linux_device *dev = nd->dev;
  struct sk_buff *skb;

  if (count == 0 || count > dev->mtu + dev->hard_header_len)
    return D_INVALID_SIZE;

  /* Allocate a sk_buff.  */
//  amt = PAGE_SIZE - (copy->offset & PAGE_MASK);
//  TODO need for test.
  amt = PAGE_SIZE - (((int) data) & PAGE_MASK);
  skblen = count;
  skb = dev_alloc_skb (skblen);
  if (!skb)
    return D_NO_MEMORY;

  /* Copy user data.  This is only required if it spans multiple pages.  */
    {
      skb->len = skblen;
      skb->tail = skb->data + skblen;
      skb->end = skb->tail;
      
      memcpy (skb->data, data, count);
//      memcpy (skb->data,
//	      ((void *) copy->cpy_page_list[0]->phys_addr
//	       + (copy->offset & PAGE_MASK)),
//	      amt);
//      count -= amt;
//      p = skb->data + amt;
//      for (i = 1; count > 0 && i < copy->cpy_npages; i++)
//	{
//	  amt = PAGE_SIZE;
//	  if (amt > count)
//	    amt = count;
//	  memcpy (p, (void *) copy->cpy_page_list[i]->phys_addr, amt);
//	  count -= amt;
//	  p += amt;
//	}
//
//      assert (count == 0);

//      vm_map_copy_discard (copy);
      vm_deallocate (mach_task_self (), (vm_address_t) data, count);
    }

  skb->dev = dev;
  skb->reply = reply_port;
  skb->reply_type = reply_port_type;

  /* Queue packet for transmission and schedule a software interrupt.  */
  // TODO should I give any protection here?
//  s = splimp ();
  if (dev->buffs[0].next != (struct sk_buff *) &dev->buffs[0]
      || (*dev->hard_start_xmit) (skb, dev))
    {
      __skb_queue_tail (&dev->buffs[0], skb);
//      mark_bh (NET_BH);
    }
//  splx (s);

  /* Send packet to filters.  */
  // TODO should I deliver the packet to other network stacks?
//  {
//    struct packet_header *packet;
//    struct ether_header *header;
//    ipc_kmsg_t kmsg;
//
//    kmsg = net_kmsg_get ();
//
//    if (kmsg != IKM_NULL)
//      {
//        /* Suitable for Ethernet only.  */
//        header = (struct ether_header *) (net_kmsg (kmsg)->header);
//        packet = (struct packet_header *) (net_kmsg (kmsg)->packet);
//        memcpy (header, skb->data, sizeof (struct ether_header));
//
//        /* packet is prefixed with a struct packet_header,
//           see include/device/net_status.h.  */
//        memcpy (packet + 1, skb->data + sizeof (struct ether_header),
//                skb->len - sizeof (struct ether_header));
//        packet->length = skb->len - sizeof (struct ether_header)
//                         + sizeof (struct packet_header);
//        packet->type = header->ether_type;
//        net_kmsg (kmsg)->sent = TRUE; /* Mark packet as sent.  */
//        s = splimp ();
//        net_packet (&dev->net_data->ifnet, kmsg, packet->length,
//                    ethernet_priority (kmsg));
//        splx (s);
//      }
//  }

  return MIG_NO_REPLY;
}

/*
 * Other network operations
 */
io_return_t
net_getstat(dev, flavor, status, count)
	struct linux_device	*dev;
	dev_flavor_t	flavor;
	dev_status_t	status;		/* pointer to OUT array */
	natural_t	*count;		/* OUT */
{
#define ETHERMTU 1500
	switch (flavor) {
	    case NET_STATUS:
	    {
		register struct net_status *ns = (struct net_status *)status;

		if (*count < NET_STATUS_COUNT)
		    return (D_INVALID_OPERATION);
		
		ns->min_packet_size = 60;
		ns->max_packet_size = ETH_HLEN + ETHERMTU;
		ns->header_format   = HDR_ETHERNET;
		ns->header_size	    = ETH_HLEN;
		ns->address_size    = ETH_ALEN;
		ns->flags	    = 0;
		ns->mapped_size	    = 0;

		*count = NET_STATUS_COUNT;
		break;
	    }
	    case NET_ADDRESS:
	    {
		register int	addr_byte_count;
		register int	addr_int_count;
		register int	i;

		addr_byte_count = ETH_ALEN;
		addr_int_count = (addr_byte_count + (sizeof(int)-1))
					 / sizeof(int);

		if (*count < addr_int_count)
		{
		  /* XXX debug hack. */
		  printf ("net_getstat: count: %d, addr_int_count: %d\n",
			  *count, addr_int_count);
		  return (D_INVALID_OPERATION);
		}

		memcpy(status, dev->dev_addr, addr_byte_count);
		if (addr_byte_count < addr_int_count * sizeof(int))
		    memset((char *)status + addr_byte_count, 0, 
			  (addr_int_count * sizeof(int)
				      - addr_byte_count));

		for (i = 0; i < addr_int_count; i++) {
		    register int word;

		    word = status[i];
		    status[i] = htonl(word);
		}
		*count = addr_int_count;
		break;
	    }
	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}

static io_return_t
device_get_status (void *d, dev_flavor_t flavor, dev_status_t status,
		   mach_msg_type_number_t *count)
{
  struct net_data *net = (struct net_data *) d;

  if (flavor == NET_FLAGS)
    {
      if (*count != sizeof(short))
	return D_INVALID_SIZE;

      *(short *) status = net->dev->flags;
      return D_SUCCESS;
    }

//  if(flavor >= SIOCIWFIRST && flavor <= SIOCIWLAST)
//    {
//      /* handle wireless ioctl */
//      if(! IW_IS_GET(flavor))
//	return D_INVALID_OPERATION;
//
//      if(*count * sizeof(int) < sizeof(struct ifreq))
//	return D_INVALID_OPERATION;
//
//      struct net_data *nd = d;
//      struct linux_device *dev = nd->dev;
//
//      if(! dev->do_ioctl)
//	return D_INVALID_OPERATION;
//
//      int result;
//
//      if (flavor == SIOCGIWRANGE || flavor == SIOCGIWENCODE
//	  || flavor == SIOCGIWESSID || flavor == SIOCGIWNICKN
//	  || flavor == SIOCGIWSPY)
//	{
//	  /*
//	   * These ioctls require an `iw_point' as their argument (i.e.
//	   * they want to return some data to userspace. 
//	   * Therefore supply some sane values and carry the data back
//	   * to userspace right behind the `struct iwreq'.
//	   */
//	  struct iw_point *iwp = &((struct iwreq *) status)->u.data;
//	  iwp->length = *count * sizeof (dev_status_t) - sizeof (struct ifreq);
//	  iwp->pointer = (void *) status + sizeof (struct ifreq);
//
//	  result = dev->do_ioctl (dev, (struct ifreq *) status, flavor);
//
//	  *count = ((sizeof (struct ifreq) + iwp->length)
//		    / sizeof (dev_status_t));
//	  if (iwp->length % sizeof (dev_status_t))
//	    (*count) ++;
//	}
//      else
//	{
//	  *count = sizeof(struct ifreq) / sizeof(int);
//	  result = dev->do_ioctl(dev, (struct ifreq *) status, flavor);
//	}
//
//      return result ? D_IO_ERROR : D_SUCCESS;
//    }
//  else
    {
      /* common get_status request */
      return net_getstat (net->dev, flavor, status, count);
    }
}

/*
 *      Change the flags of device DEV to FLAGS.
 */
int dev_change_flags (struct linux_device *dev, short flags)
{
//    if (securelevel > 0)
        flags &= ~LINUX_IFF_PROMISC;

    /*
     *    Set the flags on our device.
     */

    dev->flags = (flags &
            (LINUX_IFF_BROADCAST | LINUX_IFF_DEBUG | LINUX_IFF_LOOPBACK |
             LINUX_IFF_POINTOPOINT | LINUX_IFF_NOTRAILERS | LINUX_IFF_RUNNING |
             LINUX_IFF_NOARP | LINUX_IFF_PROMISC | LINUX_IFF_ALLMULTI
	     | LINUX_IFF_SLAVE | LINUX_IFF_MASTER | LINUX_IFF_MULTICAST))
        | (dev->flags & (LINUX_IFF_SOFTHEADERS|LINUX_IFF_UP));

    /* The flags are taken into account (multicast, promiscuous, ...)
       in the set_multicast_list handler. */
    if ((dev->flags & LINUX_IFF_UP) && dev->set_multicast_list != NULL)
        dev->set_multicast_list (dev);

    return 0;
}


static io_return_t
device_set_status(void *d, dev_flavor_t flavor, dev_status_t status,
		  mach_msg_type_number_t count)
{
  if (flavor == NET_FLAGS)
    {
      if (count != sizeof(short))
        return D_INVALID_SIZE;

      short flags = *(short *) status;
      struct net_data *net = (struct net_data *) d;

      dev_change_flags (net->dev, flags);

      /* Change the flags of the Mach device, too. */
//      net->ifnet.if_flags = net->dev->flags;
      return D_SUCCESS;
    }
    return D_INVALID_OPERATION;

//  if(flavor < SIOCIWFIRST || flavor > SIOCIWLAST)
//    return D_INVALID_OPERATION;
//
//  if(! IW_IS_SET(flavor))
//    return D_INVALID_OPERATION;
//  
//  if(count * sizeof(int) < sizeof(struct ifreq))
//    return D_INVALID_OPERATION;
//
//  struct net_data *nd = d;
//  struct linux_device *dev = nd->dev;
//
//  if(! dev->do_ioctl)
//    return D_INVALID_OPERATION;
//
//  if((flavor == SIOCSIWENCODE || flavor == SIOCSIWESSID
//      || flavor == SIOCSIWNICKN || flavor == SIOCSIWSPY)
//     && ((struct iwreq *) status)->u.data.pointer)
//    {
//      struct iw_point *iwp = &((struct iwreq *) status)->u.data;
//
//      /* safety check whether the status array is long enough ... */
//      if(count * sizeof(int) < sizeof(struct ifreq) + iwp->length)
//	return D_INVALID_OPERATION;
//
//      /* make sure, iwp->pointer points to the correct address */
//      if(iwp->pointer) iwp->pointer = (void *) status + sizeof(struct ifreq);
//    }
//  
//  int result = dev->do_ioctl(dev, (struct ifreq *) status, flavor);
//  return result ? D_IO_ERROR : D_SUCCESS;
}


static io_return_t
device_set_filter (void *d, mach_port_t port, int priority,
		   filter_t * filter, unsigned filter_count)
{
  ((struct net_data *) d)->delivery_port = port;
  return 0;
//  return net_set_filter (&((struct net_data *) d)->ifnet,
//			 port, priority, filter, filter_count);
}

struct device_emulation_ops linux_net_emulation_ops =
{
  NULL,
  NULL,
  dev_to_port,
  device_open,
  NULL,
  device_write,
  NULL,
  NULL,
  NULL,
  device_set_status,
  device_get_status,
  device_set_filter,
  NULL,
  NULL,
  NULL,
  NULL
};

/* Do any initialization required for network devices.  */
void
linux_net_emulation_init ()
{
  skb_queue_head_init (&skb_done_list);
}
