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

#ifndef __STEPGEN_H__
#define __STEPGEN_H__

#define MAXGEN	4

void stepgen(void);
void stepgen_reset(void);
void stepgen_update_config(const void *buf);
int stepgen_update_input(const void *buf);

/*
  Timing diagram:
	       ____	       ____
  STEP	   ___/    \__________/    \___
  	   ___________
  DIR	              \________________

  stepwdth    |<-->|          |
  stepspace   |    |<-------->|
  dir_hold    |<----->|       |
  dirsetup    |       |<----->|

*/

typedef struct {
	int32_t delay, velocity[MAXGEN], pwmval, io_tris, io_lat;
} stepgen_input_struct;

typedef struct {
	int32_t stpwdth[MAXGEN], dirsetp[MAXGEN], dirhold[MAXGEN];
} stepgen_config_struct;

#endif				/* __STEPGEN_H__ */
