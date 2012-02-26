PKGDIR        ?= /usr/share/libdde_linux26
L4DIR         ?= $(PKGDIR)

include Makeconf.local

TARGET         = netdde

SRC_C          = main.c

LIBS          += $(libmachdev_path) -ldde_linux26.o -ldde_linux26_net $(libddekit_path) -lfshelp -ltrivfs -lpciaccess -lz -lthreads -lshouldbeinlibc -lports $(libslab_path) $(libbpf_path)
CFLAGS        += -g -I$(PKGDIR)/include -I$(BUILDDIR)/include

# DDE configuration
include $(L4DIR)/Makeconf

include $(L4DIR)/mk/prog.mk

VERSION=2.6.29.6

SRC=linux-$(VERSION)/drivers/net

BLACKLIST = \
	    These dont build at all \
	    \
	    $(SRC)/netx-eth.c $(SRC)/smc911x.c $(wildcard $(SRC)/irda/*) \
	    $(wildcard $(SRC)/arm/*.c) $(SRC)/bfin_mac.c \
	    $(SRC)/declance.c $(wildcard $(SRC)/ehea/*) $(wildcard $(SRC)/*82596.c) \
	    $(wildcard $(SRC)/atlx/*) $(SRC)/pasemi_mac_ethtool.c \
	    $(wildcard $(SRC)/wireless/*) $(wildcard $(SRC)/wireless/*/*) \
	    $(SRC)/apne.c \
	    $(SRC)/sunbmac.c $(SRC)/sunqe.c $(SRC)/myri_sbus.c $(SRC)/sunlance.c $(SRC)/sunhme.c \
	    $(SRC)/sungem_phy.c $(SRC)/sungem.c $(SRC)/mace.c $(SRC)/bmac.c $(SRC)/sunvnet.c \
	    $(SRC)/gianfar_mii.c $(SRC)/gianfar.c $(SRC)/fec_mpc52xx_phy.c $(SRC)/ucc_geth_mii.c \
	    $(wildcard $(SRC)/fs_e$(SRC)/*) \
	    $(wildcard $(SRC)/sun3*) \
	    $(SRC)/au1000_eth.c \
	    $(SRC)/s2io.c $(SRC)/sonic.c \
	    $(wildcard $(SRC)/wimax/i2400m/*) \
	    $(wildcard $(SRC)/ibm_newemac/*) \
	    $(wildcard $(SRC)/hamradio/*) \
	    $(wildcard $(SRC)/usb/*) \
	    $(wildcard $(SRC)/sfc/*) \
	    $(wildcard $(SRC)/appletalk/*) \
	    $(wildcard $(SRC)/pcmcia/*) \
	    $(wildcard $(SRC)/tokenring/*) \
	    $(wildcard $(SRC)/wan/*) \
	    $(wildcard $(SRC)/wan/*/*) \
	    $(wildcard $(SRC)/cris/*) \
	    $(wildcard $(SRC)/fs_enet/*) \
	    $(wildcard $(SRC)/arcnet/*) \
	    $(wildcard $(SRC)/benet/*) \
	    $(wildcard $(SRC)/arc$(SRC)/*) \
	    $(SRC)/mv643xx_eth.c $(SRC)/pasemi_mac.c $(SRC)/ibmveth.c \
	    $(wildcard $(SRC)/ps3*) \
	    $(SRC)/iseries_veth.c $(SRC)/cpmac.c $(SRC)/ixgbe/ixgbe_dcb_nl.c \
	    $(wildcard $(SRC)/ucc_geth*) \
	    $(wildcard $(SRC)/mlx4/*) \
	    $(wildcard $(SRC)/bonding/*) \
	    $(SRC)/ariadne.c $(SRC)/a2065.c $(SRC)/zorro8390.c $(SRC)/hydra.c \
	    $(SRC)/macmace.c $(SRC)/dm9000.c $(SRC)/ax88796.c $(SRC)/rionet.c $(SRC)/sgiseeq.c \
	    $(SRC)/ioc3-eth.c $(SRC)/fec_mpc52xx.c $(SRC)/hplance.c $(SRC)/atarilance.c \
	    $(SRC)/macsonic.c $(SRC)/xen-netfront.c $(SRC)/jazzsonic.c $(SRC)/mac89x0.c \
	    $(wildcard $(SRC)/ixp2000/*) \
	    $(wildcard $(SRC)/skfp/*) \
	    $(SRC)/mac8390.c $(SRC)/isa-skeleton.c $(SRC)/sh_eth.c $(SRC)/spider_net.c \
	    $(SRC)/phy/fixed.c $(SRC)/stnic.c $(SRC)/mvme147.c $(SRC)/meth.c $(SRC)/macb.c \
	    $(SRC)/korina.c $(SRC)/b44.c $(SRC)/sb1250-mac.c $(SRC)/fec.c $(SRC)/ne-h8300.c \
	    $(SRC)/mipsnet.c $(SRC)/tsi108_eth.c \
	    \
	    These are missing some symbols e.g. tasklet_kill \
	    \
	    $(SRC)/ifb.c $(SRC)/niu.c $(SRC)/cxgb3/cxgb3_offload.c $(SRC)/cxgb3/sge.c \
	    $(SRC)/rrunner.c $(SRC)/sundance.c $(SRC)/ppp_synctty.c $(SRC)/ppp_async.c \
	    $(SRC)/macvlan.c $(SRC)/tun.c $(SRC)/smc91x.c \
	    $(wildcard $(SRC)/enic/*) \
	    $(wildcard $(SRC)/ixgbe/*) \
	    $(SRC)/pppox.c $(SRC)/3c59x.c $(SRC)/znet.c $(SRC)/slip.c $(SRC)/netconsole.c \
	    $(SRC)/jme.c $(SRC)/pppoe.c $(SRC)/pppol2tp.c $(SRC)/veth.c $(SRC)/ppp_deflate.c \
	    $(SRC)/defxx.c \
	    $(wildcard $(SRC)/myri10ge/*) \
	    $(SRC)/acenic.c $(SRC)/fealnx.c $(SRC)/ppp_generic.c $(SRC)/via-rhine.c \
	    $(SRC)/ppp_mppe.c $(SRC)/plip.c $(SRC)/bnx2x_main.c $(SRC)/tulip/winbond-840.c \
	    $(SRC)/amd8111e.c \
	    $(wildcard $(SRC)/igb/*) \
	    $(SRC)/chelsio/sge.c $(SRC)/chelsio/subr.c $(SRC)/chelsio/cxgb2.c \
	    $(SRC)/bnx2.c $(SRC)/tc35815.c \
	    $(wildcard $(SRC)/be$(SRC)/*) \
	    $(wildcard $(SRC)/cxgb3/*) \
	    $(SRC)/ne3210.c $(SRC)/enc28j60.c $(SRC)/r8169.c $(SRC)/xtsonic.c $(SRC)/virtio_net.c \
	    $(SRC)/dl2k.c $(SRC)/smsc911x.c $(SRC)/ibmlana.c $(SRC)/sis190.c $(SRC)/bsd_comp.c \
	    \
	    These are missing some symbols which should be there already \
	    \
	    $(wildcard $(SRC)/tulip/*)

NO_BUILD = dde/lib8390.c

SRC_ORIG := $(filter-out $(BLACKLIST),$(shell find $(SRC) -name \*.c))
SRC_TARGET := $(patsubst $(SRC)/%,dde/%,$(SRC_ORIG))

SRC_C		+= $(filter-out $(NO_BUILD),$(SRC_TARGET))

.PHONY: convert
convert:
	mkdir -p dde
	for i in $(SRC_ORIG) ; do \
		TARGET="dde/$${i#$(SRC)/}" ; \
		mkdir -p "$$(dirname "$$TARGET")" ; \
		./convert "$$i" > "$$TARGET"; \
	done
	for i in $$(find $(SRC) -name \*.h) ; do \
		TARGET="dde/$${i#$(SRC)/}" ; \
		mkdir -p "$$(dirname "$$TARGET")" ; \
		ln -f "$$i" "$$TARGET" ; \
	done
	cd dde ; patch -p1 < ../patch

klean: clean
	rm -fr dde
