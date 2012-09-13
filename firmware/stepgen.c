//    Copyright (C) 2012 GP Orcullo
//    
//    This file is part of rt-8p8c, an ethernet based interface for LinuxCNC.
//
//    This step generator code is largely based on stepgen.c 
//    by John Kasunich, Copyright (C) 2003-2007 
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#include <p32xxxx.h>
#include <plib.h>

#include <string.h>

#include "hardware.h"
#include "stepgen.h"

#define STEP_MASK	(1<<14)
#define DIR_MASK	(1<<31)

/*
  Timing diagram:
	       ____	       ____
  STEP	   ___|    |__________|    |___
  	   ___________
  DIR	              |________________
		
  stepwdth    |<-->|          |   
  stepspace   |    |<-------->|   
  dir_hold    |<----->|       |
  dirsetup    |       |<----->|
	     
*/

static int stepwdth[MAXGEN] = {0},
	   dirsetup[MAXGEN] = {0},
	   dir_hold[MAXGEN] = {0};	// must be < step period

volatile int32_t position[MAXGEN] = {0};	
static int32_t	 oldpos[MAXGEN]   = {0},
		 oldvel[MAXGEN]   = {0};

volatile stepgen_input_struct stepgen_input = {{(1<<13),(1<<12),(1<<11),(1<<10)},0};
volatile stepgen_config_struct stepgen_config = {{1,1,1,1},
						 {1,1,1,1},
						 {1,1,1,1}};

void stepgen_update_config(const void *buf)
{
	// disable interrupts
	asm volatile("di");
	asm volatile("ehb");
	memcpy((void*)&stepgen_config, buf, sizeof(stepgen_config));
	// enable interrupts
	asm volatile("ei");
	
}

int stepgen_update_input(const void *buf)
{
	// disable interrupts
	asm volatile("di");
	asm volatile("ehb");
	memcpy((void*)&stepgen_input, buf, sizeof(stepgen_input));
	// enable interrupts
	asm volatile("ei");

	// wait for delay
	while (stepgen_input.delay);

	// send result
	// disable interrupts
	asm volatile("di");
	asm volatile("ehb");
	memcpy((void*)buf, (const void*)position, sizeof(position));
	// enable interrupts
	asm volatile("ei");
	
	return 20;

}

void stepgen_reset(void)
{
	int i;
	
	for (i=0;i<MAXGEN;i++) {
		stepwdth[i] = 0;
		dirsetup[i] = 0;
		dir_hold[i] = 0;
		position[i] = 0;
		oldpos[i]   = 0;
		oldvel[i]   = 0;
		
		stepgen_input.velocity[i] = 0;
		stepgen_input.delay       = 0;

		DIR_LO_X;
		DIR_LO_Y;
		DIR_LO_Z;
		DIR_LO_A;
		
		STEPLO_X;
		STEPLO_Y;
		STEPLO_Z;
		STEPLO_A;
	}
}
	
void stepgen(void)
{

	int i, stepready;

	// counter for reply delay
	if (stepgen_input.delay) {
		stepgen_input.delay--;
	}
	
	for (i=0; i<4; i++) {
		// direction setup counter checks if 
		// a step pulse can be generated
		if (dirsetup[i]) {
			dirsetup[i]--;
		}
		
		stepready = (position[i] ^ oldpos[i]) & STEP_MASK;
		
		// don't update position counter if dirsetup[i] <> 0
		// and a step pulse is ready to be generated
		// (stretch the step-low period)
		if (!(dirsetup[i] && stepready)) {
			position[i] += stepgen_input.velocity[i];
		}
		
		// generate a pulse only if dirsetup[i] == 0
		if (!dirsetup[i] && stepready) {
			oldpos[i] = position[i];
			// start step width counter
			stepwdth[i] = stepgen_config.stpwdth[i];	
			// start direction hold counter
			dir_hold[i] = stepgen_config.dirhold[i];	
		}

		// direction hold counter checks if 
		// a direction change is allowed
		if (dir_hold[i]) {
			dir_hold[i]--;
		} else { 
		
			// check for direction change
			if ((stepgen_input.velocity[i] ^ oldvel[i]) & DIR_MASK) {
				oldvel[i] = stepgen_input.velocity[i];
				// direction setup counter
				dirsetup[i] = stepgen_config.dirsetp[i];
				
				if (stepgen_input.velocity[i] > 0) {

					if (i==0) DIR_HI_X;
					if (i==1) DIR_HI_Y;
					if (i==2) DIR_HI_Z;
					if (i==3) DIR_HI_A;
				} else {
					if (i==0) DIR_LO_X;
					if (i==1) DIR_LO_Y;
					if (i==2) DIR_LO_Z;
					if (i==3) DIR_LO_A;
				}
			}
		}
		
		if (stepwdth[i]) {
			// start generating the step pulse while
			// counter is not 0
			stepwdth[i]--;

			if (i==0) STEPHI_X;
			if (i==1) STEPHI_Y;
			if (i==2) STEPHI_Z;
			if (i==3) STEPHI_A;
		} else {
			if (i==0) STEPLO_X;
			if (i==1) STEPLO_Y;
			if (i==2) STEPLO_Z;
			if (i==3) STEPLO_A;
		}
	}
}
