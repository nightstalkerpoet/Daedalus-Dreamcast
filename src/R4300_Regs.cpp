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

#include "main.h"

#include "R4300.h"
#include "R4300_Regs.h"
#include "CPU.h"

// 32 64-bit General Purpose Registers
DWORD	g_dwPC			= 0x80000000;
u64	g_qwMultHI			= 0;
u64	g_qwMultLO			= 0;
u64	g_qwFCR0			= 0;
u64	g_qwFCR1			= 0;
DWORD	g_bLLbit		= 0;

DWORD	g_dwNewPC		= 0;
u64	g_qwGPR[32];
u64	g_qwCPR[3][32];
u64	g_qwCCR[3][32];
TLBEntry g_TLBs[32];
int		g_nDelay				= NO_DELAY;		// If this is set at the start of an instruction,
BYTE	*g_pPCMemBase			= NULL;

