/*    Copyright (C) 2012 GP Orcullo
 *    
 *    This file is part of rt-8p8c, an ethernet based interface for LinuxCNC.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <p32xxxx.h>
#include <plib.h>
#include <sys/kmem.h>
#include <peripheral/eth.h>

#include <inttypes.h>
#include <string.h>

#include "etherdev.h"
#include "hardware.h"
#include "uip.h"
#include "uip-conf.h"

#define	PHY_ADDRESS	0x00

#define NUM_RXBUF	1
#define NUM_TXBUF	1
#define BUFSIZE		(UIP_CONF_BUFFER_SIZE)

typedef struct {
	volatile union {
		struct {
			unsigned:7;
			unsigned EOWN:1;
			unsigned NPV:1;
			unsigned:7;
			unsigned bCount:11;
			unsigned:3;
			unsigned EOP:1;
			unsigned SOP:1;
		};
		unsigned int w;
	} hdr;			/* descriptor header */
	unsigned char *pEDBuff;	/* data buffer address */
	volatile union {
		struct {
			unsigned int hi;
			unsigned int lo;
		};
		unsigned long long ll;
	} stat;			/* tx packet status */
	unsigned int next_ed;	/* next descriptor */
} __attribute__((__packed__)) sEthTxDcpt;	/* hardware Tx descriptor */


static sEthTxDcpt *txdcpt[NUM_TXBUF + 1] = { 0 };
static sEthTxDcpt *rxdcpt[NUM_RXBUF] = { 0 };
static unsigned char *rxtxbuf[NUM_RXBUF + NUM_TXBUF + 1] = { 0 };

u8_t *uip_buf = 0;

extern struct uip_eth_addr uip_ethaddr;

void DelayMs(unsigned int msec)
{
	unsigned int tWait, tStart;

	tWait = (SYS_FREQ / 2000) * msec;
	tStart = ReadCoreTimer();
	/* wait for the time to pass */
	while ((ReadCoreTimer() - tStart) < tWait) ;
}

static void buffer_init()
{
	int i;

	/* Allocate RX TX buffer */
	for (i = 0; i < (NUM_RXBUF + NUM_TXBUF + 1); i++) {
		if (!rxtxbuf[i]) {
			rxtxbuf[i] = (unsigned char *)
			    malloc((unsigned char)BUFSIZE);
		}
	}

	uip_buf = rxtxbuf[0];

	/* Allocate RX descriptor */
	for (i = 0; i < (NUM_RXBUF); i++) {
		if (!rxdcpt[i]) {
			rxdcpt[i] = (sEthTxDcpt *)
			    (malloc((unsigned char)(sizeof(sEthTxDcpt))));
		}
	}

	/* initialize RX descriptors
	   this is a ring buffer */
	for (i = 0; i < (NUM_RXBUF); i++) {
		rxdcpt[i]->hdr.w = 0;		/* clear fields */
		rxdcpt[i]->hdr.EOWN = 1;	/* This is owned by HW */
		rxdcpt[i]->hdr.NPV = 1;		/* Next entry is valid pointer */
		rxdcpt[i]->hdr.bCount = 0;
		rxdcpt[i]->pEDBuff = (unsigned char *)KVA_TO_PA(rxtxbuf[i + 1]);
		rxdcpt[i]->next_ed = KVA_TO_PA(rxdcpt[i + 1]);
	}
	/* Point tail to start of ring buffer */
	rxdcpt[NUM_RXBUF - 1]->next_ed = KVA_TO_PA(rxdcpt[0]);

	/* Allocate TX descriptor */
	for (i = 0; i < (NUM_TXBUF + 1); i++) {
		if (!txdcpt[i]) {
			txdcpt[i] = (sEthTxDcpt *)
			    (malloc((unsigned char)(sizeof(sEthTxDcpt))));
		}
	}

	/* initialize TX descriptors
	   this is a linked list */
	for (i = 0; i < (NUM_TXBUF); i++) {
		txdcpt[i]->hdr.w = 0;		/* clear fields */
		txdcpt[i]->hdr.EOWN = 1;	/* This is owned by HW */
		txdcpt[i]->hdr.NPV = 1;		/* Next entry is valid pointer */
		txdcpt[i]->hdr.bCount = 0;
		txdcpt[i]->pEDBuff = (unsigned char *)
		    KVA_TO_PA(rxtxbuf[i + 1 + NUM_RXBUF]);
		txdcpt[i]->next_ed = KVA_TO_PA(txdcpt[i + 1]);
	}
	/* this is end of list */
	txdcpt[NUM_TXBUF]->hdr.w = 0;		/* clear fields */
}

static u16_t readphy(u8_t addr, u8_t reg)
{
	EMAC1MADR = (addr & 0x1f) << 8 | (reg & 0x1f);
	EMAC1MCMDSET = _EMAC1MCMD_READ_MASK;

	__asm__ __volatile__("nop; nop; nop;");	/* wait read complete */
	while (EMAC1MINDbits.MIIMBUSY) ;	/* wait until not busy */

	EMAC1MCMDCLR = _EMAC1MCMD_READ_MASK;

	return (EMAC1MRDD & 0xffff);
}

static void writephy(u8_t addr, u8_t reg, u16_t data)
{
	EMAC1MADR = (addr & 0x1f) << 8 | (reg & 0x1f);
	EMAC1MWTD = data;

	__asm__ __volatile__("nop; nop; nop;");	/* wait write complete */
	while (EMAC1MINDbits.MIIMBUSY) ;	/* wait until not busy */
}

void init_ether_pins(void)
{
	TRISBCLR = BIT_15;
	TRISDCLR = BIT_6 | BIT_8 | BIT_9;
	TRISECLR = BIT_5 | BIT_6 | BIT_7;

	TRISDSET = BIT_1 | BIT_7 | BIT_10 | BIT_11;
	TRISESET = BIT_0 | BIT_1 | BIT_2 | BIT_3 | BIT_4;
	TRISFSET = BIT_0 | BIT_1;
}

void set_pin_strap()
{
	/* Set  PHY addr to 0x01 */
	TRISECLR = BIT_0;
	LATECLR = BIT_0;
	TRISFCLR = BIT_1 | BIT_0;
	LATFSET = BIT_1;
	LATFSET = BIT_0;
}

void ether_init(void)
{

	/* Perform physical reset */
	init_phy_rst();
	clr_phy_rst();
//      set_pin_strap();
	DelayMs(100);
	set_phy_rst();

	/* Ethernet Controller Initialization
	   disable Ethernet interrupts */
	IEC1CLR = _IEC1_ETHIE_MASK;

	/* reset: disable ON, clear TXRTS, RXEN */
	ETHCON1CLR = (_ETHCON1_ON_MASK | _ETHCON1_TXRTS_MASK
		      | _ETHCON1_RXEN_MASK);

	/* wait everything down */
	while (ETHSTATbits.BUSY) ;

	/* clear the interrupt controller flag */
	IFS1CLR = _IFS1_ETHIF_MASK;

	ETHIENCLR = 0x000063ef;		/* disable all events */
	ETHIRQCLR = 0x000063ef;		/* clear any existing interrupt event */

	ETHTXSTCLR = 0xfffffffc;	/* clear tx start address */
	ETHRXSTCLR = 0xfffffffc;	/* clear rx start address */

	/* MAC Init */
	EMAC1MCFGSET = _EMAC1CFG1_SOFTRESET_MASK | _EMAC1CFG1_SIMRESET_MASK |
	    _EMAC1CFG1_RESETRMCS_MASK | _EMAC1CFG1_RESETRFUN_MASK |
	    _EMAC1CFG1_RESETTMCS_MASK | _EMAC1CFG1_RESETTFUN_MASK;

	/* delay */
	__asm__ __volatile__("nop; nop; nop;");

	/* clear reset */
	EMAC1MCFGCLR = _EMAC1CFG1_SOFTRESET_MASK | _EMAC1CFG1_SIMRESET_MASK |
	    _EMAC1CFG1_RESETRMCS_MASK | _EMAC1CFG1_RESETRFUN_MASK |
	    _EMAC1CFG1_RESETTMCS_MASK | _EMAC1CFG1_RESETTFUN_MASK;

	/* Properly initialize as digital, all the pins used by the MAC - PHY 
	   interface (normally only those pins that have shared analog 
	   functionality need to be configured).
	   All are digital pins for this app. */

	init_ether_pins();

	/* Initialize the MIIM interface */
	EMAC1TESTSET = _EMAC1MCFG_RESETMGMT_MASK;
	EMAC1TESTCLR = _EMAC1MCFG_RESETMGMT_MASK;

	EMAC1MCFGbits.CLKSEL = 0x8;	/* MDC = SYSCLK / 40 */

	/* PHY Init */
	writephy(PHY_ADDRESS, 0, 0x8000);	/* soft reset */

	__asm__ __volatile__("nop; nop; nop;");	/* delay */

	/* wait until reset is cleared */
	while (readphy(PHY_ADDRESS, 0) && 0x8000) ;

	writephy(PHY_ADDRESS, 0, 0x2100);	/* 100 Mbps, full duplex */
	writephy(PHY_ADDRESS, 0x16, 0x0001);	/* Clear NAND Tree */
	writephy(PHY_ADDRESS, 31, 0x80);	/* auto-MDIX */

	/* MAC Configuration */

	/* Set RXENABLE */
	EMAC1CFG1 = _EMAC1CFG1_RXENABLE_MASK | _EMAC1CFG1_PASSALL_MASK;

	/* Full-duplex, auto-padding, crc enable, no huge frames, length check */
	EMAC1CFG2 = _EMAC1CFG2_FULLDPLX_MASK | _EMAC1CFG2_CRCENABLE_MASK |
	    _EMAC1CFG2_PADENABLE_MASK | _EMAC1CFG2_LENGTHCK_MASK;

	/* Set back-to-back inter-packet gap */
	EMAC1IPGT = 0x15;	/* default for full-duplex. 0x12 - half-duplex */

	/* Set non back-to-back inter-packet gap */
	EMAC1IPGR = 0x0c12;	/* default value */

	/* Set collision window and the maximum number of retransmissions */
	EMAC1CLRT = 0x370f;	/* default value */

	/* Set maximum frame length */
	EMAC1MAXF = 0x05ee;	/* default value */

	/* get station MAC address */
	uip_ethaddr.addr[0] = EMAC1SA2 & 0xff;
	uip_ethaddr.addr[1] = (EMAC1SA2 & 0xff00) >> 8;
	uip_ethaddr.addr[2] = EMAC1SA1 & 0xff;
	uip_ethaddr.addr[3] = (EMAC1SA1 & 0xff00) >> 8;
	uip_ethaddr.addr[4] = EMAC1SA0 & 0xff;
	uip_ethaddr.addr[5] = (EMAC1SA0 & 0xff00) >> 8;

	/* Disable flow control */
	ETHCON1CLR = _ETHCON1_MANFC_MASK | _ETHCON1_AUTOFC_MASK;

	/* Set RX filters
	   Enable CRC, broadcast and unicast only */
	ETHRXFC =
	    _ETHRXFC_BCEN_MASK | _ETHRXFC_UCEN_MASK | _ETHRXFC_CRCOKEN_MASK;

	/* Set RX descriptor data buffer size */
	ETHCON2 = (BUFSIZE / 16) << 4;

	buffer_init();		/* initialize tx and rx descriptors */

	ETHRXST = KVA_TO_PA(rxdcpt[0]);
	ETHTXST = KVA_TO_PA(txdcpt[0]);

	ETHCON1SET = _ETHCON1_ON_MASK;		/* Enable ethernet */
	ETHCON1SET = _ETHCON1_RXEN_MASK;	/* and start RX */
}

unsigned int ether_read(void)
{
	unsigned char *tmp;
	unsigned int ret;
	int i;

	ret = 0;
	/* check if there is a pending packet */
	if (ETHIRQbits.RXDONE) {
		/* search one used descriptor */
		for (i = NUM_RXBUF - 1; i >= 0; i--)
			if (!(rxdcpt[i]->hdr.EOWN)) {
				/* make sure this is a small packet */
				if (rxdcpt[i]->hdr.EOP && rxdcpt[i]->hdr.SOP) {

					/* swap buffers */
					tmp = rxdcpt[i]->pEDBuff;
					rxdcpt[i]->pEDBuff = (unsigned char *)
					    KVA_TO_PA(uip_buf);

					uip_buf = (unsigned char *)
					    PA_TO_KVA1((unsigned int)tmp);

					ret = rxdcpt[i]->stat.lo & 0xffff;
				}
				/* release, NPV = EOWN = 1 */
				rxdcpt[i]->hdr.w = 0x00000180;
				/* decrement descriptor counter */
				ETHCON1SET = _ETHCON1_BUFCDEC_MASK;
			}
		ETHIRQCLR = _ETHIRQ_RXDONE_MASK;	/* clear done bit */
	}

	return ret;
}

void ether_send(void)
{
	unsigned char *tmp;

	/* wait until TX is not busy */
	while (ETHCON1bits.TXRTS || ETHSTATbits.TXBUSY) ;

	txdcpt[0]->hdr.w = 0xc0000180;	/* NPV = EOWN = SOP = EOP = 1 */
	txdcpt[0]->hdr.bCount = uip_len;

	/* swap buffers */
	tmp = txdcpt[0]->pEDBuff;
	txdcpt[0]->pEDBuff = (unsigned char *)KVA_TO_PA(uip_buf);

	uip_buf = (unsigned char *)PA_TO_KVA1((unsigned int)tmp);

	/* update pointer and then start sending */
	ETHTXST = KVA_TO_PA(txdcpt[0]);
	ETHCON1SET = _ETHCON1_TXRTS_MASK;
}
