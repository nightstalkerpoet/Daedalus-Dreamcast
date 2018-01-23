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

#ifndef __RDP_H__
#define __RDP_H__

#include "windef.h"

#include "ultra_sptask.h"


extern DWORD g_dwViWidth;
extern DWORD g_dwViHeight;
extern DWORD g_dwNumFrames;

typedef void (*RDPInstruction)(DWORD dwCmd0, DWORD dwCmd1);


extern BOOL	g_bRDPHalted;
extern DWORD g_dwRDPTime;

HRESULT RDPInit();
void RDPCleanup();
void RDP_Reset();
void RDP_LoadTask(OSTask * pTask);
void RDP_ExecuteTask(OSTask * pTask);


void RDPStep();
void RDPStepHalted();

void RDPHalt();

#define StartRDP() { g_bRDPHalted = FALSE; CPU_SelectCore(); }
#define StopRDP()  { g_bRDPHalted = TRUE; CPU_SelectCore();  }

void RDP_EnableGfx();
void RDP_DisableGfx();

void RDP_DumpRSPCode(char * szName, DWORD dwCRC, DWORD * pBase, DWORD dwPCBase, DWORD dwLen);
void RDP_DumpRSPData(char * szName, DWORD dwCRC, DWORD * pBase, DWORD dwPCBase, DWORD dwLen);
#endif
