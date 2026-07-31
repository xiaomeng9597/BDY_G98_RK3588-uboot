// SPDX-License-Identifier: GPL-2.0+
/*
 * rtl8169.c : U-Boot driver for the RealTek RTL8169 / RTL8125
 *
 * Masami Komiya (mkomiya@sonare.it)
 * Modified for RTL8125 2.5G support and DHCP fix
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <memalign.h>
#include <net.h>
#ifndef CONFIG_DM_ETH
#include <netdev.h>
#endif
#include <asm/io.h>
#include <pci.h>
#include <linux/delay.h>
#include <linux/printk.h>

#undef DEBUG_RTL8169
#undef DEBUG_RTL8169_TX
#undef DEBUG_RTL8169_RX

#define drv_version "v1.7-rtl8125-fix"
#define drv_date "2026-08-02"

static unsigned long ioaddr;

#define currticks()	get_timer(0)

#define MAX_UNITS 8
static int media[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1 };

#define MAC_ADDR_LEN	6
#define MAX_ETH_FRAME_SIZE	1536
#define TX_FIFO_THRESH 256
#define RX_FIFO_THRESH	7
#define RX_DMA_BURST	6
#define TX_DMA_BURST	6
#define EarlyTxThld	0x3F
#define RxPacketMaxSize 0x0800
#define InterFrameGap	0x03

#define NUM_TX_DESC	1
#ifdef CONFIG_SYS_RX_ETH_BUFFER
#define NUM_RX_DESC	CONFIG_SYS_RX_ETH_BUFFER
#else
#define NUM_RX_DESC	4
#endif
#define RX_BUF_SIZE	1536
#define RX_BUF_LEN	8192

#define RTL_MIN_IO_SIZE 0x80
#define TX_TIMEOUT  (6*HZ)

/* FIX: Ensure proper casting for MMIO macros */
#define RTL_W8(reg, val8)	writeb((val8), (void *)(ioaddr + (reg)))
#define RTL_W16(reg, val16)	writew((val16), (void *)(ioaddr + (reg)))
#define RTL_W32(reg, val32)	writel((val32), (void *)(ioaddr + (reg)))
#define RTL_R8(reg)		readb((void *)(ioaddr + (reg)))
#define RTL_R16(reg)		readw((void *)(ioaddr + (reg)))
#define RTL_R32(reg)		readl((void *)(ioaddr + (reg)))

#define ETH_FRAME_LEN	MAX_ETH_FRAME_SIZE
#define ETH_ALEN	MAC_ADDR_LEN
#define ETH_ZLEN	60

enum RTL8169_registers {
	MAC0 = 0,
	MAR0 = 8,
	TxDescStartAddrLow = 0x20,
	TxDescStartAddrHigh = 0x24,
	TxHDescStartAddrLow = 0x28,
	TxHDescStartAddrHigh = 0x2c,
	FLASH = 0x30,
	ERSR = 0x36,
	ChipCmd = 0x37,
	TxPoll_8169 = 0x38,
	IntrMask_8169 = 0x3C,
	IntrStatus_8169 = 0x3E,
	TxConfig = 0x40,
	RxConfig = 0x44,
	RxMissed = 0x4C,
	Cfg9346 = 0x50,
	Config0 = 0x51,
	Config1 = 0x52,
	Config2 = 0x53,
	Config3 = 0x54,
	Config4 = 0x55,
	Config5 = 0x56,
	MultiIntr = 0x5C,
	PHYAR = 0x60,
	TBICSR = 0x64,
	TBI_ANAR = 0x68,
	TBI_LPAR = 0x6A,
	PHYstatus = 0x6C,
	RxMaxSize = 0xDA,
	CPlusCmd = 0xE0,
	RxDescStartAddrLow = 0xE4,
	RxDescStartAddrHigh = 0xE8,
	EarlyTxThres = 0xEC,
	FuncEvent = 0xF0,
	FuncEventMask = 0xF4,
	FuncPresetState = 0xF8,
	FuncForceEvent = 0xFC,
};

/* RTL8125 specific registers */
enum RTL8125_registers {
	IntrMask_8125 = 0x38,
	IntrStatus_8125 = 0x3C,
	TxPoll_8125 = 0x90,
};

enum RTL8169_register_content {
	SYSErr = 0x8000,
	PCSTimeout = 0x4000,
	SWInt = 0x0100,
	TxDescUnavail = 0x80,
	RxFIFOOver = 0x40,
	RxUnderrun = 0x20,
	RxOverflow = 0x10,
	TxErr = 0x08,
	TxOK = 0x04,
	RxErr = 0x02,
	RxOK = 0x01,

	RxRES = 0x00200000,
	RxCRC = 0x00080000,
	RxRUNT = 0x00100000,
	RxRWT = 0x00400000,

	CmdReset = 0x10,
	CmdRxEnb = 0x08,
	CmdTxEnb = 0x04,
	RxBufEmpty = 0x01,

	Cfg9346_Lock = 0x00,
	Cfg9346_Unlock = 0xC0,

	AcceptErr = 0x20,
	AcceptRunt = 0x10,
	AcceptBroadcast = 0x08,
	AcceptMulticast = 0x04,
	AcceptMyPhys = 0x02,
	AcceptAllPhys = 0x01,

	RxCfgFIFOShift = 13,
	RxCfgDMAShift = 8,

	TxInterFrameGapShift = 24,
	TxDMAShift = 8,

	TBI_Enable = 0x80,
	TxFlowCtrl = 0x40,
	RxFlowCtrl = 0x20,
	_1000bpsF = 0x10,
	_100bps = 0x08,
	_10bps = 0x04,
	LinkStatus = 0x02,
	FullDup = 0x01,

	PHY_CTRL_REG = 0,
	PHY_STAT_REG = 1,
	PHY_AUTO_NEGO_REG = 4,
	PHY_1000_CTRL_REG = 9,

	PHY_Restart_Auto_Nego = 0x0200,
	PHY_Enable_Auto_Nego = 0x1000,
	PHY_Auto_Nego_Comp = 0x0020,

	PHY_Cap_10_Half = 0x0020,
	PHY_Cap_10_Full = 0x0040,
	PHY_Cap_100_Half = 0x0080,
	PHY_Cap_100_Full = 0x0100,
	PHY_Cap_1000_Full = 0x0200,
	PHY_Cap_Null = 0x0,

	_10_Half = 0x01,
	_10_Full = 0x02,
	_100_Half = 0x04,
	_100_Full = 0x08,
	_1000_Full = 0x10,

	TBILinkOK = 0x02000000,
	RxDv_Gated_En = 0x80000,
};

static struct {
	const char *name;
	u8 version;
	u32 RxConfigMask;
} rtl_chip_info[] = {
	{"RTL-8169", 0x00, 0xff7e1880,},
	{"RTL-8169", 0x04, 0xff7e1880,},
	{"RTL-8169", 0x00, 0xff7e1880,},
	{"RTL-8169s/8110s",	0x02, 0xff7e1880,},
	{"RTL-8169s/8110s",	0x04, 0xff7e1880,},
	{"RTL-8169sb/8110sb",	0x10, 0xff7e1880,},
	{"RTL-8169sc/8110sc",	0x18, 0xff7e1880,},
	{"RTL-8168b/8111sb",	0x30, 0xff7e1880,},
	{"RTL-8168b/8111sb",	0x38, 0xff7e1880,},
	{"RTL-8168d/8111d",	0x28, 0xff7e1880,},
	{"RTL-8168evl/8111evl",	0x2e, 0xff7e1880,},
	{"RTL-8168/8111g",	0x4c, 0xff7e1880,},
	{"RTL-8101e",		0x34, 0xff7e1880,},
	{"RTL-8100e",		0x32, 0xff7e1880,},
	{"RTL-8125B",		0x64, 0xff7e1880,},
};

enum _DescStatusBit {
	OWNbit = 0x80000000,
	EORbit = 0x40000000,
	FSbit = 0x20000000,
	LSbit = 0x10000000,
};

struct TxDesc {
	u32 status;
	u32 vlan_tag;
	u32 buf_addr;
	u32 buf_Haddr;
};

struct RxDesc {
	u32 status;
	u32 vlan_tag;
	u32 buf_addr;
	u32 buf_Haddr;
};

static unsigned char rxdata[RX_BUF_LEN];

#define RTL8169_DESC_SIZE 16

#if ARCH_DMA_MINALIGN > 256
#  define RTL8169_ALIGN ARCH_DMA_MINALIGN
#else
#  define RTL8169_ALIGN 256
#endif

#if RTL8169_DESC_SIZE < ARCH_DMA_MINALIGN
#if !defined(CONFIG_SYS_NONCACHED_MEMORY) && \
	!defined(CONFIG_SYS_DCACHE_OFF) && !defined(CONFIG_X86) && !defined(CONFIG_RISCV)
#warning "cache-line size is larger than descriptor size. Define CONFIG_SYS_NONCACHED_MEMORY for RTL8125 stability."
#endif
#endif

DEFINE_ALIGN_BUFFER(u8, txb, NUM_TX_DESC * RX_BUF_SIZE, RTL8169_ALIGN);
DEFINE_ALIGN_BUFFER(u8, rxb, NUM_RX_DESC * RX_BUF_SIZE, RTL8169_ALIGN);

struct rtl8169_private {
	ulong iobase;
	void *mmio_addr;
	int chipset;
	unsigned long cur_rx;
	unsigned long cur_tx;
	unsigned long dirty_tx;
	struct TxDesc *TxDescArray;
	struct RxDesc *RxDescArray;
	unsigned char *RxBufferRings;
	unsigned char *RxBufferRing[NUM_RX_DESC];
	unsigned char *Tx_skbuff[NUM_TX_DESC];
	u16 device_id; /* FIX: Store device ID for non-DM path */
} tpx;

static struct rtl8169_private *tpc;

static const unsigned int rtl8169_rx_config =
	(RX_FIFO_THRESH << RxCfgFIFOShift) | (RX_DMA_BURST << RxCfgDMAShift);

static struct pci_device_id supported[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8125) },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8161) },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8167) },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8168) },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8169) },
	{}
};

void mdio_write(int RegAddr, int value)
{
	int i;
	RTL_W32(PHYAR, 0x80000000 | (RegAddr & 0xFF) << 16 | value);
	udelay(1000);
	for (i = 2000; i > 0; i--) {
		if (!(RTL_R32(PHYAR) & 0x80000000))
			break;
		udelay(100);
	}
}

int mdio_read(int RegAddr)
{
	int i, value = -1;
	RTL_W32(PHYAR, 0x0 | (RegAddr & 0xFF) << 16);
	udelay(1000);
	for (i = 2000; i > 0; i--) {
		if (RTL_R32(PHYAR) & 0x80000000) {
			value = (int)(RTL_R32(PHYAR) & 0xFFFF);
			break;
		}
		udelay(100);
	}
	return value;
}

static int rtl8169_init_board(unsigned long dev_iobase, const char *name)
{
	int i;
	u32 tmp;

	ioaddr = dev_iobase;
	RTL_W8(ChipCmd, CmdReset);
	for (i = 1000; i > 0; i--) {
		if ((RTL_R8(ChipCmd) & CmdReset) == 0)
			break;
		udelay(10);
	}

	tmp = RTL_R32(TxConfig);
	tmp = ((tmp & 0x7c000000) + ((tmp & 0x00800000) << 2)) >> 24;

	for (i = ARRAY_SIZE(rtl_chip_info) - 1; i >= 0; i--) {
		if (tmp == rtl_chip_info[i].version) {
			tpc->chipset = i;
			goto match;
		}
	}
	printf("PCI device %s: unknown chip version, assuming RTL-8169\n", name);
	tpc->chipset = 0;

	match:
	return 0;
}

static void *rtl_alloc_descs(unsigned int num)
{
	size_t size = num * RTL8169_DESC_SIZE;
#ifdef CONFIG_SYS_NONCACHED_MEMORY
	return (void *)noncached_alloc(size, RTL8169_ALIGN);
#else
	return memalign(RTL8169_ALIGN, size);
#endif
}

static void rtl_inval_rx_desc(struct RxDesc *desc)
{
#ifndef CONFIG_SYS_NONCACHED_MEMORY
	unsigned long start = (unsigned long)desc & ~(ARCH_DMA_MINALIGN - 1);
	unsigned long end = ALIGN(start + sizeof(*desc), ARCH_DMA_MINALIGN);
	invalidate_dcache_range(start, end);
#endif
}

static void rtl_flush_rx_desc(struct RxDesc *desc)
{
#ifndef CONFIG_SYS_NONCACHED_MEMORY
	flush_cache((unsigned long)desc, sizeof(*desc));
#endif
}

static void rtl_inval_tx_desc(struct TxDesc *desc)
{
#ifndef CONFIG_SYS_NONCACHED_MEMORY
	unsigned long start = (unsigned long)desc & ~(ARCH_DMA_MINALIGN - 1);
	unsigned long end = ALIGN(start + sizeof(*desc), ARCH_DMA_MINALIGN);
	invalidate_dcache_range(start, end);
#endif
}

static void rtl_flush_tx_desc(struct TxDesc *desc)
{
#ifndef CONFIG_SYS_NONCACHED_MEMORY
	flush_cache((unsigned long)desc, sizeof(*desc));
#endif
}

static void rtl_inval_buffer(void *buf, size_t size)
{
	unsigned long start = (unsigned long)buf & ~(ARCH_DMA_MINALIGN - 1);
	unsigned long end = ALIGN(start + size, ARCH_DMA_MINALIGN);
	invalidate_dcache_range(start, end);
}

static void rtl_flush_buffer(void *buf, size_t size)
{
	flush_cache((unsigned long)buf, size);
}

/**************************************************************************
RECV
***************************************************************************/
#ifdef CONFIG_DM_ETH
static int rtl_recv_common(struct udevice *dev, unsigned long dev_iobase, uchar **packetp)
#else
static int rtl_recv_common(pci_dev_t dev, unsigned long dev_iobase, uchar **packetp)
#endif
{
	int cur_rx;
	int length = 0;

	ioaddr = dev_iobase;
	cur_rx = tpc->cur_rx;

	rtl_inval_rx_desc(&tpc->RxDescArray[cur_rx]);

	if ((le32_to_cpu(tpc->RxDescArray[cur_rx].status) & OWNbit) == 0) {
		if (!(le32_to_cpu(tpc->RxDescArray[cur_rx].status) & RxRES)) {
			length = (int)(le32_to_cpu(tpc->RxDescArray[cur_rx].status) & 0x00001FFF) - 4;
			rtl_inval_buffer(tpc->RxBufferRing[cur_rx], length);
			memcpy(rxdata, tpc->RxBufferRing[cur_rx], length);

			if (cur_rx == NUM_RX_DESC - 1)
				tpc->RxDescArray[cur_rx].status = cpu_to_le32((OWNbit | EORbit) + RX_BUF_SIZE);
			else
				tpc->RxDescArray[cur_rx].status = cpu_to_le32(OWNbit + RX_BUF_SIZE);

#ifdef CONFIG_DM_ETH
			tpc->RxDescArray[cur_rx].buf_addr = cpu_to_le32(
				dm_pci_mem_to_phys(dev, (pci_addr_t)(unsigned long)tpc->RxBufferRing[cur_rx]));
#else
			tpc->RxDescArray[cur_rx].buf_addr = cpu_to_le32(
				pci_mem_to_phys(dev, (pci_addr_t)(unsigned long)tpc->RxBufferRing[cur_rx]));
#endif
			rtl_flush_rx_desc(&tpc->RxDescArray[cur_rx]);
#ifdef CONFIG_DM_ETH
			*packetp = rxdata;
#else
			net_process_received_packet(rxdata, length);
#endif
		} else {
			puts("Error Rx");
			length = -EIO;
		}
		cur_rx = (cur_rx + 1) % NUM_RX_DESC;
		tpc->cur_rx = cur_rx;
		return length;
	} else {
		/* FIX: Use stored device_id for non-DM, pplat for DM */
		u32 IntrStatus = IntrStatus_8169;
#ifdef CONFIG_DM_ETH
		struct pci_child_platdata *pplat = dev_get_parent_platdata(dev);
		if (pplat->device == 0x8125)
			IntrStatus = IntrStatus_8125;
#else
		if (tpc->device_id == 0x8125)
			IntrStatus = IntrStatus_8125;
#endif
		ushort sts = RTL_R8(IntrStatus);
		RTL_W8(IntrStatus, sts & ~(TxErr | RxErr | SYSErr));
		udelay(100);
	}
	tpc->cur_rx = cur_rx;
	return 0;
}

#ifdef CONFIG_DM_ETH
int rtl8169_eth_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct rtl8169_private *priv = dev_get_priv(dev);
	return rtl_recv_common(dev, priv->iobase, packetp);
}
#else
static int rtl_recv(struct eth_device *dev)
{
	return rtl_recv_common((pci_dev_t)(unsigned long)dev->priv, dev->iobase, NULL);
}
#endif

#define HZ 1000
/**************************************************************************
SEND
***************************************************************************/
#ifdef CONFIG_DM_ETH
static int rtl_send_common(struct udevice *dev, unsigned long dev_iobase, void *packet, int length)
#else
static int rtl_send_common(pci_dev_t dev, unsigned long dev_iobase, void *packet, int length)
#endif
{
	u32 to;
	u8 *ptxb;
	int entry = tpc->cur_tx % NUM_TX_DESC;
	u32 len = length;
	int ret;

	ioaddr = dev_iobase;
	ptxb = tpc->Tx_skbuff[entry * MAX_ETH_FRAME_SIZE];
	memcpy(ptxb, (char *)packet, (int)length);

	while (len < ETH_ZLEN)
		ptxb[len++] = '\0';

	rtl_flush_buffer(ptxb, ALIGN(len, RTL8169_ALIGN));

	tpc->TxDescArray[entry].buf_Haddr = 0;
#ifdef CONFIG_DM_ETH
	tpc->TxDescArray[entry].buf_addr = cpu_to_le32(
		dm_pci_mem_to_phys(dev, (pci_addr_t)(unsigned long)ptxb));
#else
	tpc->TxDescArray[entry].buf_addr = cpu_to_le32(
		pci_mem_to_phys(dev, (pci_addr_t)(unsigned long)ptxb));
#endif
	if (entry != (NUM_TX_DESC - 1))
		tpc->TxDescArray[entry].status = cpu_to_le32((OWNbit | FSbit | LSbit) | ((len > ETH_ZLEN) ? len : ETH_ZLEN));
	else
		tpc->TxDescArray[entry].status = cpu_to_le32((OWNbit | EORbit | FSbit | LSbit) | ((len > ETH_ZLEN) ? len : ETH_ZLEN));

	rtl_flush_tx_desc(&tpc->TxDescArray[entry]);

	/* FIX: Correct TxPoll register selection based on device ID */
#ifdef CONFIG_DM_ETH
	struct pci_child_platdata *pplat = dev_get_parent_platdata(dev);
	if (pplat->device == 0x8125)
		RTL_W8(TxPoll_8125, 0x1);
	else
		RTL_W8(TxPoll_8169, 0x40);
#else
	if (tpc->device_id == 0x8125)
		RTL_W8(TxPoll_8125, 0x1);
	else
		RTL_W8(TxPoll_8169, 0x40);
#endif

	tpc->cur_tx++;
	to = currticks() + TX_TIMEOUT;
	do {
		rtl_inval_tx_desc(&tpc->TxDescArray[entry]);
	} while ((le32_to_cpu(tpc->TxDescArray[entry].status) & OWNbit) && (currticks() < to));

	if (currticks() >= to)
		ret = -ETIMEDOUT;
	else
		ret = 0;

	udelay(20);
	return ret;
}

#ifdef CONFIG_DM_ETH
int rtl8169_eth_send(struct udevice *dev, void *packet, int length)
{
	struct rtl8169_private *priv = dev_get_priv(dev);
	return rtl_send_common(dev, priv->iobase, packet, length);
}
#else
static int rtl_send(struct eth_device *dev, void *packet, int length)
{
	return rtl_send_common((pci_dev_t)(unsigned long)dev->priv, dev->iobase, packet, length);
}
#endif

static void rtl8169_set_rx_mode(void)
{
	u32 mc_filter[2];
	int rx_mode;
	u32 tmp;

	rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
	mc_filter[1] = mc_filter[0] = 0xffffffff;

	tmp = rtl8169_rx_config | rx_mode | (RTL_R32(RxConfig) & rtl_chip_info[tpc->chipset].RxConfigMask);
	RTL_W32(RxConfig, tmp);
	RTL_W32(MAR0 + 0, mc_filter[0]);
	RTL_W32(MAR0 + 4, mc_filter[1]);
}

#ifdef CONFIG_DM_ETH
static void rtl8169_hw_start(struct udevice *dev)
#else
static void rtl8169_hw_start(pci_dev_t dev)
#endif
{
	u32 i;

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	if (tpc->chipset <= 5)
		RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W8(EarlyTxThres, EarlyTxThld);
	RTL_W16(RxMaxSize, RxPacketMaxSize);

	i = rtl8169_rx_config | (RTL_R32(RxConfig) & rtl_chip_info[tpc->chipset].RxConfigMask);
	RTL_W32(RxConfig, i);
	RTL_W32(TxConfig, (TX_DMA_BURST << TxDMAShift) | (InterFrameGap << TxInterFrameGapShift));

	tpc->cur_rx = 0;

#ifdef CONFIG_DM_ETH
	RTL_W32(TxDescStartAddrLow, dm_pci_mem_to_phys(dev, (pci_addr_t)(unsigned long)tpc->TxDescArray));
	RTL_W32(RxDescStartAddrLow, dm_pci_mem_to_phys(dev, (pci_addr_t)(unsigned long)tpc->RxDescArray));
#else
	RTL_W32(TxDescStartAddrLow, pci_mem_to_phys(dev, (pci_addr_t)(unsigned long)tpc->TxDescArray));
	RTL_W32(RxDescStartAddrLow, pci_mem_to_phys(dev, (pci_addr_t)(unsigned long)tpc->RxDescArray));
#endif
	RTL_W32(TxDescStartAddrHigh, 0);
	RTL_W32(RxDescStartAddrHigh, 0);

	if (tpc->chipset > 5)
		RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W8(Cfg9346, Cfg9346_Lock);
	udelay(10);

	RTL_W32(RxMissed, 0);
	rtl8169_set_rx_mode();
	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xF000);
}

#ifdef CONFIG_DM_ETH
static void rtl8169_init_ring(struct udevice *dev)
#else
static void rtl8169_init_ring(pci_dev_t dev)
#endif
{
	int i;

	tpc->cur_rx = 0;
	tpc->cur_tx = 0;
	tpc->dirty_tx = 0;
	memset(tpc->TxDescArray, 0, NUM_TX_DESC * sizeof(struct TxDesc));
	memset(tpc->RxDescArray, 0, NUM_RX_DESC * sizeof(struct RxDesc));

	for (i = 0; i < NUM_TX_DESC; i++)
		tpc->Tx_skbuff[i] = &txb[i];

	for (i = 0; i < NUM_RX_DESC; i++) {
		if (i == (NUM_RX_DESC - 1))
			tpc->RxDescArray[i].status = cpu_to_le32((OWNbit | EORbit) + RX_BUF_SIZE);
		else
			tpc->RxDescArray[i].status = cpu_to_le32(OWNbit + RX_BUF_SIZE);

		tpc->RxBufferRing[i] = &rxb[i * RX_BUF_SIZE];
#ifdef CONFIG_DM_ETH
		tpc->RxDescArray[i].buf_addr = cpu_to_le32(dm_pci_mem_to_phys(dev, (pci_addr_t)(unsigned long)tpc->RxBufferRing[i]));
#else
		tpc->RxDescArray[i].buf_addr = cpu_to_le32(pci_mem_to_phys(dev, (pci_addr_t)(unsigned long)tpc->RxBufferRing[i]));
#endif
		rtl_flush_rx_desc(&tpc->RxDescArray[i]);
	}
}

#ifdef CONFIG_DM_ETH
static void rtl8169_common_start(struct udevice *dev, unsigned char *enetaddr, unsigned long dev_iobase)
#else
static void rtl8169_common_start(pci_dev_t dev, unsigned char *enetaddr, unsigned long dev_iobase)
#endif
{
	int i;
	ioaddr = dev_iobase;

	rtl8169_init_ring(dev);
	rtl8169_hw_start(dev);

	for (i = 0; i < 192; i++)
		txb[i] = 0xFF;
	for (i = 0; i < 6; i++)
		txb[i] = enetaddr[i];
}

#ifdef CONFIG_DM_ETH
static int rtl8169_eth_start(struct udevice *dev)
{
	struct eth_pdata *plat = dev_get_platdata(dev);
	struct rtl8169_private *priv = dev_get_priv(dev);
	rtl8169_common_start(dev, plat->enetaddr, priv->iobase);
	return 0;
}
#else
static int rtl_reset(struct eth_device *dev, bd_t *bis)
{
	rtl8169_common_start((pci_dev_t)(unsigned long)dev->priv, dev->enetaddr, dev->iobase);
	return 0;
}
#endif

static void rtl_halt_common(unsigned long dev_iobase, u16 device_id)
{
	int i;
	ioaddr = dev_iobase;

	RTL_W8(ChipCmd, 0x00);

	/* FIX: Correct interrupt mask register for RTL8125 */
	if (device_id == 0x8125)
		RTL_W16(IntrMask_8125, 0x0000);
	else
		RTL_W16(IntrMask_8169, 0x0000);

	RTL_W32(RxMissed, 0);
	for (i = 0; i < NUM_RX_DESC; i++)
		tpc->RxBufferRing[i] = NULL;
}

#ifdef CONFIG_DM_ETH
void rtl8169_eth_stop(struct udevice *dev)
{
	struct rtl8169_private *priv = dev_get_priv(dev);
	struct pci_child_platdata *pplat = dev_get_parent_platdata(dev);
	rtl_halt_common(priv->iobase, pplat->device);
}
#else
static void rtl_halt(struct eth_device *dev)
{
	rtl_halt_common(dev->iobase, tpc->device_id);
}
#endif

static int rtl_init(unsigned long dev_ioaddr, const char *name, unsigned char *enetaddr)
{
	static int board_idx = -1;
	int i, rc;
	int option = -1, Cap10_100 = 0, Cap1000 = 0;

	ioaddr = dev_ioaddr;
	board_idx++;
	tpc = &tpx;

	rc = rtl8169_init_board(ioaddr, name);
	if (rc)
		return rc;

	for (i = 0; i < MAC_ADDR_LEN; i++)
		enetaddr[i] = RTL_R8(MAC0 + i);

	if (!(RTL_R8(PHYstatus) & TBI_Enable)) {
		int val = mdio_read(PHY_AUTO_NEGO_REG);
		option = (board_idx >= MAX_UNITS) ? 0 : media[board_idx];

		if (option > 0) {
			Cap10_100 = 0; Cap1000 = 0;
			switch (option) {
			case _10_Half: Cap10_100 = PHY_Cap_10_Half; break;
			case _10_Full: Cap10_100 = PHY_Cap_10_Full; break;
			case _100_Half: Cap10_100 = PHY_Cap_100_Half; break;
			case _100_Full: Cap10_100 = PHY_Cap_100_Full; break;
			case _1000_Full: Cap1000 = PHY_Cap_1000_Full; break;
			default: break;
			}
			mdio_write(PHY_AUTO_NEGO_REG, Cap10_100 | (val & 0x1F));
			mdio_write(PHY_1000_CTRL_REG, Cap1000);
		} else {
			mdio_write(PHY_AUTO_NEGO_REG,
			           PHY_Cap_10_Half | PHY_Cap_10_Full |
			           PHY_Cap_100_Half | PHY_Cap_100_Full | (val & 0x1F));
			mdio_write(PHY_1000_CTRL_REG, PHY_Cap_1000_Full);
		}

		mdio_write(PHY_CTRL_REG, PHY_Enable_Auto_Nego | PHY_Restart_Auto_Nego);

		/* FIX: Increase wait time for RTL8125 2.5G PHY stabilization */
		udelay(1000);
		for (i = 20000; i > 0; i--) {
			if (mdio_read(PHY_STAT_REG) & PHY_Auto_Nego_Comp) {
				udelay(100);
				break;
			}
			udelay(100);
		}
	} else {
		udelay(100);
	}

	tpc->RxDescArray = rtl_alloc_descs(NUM_RX_DESC);
	if (!tpc->RxDescArray) return -ENOMEM;
	tpc->TxDescArray = rtl_alloc_descs(NUM_TX_DESC);
	if (!tpc->TxDescArray) return -ENOMEM;

	return 0;
}

#ifndef CONFIG_DM_ETH
int rtl8169_initialize(bd_t *bis)
{
	pci_dev_t devno;
	int card_number = 0;
	struct eth_device *dev;
	u32 iobase;
	int idx = 0;

	while (1) {
		unsigned int region;
		u16 device;
		int err;

		if ((devno = pci_find_devices(supported, idx++)) < 0)
			break;

		pci_read_config_word(devno, PCI_DEVICE_ID, &device);

		/* FIX: Explicitly force BAR2 for RTL8125 and RTL8168 */
		switch (device) {
		case 0x8125:
		case 0x8168:
		case 0x8161:
			region = 2;
			break;
		default:
			region = 1;
			break;
		}

		pci_read_config_dword(devno, PCI_BASE_ADDRESS_0 + (region * 4), &iobase);
		iobase &= ~0xf;

		dev = malloc(sizeof(*dev));
		if (!dev) break;
		memset(dev, 0, sizeof(*dev));
		sprintf(dev->name, "RTL8169#%d", card_number);

		dev->priv = (void *)(unsigned long)devno;
		dev->iobase = (int)pci_mem_to_phys(devno, iobase);
		dev->init = rtl_reset;
		dev->halt = rtl_halt;
		dev->send = rtl_send;
		dev->recv = rtl_recv;

		/* FIX: Store device ID in private data for non-DM path */
		tpc = &tpx;
		tpc->device_id = device;

		err = rtl_init(dev->iobase, dev->name, dev->enetaddr);
		if (err < 0) {
			free(dev);
			continue;
		}

		/* FIX: Apply FuncEvent WAR for non-DM as well */
		ioaddr = dev->iobase;
		u32 val = RTL_R32(FuncEvent);
		val &= ~RxDv_Gated_En;
		RTL_W32(FuncEvent, val);

		eth_register(dev);
		card_number++;
	}
	return card_number;
}
#endif

#ifdef CONFIG_DM_ETH
static int rtl8169_eth_probe(struct udevice *dev)
{
	struct pci_child_platdata *pplat = dev_get_parent_platdata(dev);
	struct rtl8169_private *priv = dev_get_priv(dev);
	struct eth_pdata *plat = dev_get_platdata(dev);
	u32 iobase;
	int region;
	int ret;

	/* FIX: Explicitly force BAR2 for RTL8125 */
	switch (pplat->device) {
	case 0x8125:
	case 0x8168:
	case 0x8161:
		region = 2;
		break;
	default:
		region = 1;
		break;
	}

	dm_pci_read_config32(dev, PCI_BASE_ADDRESS_0 + region * 4, &iobase);
	iobase &= ~0xf;
	priv->iobase = (ulong)dm_pci_mem_to_phys(dev, iobase);

	/* FIX: Store device ID in private data */
	priv->device_id = pplat->device;
	tpc = priv;

	ret = rtl_init(priv->iobase, dev->name, plat->enetaddr);
	if (ret < 0)
		return ret;

	/* FIX: Clear RxDv_Gated_En to prevent DHCP failure after kernel reboot */
	ioaddr = priv->iobase;
	u32 val = RTL_R32(FuncEvent);
	debug("%s: FuncEvent/Misc (0xF0) = 0x%08X\n", __func__, val);
	val &= ~RxDv_Gated_En;
	RTL_W32(FuncEvent, val);

	return 0;
}

static const struct eth_ops rtl8169_eth_ops = {
	.start	= rtl8169_eth_start,
	.send	= rtl8169_eth_send,
	.recv	= rtl8169_eth_recv,
	.stop	= rtl8169_eth_stop,
};

static const struct udevice_id rtl8169_eth_ids[] = {
	{ .compatible = "realtek,rtl8169" },
	{ }
};

U_BOOT_DRIVER(eth_rtl8169) = {
	.name	= "eth_rtl8169",
	.id	= UCLASS_ETH,
	.of_match = rtl8169_eth_ids,
	.probe	= rtl8169_eth_probe,
	.ops	= &rtl8169_eth_ops,
	.priv_auto_alloc_size = sizeof(struct rtl8169_private),
	.platdata_auto_alloc_size = sizeof(struct eth_pdata),
};

U_BOOT_PCI_DEVICE(eth_rtl8169, supported);
#endif