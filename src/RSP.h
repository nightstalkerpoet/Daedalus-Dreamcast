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

#ifndef __RSP_H__
#define __RSP_H__



void RSP_Reset(void);

void RSPStep(void);
void RSPStepHalted(void);


#define RSPPC() (*(DWORD *)(g_pMemoryBuffers[MEM_SP_PC_REG]))

extern BOOL			g_bRSPHalted;
extern DWORD		g_dwRSPGPR[32];

#define StartRSP() { g_bRSPHalted = FALSE; CPU_SelectCore(); }
#define StopRSP()  { g_bRSPHalted = TRUE; CPU_SelectCore(); }

															
void RSP_DumpVector(DWORD dwReg);
void RSP_DumpVectors(DWORD dwReg1, DWORD dwReg2, DWORD dwReg3);

#endif
