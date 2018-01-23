/*
Copyright (C) 2001 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef __R4300_REGS_H__
#define __R4300_REGS_H__

#include "ultra_types.h"

struct TLBEntry
{
	DWORD pagemask, hi, pfne, pfno;

	// For speed/convenience
	DWORD mask, mask2;			// Mask, Mask/2
	DWORD vpnmask, vpn2mask;	// Vpn Mask, VPN/2 Mask
	DWORD pfnohi, pfnehi;		// Even/Odd highbits
	DWORD g, checkbit;

	DWORD addrcheck;			// vpn2 & vpnmask
	DWORD lastaccessed;			// for least recently used algorithm
};


extern DWORD	g_dwPC;
extern u64	g_qwMultHI;
extern u64	g_qwMultLO;
extern u64	g_qwFCR0;
extern u64	g_qwFCR1;
extern DWORD	g_bLLbit;

extern DWORD	g_dwNewPC;
extern u64	g_qwGPR[32];
extern u64	g_qwCPR[3][32];
extern u64	g_qwCCR[3][32];
extern TLBEntry g_TLBs[32];
extern int		g_nDelay;
extern BYTE		*g_pPCMemBase;





#endif
