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

#ifndef __DEADALUSOS_H__
#define __DEADALUSOS_H__

#include "ultra_types.h"
#include "patch.h"
#include "patch_symbols.h"

typedef std::vector < u32 > QueueVector;

extern QueueVector g_MessageQueues;

HRESULT OS_Reset();
void OS_HLE_osCreateMesgQueue(DWORD dwQueue, DWORD dwMsgBuffer, DWORD dwMsgCount);
s64 OS_HLE___osProbeTLB(u32 vaddr);

void OS_HLE_EnableThreadFP();

inline BOOL OS_HLE_DoFPStuff()
{
	return g_osActiveThread_v.bFound;
}

#endif //__DEADALUSOS_H__

