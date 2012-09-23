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

#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#define SYS_FREQ                        (80000000ul)	/* Hz */
#define GetSystemClock()		(SYS_FREQ)
#define	GetPeripheralClock()		(GetSystemClock())
#define	GetInstructionClock()		(GetSystemClock())

#define LED0_TRIS			TRISFbits.TRISF3
#define LED0_IO				LATFbits.LATF3

#define DO0_TRIS			TRISDbits.TRISD0
#define DO0_IO				LATDbits.LATD0
#define DO0_IO_1			(LATDSET = _LATD_LATD0_MASK)
#define DO0_IO_0			(LATDCLR = _LATD_LATD0_MASK)

#define init_phy_rst()			(TRISCCLR = BIT_14)
#define set_phy_rst()			(LATCSET = BIT_14)
#define clr_phy_rst()			(LATCCLR = BIT_14)

#define STEPHI_A			(LATGSET = BIT_7)
#define STEPLO_A			(LATGCLR = BIT_7)
#define DIR_HI_A			(LATGSET = BIT_6)
#define DIR_LO_A			(LATGCLR = BIT_6)

#define STEPHI_X			(LATDSET = BIT_5)
#define STEPLO_X			(LATDCLR = BIT_5)
#define DIR_HI_X			(LATDSET = BIT_4)
#define DIR_LO_X			(LATDCLR = BIT_4)

#define STEPHI_Y			(LATDSET = BIT_3)
#define STEPLO_Y			(LATDCLR = BIT_3)
#define DIR_HI_Y			(LATDSET = BIT_2)
#define DIR_LO_Y			(LATDCLR = BIT_2)

#define STEPHI_Z			(LATGSET = BIT_9)
#define STEPLO_Z			(LATGCLR = BIT_9)
#define DIR_HI_Z			(LATGSET = BIT_8)
#define DIR_LO_Z			(LATGCLR = BIT_8)

#define DIR_HI_PWM			(LATFSET = BIT_4)
#define DIR_LO_PWM			(LATFCLR = BIT_4)

/* configure i/o pins as inputs */
#define reset_io()								\
	do {									\
		/* IO_00 to IO_14 */						\
		TRISBSET = 0x7FFF;						\
		/* IO_15 */							\
		TRISCSET = BIT_13;						\
	} while (0)

#define update_io(iotris, iolat)						\
	do {									\
		TRISB =  (TRISB & 0x8000) | (0x7FFF & (iotris));		\
		LATB  =  (LATB  & 0x8000) | (0x7FFF & (iolat));			\
		TRISC =  (TRISC & 0xDFFF) | (0x2000 & ((iotris) >> 2));		\
		LATC  =  (LATC  & 0xDFFF) | (0x2000 & ((iolat)  >> 2));		\
	} while (0)

#define read_io(a)								\
	do {									\
		(a) = PORTB & 0x7FFF;						\
		(a) |= (PORTC & 0x2000) << 2;					\
	} while (0)

#endif				/* __HARDWARE_H__ */
