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

#include <stdio.h>
#include <string.h>

#include "timer.h"
#include "uip.h"
#include "uip_arp.h"
#include "etherdev.h"
#include "hardware.h"
#include "stepgen.h"

#pragma config \
	FPLLODIV = DIV_1, \
	FPLLMUL = MUL_20, \
	FPLLIDIV = DIV_2, \
	FWDTEN = OFF, \
	FPBDIV = DIV_1,\
	POSCMOD = XT, \
	FNOSC = PRIPLL, \
	CP = OFF, \
	FSOSCEN = OFF, \
	WDTPS = PS4096

#pragma config \
	FMIIEN = ON, \
	FETHIO = ON		/* external PHY in MII/normal configuration */

#define BASEFREQ			100000
#define CORE_TICK_RATE	        	(SYS_FREQ/2/BASEFREQ)
#define CORE_DIVIDER			(BASEFREQ/CLOCK_CONF_SECOND)

#define ENABLE_WATCHDOG
#define ENABLE_TIMEOUT

struct timer timeout_timer;
static clock_time_t volatile timeval = 0;
#if defined(ENABLE_TIMEOUT)
int alive = 0;
#endif

clock_time_t clock_time(void)
{
	return timeval;
}

#define UDPBUF ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])
#define BUF    ((struct uip_eth_hdr *)&uip_buf[0])

void init_io_ports()
{
	/* disable all analog pins */
	AD1PCFG = 0xFFFF;

	LED0_TRIS = 0;
	LED0_IO = 0;

	// TODO, PWM
	OpenOC1(OC_OFF | OC_TIMER_MODE16 | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE, 0,0);

	/* configure step and dir pins
	   enable open collector */
	TRISDCLR = BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_0;
	ODCDSET = BIT_5 | BIT_4 | BIT_3 | BIT_2;

	TRISGCLR = BIT_7 | BIT_6 | BIT_9 | BIT_8;
	ODCGSET = BIT_7 | BIT_6 | BIT_9 | BIT_8;

	TRISFCLR = BIT_4;
	ODCFSET = BIT_4;

}

int main(void)
{
	int i;
	uip_ipaddr_t ipaddr;
	struct timer periodic_timer, arp_timer;
	struct uip_udp_conn *c;

	init_io_ports();

	/* Disable JTAG port */
	DDPCONbits.JTAGEN = 0;
	/* Enable optimal performance */
	SYSTEMConfigPerformance(GetSystemClock());
	/* Use 1:1 CPU Core:Peripheral clocks */
	OSCSetPBDIV(OSC_PB_DIV_1);

	/* configure the core timer roll-over rate */
	OpenCoreTimer(CORE_TICK_RATE);

	/* set up the core timer interrupt with a priority of 7
	   and zero sub-priority */
	mConfigIntCoreTimer((CT_INT_ON | CT_INT_PRIOR_7 | CT_INT_SUB_PRIOR_0));

	INTEnableSystemMultiVectoredInt();

	timer_set(&periodic_timer, CLOCK_SECOND / 2);
	timer_set(&arp_timer, CLOCK_SECOND * 10);

	ether_init();
	uip_init();

#if defined(ENABLE_WATCHDOG)
	WDTCONSET = 0x8000;
#endif

	uip_ipaddr(ipaddr, 10, 0, 0, 2);
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, 255, 255, 255, 0);
	uip_setnetmask(ipaddr);

	/* setup UDP port to listen to */
	c = uip_udp_new(NULL, HTONS(0));
	if (c != NULL)
		uip_udp_bind(c, HTONS(8888));

	/* main UIP control loop */
	while (1) {
		uip_len = ether_read();
		if (uip_len > 0) {
			if (BUF->type == htons(UIP_ETHTYPE_IP)) {
				uip_arp_ipin();
				uip_input();
				/* If the above function invocation resulted
				   in data that should be sent out on the
				   network, the global variable
				   uip_len is set to a value > 0. */
				if (uip_len > 0) {
					uip_arp_out();
					ether_send();
				}
			} else if (BUF->type == htons(UIP_ETHTYPE_ARP)) {
				uip_arp_arpin();
				/* If the above function invocation resulted
				   in data that should be sent out on the
				   network, the global variable
				   uip_len is set to a value > 0. */
				if (uip_len > 0)
					ether_send();
			}
		} else if (timer_expired(&periodic_timer)) {

			timer_reset(&periodic_timer);
			LED0_IO ^= 1;
#if defined(ENABLE_TIMEOUT)
			if (alive)
				alive = 0;
			else
				stepgen_reset();
#endif

			for (i = 0; i < UIP_UDP_CONNS; i++) {
				uip_udp_periodic(i);
				/* If the above function invocation resulted
				   in data that should be sent out on the
				   network, the global variable
				   uip_len is set to a value > 0. */
				if (uip_len > 0) {
					uip_arp_out();
					ether_send();
				}
			}

			/* Call the ARP timer function every 10 seconds. */
			if (timer_expired(&arp_timer)) {
				timer_reset(&arp_timer);
				uip_arp_timer();
			}
		}
#if defined(ENABLE_WATCHDOG)
		WDTCONSET = 0x01;
#endif
	}

}

/* this procedure is called whenever UDP data is received */
void udp_appcall(void)
{
	u16_t len;
	int32_t x;

	/* process received data */
	if (uip_newdata()) {
		len = uip_datalen();
		if (len > 3) {
			memcpy((void *)&x, (const void *)uip_appdata,
			       sizeof(x));

			switch (x) {
			case 0x5453523E:	/* >RST */
				stepgen_reset();
				len = 4;
				break;
			case 0x444D433E:	/* >CMD */
				len = stepgen_update_input((const void *)
							   uip_appdata + 4);
#if defined(ENABLE_TIMEOUT)
				/* interface is alive */
				alive = 1;
#endif
				break;
			case 0x4746433E:	/* >CFG */
				stepgen_update_config((const void *)uip_appdata
						      + 4);
				len = 4;
				break;
			}
			*((char *)uip_appdata) = '<';
		}
		uip_ipaddr_copy(&uip_udp_conn->ripaddr, &UDPBUF->srcipaddr);
		uip_udp_conn->rport = UDPBUF->srcport;
		uip_udp_send(len);

	} else if (uip_poll())
		uip_udp_conn->rport = 0;     /* close remote port connection */
}

/* stepgen code is called every timer interrupt */
void __ISR(_CORE_TIMER_VECTOR, ipl7) CoreTimerHandler(void)
{
	static int count = CORE_DIVIDER;

	/* update the period */
	UpdateCoreTimer(CORE_TICK_RATE);

	/* scale UIP timer value */
	if (--count <= 0) {
		timeval++;
		count = CORE_DIVIDER;
	}

	stepgen();

	/* clear the interrupt flag */
	mCTClearIntFlag();
}
