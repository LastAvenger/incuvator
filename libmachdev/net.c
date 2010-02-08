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
#include <string.h>
#include <arpa/inet.h>

#include "mach_U.h"

#include <mach.h>
#include <hurd.h>

#define MACH_INCLUDE

#include "vm_param.h"
#include "device_reply_U.h"
#include "dev_hdr.h"
#include "if_ether.h"
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
  struct emul_device device;	/* generic device structure */
  mach_port_t delivery_port;
  struct net_device *dev;	/* Linux network device structure */
  struct net_data *next;
};

struct skb_reply
{
  mach_port_t reply;
  mach_msg_type_name_t reply_type;
  int pkglen;
};

struct sk_buff;
void skb_done_queue(struct sk_buff *skb);
struct sk_buff *skb_done_dequeue();
void linux_net_emulation_init ();
void *skb_reply(struct sk_buff *skb);
int netdev_flags(struct net_device *dev);
char *netdev_addr(struct net_device *dev);
int dev_change_flags (struct net_device *dev, short flags);
int linux_pkg_xmit (char *pkg_data, int len, void *del_data,
		    int (*del_func) (struct sk_buff *, void *),
		    struct net_device *dev);
struct net_device *search_netdev (char *name);
void kfree_skb (struct sk_buff *skb);
int dev_open(struct net_device *dev);
void *l4dde26_register_rx_callback(void *cb);
void skb_done_head_init();

struct net_data *nd_head;

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

struct net_data *search_nd (struct net_device *dev)
{
  struct net_data *nd = nd_head;

  //TODO protected by locks.
  while (nd)
    {
      if (nd->dev == dev)
	return nd;
      nd = nd->next;
    }
  return NULL;
}

/* Linux kernel network support routines.  */

/* actions before freeing the sk_buff SKB.
 * If it returns 1, the packet will be deallocated later. */
int 
pre_kfree_skb (struct sk_buff *skb, void *data)
{
  struct skb_reply *reply = data;
  extern void wakeup_io_done_thread ();

  /* Queue sk_buff on done list if there is a
     page list attached or we need to send a reply.
     Wakeup the iodone thread to process the list.  */
  if (reply && MACH_PORT_VALID (reply->reply))
    {
      if (MACH_PORT_VALID (reply->reply))
	{
	  ds_device_write_reply (reply->reply, reply->reply_type,
				 0, reply->pkglen);
	  reply->reply = MACH_PORT_NULL;
	}
    }
  /* deallocate skb_reply before freeing the packet. */
  free (data);
  return 0;
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
      return err;
    }

  return MACH_MSG_SUCCESS;
}

/* Accept packet SKB received on an interface.  */
void
netif_rx_handle (char *data, int len, struct net_device *dev)
{
  int pack_size;
  net_rcv_msg_t net_msg;
  struct ether_header *eh;
  struct packet_header *ph;
  struct net_data *nd;

  if (print_packet_size)
    printf ("netif_rx: length %d\n", len);

  nd = search_nd(dev);
  assert (nd);

  /* Allocate a kernel message buffer.  */
  net_msg = malloc (sizeof (*net_msg));
  if (!net_msg)
    return;

  pack_size = len - sizeof (struct ethhdr);
  /* remember message sizes must be rounded up */
  net_msg->msg_hdr.msgh_size = (((mach_msg_size_t) (sizeof(struct net_rcv_msg)
					       - NET_RCV_MAX + pack_size)) + 3) & ~3;

  /* Copy packet into message buffer.  */
  eh = (struct ether_header *) (net_msg->header);
  ph = (struct packet_header *) (net_msg->packet);
  memcpy (eh, data, sizeof (struct ether_header));
  /* packet is prefixed with a struct packet_header,
     see include/device/net_status.h.  */
  memcpy (ph + 1, data + sizeof (struct ether_header), pack_size);
  ph->type = eh->h_proto;
  ph->length = pack_size + sizeof (struct packet_header);

  net_msg->sent = FALSE; /* Mark packet as received.  */

  net_msg->header_type = header_type;
  net_msg->packet_type = packet_type;
  net_msg->net_rcv_msg_packet_count = ph->length;
  deliver_msg (nd->delivery_port, net_msg);
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

#if 0     
/*    
 * Initialize send and receive queues on an interface.
 */   
void if_init_queues(ifp)
     register struct ifnet *ifp;
{     
  IFQ_INIT(&ifp->if_snd);
  queue_init(&ifp->if_rcv_port_list);
  queue_init(&ifp->if_snd_port_list);
  simple_lock_init(&ifp->if_rcv_port_list_lock);
  simple_lock_init(&ifp->if_snd_port_list_lock);
}
#endif

static io_return_t
device_open (mach_port_t reply_port, mach_msg_type_name_t reply_port_type,
	     dev_mode_t mode, char *name, device_t *devp)
{
  io_return_t err = D_SUCCESS;
  struct net_device *dev;
  struct net_data *nd;
//  struct ifnet *ifp;

  /* Search for the device.  */
  dev = search_netdev (name);
  if (!dev)
    return D_NO_SUCH_DEVICE;

  /* Allocate and initialize device data if this is the first open.  */
  nd = search_nd (dev);
  if (!nd)
    {
      err = create_device_port (sizeof (*nd), &nd);
      if (err)
	goto out;
	
      nd->dev = dev;
      nd->device.emul_data = nd;
      nd->device.emul_ops = &linux_net_emulation_ops;
      nd->next = nd_head;
      nd_head = nd;
#if 0
      ipc_kobject_set (nd->port, (ipc_kobject_t) & nd->device, IKOT_DEVICE);
      notify = ipc_port_make_sonce (nd->port);
      ip_lock (nd->port);
      ipc_port_nsrequest (nd->port, 1, notify, &notify);
      assert (notify == IP_NULL);

      ifp = &nd->ifnet;
      ifp->if_unit = dev->name[strlen (dev->name) - 1] - '0';
      ifp->if_flags = IFF_UP | IFF_RUNNING;
      ifp->if_mtu = dev->mtu;
      ifp->if_header_size = dev->hard_header_len;
      ifp->if_header_format = dev->type;
      ifp->if_address_size = dev->addr_len;
      ifp->if_address = dev->dev_addr;
      if_init_queues (ifp);
#endif

      if (dev_open(dev) < 0)
	err = D_NO_SUCH_DEVICE;

    out:
      if (err)
	{
	  if (nd)
	    {
	      ports_destroy_right (nd);
	      nd = NULL;
	    }
	}
      else
	{
#if 0
	  /* IPv6 heavily relies on multicasting (especially router and
	     neighbor solicits and advertisements), so enable reception of
	     those multicast packets by setting `LINUX_IFF_ALLMULTI'.  */
	  dev->flags |= LINUX_IFF_UP | LINUX_IFF_RUNNING | LINUX_IFF_ALLMULTI;
	  skb_queue_head_init (&dev->buffs[0]);

	  if (dev->set_multicast_list)
	    dev->set_multicast_list (dev);
#endif
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
  struct net_data *nd = d;
  struct net_device *dev = nd->dev;
  struct skb_reply *skb_reply = malloc (sizeof (*skb_reply));
  error_t err;

  if (skb_reply == NULL)
    return D_NO_MEMORY;

  skb_reply->pkglen = count;
  skb_reply->reply = reply_port;
  skb_reply->reply_type = reply_port_type;

  err = linux_pkg_xmit (data, count, skb_reply, pre_kfree_skb, dev);
  vm_deallocate (mach_task_self (), (vm_address_t) data, count);
  if (err)
    return err;

  /* Send packet to filters.  */
  // TODO should I deliver the packet to other network stacks?
#if 0
  {
    struct packet_header *packet;
    struct ether_header *header;
    ipc_kmsg_t kmsg;

    kmsg = net_kmsg_get ();

    if (kmsg != IKM_NULL)
      {
        /* Suitable for Ethernet only.  */
        header = (struct ether_header *) (net_kmsg (kmsg)->header);
        packet = (struct packet_header *) (net_kmsg (kmsg)->packet);
        memcpy (header, skb->data, sizeof (struct ether_header));

        /* packet is prefixed with a struct packet_header,
           see include/device/net_status.h.  */
        memcpy (packet + 1, skb->data + sizeof (struct ether_header),
                skb->len - sizeof (struct ether_header));
        packet->length = skb->len - sizeof (struct ether_header)
                         + sizeof (struct packet_header);
        packet->type = header->ether_type;
        net_kmsg (kmsg)->sent = TRUE; /* Mark packet as sent.  */
        s = splimp ();
        net_packet (&dev->net_data->ifnet, kmsg, packet->length,
                    ethernet_priority (kmsg));
        splx (s);
      }
  }
#endif

  return MIG_NO_REPLY;
}

/*
 * Other network operations
 */
io_return_t
net_getstat(dev, flavor, status, count)
	struct net_device	*dev;
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

		memcpy(status, netdev_addr(dev), addr_byte_count);
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

      *(short *) status = netdev_flags (net->dev);
      return D_SUCCESS;
    }

#if 0
  if(flavor >= SIOCIWFIRST && flavor <= SIOCIWLAST)
    {
      /* handle wireless ioctl */
      if(! IW_IS_GET(flavor))
	return D_INVALID_OPERATION;

      if(*count * sizeof(int) < sizeof(struct ifreq))
	return D_INVALID_OPERATION;

      struct net_data *nd = d;
      struct linux_device *dev = nd->dev;

      if(! dev->do_ioctl)
	return D_INVALID_OPERATION;

      int result;

      if (flavor == SIOCGIWRANGE || flavor == SIOCGIWENCODE
	  || flavor == SIOCGIWESSID || flavor == SIOCGIWNICKN
	  || flavor == SIOCGIWSPY)
	{
	  /*
	   * These ioctls require an `iw_point' as their argument (i.e.
	   * they want to return some data to userspace. 
	   * Therefore supply some sane values and carry the data back
	   * to userspace right behind the `struct iwreq'.
	   */
	  struct iw_point *iwp = &((struct iwreq *) status)->u.data;
	  iwp->length = *count * sizeof (dev_status_t) - sizeof (struct ifreq);
	  iwp->pointer = (void *) status + sizeof (struct ifreq);

	  result = dev->do_ioctl (dev, (struct ifreq *) status, flavor);

	  *count = ((sizeof (struct ifreq) + iwp->length)
		    / sizeof (dev_status_t));
	  if (iwp->length % sizeof (dev_status_t))
	    (*count) ++;
	}
      else
	{
	  *count = sizeof(struct ifreq) / sizeof(int);
	  result = dev->do_ioctl(dev, (struct ifreq *) status, flavor);
	}

      return result ? D_IO_ERROR : D_SUCCESS;
    }
  else
#endif
    {
      /* common get_status request */
      return net_getstat (net->dev, flavor, status, count);
    }
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

      return D_SUCCESS;
    }
    return D_INVALID_OPERATION;

#if 0
  if(flavor < SIOCIWFIRST || flavor > SIOCIWLAST)
    return D_INVALID_OPERATION;

  if(! IW_IS_SET(flavor))
    return D_INVALID_OPERATION;
  
  if(count * sizeof(int) < sizeof(struct ifreq))
    return D_INVALID_OPERATION;

  struct net_data *nd = d;
  struct linux_device *dev = nd->dev;

  if(! dev->do_ioctl)
    return D_INVALID_OPERATION;

  if((flavor == SIOCSIWENCODE || flavor == SIOCSIWESSID
      || flavor == SIOCSIWNICKN || flavor == SIOCSIWSPY)
     && ((struct iwreq *) status)->u.data.pointer)
    {
      struct iw_point *iwp = &((struct iwreq *) status)->u.data;

      /* safety check whether the status array is long enough ... */
      if(count * sizeof(int) < sizeof(struct ifreq) + iwp->length)
	return D_INVALID_OPERATION;

      /* make sure, iwp->pointer points to the correct address */
      if(iwp->pointer) iwp->pointer = (void *) status + sizeof(struct ifreq);
    }
  
  int result = dev->do_ioctl(dev, (struct ifreq *) status, flavor);
  return result ? D_IO_ERROR : D_SUCCESS;
#endif
}


static io_return_t
device_set_filter (void *d, mach_port_t port, int priority,
		   filter_t * filter, unsigned filter_count)
{
  ((struct net_data *) d)->delivery_port = port;
  return 0;
#if 0
  return net_set_filter (&((struct net_data *) d)->ifnet,
			 port, priority, filter, filter_count);
#endif
}

/* Do any initialization required for network devices.  */
void linux_net_emulation_init ()
{
  skb_done_head_init();
  l4dde26_register_rx_callback(netif_rx_handle);
}

struct device_emulation_ops linux_net_emulation_ops =
{
  linux_net_emulation_init,
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
