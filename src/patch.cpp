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

#include "debug.h"
#include "memory.h"
#include "cpu.h"		// For AddCPUJob()
#include "R4300.h"
#include "R4300_Regs.h"
#include "patch.h"
#include "registers.h"
#include "ROM.h"

#include "daedalus_crc.h"
#include "patch_symbols.h"
#include "os.h"

#include <stddef.h>		// offsetof


#include "ultra_os.h"
#include "ultra_rcp.h"
#include "ultra_sptask.h"
#include "ultra_r4300.h"

#include "OSMesgQueue.h"

#include "rdp.h"		// RDP_LoadTask
#include "DBGConsole.h"

extern PatchSymbol g___osDispatchThread_s;
extern PatchSymbol g___osEnqueueAndYield_s;


typedef struct 
{
	DWORD dwThread;
	DWORD dwTime;
} ThreadTimes;

std::vector<ThreadTimes> g_ThreadTimes;

/*
osSystemTimeHi is 0x80367110
osSystemTimeLo is 0x80367114
osSystemCount is 0x80367118
osSystemLastCount is 0x80367120
osNullMsgQueue is 0x803359a0
osThreadQueue is 0x803359a8
osActiveThread is 0x803359b0
osEventMesgArray is 0x80364ba0
osThreadDieRA is 0x80327ea8
osGlobalThreadList is 0x803359ac

osDispatchThreadRCPThingamy is 0x80339a40
*****osMarioKartUnk is 0x80000000
osEepPifBuffer is 0x80367170
osEepPifThingamy is 0x803671ac
osEepPifThingamy2 is 0x80367090
osAiSetNextBufferThingy is 0x80335930
osPiAccessQueueCreated is 0x80335a50
osPiAccessQueueBuffer is 0x80367130


// MarioKart
osSystemTimeHi is 0x80197600
osSystemTimeLo is 0x80197604
osSystemCount is 0x80197608
osSystemLastCount is 0x80197610
osNullMsgQueue is 0x800eb3a0
osEventMesgArray is 0x80196440
osThreadDieRA is 0x800d1aa0
osGlobalThreadList is 0x800eb3ac
osThreadQueue is 0x800eb3a8
osActiveThread is 0x800eb3b0


osDispatchThreadRCPThingamy is 0x800f3c10
osMarioKartUnk is 0x800ea5ec
osEepPifBuffer is 0x80197660
osEepPifThingamy is 0x8019769c
osEepPifThingamy2 is 0x80196540
osAiSetNextBufferThingy is 0x800eb370
osPiAccessQueueCreated is 0x800eb440
osPiAccessQueueBuffer is 0x80197620

*/


//static const DWORD g_dwSystemTimeHi		= 0x80367110;
//static const DWORD g_dwSystemTimeLo		= 0x80367114;
//static const DWORD g_dwSystemCount		= 0x80367118;
//static const DWORD g_dwNullMsgQueue		= 0x803359a0;	
//static const DWORD g_dwThreadQueue		= 0x803359a8;
//static const DWORD g_dwActiveThread		= 0x803359b0;
//static const DWORD g_dwEventMsgBase		= 0x80364ba0;
//static const DWORD g_dwThreadDieRA		= 0x80327ea8;
//static const DWORD g_dwGlobalThreadList	= 0x803359ac;
//static const DWORD g_dwosDispatchThreadRCPThingamy = 0x80339a40;
//static const DWORD g_dwPiAccessQueueCreated = 0x80335a50;
//static const DWORD g_dwPiAccessQueueBuffer = 0x80367130;


//static const DWORD g_dwTimerList		= 0x80335940;
//static const DWORD g_dwFrameCount		= 0x8036711c;

//static const DWORD g_dwFaultedThread	= 0x803359b4;
//static const DWORD g_dwActiveQueue		= 0x80335a20; - not sure if this is correct - ultrahle seems to incorrectly identify
//static const DWORD g_dwIP7Flag			= 0x80335a44;
//static const DWORD g_dwIP6Flag			= 0x80335a48;
//static const DWORD g_dwIntLookup		= 0x80339980;	// fuddle interrupt bits and lookup byte value
//static const DWORD g_dwIntLookup2		= 0x803399a0;	// fuddle interrupt bits and lookup word value
//static const DWORD g_dwKernelHWThread	= 0x803672b0;
//static const DWORD g_dwLastCount		= 0x80367120;
//static const DWORD g_dwSiAccessQueueCreated = 0x80335a60;
//static const DWORD g_dwSiAccessQueueBuffer = 0x80367150;
//static const DWORD g_dwSiAccessQueue = 0x80367158;
//static const DWORD g_dwPiAccessQueue = 0x80367138;
// osEepBuffer 0x80367170

//g_dwViThingamme 0x80335a24 points to Vi info of some description
//static const DWORD dwSpTempTask = 0x80364c20;
// 
static DWORD g_dwControllerMQ = 0;

static DWORD g_dwNumNormalize = 0;
static DWORD g_dwNumMtxF2L = 0;
static DWORD g_dwNumMtxIdent = 0;
static DWORD g_dwNumMtxTranslate = 0;
static DWORD g_dwNumMtxScale = 0;
static DWORD g_dwNumMtxRotate = 0;
static DWORD g_dwNumCosSin = 0;



static const char * g_szEventStrings[23] = 
{
	"OS_EVENT_SW1",
	"OS_EVENT_SW2",
	"OS_EVENT_CART", 
	"OS_EVENT_COUNTER",
	"OS_EVENT_SP",
	"OS_EVENT_SI",
	"OS_EVENT_AI",
	"OS_EVENT_VI",
	"OS_EVENT_PI",
	"OS_EVENT_DP",
	"OS_EVENT_CPU_BREAK",
	"OS_EVENT_SP_BREAK",
	"OS_EVENT_FAULT",
	"OS_EVENT_THREADSTATUS",
	"OS_EVENT_PRENMI",
	"OS_EVENT_RDB_READ_DONE",
	"OS_EVENT_RDB_LOG_DONE",
	"OS_EVENT_RDB_DATA_DONE",
	"OS_EVENT_RDB_REQ_RAMROM",
	"OS_EVENT_RDB_FREE_RAMROM",
	"OS_EVENT_RDB_DBG_DONE",
	"OS_EVENT_RDB_FLUSH_PROF",
	"OS_EVENT_RDB_ACK_PROF"
};


inline BOOL IsSpDeviceBusy()
{
	DWORD dwStatus = Read32Bits(PHYS_TO_K1(SP_STATUS_REG));

	if (dwStatus & (SP_STATUS_IO_FULL | SP_STATUS_DMA_FULL | SP_STATUS_DMA_BUSY))
		return TRUE;
	else
		return FALSE;
}

inline DWORD SpGetStatus()
{
	return Read32Bits(PHYS_TO_K1(SP_STATUS_REG));
}

#define TEST_DISABLE_FUNCS //return PATCH_RET_NOT_PROCESSED;





BOOL g_bPatchesInstalled = FALSE;

//DWORD g_dwOSStart = 0x00240000;
//DWORD g_dwOSEnd   = 0x00380000;


void Patch_ResetSymbolTable();
void Patch_RecurseAndFind();
BOOL Patch_LocateFunction(PatchSymbol * ps);
static BOOL Patch_VerifyLocation(PatchSymbol * ps, DWORD dwIndex);
static BOOL Patch_VerifyLocation_CheckSignature(PatchSymbol * ps, PatchSignature * psig, DWORD dwIndex);

static void Patch_ApplyPatch(DWORD i);


void Patch_Reset()
{
	g_bPatchesInstalled = FALSE;

	Patch_ResetSymbolTable();
}

void Patch_ResetSymbolTable()
{
	LONG i;

	// Loops through all symbols, until name is null
	for (i = 0; i < ARRAYSIZE(g_PatchSymbols); i++)
	{
		g_PatchSymbols[i]->bFound = FALSE;
	}

	for (i = 0; i < ARRAYSIZE(g_PatchVariables); i++)
	{
		g_PatchVariables[i]->bFound = FALSE;
		g_PatchVariables[i]->bFoundHi = FALSE;
		g_PatchVariables[i]->bFoundLo = FALSE;
	}
}


void Patch_ApplyPatches()
{
	if (!g_bApplyPatches)
		return;

	Patch_RecurseAndFind();

	// Do this every time or just when originally patched
	/*hr = */OS_Reset();

	// This may already be set if this function has been called before
	g_bPatchesInstalled = TRUE;
}


void Patch_ApplyPatch(DWORD i)
{
	DWORD dwPC = g_PatchSymbols[i]->dwLocation;
	DWORD  * pdwPCMemBase = g_pu32RamBase + (dwPC>>2);
	DWORD dwOriginalOp = *pdwPCMemBase;

	// Check not alreay patched...
	if (op(dwOriginalOp) == OP_PATCH)
	{
		// Already patched
		//DBGConsole_Msg(0, "[M%s is already patched]", g_PatchSymbols[i]->szName);
	}
	else if (op(dwOriginalOp) == OP_SRHACK_UNOPT ||
		op(dwOriginalOp) == OP_SRHACK_OPT ||
		op(dwOriginalOp) == OP_SRHACK_NOOPT)
	{
		// Already patched
		DBGConsole_Msg(0, "[W%s] is being repatched with HLE code", g_PatchSymbols[i]->szName);

		g_PatchSymbols[i]->dwReplacedOp = GetCorrectOp(dwOriginalOp);
		*pdwPCMemBase = make_op(OP_PATCH) | i;	// Write patch
	}
	else
	{
		// Not been patched before
		g_PatchSymbols[i]->dwReplacedOp = dwOriginalOp;
		*pdwPCMemBase = make_op(OP_PATCH) | i;	// Write patch
	}
}

// Return the location of a symbol
DWORD Patch_GetSymbolAddress(LPCSTR szName)
{

	DWORD p;

	// Search new list
	for (p = 0; p < ARRAYSIZE(g_PatchSymbols); p++)
	{
		// Skip symbol if already found, or if it is a variable
		if (!g_PatchSymbols[p]->bFound)
			continue;

		if (lstrcmpi(g_PatchSymbols[p]->szName, szName) == 0)
			return PHYS_TO_K0(g_PatchSymbols[p]->dwLocation);

	}

	// The patch was not found
	return ~0;

}

// Given a location, this function returns the name of the matching
// symbol (if there is one)
LPCSTR Patch_GetJumpAddressName(DWORD dwJumpTarget)
{
	DWORD p;
	DWORD * pdwOpBase;
	DWORD * pdwPatchBase;

	if (InternalReadAddress(dwJumpTarget, (void **)&pdwOpBase) == MEM_UNUSED)
		return "??";

	// Search new list
	for (p = 0; p < ARRAYSIZE(g_PatchSymbols); p++)
	{
		// Skip symbol if already found, or if it is a variable
		if (!g_PatchSymbols[p]->bFound)
			continue;

		pdwPatchBase = g_pu32RamBase + (g_PatchSymbols[p]->dwLocation>>2);


		// Symbol not found, attempt to locate on this pass. This may
		// fail if all dependent symbols are not found
		if (pdwPatchBase == pdwOpBase)
		{
			return g_PatchSymbols[p]->szName;
		}

	}

	// The patch was not found
	return "?";
}


void Patch_DumpOsThreadInfo()
{
	DWORD dwThread;
	DWORD dwCurrentThread;
	
	DWORD dwPri;
	DWORD dwQueue;
	WORD wState;
	WORD wFlags;
	DWORD dwID;
	DWORD dwFP;

	DWORD dwFirstThread;
	
	dwCurrentThread = Read32Bits(VAR_ADDRESS(osActiveThread));

	dwFirstThread = Read32Bits(VAR_ADDRESS(osGlobalThreadList));

	dwThread = dwFirstThread;
	DBGConsole_Msg(0, "");
	  DBGConsole_Msg(0, "Threads:      Pri   Queue       State   Flags   ID          FP Used");
	//DBGConsole_Msg(0, "  0x01234567, xxxx, 0x01234567, 0x0123, 0x0123, 0x01234567, 0x01234567",
	while (dwThread)
	{
		dwPri = Read32Bits(dwThread + offsetof(OSThread, priority));
		dwQueue = Read32Bits(dwThread + offsetof(OSThread, queue));
		wState = Read16Bits(dwThread + offsetof(OSThread, state));
		wFlags = Read16Bits(dwThread + offsetof(OSThread, flags));
		dwID = Read32Bits(dwThread + offsetof(OSThread, id));
		dwFP = Read32Bits(dwThread + offsetof(OSThread, fp));

		// Hack to avoid null thread
		if (dwPri == 0xFFFFFFFF)
			break;

		if (dwThread == dwCurrentThread)
		{
			DBGConsole_Msg(0, "->0x%08x, % 4d, 0x%08x, 0x%04x, 0x%04x, 0x%08x, 0x%08x",
				dwThread, dwPri, dwQueue, wState, wFlags, dwID, dwFP);
		}
		else
		{
			DBGConsole_Msg(0, "  0x%08x, % 4d, 0x%08x, 0x%04x, 0x%04x, 0x%08x, 0x%08x",
				dwThread, dwPri, dwQueue, wState, wFlags, dwID, dwFP);
		}
		dwThread = Read32Bits(dwThread + offsetof(OSThread, tlnext));

		if (dwThread == dwFirstThread)
			break;
	}


	/*for (LONG i = 0; i < g_ThreadTimes.size(); i++)
	{
		DBGConsole_Msg(0, "0x%08x: 0x%08x", g_ThreadTimes[i].dwThread, g_ThreadTimes[i].dwTime);
	}*/

}

void Patch_DumpOsEventInfo()
{
	DWORD i;
	DWORD dwQueue;
	DWORD dwMsg;

	if (!VAR_FOUND(osEventMesgArray))
	{
		DBGConsole_Msg(0, "osSetEventMesg not patched, event table unknown");
		return;
	}

	DBGConsole_Msg(0, "");
	DBGConsole_Msg(0, "Events:                      Queue      Message");
	//DBGConsole_Msg(0, "  xxxxxxxxxxxxxxxxxxxxxxxxxx 0x01234567 0x01234567",..);
	for (i = 0; i <	23; i++)
	{
		dwQueue = Read32Bits(VAR_ADDRESS(osEventMesgArray) + (i * 8) + 0x0);
		dwMsg   = Read32Bits(VAR_ADDRESS(osEventMesgArray) + (i * 8) + 0x4);

		
		DBGConsole_Msg(0, "  %-26s 0x%08x 0x%08x",
			g_szEventStrings[i], dwQueue, dwMsg);
	}
}






void Patch_RecurseAndFind()
{
	LONG i;
	LONG j;
	LONG nFound;
	DWORD dwFirst;
	DWORD dwLast;

	DBGConsole_Msg(0, "Searching for os functions. This may take several seconds...");
	DBGConsole_Msg(0, "");

	// Keep looping until a pass does not resolve any more symbols
	nFound = 0;

	// Loops through all symbols, until name is null
	for (i = 0; i < ARRAYSIZE(g_PatchSymbols); i++)
	{

	
		TCHAR szMsg[300];
		wsprintf(szMsg, "OS HLE: %d / %d Looking for %s", i, ARRAYSIZE(g_PatchSymbols), g_PatchSymbols[i]->szName);

		DBGConsole_MsgOverwrite(0, "OS HLE: %d / %d Looking for [G%s]",
			i, ARRAYSIZE(g_PatchSymbols), g_PatchSymbols[i]->szName);


		// Skip symbol if already found, or if it is a variable
		if (g_PatchSymbols[i]->bFound)
			continue;

		// Symbol not found, attempt to locate on this pass. This may
		// fail if all dependent symbols are not found
		if (Patch_LocateFunction(g_PatchSymbols[i]))
			nFound++;
	}
	DBGConsole_MsgOverwrite(0, "OS HLE: %d / %d All done",
		ARRAYSIZE(g_PatchSymbols), ARRAYSIZE(g_PatchSymbols));

	dwFirst = ~0;
	dwLast = 0;

	nFound = 0;
	for (i = 0; i < ARRAYSIZE(g_PatchSymbols); i++)
	{
		if (!g_PatchSymbols[i]->bFound)
		{
			DBGConsole_Msg(0, "[W%s] not found", g_PatchSymbols[i]->szName);			
		}
		else
		{
			BOOL bFoundDuplicate;

			// Find duplicates! (to avoid showing the same clash twice, only scan up to the first symbol)
			bFoundDuplicate = FALSE;
			for (j = 0; j < i; j++)
			{
				if (g_PatchSymbols[i]->bFound &&
					g_PatchSymbols[j]->bFound &&
					(g_PatchSymbols[i]->dwLocation ==
					g_PatchSymbols[j]->dwLocation))
				{
						DBGConsole_Msg(0, "Warning [C%s==%s]",
							g_PatchSymbols[i]->szName,
							g_PatchSymbols[j]->szName);

					// Don't patch!
					g_PatchSymbols[i]->bFound = FALSE;
					g_PatchSymbols[j]->bFound = FALSE;
					bFoundDuplicate = TRUE;
					break;
				}
			}

			if (!bFoundDuplicate)
			{
				DWORD dwLocation = g_PatchSymbols[i]->dwLocation;
				if (dwLocation < dwFirst) dwFirst = dwLocation;
				if (dwLocation > dwLast)  dwLast = dwLocation;


				// Actually patch:
				Patch_ApplyPatch(i);

				nFound++;

			}
		}
	}

	DBGConsole_Msg(0, "%d/%d symbols identified, in range 0x%08x -> 0x%08x",
		nFound, ARRAYSIZE(g_PatchSymbols), dwFirst, dwLast);



	TCHAR szMsg[300];
	nFound = 0;
	for (i = 0; i < ARRAYSIZE(g_PatchVariables); i++)
	{
		if (!g_PatchVariables[i]->bFound)
		{
			DBGConsole_Msg(0, "[W%s] not found", g_PatchVariables[i]->szName);			
		}
		else
		{

			// Find duplicates! (to avoid showing the same clash twice, only scan up to the first symbol)
			for (j = 0; j < i; j++)
			{
				if (g_PatchVariables[i]->bFound &&
					g_PatchVariables[j]->bFound &&
					(g_PatchVariables[i]->dwLocation ==
					g_PatchVariables[j]->dwLocation))
				{
						DBGConsole_Msg(0, "Warning [C%s==%s]",
							g_PatchVariables[i]->szName,
							g_PatchVariables[j]->szName);
				}
			}

			nFound++;
		}
	}
	DBGConsole_Msg(0, "%d/%d variables identified", nFound, ARRAYSIZE(g_PatchVariables));

}


// Attempt to locate this symbol. 
BOOL Patch_LocateFunction(PatchSymbol * ps)
{
	DWORD i;
	DWORD s;
	BOOL bFound;
	DWORD dwOp;
	DWORD * pdwCodeBase;

	pdwCodeBase = g_pu32RamBase;


	for (s = 0; s < ps->pSignatures[s].nNumOps; s++)
	{
		PatchSignature * psig;
		psig = &ps->pSignatures[s];

		// Sweep through OS range
		for (i = 0; i < (g_dwRamSize>>2); i++)
		{
			dwOp = op(GetCorrectOp(pdwCodeBase[i]));

			// First op must match!
			if (psig->firstop != dwOp)
				continue;

			// See if function i exists at this location
			bFound = Patch_VerifyLocation_CheckSignature(ps, psig, i);
			if (bFound)
				return TRUE;

		}


	}

	return FALSE;	

}


#define JumpTarget(dwOp, dwAddr) (((dwAddr) & 0xF0000000) | (target((dwOp))<<2))


// Check that the function i is located at address dwIndex 
BOOL Patch_VerifyLocation(PatchSymbol * ps, DWORD dwIndex)
{
	DWORD s;
	BOOL bFound;

	// We may have already located this symbol. 
	if (ps->bFound)
	{	
		// The location must match!
		return (ps->dwLocation == (dwIndex<<2));
	}

	// Fail if index is outside of indexable memory
	if (dwIndex > g_dwRamSize>>2)
		return FALSE;


	for (s = 0; s < ps->pSignatures[s].nNumOps; s++)
	{
		bFound = Patch_VerifyLocation_CheckSignature(ps, &ps->pSignatures[s], dwIndex);
		if (bFound)
			return TRUE;
	}

	// Not found!
	return FALSE;
}


BOOL Patch_VerifyLocation_CheckSignature(PatchSymbol * ps,
										 PatchSignature * psig,
										 DWORD dwIndex)
{
	DWORD m;
	DWORD dwOp;
	PatchCrossRef * pcr = psig->pCrossRefs;
	BOOL bCrossRefVarSet = FALSE;
	u32 crc;
	u32 partial_crc;

	DWORD * pdwCodeBase;

	pdwCodeBase = g_pu32RamBase;

	PatchCrossRef dummy_cr = {~0, PX_JUMP, NULL };

	if (pcr == NULL)
		pcr = &dummy_cr;



	DWORD dwLastOffset = pcr->dwOffset;

	crc = 0;
	partial_crc = 0;
	for (m = 0; m < psig->nNumOps; m++)
	{
		// Get the actual opcode at this address, not patched/compiled code
		dwOp = GetCorrectOp(pdwCodeBase[dwIndex+m]);
		// This should be ok - so long as we patch all functions at once.

		// Check if a cross reference is in effect here
		if (pcr->dwOffset == m)
		{
			// This is a cross reference. 
			switch (pcr->nType)
			{
			case PX_JUMP:
				{
					BOOL bJumpFound;
					DWORD dwTargetIndex = JumpTarget(dwOp, (dwIndex+m)<<2)>>2;

					// If the opcode at this address is not a Jump/Jal then
					// this can't match
					if (op(dwOp) != OP_JAL && op(dwOp) != OP_J)
						goto fail_find;

					// This is a jump, the jump target must match the 
					// symbol pointed to by this function. Recurse
					bJumpFound = Patch_VerifyLocation(pcr->pSymbol, dwTargetIndex);
					if (!bJumpFound)
						goto fail_find;

					dwOp &= ~0x03ffffff;		// Mask out jump location
				}
				break;
			case PX_VARIABLE_HI:
				{
					// The data element should be consistant with the symbol
					if (pcr->pVariable->bFoundHi)
					{
						if (pcr->pVariable->wHiPart != (dwOp & 0xFFFF))
							goto fail_find;
	
					}
					else
					{
						// Assume this is the correct symbol
						pcr->pVariable->bFoundHi = TRUE;
						pcr->pVariable->wHiPart = (WORD)(dwOp & 0xFFFF);

						bCrossRefVarSet = TRUE;

						// If other half has been identified, set the location
						if (pcr->pVariable->bFoundLo)
							pcr->pVariable->dwLocation = (pcr->pVariable->wHiPart<<16) + (short)(pcr->pVariable->wLoPart);
					}

					dwOp &= ~0x0000ffff;		// Mask out low halfword
				}
				break;
			case PX_VARIABLE_LO:
				{
					// The data element should be consistant with the symbol
					if (pcr->pVariable->bFoundLo)
					{
						if (pcr->pVariable->wLoPart != (dwOp & 0xFFFF))
							goto fail_find;
	
					}
					else
					{
						// Assume this is the correct symbol
						pcr->pVariable->bFoundLo = TRUE;
						pcr->pVariable->wLoPart = (WORD)(dwOp & 0xFFFF);

						bCrossRefVarSet = TRUE;

						// If other half has been identified, set the location
						if (pcr->pVariable->bFoundHi)
							pcr->pVariable->dwLocation = (pcr->pVariable->wHiPart<<16) + (short)(pcr->pVariable->wLoPart);
					}

					dwOp &= ~0x0000ffff;		// Mask out low halfword
				}
				break;
			}
			
			// We've handled this cross ref - point to the next one
			// ready for the next match.
			pcr++;

			// If pcr->dwOffset == ~0, then there are no more in the array
			// This is okay, as the comparison with m above will never match
			if (pcr->dwOffset < dwLastOffset)
			{
				DBGConsole_Msg(0, "%s: CrossReference offsets out of order", ps->szName);
			}
			dwLastOffset = pcr->dwOffset;

		}
		else
		{
			if (op(dwOp) == OP_J)
			{
				dwOp &= ~0x03ffffff;		// Mask out jump location				
			}
		}

		// If this is currently less than 4 ops in, add to the partial crc
		if (m < PATCH_PARTIAL_CRC_LEN)
			partial_crc = daedalus_crc32(partial_crc, (u8*)&dwOp, 4);

		// Here, check the partial crc if m == 3
		if (m == (PATCH_PARTIAL_CRC_LEN-1))
		{
			if (partial_crc != psig->partial_crc)
			{
				goto fail_find;
			}
		}

		// Add to the total crc
		crc = daedalus_crc32(crc, (u8*)&dwOp, 4);


	}

	// Check if the complete crc matches!
	if (crc != psig->crc)
	{
		goto fail_find;
	}


	// We have located the symbol
	ps->bFound = TRUE;
	ps->dwLocation = dwIndex<<2;
	ps->pFunction = psig->pFunction;		// Install this function

	if (bCrossRefVarSet)
	{
		// Loop through symbols, setting variables if both high/low found
		for (pcr = psig->pCrossRefs; pcr->dwOffset != ~0; pcr++)
		{
			if (pcr->nType == PX_VARIABLE_HI ||
				pcr->nType == PX_VARIABLE_LO)
			{
				if (pcr->pVariable->bFoundLo && pcr->pVariable->bFoundHi)
				{
					pcr->pVariable->bFound = TRUE;
				}
			}
		}

	}

	return TRUE;

// Goto - ugh
fail_find:

	// Loop through symbols, clearing variables if they have been set
	if (bCrossRefVarSet)
	{
		for (pcr = psig->pCrossRefs; pcr->dwOffset != ~0; pcr++)
		{
			if (pcr->nType == PX_VARIABLE_HI ||
				pcr->nType == PX_VARIABLE_LO)
			{
				if (!pcr->pVariable->bFound)
				{
					pcr->pVariable->bFoundLo = FALSE;
					pcr->pVariable->bFoundHi = FALSE;
				}
			}
		}
	}

	return FALSE;

}




/////////////////////////
/////////////////////////


inline DWORD VirtToPhys(DWORD dwVAddr)
{
	if (IS_KSEG0(dwVAddr))
	{
		return K0_TO_PHYS(dwVAddr);
	}
	else if (IS_KSEG1(dwVAddr))
	{
		return K1_TO_PHYS(dwVAddr);
	}
	else
	{
		//Patch_osProbeTLB();			// a0 holds correct address
		DBGConsole_Msg(0, "VirtToPhys failed!");
		return 0;
	}
}


#include "patch_thread_hle.inl"
#include "patch_cache_hle.inl"
#include "patch_ai_hle.inl"
#include "patch_eeprom_hle.inl"
#include "patch_gu_hle.inl"
#include "patch_math_hle.inl"
#include "patch_mesg_hle.inl"
#include "patch_pi_hle.inl"
#include "patch_regs_hle.inl"
#include "patch_si_hle.inl"
#include "patch_sp_hle.inl"
#include "patch_timer_hle.inl"
#include "patch_tlb_hle.inl"
#include "patch_util_hle.inl"
#include "patch_vi_hle.inl"





/////////////////////////////////////////////////////




DWORD Patch_osContInit()
{
TEST_DISABLE_FUNCS
	//s32		osContInit(OSMesgQueue * mq, u8 *, OSContStatus * cs);
	DWORD dwMQ       = (u32)g_qwGPR[REG_a0];
	DWORD dwAttached = (u32)g_qwGPR[REG_a1];
	DWORD dwCS       = (u32)g_qwGPR[REG_a2];

	g_dwControllerMQ = dwMQ;

	DBGConsole_Msg(0, "osContInit(0x%08x, 0x%08x, 0x%08x), ra = 0x%08x",
		dwMQ, dwAttached, dwCS, (u32)g_qwGPR[REG_ra]);

	return PATCH_RET_NOT_PROCESSED;
}



DWORD Patch___osContAddressCrc()
{
TEST_DISABLE_FUNCS
	DWORD dwAddress = (u32)g_qwGPR[REG_a0];

	DBGConsole_Msg(0, "__osContAddressCrc(0x%08x)", dwAddress);
	return PATCH_RET_NOT_PROCESSED;
}


DWORD Patch_osSpTaskYield_Mario()
{
	return PATCH_RET_NOT_PROCESSED;
}
DWORD Patch_osSpTaskYield_Rugrats()
{
	return PATCH_RET_NOT_PROCESSED;
}



DWORD Patch_osSpTaskYielded()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch___osSpTaskLoadInitTask()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch___osSpRawReadIo_Mario()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch___osSpRawReadIo_Zelda()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch___osSpRawWriteIo_Mario()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch___osSpRawWriteIo_Zelda()
{
	return PATCH_RET_NOT_PROCESSED;
}


DWORD Patch___osSiDeviceBusy()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch___osSiRawStartDma_Mario()
{
	return PATCH_RET_NOT_PROCESSED;
}
DWORD Patch___osSiRawStartDma_Rugrats()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch_osMapTLBRdb()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch_osUnmapTLBAll_Mario()
{
	return PATCH_RET_NOT_PROCESSED;
}
// Identical to mario, different loop structure
DWORD Patch_osUnmapTLBAll_Rugrats()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch_osViSetEvent()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch___osAiDeviceBusy()
{
	return PATCH_RET_NOT_PROCESSED;
}


DWORD Patch_A__ull_rem()
{
	return PATCH_RET_NOT_PROCESSED;
}





DWORD Patch___ull_rem()
{
	return PATCH_RET_NOT_PROCESSED;
}



DWORD Patch___ull_divremi()
{
	return PATCH_RET_NOT_PROCESSED;
}


DWORD Patch___lldiv()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch___ldiv()
{
	return PATCH_RET_NOT_PROCESSED;
}


DWORD Patch___osPackRequestData()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch___osContGetInitData()
{
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch_osSetTimer()
{
	return PATCH_RET_NOT_PROCESSED;
}
DWORD Patch_osSetIntMask()
{
	return PATCH_RET_NOT_PROCESSED;
}
DWORD Patch___osEepromRead_Prepare()
{
	return PATCH_RET_NOT_PROCESSED;
}
DWORD Patch___osEepromWrite_Prepare()
{
	return PATCH_RET_NOT_PROCESSED;
}

#include "patch_symbols.inl"

