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

// This file contains high level os emulation routines
#include "os.h"
#include "OSMesgQueue.h"
#include "ultra_r4300.h"

// For keeping track of message queues we've created:
QueueVector g_MessageQueues;


HRESULT OS_Reset()
{
	g_MessageQueues.clear();

	return S_OK;
}

void OS_HLE_osCreateMesgQueue(DWORD dwQueue, DWORD dwMsgBuffer, DWORD dwMsgCount)
{
	COSMesgQueue q(dwQueue);

	q.SetEmptyQueue(VAR_ADDRESS(osNullMsgQueue));
	q.SetFullQueue(VAR_ADDRESS(osNullMsgQueue));
	q.SetValidCount(0);
	q.SetFirst(0);
	q.SetMsgCount(dwMsgCount);
	q.SetMesgArray(dwMsgBuffer);

	//DBGConsole_Msg(0, "osCreateMsgQueue(0x%08x, 0x%08x, %d)",
	//	dwQueue, dwMsgBuffer, dwMsgCount);

	LONG i;

	for (i = 0; i < g_MessageQueues.size(); i++)
	{
		if (g_MessageQueues[i] == dwQueue)
			return;		// Already in list

	}
	g_MessageQueues.push_back(dwQueue);
}

// ENTRYHI left untouched after call
s64 OS_HLE___osProbeTLB(u32 vaddr)
{   
	DWORD dwPAddr = ~0;	// Return -1 on failure
	
	DWORD dwPID = (u32)g_qwCPR[0][C0_ENTRYHI] & TLBHI_PIDMASK;
	DWORD dwVPN2 = vaddr & TLBHI_VPN2MASK;
	DWORD dwPageMask;
	DWORD dwEntryLo;
	DWORD i;

	// Code from TLBP and TLBR

    for(i = 0; i < 32; i++)
	{
		if( ((g_TLBs[i].hi & TLBHI_VPN2MASK) == dwVPN2) &&
			(
				(g_TLBs[i].g) ||
				((g_TLBs[i].hi & TLBHI_PIDMASK) == dwPID)
			) )
		{
			// We've found the page, do TLBR
			dwPageMask = g_TLBs[i].mask;

			dwPageMask += 0x2000;
			dwPageMask >>= 1;

			if ((vaddr & dwPageMask) == 0)
			{
				// Even Page (EntryLo0)
				dwEntryLo = g_TLBs[i].pfne | g_TLBs[i].g;
			}
			else
			{
				// Odd Page (EntryLo1)
				dwEntryLo = g_TLBs[i].pfno | g_TLBs[i].g;
			}	

			dwPageMask--;

			// If valid is not set, then the page is invalid
			if (dwEntryLo & TLBLO_V != 0)
			{
				dwEntryLo &= TLBLO_PFNMASK;
				dwEntryLo <<= TLBLO_PFNSHIFT;

				dwPAddr = dwEntryLo + (dwPageMask & vaddr);
			}

			break;
        }       
    }

	//DBGConsole_Msg(0, "Probe: 0x%08x -> 0x%08x", vaddr, dwPAddr);
	return (s64)(s32)dwPAddr;

}


void OS_HLE_EnableThreadFP()
{	
	DWORD dwThread;

	// Get the current active thread:
	dwThread = Read32Bits(VAR_ADDRESS(osActiveThread));

	// Set thread fp used
	Write32Bits(dwThread + offsetof(OSThread, fp), 1);

	// Set SR_CU1 bit now
	g_qwCPR[0][C0_SR] |= SR_CU1;
}

