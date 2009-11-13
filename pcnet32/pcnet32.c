/* pcnet32.c: An AMD PCnet32 ethernet driver for linux. */
/*
 *  Copyright 1996,97 Thomas Bogendoerfer, 1993-1995,1998 Donald Becker
 * 	Copyright 1993 United States Government as represented by the
 * 	Director, National Security Agency.
 *
 * 	Derived from the lance driver written 1993-1995 by Donald Becker.
 *
 * 	This software may be used and distributed according to the terms
 * 	of the GNU Public License, incorporated herein by reference.
 *
 * 	This driver is for AMD PCnet-PCI based ethercards
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <pciaccess.h>

#include "mach_U.h"

#include <sys/io.h>
#include <hurd.h>
#include <mach.h>
#include <cthreads.h>

#include "linux-types.h"
#include "if_ether.h"
#include "pci.h"
#include "netdevice.h"
#include "skbuff.h"
#include "bitops.h"
#include "skbuff.h"
#include "irq.h"
#include "util.h"

#include "device_U.h"

static const char *version = "pcnet32.c:v0.99B 4/4/98 DJBecker/TSBogend.\n";

/* A few user-configurable values. */

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;

/*
 * Set the number of Tx and Rx buffers, using Log_2(# buffers).
 * Reasonable default values are 4 Tx buffers, and 16 Rx buffers.
 * That translates to 2 (4 == 2^^2) and 4 (16 == 2^^4).
 */
#define PCNET_LOG_TX_BUFFERS 4
#define PCNET_LOG_RX_BUFFERS 4

/* Driver verbosity level.  0 = no messages, 7 = wordy death.
   Modify here, or when loading as a module. */
static int pcnet32_debug = 1;

/*
 * 				Theory of Operation
 *
 * This driver uses the same software structure as the normal lance
 * driver. So look for a verbose description in lance.c. The differences
 * to the normal lance driver is the use of the 32bit mode of PCnet32
 * and PCnetPCI chips. Because these chips are 32bit chips, there is no
 * 16MB limitation and we don't need bounce buffers.
 */

/*
 * History:
 * v0.01:  Initial version
 *         only tested on Alpha Noname Board
 * v0.02:  changed IRQ handling for new interrupt scheme (dev_id)
 *         tested on a ASUS SP3G
 * v0.10:  fixed an odd problem with the 79C794 in a Compaq Deskpro XL
 *         looks like the 974 doesn't like stopping and restarting in a
 *         short period of time; now we do a reinit of the lance; the
 *         bug was triggered by doing ifconfig eth0 <ip> broadcast <addr>
 *         and hangs the machine (thanks to Klaus Liedl for debugging)
 * v0.12:  by suggestion from Donald Becker: Renamed driver to pcnet32,
 *         made it standalone (no need for lance.c)
 * v0.13:  added additional PCI detecting for special PCI devices (Compaq)
 * v0.14:  stripped down additional PCI probe (thanks to David C Niemi
 *         and sveneric@xs4all.nl for testing this on their Compaq boxes)
 * v0.15:  added 79C965 (VLB) probe
 *         added interrupt sharing for PCI chips
 * v0.16:  fixed set_multicast_list on Alpha machines
 * v0.17:  removed hack from dev.c; now pcnet32 uses ethif_probe in Space.c
 * v0.19:  changed setting of autoselect bit
 * v0.20:  removed additional Compaq PCI probe; there is now a working one
 *	   in arch/i386/bios32.c
 * v0.21:  added endian conversion for ppc, from work by cort@cs.nmt.edu
 * v0.22:  added printing of status to ring dump
 * v0.23:  changed enet_statistics to net_devive_stats
 * v0.99: Changes for 2.0.34 final release. -djb
 */

void netif_rx (struct sk_buff *skb);

#ifndef __powerpc__
#define le16_to_cpu(val)  (val)
#define le32_to_cpu(val)  (val)
#endif
#if (LINUX_VERSION_CODE < 0x20123)
//#define test_and_set_bit(val, addr) set_bit(val, addr)
#endif

#define printk(format, ...) do				\
{							\
  fprintf (stderr , format, ## __VA_ARGS__);		\
  fflush (stderr);					\
} while (0)

// convertion between the virtual address and the physical address
#define LP_VIRT_TO_BUS(addr,lp) ((vm_address_t) (addr) - (vm_address_t) (lp) + (lp)->paddr)
#define BUF_VIRT_TO_BUS(addr,lp) ((vm_address_t) (addr) - (lp)->rx_buffs + (lp)->rx_buffs_paddr)
#define BUF_BUS_TO_VIRT(addr,lp) ((vm_address_t) (addr) - (lp)->rx_buffs_paddr + (lp)->rx_buffs)
#define SKB_VIRT_TO_BUS(addr,skb) virt_to_phys (addr)

#define eth_copy_and_sum(dest, src, length, base) \
          memcpy((dest)->data, src, length)

// TODO I hope it is OK.
#define eth_type_trans(skb,dev) 0

#define TX_RING_SIZE			(1 << (PCNET_LOG_TX_BUFFERS))
#define TX_RING_MOD_MASK		(TX_RING_SIZE - 1)
#define TX_RING_LEN_BITS		((PCNET_LOG_TX_BUFFERS) << 12)

#define RX_RING_SIZE			(1 << (PCNET_LOG_RX_BUFFERS))
#define RX_RING_MOD_MASK		(RX_RING_SIZE - 1)
#define RX_RING_LEN_BITS		((PCNET_LOG_RX_BUFFERS) << 4)

#define PKT_BUF_SZ		1544

/* Offsets from base I/O address. */
enum pcnet_offsets { PCNET32_DATA=0x10, PCNET32_ADDR=0x12, PCNET32_RESET=0x14,
					 PCNET32_BUS_IF=0x16,};
#define PCNET32_TOTAL_SIZE 0x20

/* The PCNET32 Rx and Tx ring descriptors. */
struct pcnet32_rx_head {
	u32 base;
	s16 buf_length;
	s16 status;
	u32 msg_length;
	u32 reserved;
};

struct pcnet32_tx_head {
	u32 base;
	s16 length;
	s16 status;
	u32 misc;
	u32 reserved;
};

/* The PCNET32 32-Bit initialization block, described in databook. */
struct pcnet32_init_block {
	u16 mode;
	u16 tlen_rlen;
	u8  phys_addr[6];
	u16 reserved;
	u32 filter[2];
	/* Receive and transmit ring base, along with extra bits. */
	u32 rx_ring;
	u32 tx_ring;
};

struct pcnet32_private {
	/* The Tx and Rx ring entries must be aligned on 16-byte boundaries
	   in 32bit mode. */
	struct pcnet32_rx_head   rx_ring[RX_RING_SIZE];
	struct pcnet32_tx_head   tx_ring[TX_RING_SIZE];
	struct pcnet32_init_block	init_block;
	const char *name;
	struct device *next_module;
	/* The saved address of a sent-in-place packet/buffer, for skfree(). */
	struct sk_buff* tx_skbuff[TX_RING_SIZE];
	unsigned long rx_buffs;		/* Address of Rx and Tx buffers. */
	vm_address_t rx_buffs_paddr;	/* The physical address of the buffers above */
	int cur_rx, cur_tx;			/* The next free ring entry */
	int dirty_rx, dirty_tx;		/* The ring entries to be free()ed. */
	struct enet_statistics stats;
	char tx_full;
	unsigned long lock;

	/* The physical address of the structure. */
	vm_address_t paddr;
};

static struct pcnet_chip_type {
	int id_number;
	const char *name;
	int flags;
} chip_table[] = {
	{0x2420, "PCnet/PCI 79C970", 0},
	{0x2430, "PCnet32", 0},
	{0x2621, "PCnet/PCI II 79C970A", 0},
	{0x2623, "PCnet/FAST 79C971", 0},
	{0x2624, "PCnet/FAST+ 79C972", 0},
	{0x0, 	 "PCnet32 (unknown)", 0},
};

/* Index of functions. */
int  pcnet32_probe(struct device *dev);
void pcnet32_interrupt(int irq);
static int  pcnet32_probe1(struct device *dev, unsigned int ioaddr, unsigned char irq_line);
static int  pcnet32_open(struct device *dev);
static void pcnet32_init_ring(struct device *dev);
static int  pcnet32_start_xmit(struct sk_buff *skb, struct device *dev);
static int  pcnet32_rx(struct device *dev);
static int  pcnet32_close(struct device *dev);
static struct enet_statistics *pcnet32_get_stats(struct device *dev);
static void pcnet32_set_multicast_list(struct device *dev);

struct device * init_etherdev(struct device *dev, int sizeof_priv);

mach_port_t master_device;
mach_port_t priv_host;

struct device *ether_dev;

struct mutex global_lock = MUTEX_INITIALIZER;
struct mutex skb_queue_lock = MUTEX_INITIALIZER;


/* A list of all installed PCnet32 devices, for removing the driver module. */
static struct device *root_pcnet32_dev = NULL;

int check_region(unsigned int from, unsigned int num)
{
	// check the ioport region before probing
	// it isn't needed for this test program.
	return 0;
}

void request_region(unsigned int from, unsigned int num, const char *name)
{
}

int pcnet32_probe (struct device *dev)
{
	int cards_found = 0;
	error_t err;
	struct pci_device *pci_dev;
	struct pci_device_iterator *dev_iter;

	dev_iter = pci_slot_match_iterator_create (NULL);
	while ((pci_dev = pci_device_next (dev_iter)) != NULL) {
		u8 irq_line;
		u16 pci_command, new_command;
		u32 pci_ioaddr;

		if (pci_dev->vendor_id != PCI_VENDOR_ID_AMD)
			continue;
		if (pci_dev->device_id != PCI_DEVICE_ID_AMD_LANCE)
			continue;

		err = pci_device_cfg_read_u8 (pci_dev, &irq_line,
					      PCI_INTERRUPT_LINE);
		if (err) {
			error (0, err, "pci_device_cfg_read");
			break;
		}

		err = pci_device_cfg_read_u32 (pci_dev, &pci_ioaddr,
					       PCI_BASE_ADDRESS_0);
		if (err) {
			error (0, err, "pci_device_cfg_read");
			break;
		}
		/* Remove I/O space marker in bit 0. */
		pci_ioaddr &= ~3;

		/* Avoid already found cards from previous pcnet32_probe() calls */
		if (check_region(pci_ioaddr, PCNET32_TOTAL_SIZE))
			continue;

		/* Activate the card: fix for brain-damaged Win98 BIOSes. */
		err = pci_device_cfg_read_u16 (pci_dev, &pci_command,
					       PCI_COMMAND);
		if (err) {
			error (0, err, "pci_device_cfg_read");
			break;
		}
		new_command = pci_command | PCI_COMMAND_MASTER|PCI_COMMAND_IO;
		if (pci_command != new_command) {
			printk("  The PCI BIOS has not enabled the AMD Ethernet"
				   " device at %2x-%2x."
				   "  Updating PCI command %4.4x->%4.4x.\n",
				   pci_dev->bus, pci_dev->func,
				   pci_command, new_command);
			err = pci_device_cfg_write_u16 (pci_dev, new_command,
							PCI_COMMAND);
			if (err) {
			    error (0, err, "pci_device_cfg_write");
			    break;
			}
		}

		if (pcnet32_probe1(dev, pci_ioaddr, irq_line) != 0) {
			/* Should never happen. */
			printk("pcnet32.c: Probe of PCI card at %#x failed.\n",
				   pci_ioaddr);
		} else
			dev = 0;
		cards_found++;
	}
	pci_iterator_destroy (dev_iter);

	return cards_found ? 0 : -ENODEV;
}


/* pcnet32_probe1 */
static int pcnet32_probe1(struct device *dev, unsigned int ioaddr, unsigned char irq_line)
{
	struct pcnet32_private *lp;
	int i;
	const char *chipname;
	vm_address_t lp_vaddr;
	vm_address_t lp_paddr;
	vm_address_t buf_vaddr;
	vm_address_t buf_paddr;
	error_t err;

	/* Make all io ports accessible. */
	if (ioperm (ioaddr, PCNET32_TOTAL_SIZE, 1) < 0)
	  return ENODEV;

	/* check if there is really a pcnet chip on that ioaddr */
	if ((inb(ioaddr + 14) != 0x57) || (inb(ioaddr + 15) != 0x57))
		return ENODEV;

	inw(ioaddr+PCNET32_RESET); /* Reset the PCNET32 */

	outw(0x0000, ioaddr+PCNET32_ADDR); /* Switch to window 0 */
	if (inw(ioaddr+PCNET32_DATA) != 0x0004)
		return ENODEV;

	/* Get the version of the chip. */
	outw(88, ioaddr+PCNET32_ADDR);
	if (inw(ioaddr+PCNET32_ADDR) != 88) {
		/* should never happen */
		return ENODEV;
	} else {			/* Good, it's a newer chip. */
		int chip_version = inw(ioaddr+PCNET32_DATA);
		outw(89, ioaddr+PCNET32_ADDR);
		chip_version |= inw(ioaddr+PCNET32_DATA) << 16;
		if (pcnet32_debug > 2)
			printk("  PCnet chip version is %#x.\n", chip_version);
		if ((chip_version & 0xfff) != 0x003)
			return ENODEV;
		chip_version = (chip_version >> 12) & 0xffff;
		for (i = 0; chip_table[i].id_number; i++)
			if (chip_table[i].id_number == chip_version)
				break;
		chipname = chip_table[i].name;
	}

	dev = init_etherdev(dev, 0);
	ether_dev = dev;

	printk("%s: %s at %#3x,", dev->name, chipname, ioaddr);

	/* There is a 16 byte station address PROM at the base address.
	   The first six bytes are the station address. */
	for (i = 0; i < 6; i++)
		printk(" %2.2x", dev->dev_addr[i] = inb(ioaddr + i));
		
	printk("\n");

	dev->base_addr = ioaddr;
	request_region(ioaddr, PCNET32_TOTAL_SIZE, dev->name);

	/* Data structures used by the PCnet32 are 16byte aligned and DMAble. */
	err = vm_dma_buff_alloc (priv_host, mach_task_self (),
				 sizeof(*lp)+15, &lp_vaddr, &lp_paddr);
	if (err)
	  debug ("cannot allocate memory for the DMA buffer.");
	lp = (struct pcnet32_private *) lp_vaddr;
	//		(((unsigned long)kmalloc(sizeof(*lp)+15, GFP_DMA | GFP_KERNEL)+15) & ~15);

	memset(lp, 0, sizeof(*lp));
	dev->priv = lp;
	lp->paddr = lp_paddr;

	lp->next_module = root_pcnet32_dev;
	root_pcnet32_dev = dev;

    lp->name = chipname;
    
    err = vm_dma_buff_alloc (priv_host, mach_task_self (),
			     PKT_BUF_SZ*RX_RING_SIZE, &buf_vaddr, &buf_paddr);
    if (err)
      debug ("cannot allocate memory for the DMA buffer.");
    lp->rx_buffs = buf_vaddr;
    lp->rx_buffs_paddr = buf_paddr;
//    lp->rx_buffs = (unsigned long) kmalloc(PKT_BUF_SZ*RX_RING_SIZE, GFP_DMA | GFP_KERNEL);

    lp->init_block.mode = le16_to_cpu(0x0003);	/* Disable Rx and Tx. */
    lp->init_block.tlen_rlen = le16_to_cpu(TX_RING_LEN_BITS | RX_RING_LEN_BITS);
    for (i = 0; i < 6; i++)
      lp->init_block.phys_addr[i] = dev->dev_addr[i];
    lp->init_block.filter[0] = 0x00000000;
    lp->init_block.filter[1] = 0x00000000;
//    lp->init_block.rx_ring = (u32)le32_to_cpu(virt_to_bus(lp->rx_ring));
//    lp->init_block.tx_ring = (u32)le32_to_cpu(virt_to_bus(lp->tx_ring));
    lp->init_block.rx_ring = (u32)le32_to_cpu(LP_VIRT_TO_BUS (lp->rx_ring, lp));
    lp->init_block.tx_ring = (u32)le32_to_cpu(LP_VIRT_TO_BUS (lp->tx_ring, lp));

    /* switch pcnet32 to 32bit mode */
    outw(0x0014, ioaddr+PCNET32_ADDR);
    outw(0x0002, ioaddr+PCNET32_BUS_IF);

    outw(0x0001, ioaddr+PCNET32_ADDR);
    inw(ioaddr+PCNET32_ADDR);
    outw(LP_VIRT_TO_BUS(&lp->init_block, lp) & 0xffff, ioaddr+PCNET32_DATA);
    outw(0x0002, ioaddr+PCNET32_ADDR);
    inw(ioaddr+PCNET32_ADDR);
    outw(LP_VIRT_TO_BUS(&lp->init_block, lp) >> 16, ioaddr+PCNET32_DATA);
    outw(0x0000, ioaddr+PCNET32_ADDR);
    inw(ioaddr+PCNET32_ADDR);

    dev->irq = irq_line;

    if (pcnet32_debug > 0)
      printk(version);

    /* The PCNET32-specific entries in the device structure. */
    dev->open = &pcnet32_open;
    dev->hard_start_xmit = &pcnet32_start_xmit;
    dev->stop = &pcnet32_close;
    dev->get_stats = &pcnet32_get_stats;
    dev->set_multicast_list = &pcnet32_set_multicast_list;

    device_irq_enable (master_device, irq_line, TRUE);
    /* Fill in the generic fields of the device structure. */
    ether_setup(dev);
    return 0;
}


static int
pcnet32_open(struct device *dev)
{
#define SA_SHIRQ 0x04000000
	struct pcnet32_private *lp = (struct pcnet32_private *)dev->priv;
	unsigned int ioaddr = dev->base_addr;
	int i;

	if (dev->irq == 0 ||
		request_irq(dev, &pcnet32_interrupt, SA_SHIRQ)) {
		return -EAGAIN;
	}

	/* Reset the PCNET32 */
	inw(ioaddr+PCNET32_RESET);

	/* switch pcnet32 to 32bit mode */
	outw(0x0014, ioaddr+PCNET32_ADDR);
	outw(0x0002, ioaddr+PCNET32_BUS_IF);

	/* Turn on auto-select of media (AUI, BNC). */
	outw(0x0002, ioaddr+PCNET32_ADDR);
	/* only touch autoselect bit */
	outw(inw(ioaddr+PCNET32_BUS_IF) | 0x0002, ioaddr+PCNET32_BUS_IF);

	if (pcnet32_debug > 1)
		printk("%s: pcnet32_open() irq %d tx/rx rings %#x/%#x init %#x.\n",
			   dev->name, dev->irq,
		           (u32) LP_VIRT_TO_BUS(lp->tx_ring, lp),
		           (u32) LP_VIRT_TO_BUS(lp->rx_ring, lp),
			   (u32) LP_VIRT_TO_BUS(&lp->init_block, lp));

	/* check for ATLAS T1/E1 LAW card */
	if (dev->dev_addr[0] == 0x00 && dev->dev_addr[1] == 0xe0 && dev->dev_addr[2] == 0x75) {
		/* select GPSI mode */
		lp->init_block.mode = 0x0100;
		outw(0x0002, ioaddr+PCNET32_ADDR);
		outw(inw(ioaddr+PCNET32_BUS_IF) & ~2, ioaddr+PCNET32_BUS_IF);
		/* switch full duplex on */
		outw(0x0009, ioaddr+PCNET32_ADDR);
		outw(inw(ioaddr+PCNET32_BUS_IF) | 1, ioaddr+PCNET32_BUS_IF);
	} else
		lp->init_block.mode = 0x0000;
	lp->init_block.filter[0] = 0x00000000;
	lp->init_block.filter[1] = 0x00000000;
	pcnet32_init_ring(dev);

	/* Re-initialize the PCNET32, and start it when done. */
	outw(0x0001, ioaddr+PCNET32_ADDR);
	outw(LP_VIRT_TO_BUS(&lp->init_block, lp) &0xffff, ioaddr+PCNET32_DATA);
	outw(0x0002, ioaddr+PCNET32_ADDR);
	outw(LP_VIRT_TO_BUS(&lp->init_block, lp) >> 16, ioaddr+PCNET32_DATA);

	outw(0x0004, ioaddr+PCNET32_ADDR);
	outw(0x0915, ioaddr+PCNET32_DATA);

	outw(0x0000, ioaddr+PCNET32_ADDR);
	outw(0x0001, ioaddr+PCNET32_DATA);

	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;
	i = 0;
	while (i++ < 100)
		if (inw(ioaddr+PCNET32_DATA) & 0x0100)
			break;
	/*
	 * We used to clear the InitDone bit, 0x0100, here but Mark Stockton
	 * reports that doing so triggers a bug in the '974.
	 */
 	outw(0x0042, ioaddr+PCNET32_DATA);

	if (pcnet32_debug > 2)
		printk("%s: PCNET32 open after %d ticks, init block %#x csr0 %4.4x.\n",
			   dev->name, i, (u32) LP_VIRT_TO_BUS(&lp->init_block, lp), inw(ioaddr+PCNET32_DATA));

	return 0;					/* Always succeed */
}

/*
 * The LANCE has been halted for one reason or another (busmaster memory
 * arbitration error, Tx FIFO underflow, driver stopped it to reconfigure,
 * etc.).  Modern LANCE variants always reload their ring-buffer
 * configuration when restarted, so we must reinitialize our ring
 * context before restarting.  As part of this reinitialization,
 * find all packets still on the Tx ring and pretend that they had been
 * sent (in effect, drop the packets on the floor) - the higher-level
 * protocols will time out and retransmit.  It'd be better to shuffle
 * these skbs to a temp list and then actually re-Tx them after
 * restarting the chip, but I'm too lazy to do so right now.  dplatt@3do.com
 */

static void
pcnet32_purge_tx_ring(struct device *dev)
{
	struct pcnet32_private *lp = (struct pcnet32_private *)dev->priv;
	int i;

	for (i = 0; i < TX_RING_SIZE; i++) {
		if (lp->tx_skbuff[i]) {
			dev_kfree_skb(lp->tx_skbuff[i], FREE_WRITE);
			lp->tx_skbuff[i] = NULL;
		}
	}
}


/* Initialize the PCNET32 Rx and Tx rings. */
static void
pcnet32_init_ring(struct device *dev)
{
	struct pcnet32_private *lp = (struct pcnet32_private *)dev->priv;
	int i;

	lp->lock = 0, lp->tx_full = 0;
	lp->cur_rx = lp->cur_tx = 0;
	lp->dirty_rx = lp->dirty_tx = 0;

	for (i = 0; i < RX_RING_SIZE; i++) {
		lp->rx_ring[i].base = (u32)le32_to_cpu(BUF_VIRT_TO_BUS((char *)lp->rx_buffs + i*PKT_BUF_SZ, lp));
		lp->rx_ring[i].buf_length = le16_to_cpu(-PKT_BUF_SZ);
	        lp->rx_ring[i].status = le16_to_cpu(0x8000);
	}
	/* The Tx buffer address is filled in as needed, but we do need to clear
	   the upper ownership bit. */
	for (i = 0; i < TX_RING_SIZE; i++) {
	        lp->tx_ring[i].base = 0;
	        lp->tx_ring[i].status = 0;
	}

	lp->init_block.tlen_rlen = TX_RING_LEN_BITS | RX_RING_LEN_BITS;
	for (i = 0; i < 6; i++)
		lp->init_block.phys_addr[i] = dev->dev_addr[i];
	lp->init_block.rx_ring = (u32)le32_to_cpu(LP_VIRT_TO_BUS(lp->rx_ring, lp));
	lp->init_block.tx_ring = (u32)le32_to_cpu(LP_VIRT_TO_BUS(lp->tx_ring, lp));
}

static void
pcnet32_restart(struct device *dev, unsigned int csr0_bits, int must_reinit)
{
        int i;
	unsigned int ioaddr = dev->base_addr;

	pcnet32_purge_tx_ring(dev);
	pcnet32_init_ring(dev);

	outw(0x0000, ioaddr + PCNET32_ADDR);
        /* ReInit Ring */
        outw(0x0001, ioaddr + PCNET32_DATA);
	i = 0;
	while (i++ < 100)
		if (inw(ioaddr+PCNET32_DATA) & 0x0100)
			break;

	outw(csr0_bits, ioaddr + PCNET32_DATA);
}

static int
pcnet32_start_xmit(struct sk_buff *skb, struct device *dev)
{
	struct pcnet32_private *lp = (struct pcnet32_private *)dev->priv;
	unsigned int ioaddr = dev->base_addr;
	int entry;
//	unsigned long flags;

	/* Transmitter timeout, serious problems. */
	if (dev->tbusy) {
		// TODO is it OK to simply comment them?
//		int tickssofar = jiffies - dev->trans_start;
//		if (tickssofar < 20)
//			return 1;
		outw(0, ioaddr+PCNET32_ADDR);
		printk("%s: transmit timed out, status %4.4x, resetting.\n",
			   dev->name, inw(ioaddr+PCNET32_DATA));
		outw(0x0004, ioaddr+PCNET32_DATA);
		lp->stats.tx_errors++;
#ifndef final_version
		{
			int i;
			printk(" Ring data dump: dirty_tx %d cur_tx %d%s cur_rx %d.",
				   lp->dirty_tx, lp->cur_tx, lp->tx_full ? " (full)" : "",
				   lp->cur_rx);
			for (i = 0 ; i < RX_RING_SIZE; i++)
				printk("%s %08x %04x %08x %04x", i & 1 ? "" : "\n ",
					   lp->rx_ring[i].base, -lp->rx_ring[i].buf_length,
					   lp->rx_ring[i].msg_length, (unsigned)lp->rx_ring[i].status);
			for (i = 0 ; i < TX_RING_SIZE; i++)
				printk("%s %08x %04x %08x %04x", i & 1 ? "" : "\n ",
					   lp->tx_ring[i].base, -lp->tx_ring[i].length,
					   lp->tx_ring[i].misc, (unsigned)lp->tx_ring[i].status);
			printk("\n");
		}
#endif
		pcnet32_restart(dev, 0x0042, 1);

		dev->tbusy = 0;
		// TODO maybe I should uncomment it later.
//		dev->trans_start = jiffies;

		return 0;
	}

	if (pcnet32_debug > 3) {
		outw(0x0000, ioaddr+PCNET32_ADDR);
		printk("%s: pcnet32_start_xmit() called, csr0 %4.4x.\n", dev->name,
			   inw(ioaddr+PCNET32_DATA));
		outw(0x0000, ioaddr+PCNET32_DATA);
	}

	/* Block a timer-based transmit from overlapping.  This could better be
	   done with atomic_swap(1, dev->tbusy), but set_bit() works as well. */
	if (test_and_set_bit(0, (void*)&dev->tbusy) != 0) {
		printk("%s: Transmitter access conflict.\n", dev->name);
		return 1;
	}

	if (test_and_set_bit(0, (void*)&lp->lock) != 0) {
		if (pcnet32_debug > 0)
			printk("%s: tx queue lock!.\n", dev->name);
		/* don't clear dev->tbusy flag. */
		return 1;
	}

	/* Fill in a Tx ring entry */

	/* Mask to ring buffer boundary. */
	entry = lp->cur_tx & TX_RING_MOD_MASK;

	/* Caution: the write order is important here, set the base address
	   with the "ownership" bits last. */

	lp->tx_ring[entry].length = le16_to_cpu(-skb->len);

	lp->tx_ring[entry].misc = 0x00000000;

	lp->tx_skbuff[entry] = skb;
	lp->tx_ring[entry].base = (u32)le32_to_cpu(SKB_VIRT_TO_BUS(skb->data, skb));
	lp->tx_ring[entry].status = le16_to_cpu(0x8300);

	lp->cur_tx++;

	/* Trigger an immediate send poll. */
	outw(0x0000, ioaddr+PCNET32_ADDR);
	outw(0x0048, ioaddr+PCNET32_DATA);

//	dev->trans_start = jiffies;

//	save_flags(flags);
//	cli();
	mutex_lock (&global_lock);
	lp->lock = 0;
	if (lp->tx_ring[(entry+1) & TX_RING_MOD_MASK].base == 0)
		clear_bit(0, (void*)&dev->tbusy);
	else
		lp->tx_full = 1;
//	restore_flags(flags);
	mutex_unlock (&global_lock);

	return 0;
}

/* The PCNET32 interrupt handler. */
void
pcnet32_interrupt(int irq)
{
	extern mach_port_t master_device;
	error_t err;
	struct device *dev = ether_dev;
	struct pcnet32_private *lp;
	unsigned int csr0, ioaddr;
	int boguscnt = max_interrupt_work;
	int must_restart;

	if (dev == NULL) {
		printk ("pcnet32_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}

	ioaddr = dev->base_addr;
	lp = (struct pcnet32_private *)dev->priv;
	if (dev->interrupt)
		printk("%s: Re-entering the interrupt handler.\n", dev->name);

	dev->interrupt = 1;

	outw(0x00, dev->base_addr + PCNET32_ADDR);
	while ((csr0 = inw(dev->base_addr + PCNET32_DATA)) & 0x8600
		   && --boguscnt >= 0) {
		/* Acknowledge all of the current interrupt sources ASAP. */
		outw(csr0 & ~0x004f, dev->base_addr + PCNET32_DATA);

		must_restart = 0;

		if (pcnet32_debug > 5)
			printk("%s: interrupt  csr0=%#2.2x new csr=%#2.2x.\n",
				   dev->name, csr0, inw(dev->base_addr + PCNET32_DATA));

		if (csr0 & 0x0400) {		/* Rx interrupt */ 
			pcnet32_rx(dev);
		}

		if (csr0 & 0x0200) {		/* Tx-done interrupt */
			mutex_lock (&global_lock);
			int dirty_tx = lp->dirty_tx;

			while (dirty_tx < lp->cur_tx) {
				int entry = dirty_tx & TX_RING_MOD_MASK;
				int status = (short)le16_to_cpu(lp->tx_ring[entry].status);

				if (status < 0)
					break;			/* It still hasn't been Txed */

				lp->tx_ring[entry].base = 0;

				if (status & 0x4000) {
					/* There was an major error, log it. */
					int err_status = le16_to_cpu(lp->tx_ring[entry].misc);
					lp->stats.tx_errors++;
					if (err_status & 0x04000000) lp->stats.tx_aborted_errors++;
					if (err_status & 0x08000000) lp->stats.tx_carrier_errors++;
					if (err_status & 0x10000000) lp->stats.tx_window_errors++;
					if (err_status & 0x40000000) {
						/* Ackk!  On FIFO errors the Tx unit is turned off! */
						lp->stats.tx_fifo_errors++;
						/* Remove this verbosity later! */
						printk("%s: Tx FIFO error! Status %4.4x.\n",
							   dev->name, csr0);
						/* Restart the chip. */
						must_restart = 1;
					}
				} else {
					if (status & 0x1800)
						lp->stats.collisions++;
					lp->stats.tx_packets++;
				}

				/* We must free the original skb */
				if (lp->tx_skbuff[entry]) {
					dev_kfree_skb(lp->tx_skbuff[entry], FREE_WRITE);
					lp->tx_skbuff[entry] = 0;
				}
				dirty_tx++;
			}

#ifndef final_version
			if (lp->cur_tx - dirty_tx >= TX_RING_SIZE) {
				printk("out-of-sync dirty pointer, %d vs. %d, full=%d.\n",
					   dirty_tx, lp->cur_tx, lp->tx_full);
				dirty_tx += TX_RING_SIZE;
			}
#endif

			if (lp->tx_full && dev->tbusy
				&& dirty_tx > lp->cur_tx - TX_RING_SIZE + 2) {
				/* The ring is no longer full, clear tbusy. */
				lp->tx_full = 0;
				clear_bit(0, (void*)&dev->tbusy);
				// TODO comment it temporarily
//				mark_bh(NET_BH);
			}

			lp->dirty_tx = dirty_tx;
			mutex_unlock (&global_lock);
		}

		/* Log misc errors. */
		if (csr0 & 0x4000) lp->stats.tx_errors++; /* Tx babble. */
		if (csr0 & 0x1000) {
			/*
			 * this happens when our receive ring is full. This 
			 * shouldn't be a problem as we will see normal rx 
			 * interrupts for the frames in the receive ring. But 
			 * there are some PCI chipsets (I can reproduce this 
			 * on SP3G with Intel saturn chipset) which have some-
			 * times problems and will fill up the receive ring 
			 * with error descriptors. In this situation we don't 
			 * get a rx interrupt, but a missed frame interrupt 
			 * sooner or later. So we try to clean up our receive 
			 * ring here.
			 */
			pcnet32_rx(dev);
			lp->stats.rx_errors++; /* Missed a Rx frame. */
		}
		if (csr0 & 0x0800) {
			printk("%s: Bus master arbitration failure, status %4.4x.\n",
				   dev->name, csr0);
			/* Restart the chip. */
			must_restart = 1;
		}

		if (must_restart) {
			/* stop the chip to clear the error condition, then restart */
			outw(0x0000, dev->base_addr + PCNET32_ADDR);
			outw(0x0004, dev->base_addr + PCNET32_DATA);
			pcnet32_restart(dev, 0x0002, 0);
		}
	}

	/* Clear any other interrupt, and set interrupt enable. */
	outw(0x0000, dev->base_addr + PCNET32_ADDR);
	outw(0x7940, dev->base_addr + PCNET32_DATA);

	if (pcnet32_debug > 4)
		printk("%s: exiting interrupt, csr%d=%#4.4x.\n",
			   dev->name, inw(ioaddr + PCNET32_ADDR),
			   inw(dev->base_addr + PCNET32_DATA));

	dev->interrupt = 0;
	err = device_irq_enable (master_device, irq, TRUE);
	return;
}

static int
pcnet32_rx(struct device *dev)
{
	struct pcnet32_private *lp = (struct pcnet32_private *)dev->priv;
	int entry = lp->cur_rx & RX_RING_MOD_MASK;
	int i;

	/* If we own the next entry, it's a new packet. Send it up. */
	while ((short)le16_to_cpu(lp->rx_ring[entry].status) >= 0) {
		int status = (short)le16_to_cpu(lp->rx_ring[entry].status) >> 8;

		if (status != 0x03) {			/* There was an error. */
			/* There is a tricky error noted by John Murphy,
			   <murf@perftech.com> to Russ Nelson: Even with full-sized
			   buffers it's possible for a jabber packet to use two
			   buffers, with only the last correctly noting the error. */
			if (status & 0x01)	/* Only count a general error at the */
				lp->stats.rx_errors++; /* end of a packet.*/
			if (status & 0x20) lp->stats.rx_frame_errors++;
			if (status & 0x10) lp->stats.rx_over_errors++;
			if (status & 0x08) lp->stats.rx_crc_errors++;
			if (status & 0x04) lp->stats.rx_fifo_errors++;
			lp->rx_ring[entry].status &= le16_to_cpu(0x03ff);
		}
		else
		{
			/* Malloc up new buffer, compatible with net-2e. */
			short pkt_len = (le32_to_cpu(lp->rx_ring[entry].msg_length) & 0xfff)-4;
			struct sk_buff *skb;

			if(pkt_len < 60) {
				printk("%s: Runt packet!\n",dev->name);
				lp->stats.rx_errors++;
			} else {
				skb = dev_alloc_skb(pkt_len+2);
				if (skb == NULL) {
					printk("%s: Memory squeeze, deferring packet.\n",
						   dev->name);
					for (i=0; i < RX_RING_SIZE; i++)
						if ((short)le16_to_cpu(lp->rx_ring[(entry+i) & RX_RING_MOD_MASK].status) < 0)
							break;

					if (i > RX_RING_SIZE -2)
					{
						lp->stats.rx_dropped++;
						lp->rx_ring[entry].status |= le16_to_cpu(0x8000);
						lp->cur_rx++;
					}
					break;
				}
				skb->dev = dev;
				skb_reserve(skb,2);	/* 16 byte align */
				skb_put(skb,pkt_len);	/* Make room */
				eth_copy_and_sum(skb,
					(unsigned char *)BUF_BUS_TO_VIRT(le32_to_cpu(lp->rx_ring[entry].base), lp),
					pkt_len,0);
				skb->protocol=eth_type_trans(skb,dev);
				netif_rx(skb);
				lp->stats.rx_packets++;
			}
		}
		/* The docs say that the buffer length isn't touched, but Andrew Boyd
		   of QNX reports that some revs of the 79C965 clear it. */
		lp->rx_ring[entry].buf_length = le16_to_cpu(-PKT_BUF_SZ);
		lp->rx_ring[entry].status |= le16_to_cpu(0x8000);
		entry = (++lp->cur_rx) & RX_RING_MOD_MASK;
	}

	/* We should check that at least two ring entries are free.	 If not,
	   we should free one and mark stats->rx_dropped++. */

	return 0;
}

static int
pcnet32_close(struct device *dev)
{
	unsigned int ioaddr = dev->base_addr;
	struct pcnet32_private *lp = (struct pcnet32_private *)dev->priv;

	dev->start = 0;
	set_bit(0, (void*)&dev->tbusy);

	outw(112, ioaddr+PCNET32_ADDR);
	lp->stats.rx_missed_errors = inw(ioaddr+PCNET32_DATA);

	outw(0, ioaddr+PCNET32_ADDR);

	if (pcnet32_debug > 1)
		printk("%s: Shutting down ethercard, status was %2.2x.\n",
			   dev->name, inw(ioaddr+PCNET32_DATA));

	/* We stop the PCNET32 here -- it occasionally polls
	   memory if we don't. */
	outw(0x0004, ioaddr+PCNET32_DATA);

	free_irq(dev);

	return 0;
}

static struct enet_statistics *pcnet32_get_stats(struct device *dev)
{
	struct pcnet32_private *lp = (struct pcnet32_private *)dev->priv;
	unsigned int ioaddr = dev->base_addr;
	unsigned short saved_addr;
//	unsigned long flags;

//	save_flags(flags);
//	cli();
	mutex_lock (&global_lock);
	saved_addr = inw(ioaddr+PCNET32_ADDR);
	outw(112, ioaddr+PCNET32_ADDR);
	lp->stats.rx_missed_errors = inw(ioaddr+PCNET32_DATA);
	outw(saved_addr, ioaddr+PCNET32_ADDR);
	mutex_unlock (&global_lock);
//	restore_flags(flags);

	return &lp->stats;
}

/* Set or clear the multicast filter for this adaptor.
 */

static void pcnet32_set_multicast_list(struct device *dev)
{
#define IFF_PROMISC 0x100
#define IFF_ALLMULTI 0x200
	unsigned int ioaddr = dev->base_addr;
	struct pcnet32_private *lp = (struct pcnet32_private *)dev->priv;

	if (dev->flags&IFF_PROMISC) {
		/* Log any net taps. */
		printk("%s: Promiscuous mode enabled.\n", dev->name);
		lp->init_block.mode |= 0x8000;
	} else {
		int num_addrs=dev->mc_count;
		if(dev->flags&IFF_ALLMULTI)
			num_addrs=1;
		/* FIXIT: We don't use the multicast table, but rely on upper-layer filtering. */
		memset(lp->init_block.filter , (num_addrs == 0) ? 0 : -1, sizeof(lp->init_block.filter));
		lp->init_block.mode &= ~0x8000;
	}

	outw(0, ioaddr+PCNET32_ADDR);
	outw(0x0004, ioaddr+PCNET32_DATA); /* Temporarily stop the lance. */

	pcnet32_restart(dev, 0x0042, 0); /*  Resume normal operation */

}

boolean_t
ioperm_ports ()
{
  return TRUE;
}
