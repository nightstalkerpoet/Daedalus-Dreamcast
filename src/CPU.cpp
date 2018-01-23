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


// Stuff to handle Processor
#include "main.h"
#include "ultra_R4300.h"

#include "resource.h"
#include "Registers.h"			// For REG_?? defines
#include "debug.h"
#include "CPU.h"
#include "RSP.h"
#include "RDP.h"
#include "Memory.h"
#include "OpCode.h"
#include "Interrupt.h"
#include "OS.h"
#include "SR.h"
#include "Patch.h"			// GetCorrectOp

#include "R4300.h"
#include "R4300_Regs.h"

#include "DBGConsole.h"
#include "ultra_r4300.h"
#include <cstdlib>

//#define VID_CLOCK	777809		// From LaC
#define VID_CLOCK	625000
extern "C" int usleep( DWORD c );

std::vector< DBG_BreakPoint > g_BreakPoints;

DWORD	g_dwCPUStuffToDo		= 0;
BOOL	g_bCPURunning			= FALSE;
DWORD	g_dwVIs					= 0;

static DWORD g_dwCurrentVITimeSample = 0;
#define MAX_VI_TIME_SAMPLES	3
static float g_fVITimeSamples[MAX_VI_TIME_SAMPLES];
float g_fCurrentRate;

static void CPU_RemoveHeadEvent();
CPUEvent * g_pFirstCPUEvent = NULL;
static CPUEvent * g_pCPUEventPool = NULL;





static HANDLE g_hCPUThread				= NULL;
static HANDLE g_hAudioThread			= NULL;

static DWORD CPUThreadFunc(LPVOID * lpVoid);
static DWORD AudioThreadFunc(LPVOID * lpVoid);
static void CPU_GoWithRSP_Dynarec(void);
static void CPU_GoWithRSP_NoDynarec(void);
static void CPU_GoWithNoRSP_Dynarec(void);
static void CPU_GoWithNoRSP_NoDynarec(void);

static void (* g_pCPUCore)() = CPU_GoWithNoRSP_Dynarec;
typedef void (*RSPStepInstruction)();

RSPStepInstruction				g_pRSPStepInstr = RSPStepHalted;
RSPStepInstruction g_RSPStepMatrix[2][2] =
{
	{ RDPStep, RSPStep },
	{ RDPStepHalted, RSPStepHalted }
};

// TODO - On CPU_Fini, delete linked lists of events!

typedef struct
{
	DWORD dwAddr;
	DWORD count;
} PCCount;
std::vector<PCCount> g_PCCount;

static int ComparePCCount(const void * p1, const void * p2)
{
	PCCount * id1 = (PCCount *)p1;
	PCCount * id2 = (PCCount *)p2;

	if (id1->count < id2->count)
		return 1;
	else if (id1->count == id2->count)
		return 0;
	else
		return -1;
}

static void SortPCCount()
{
	// Sort the items
	qsort(&g_PCCount[0], g_PCCount.size(), sizeof(g_PCCount[0]), ComparePCCount);	
}


void CPU_Reset(DWORD dwNewPC)
{
	LONG i;
	DWORD dwReg;
	DWORD dwCPU;
	DBGConsole_Msg(0, "Resetting with PC of 0x%08x", dwNewPC);

	// Initialise the array - assume perfect timekeeping
	for (i = 0; i < MAX_VI_TIME_SAMPLES; i++)
	{
		g_fVITimeSamples[i] = DESIRED_VI_INTERVAL;
	}
	// Point to the first sample
	g_fCurrentRate = 0.0f;
	g_dwCurrentVITimeSample = 0;


	if (g_bUseDynarec)
		g_pCPUCore = CPU_GoWithNoRSP_Dynarec;
	else
		g_pCPUCore = CPU_GoWithNoRSP_NoDynarec;
	CPU_SetPC(dwNewPC);
	g_qwMultHI = 0;
	g_qwMultLO = 0;
	for (dwReg = 0; dwReg < 32; dwReg++)
	{
		g_qwGPR[dwReg] = 0;
		for (dwCPU = 0; dwCPU < 3; dwCPU++)
			g_qwCPR[dwCPU][dwReg] = 0;
	}
	
	// Init TLBs:
	for (i = 0; i < 32; i++)
	{	
		g_TLBs[i].pfno = 0;	// Clear Valid bit;
		g_TLBs[i].pfne = 0;	// Clear Valid bit;

		g_TLBs[i].pagemask = 0;

		g_TLBs[i].g		   = 1;

		g_TLBs[i].mask     =  g_TLBs[i].pagemask | 0x00001FFF;
		g_TLBs[i].mask2    =  g_TLBs[i].mask>>1;
		g_TLBs[i].vpnmask  = ~g_TLBs[i].mask;
		g_TLBs[i].vpn2mask =  g_TLBs[i].vpnmask>>1;

		g_TLBs[i].pfnehi = ((g_TLBs[i].pfne<<TLBLO_PFNSHIFT) & g_TLBs[i].vpn2mask);
		g_TLBs[i].pfnohi = ((g_TLBs[i].pfno<<TLBLO_PFNSHIFT) & g_TLBs[i].vpn2mask);
		
		g_TLBs[i].addrcheck = 0;
		g_TLBs[i].lastaccessed = 0;
	}


	// From R4300 manual			
    g_qwCPR[0][C0_RAND]		= 32-1;			// TLBENTRIES-1
    //g_qwCPR[0][C0_SR]		= 0x70400004;	//*SR_FR |*/ SR_ERL | SR_CU2|SR_CU1|SR_CU0;
	R4300_SetSR(0x70400004);
    g_qwCPR[0][C0_PRID]		= 0x00000b00;
	g_qwCPR[0][C0_CONFIG]	= 0x0006E463;	// 0x00066463;	
	g_qwCPR[0][C0_WIRED]   = 0x0;	

	g_qwCCR[1][0] = 0x00000511;

	// Look for Game boot address jump (so we know when to patch)
	// This op re-patches itself to R4300_Special_JR when 
	// the jump is detected
	R4300SpecialInstruction[OP_JR] = R4300_Special_JR_CheckBootAddress;

	g_nDelay = NO_DELAY;
	g_dwCPUStuffToDo = 0;
	g_dwVIs = 0;

	// Clear event list:
	while (g_pFirstCPUEvent != NULL)
	{
		CPUEvent * pVictim = g_pFirstCPUEvent;

		// Point to next event
		g_pFirstCPUEvent = pVictim->pNext;

		// Add to pool:
		pVictim->pNext = g_pCPUEventPool;
		g_pCPUEventPool = pVictim;
	}
	CPU_AddEvent(VID_CLOCK, CPU_EVENT_VBL);

	g_BreakPoints.clear();

	g_PCCount.clear();
}


///////////////////////////////
// Thread stuff

BOOL StartCPUThread(LPSTR szReason, LONG nLen)
{
	DWORD dwID;
	g_bCPURunning = true;
		while (g_bCPURunning)
		{
			// This function is either CPU_GoWithRSP or CPU_GoWithNoRSP
			// depending on whether the RSP is halted or not
			g_pCPUCore();
		}

	// If the thread is already running, just return
/*	if (g_hCPUThread)
		return TRUE;

	g_PCCount.clear();
	// Attempt to create the thread
	g_bCPURunning = TRUE;

    /*KOS
	g_hAudioThread = CreateThread(NULL, 0,
		(LPTHREAD_START_ROUTINE)AudioThreadFunc, NULL,
		CREATE_SUSPENDED,
		&dwID);

	ResumeThread(g_hAudioThread);

	g_hCPUThread = CreateThread(NULL, 0,
		(LPTHREAD_START_ROUTINE)CPUThreadFunc, NULL,
		CREATE_SUSPENDED,
		&dwID);
	ResumeThread(g_hCPUThread);
	*/
}

void
StopCPUThread(void)
{
	// If it's no running, just return silently
	if (g_hCPUThread == NULL)
		return;

	// If it is running, we need to signal for it to stop
	AddCPUJob(CPU_STOP_RUNNING);

	// Wait forever for it to finish. It will clear/close g_hCPUThread when it exits
	while (g_hCPUThread != NULL)
	{
		DBGConsole_Msg(0, "Waiting for CPU thread (0x%08x) to finish", g_hCPUThread);
		/*KOS
		WaitForSingleObject(g_hCPUThread, 1000); //INFINITE); 
        */
	}

	//PostMessage(g_hMainWindow, MWM_ENDEMU, 0,0);

	DBGConsole_Msg(0, "CPU Thread finished");


	SortPCCount();
	for (LONG i = 0; i < 20 && i < g_PCCount.size(); i++)
	{
		DBGConsole_Msg(0, "hits at 0x%08x were %d",
		g_PCCount[i].dwAddr, g_PCCount[i].count);
	}
	
}

DWORD AudioThreadFunc(LPVOID *lpVoid)
{
    /*KOS
	BOOL bResult; 
	CAudioPlugin * pAiPlugin;

	DBGConsole_Msg(0, "Audio thread started");

	// Force reload on first loop!
	g_bReloadAiPlugin = TRUE;

	pAiPlugin = NULL;

	// Audio plugin may die on us - in which case the wrapper will unload the plugin
	while (g_bCPURunning)
	{
		if (g_bReloadAiPlugin)
		{
			pAiPlugin = g_pAiPlugin;

			// Close the existing plugin if it's already opened
			if (pAiPlugin != NULL)
			{
				g_pAiPlugin = NULL;
				pAiPlugin->CloseDll();
				pAiPlugin->Close();
				delete pAiPlugin;
				pAiPlugin = NULL;
			}
			
			// Only try to init if an audio plugin has been specified
			if (lstrlen(g_szAiPluginFileName) > 0)
			{
				DBGConsole_Msg(0, "Initialising Audio Plugin [C%s]", g_szAiPluginFileName);
				pAiPlugin = new CAudioPlugin(g_szAiPluginFileName);

				bResult = FALSE;
				if (pAiPlugin != NULL)
				{
					bResult = pAiPlugin->Open();
					if (bResult)
						bResult = pAiPlugin->Init();

					// Stupid hack to force plugins to create the sound buffer
					pAiPlugin->DacrateChanged(SYSTEM_NTSC);
				}


				if (!bResult)
				{
					DBGConsole_Msg(0, "Error loading audio plugin");
					break;
				}
			}

			g_pAiPlugin = pAiPlugin;
			g_bReloadAiPlugin = FALSE;
		}

		if (g_pAiPlugin != NULL)
		{

			// This does most of the business
			g_pAiPlugin->Update(FALSE);

			// Check if an exception occured?
		}
		Sleep(10);
	}

	DBGConsole_Msg(0, "Audio thread stopping");
	{
			
		// Make a copy of the plugin, and set the global pointer to NULL; 
		// This stops other threads from trying to access the plugin
		// while we're in the process of shutting it down.
		pAiPlugin = g_pAiPlugin;

		if (pAiPlugin != NULL)
		{
			g_pAiPlugin = NULL;
			pAiPlugin->CloseDll();
			pAiPlugin->Close();
			delete pAiPlugin;
			pAiPlugin = NULL;
		}
	}
		
	CloseHandle(g_hAudioThread);
	g_hAudioThread = NULL;
*/

	return 0;
}

DWORD CPUThreadFunc(LPVOID *lpVoid)
{
/*KOS
	CGraphicsPlugin * pGfxPlugin;
	BOOL bResult;


	//g_bReloadGfxPlugin = TRUE;

	{
		pGfxPlugin = g_pGfxPlugin;

		// Close the existing plugin if it's already opened
		if (pGfxPlugin != NULL)
		{
			g_pGfxPlugin = NULL;
			pGfxPlugin->CloseDll();
			pGfxPlugin->Close();
			delete pGfxPlugin;
			pGfxPlugin = NULL;
		}

		if (lstrlen(g_szGfxPluginFileName) > 0)
		{
			DBGConsole_Msg(0, "Initialising Graphics Plugin [C%s]", g_szGfxPluginFileName);
			pGfxPlugin = new CGraphicsPlugin(g_szGfxPluginFileName);

			bResult = FALSE;
			if (pGfxPlugin != NULL)
			{
				bResult = pGfxPlugin->Open();
				if (bResult)
					bResult = pGfxPlugin->Init();

				if (bResult)
					pGfxPlugin->RomOpen();
			}

			if (!bResult)
			{
				DBGConsole_Msg(0, "Error loading graphics plugin");
				pGfxPlugin->Close();
				pGfxPlugin = NULL;
			}
		}

		g_pGfxPlugin = pGfxPlugin;
	}


	// From Lkb- avoid GFX exceptions when the RDP is configured for the ROM ucode
	//PostMessage(g_hMainWindow, MWM_STARTEMU, 0,0);
	SendMessage(g_hMainWindow, MWM_STARTEMU, 0,0);


	try
	{
		while (g_bCPURunning)
		{
			// This function is either CPU_GoWithRSP or CPU_GoWithNoRSP
			// depending on whether the RSP is halted or not
			g_pCPUCore();
		}
	}
	catch (...)
	{
		if (g_bTrapExceptions)
		{
			MessageBox(g_hMainWindow, CResourceString(IDS_CPUTHREAD_EXCEPTION),
									  g_szDaedalusName, MB_ICONEXCLAMATION|MB_OK);
			g_bCPURunning = FALSE;
		}
		else
			throw;
	}


	{

		// Make a copy of the plugin, and set the global pointer to NULL;
		// This stops other threads from trying to access the plugin
		// while we're in the process of shutting it down.
		pGfxPlugin = g_pGfxPlugin;

		if (pGfxPlugin != NULL)
		{
			g_pGfxPlugin = NULL;
			pGfxPlugin->CloseDll();
			pGfxPlugin->Close();
			delete pGfxPlugin;
			pGfxPlugin = NULL;
		}
	}


	// Update the screen. It's probably better handle elsewhere...
	DBGConsole_UpdateDisplay();

	// Close handle to our thread and exit
	CloseHandle(g_hCPUThread);
	g_hCPUThread = NULL;

	// This causes a few problems with synchronisation if we call here
	// (it passes a message to the main window, but the main window's
	// handling thread is busy waiting for this thread to exit). I've
	// put the call at the end of StopCPUThread()
	//Main_ActivateList();
	PostMessage(g_hMainWindow, MWM_ENDEMU, 0,0);
*/

	return 0;
}

void CPU_SelectCore()
{
	if (g_bUseDynarec)
	{
		if (!g_bRSPHalted || !g_bRDPHalted)
			g_pCPUCore = CPU_GoWithRSP_Dynarec;
		else
			g_pCPUCore = CPU_GoWithNoRSP_Dynarec;
	}
	else
	{
		if (!g_bRSPHalted || !g_bRDPHalted)
			g_pCPUCore = CPU_GoWithRSP_NoDynarec;
		else
			g_pCPUCore = CPU_GoWithNoRSP_NoDynarec;

	}

	g_pRSPStepInstr = g_RSPStepMatrix[g_bRSPHalted ? 1:0][g_bRDPHalted ? 1:0]; 

	AddCPUJob(CPU_CHANGE_CORE);
}

void
CPU_Halt(LPCTSTR szReason)
{
	DBGConsole_Msg(0, "CPU Halting: %s", szReason);
	AddCPUJob(CPU_STOP_RUNNING);

}

void CPU_AddBreakPoint(DWORD dwAddress)
{
	DWORD * pdwOp;
	DWORD dwBase;

	// Force 4 byte alignment
	dwAddress &= 0xFFFFFFFC;

	dwBase = InternalReadAddress(dwAddress, (void**)&pdwOp);
	if (dwBase == MEM_UNUSED)
	{
		DBGConsole_Msg(0, "Invalid Address for BreakPoint: 0x%08x", dwAddress);
	}
	else
	{
		DBG_BreakPoint bpt;
		DBGConsole_Msg(0, "[YInserting BreakPoint at 0x%08x]", dwAddress);
		
		bpt.dwOriginalOp = *pdwOp;
		bpt.bEnabled = TRUE;
		bpt.bTemporaryDisable = FALSE;
		g_BreakPoints.push_back(bpt);
		*pdwOp = make_op(OP_DBG_BKPT) | (g_BreakPoints.size() - 1);
	}
}

void CPU_EnableBreakPoint(DWORD dwAddress, BOOL bEnabled)
{
	DWORD * pdwOp;
	DWORD dwBase;

	// Force 4 byte alignment
	dwAddress &= 0xFFFFFFFC;

	dwBase = InternalReadAddress(dwAddress, (void**)&pdwOp);
	if (dwBase == MEM_UNUSED)
	{
		DBGConsole_Msg(0, "Invalid Address for BreakPoint: 0x%08x", dwAddress);
	}
	else
	{
		DWORD dwOp;
		DWORD dwBreakPoint;

		dwOp = *pdwOp;

		if (op(dwOp) != OP_DBG_BKPT)
		{
			DBGConsole_Msg(0, "[YNo breakpoint is set at 0x%08x]", dwAddress);
			return;
		}

		// Entry is in lower 26 bits...
		dwBreakPoint = dwOp & 0x03FFFFFF;

		if (dwBreakPoint < g_BreakPoints.size())
		{
			g_BreakPoints[dwBreakPoint].bEnabled = bEnabled;
			// Alwyas disable
			g_BreakPoints[dwBreakPoint].bTemporaryDisable = FALSE;
		}
	}
}


static void CPU_HANDLE_COUNT_INTERRUPT()
{
	CPUEvent * pEvent = g_pFirstCPUEvent;
	if (pEvent == NULL)
		return;

	// Remove this event from the list, and place in the pool. 
	// The pointer is still valid afterwards
	CPU_RemoveHeadEvent();
	

	switch (pEvent->dwEventType)
	{
	case CPU_EVENT_VBL:
		{
            // Add another Interrupt at the next time:
			CPU_AddEvent(VID_CLOCK, CPU_EVENT_VBL);

            /*KOS
            LARGE_INTEGER liNow;
			LARGE_INTEGER liElapsed;
			float fElapsed; 
			float fSleepTime;
			float fSumOfVITimeSamples;

			// Add another Interrupt at the next time:
			CPU_AddEvent(VID_CLOCK, CPU_EVENT_VBL);

			g_dwVIs++;
            /*KOS
			{
				float fCurrentRate;
				DWORD dwSleepTime;

				// We're aiming for 60 vis/sec
				// This is a bit of a hack - busy wait until we're exactly on schedule
				do {

					// Check how long since last vi
					QueryPerformanceCounter(&liNow);

					liElapsed.QuadPart = liNow.QuadPart - g_liLastVITime.QuadPart;
					fElapsed = (float)liElapsed.QuadPart / (float)g_liFreq.QuadPart;

					// We could do a "rolling" sum (subtract the item to be replaced, add the
					// new values) but this will slowy degrade through fp inaccuracy
					g_fVITimeSamples[g_dwCurrentVITimeSample] = fElapsed;

					// Update the running total - subtract the value we removed, and add the new value
					fSumOfVITimeSamples = 0.0f;
					for (int i = 0; i < MAX_VI_TIME_SAMPLES; i++)
						fSumOfVITimeSamples += g_fVITimeSamples[i];
					fCurrentRate = fSumOfVITimeSamples / (float)MAX_VI_TIME_SAMPLES;

					// If we need to wait over 1 millisec, use Sleep()?
					fSleepTime = DESIRED_VI_INTERVAL - fCurrentRate;
					if (fSleepTime > 0)
					{
						dwSleepTime = (DWORD)(fSleepTime * 1000.0f);

						//DBGConsole_Msg(0, "Sleeping for %f -> %d", fSleepTime, (DWORD)(fSleepTime * 1000));

					//	if (dwSleepTime > 0)
					//		Sleep(dwSleepTime);
					}

				}
				while (g_bSpeedSync && fCurrentRate < DESIRED_VI_INTERVAL);

				g_fCurrentRate = fCurrentRate;

				g_liLastVITime = liNow;
				// Point to next sample, and loop round if necessary
				g_dwCurrentVITimeSample = (g_dwCurrentVITimeSample + 1) % MAX_VI_TIME_SAMPLES;
			}*/


			Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_VI);
			//Don't just set flag - actually do interrupt
			//AddCPUJob(CPU_CHECK_INTERRUPTS);
			R4300_Interrupt_MI();

            /*KOS
			if (g_pGfxPlugin != NULL)
			{
				g_pGfxPlugin->UpdateScreen();
   }*/
		}
		break;
	case CPU_EVENT_COMPARE:
		{
			//DBGConsole_Msg(0, "Compare Interrupt 0x%08x", (u32)g_qwCPR[0][C0_COMPARE]);
			//CPU_Halt("Compare");
			R4300_Interrupt_Compare();
		}
		break;
	default:
		{
			DBGConsole_Msg(0, "Unknown timer interrupt");
			if (g_pFirstCPUEvent == NULL)
			{
				// Ensure we always have an event lined up
				CPU_AddEvent(VID_CLOCK, CPU_EVENT_VBL);
			}
			
		}
		break;
	}
}


void CPU_SetCompare(u64 qwNewValue)
{
	g_qwCPR[0][C0_CAUSE] &= ~CAUSE_IP8;

	DPF(DEBUG_COMPARE, "COMPARE set to 0x%08x.", (u32)qwNewValue);
	//DBGConsole_Msg(0, "COMPARE set to 0x%08x Count is 0x%08x.", (u32)qwNewValue, (u32)g_qwCPR[0][C0_COUNT]);

	// Add an event for this compare:
	if (qwNewValue == g_qwCPR[0][C0_COMPARE])
	{
		//	DBGConsole_Msg(0, "Clear");
	}
	else
	{
		if ((u32)qwNewValue > (u32)g_qwCPR[0][C0_COUNT])
		{
			CPU_AddEvent((u32)qwNewValue - (u32)g_qwCPR[0][C0_COUNT], CPU_EVENT_COMPARE);									
		}
		else if (qwNewValue != 0)
		{
			//DBGConsole_Msg(0, "COMPARE set to 0x%08x%08x < Count is 0x%08x%08x.",
			//	(u32)(qwNewValue>>32), (u32)qwNewValue,
			//	(u32)(g_qwCPR[0][C0_COUNT]>>32), (u32)g_qwCPR[0][C0_COUNT]);

			CPU_AddEvent((u32)qwNewValue - (u32)g_qwCPR[0][C0_COUNT], CPU_EVENT_COMPARE);									
			//DBGConsole_Msg(0, "0x%08x", (u32)qwNewValue - (u32)g_qwCPR[0][C0_COUNT]);
		}
		g_qwCPR[0][C0_COMPARE] = qwNewValue;
	}


}


// This is exactly the same as the no dynarec version, but it ensures
// the the instruction to be executed is not a patch / dynarec hack
inline void CPU_EXECUTE_OP_WITH_ENSURED_SINGLE_STEP()
{
	DWORD dwOp;

	dwOp = *(DWORD *)g_pPCMemBase;

	// Handle breakpoints correctly
	if (op(dwOp) == OP_DBG_BKPT)
	{
		// Turn temporary disable on to allow instr to be processed

		DWORD dwBreakPoint;

		// Entry is in lower 26 bits...
		dwBreakPoint = dwOp & 0x03FFFFFF;

		if (dwBreakPoint < g_BreakPoints.size())
		{
			if (g_BreakPoints[dwBreakPoint].bEnabled)
				g_BreakPoints[dwBreakPoint].bTemporaryDisable = TRUE;
		}

	}
	else
	{
		dwOp = GetCorrectOp(dwOp);
	}


	R4300Instruction[op(dwOp)](dwOp);

	// Increment count register
	*(DWORD *)&g_qwCPR[0][C0_COUNT] = *(DWORD *)&g_qwCPR[0][C0_COUNT]+1;

	//g_pFirstCPUEvent->dwCount--;
	if (g_pFirstCPUEvent->dwCount-- == 1)
	{
		AddCPUJob(CPU_CHECK_COUNT_INTERRUPT);
	}

	switch (g_nDelay)
	{
		case DO_DELAY:
			// We've got a delayed instruction to execute. Increment
			// PC as normal, so that subsequent instruction is executed
			INCREMENT_PC();
			g_nDelay = EXEC_DELAY;

			break;
		case EXEC_DELAY:
			// We've just executed the delayed instr. Now carry out jump as stored in g_dwNewPC;
			CPU_SetPC(g_dwNewPC);
			g_nDelay = NO_DELAY;
			break;
		case NO_DELAY:
			// Normal operation - just increment the PC
			INCREMENT_PC();
			break;
	}
	
}


void AddPCCount(DWORD dwPC)
{
	BOOL bFound = FALSE;
	for (LONG i = 0; i < g_PCCount.size(); i++)
	{
		if (g_PCCount[i].dwAddr == dwPC)
		{
			g_PCCount[i].count++;
			bFound = TRUE;
			break;
		}
	}
	if (!bFound)
	{
		PCCount pcc;
		pcc.dwAddr = dwPC;
		pcc.count= 1;
		g_PCCount.push_back(pcc);
	}
	if ((g_PCCount.size() % 10000) == 0)
	{
		// OPtimise slightly
		SortPCCount();
	}
}


DWORD dwLastCount = 0;
inline void CPU_EXECUTE_OP_WITH_NO_DYNAREC()
{
	DWORD dwOp;
   	dwOp = *(DWORD *)g_pPCMemBase;
	R4300Instruction[op(dwOp)](dwOp);
	
	dwLastCount++;

	// Increment count register
	*(DWORD *)&g_qwCPR[0][C0_COUNT] = *(DWORD *)&g_qwCPR[0][C0_COUNT]+1;

	//g_pFirstCPUEvent->dwCount--;
	if (g_pFirstCPUEvent->dwCount-- == 1)
	{
		AddCPUJob(CPU_CHECK_COUNT_INTERRUPT);
	}

	switch (g_nDelay)
	{
		case DO_DELAY:
			// We've got a delayed instruction to execute. Increment
			// PC as normal, so that subsequent instruction is executed
			INCREMENT_PC();
			g_nDelay = EXEC_DELAY;

			break;
		case EXEC_DELAY:
			// We've just executed the delayed instr. Now carry out jump as stored in g_dwNewPC;
			CPU_SetPC(g_dwNewPC);
			g_nDelay = NO_DELAY;

			//AddPCCount(g_dwPC);

			break;
		case NO_DELAY:
			// Normal operation - just increment the PC
			INCREMENT_PC();
			break;
	}

	
}

inline void CPU_EXECUTE_OP_WITH_DYNAREC()
{
	DWORD dwOp;

	dwOp = *(DWORD *)g_pPCMemBase;
	R4300Instruction[op(dwOp)](dwOp);

	// Increment count register
	*(DWORD *)&g_qwCPR[0][C0_COUNT] = *(DWORD *)&g_qwCPR[0][C0_COUNT]+1;

	//g_pFirstCPUEvent->dwCount--;
	if (g_pFirstCPUEvent->dwCount-- == 1)
	{
		AddCPUJob(CPU_CHECK_COUNT_INTERRUPT);
	}

	switch (g_nDelay)
	{
		case DO_DELAY:
			// We've got a delayed instruction to execute. Increment
			// PC as normal, so that subsequent instruction is executed
			INCREMENT_PC();
			g_nDelay = EXEC_DELAY;

			break;
		case EXEC_DELAY:
			// We've just executed the delayed instr. Now carry out jump as stored in g_dwNewPC;
			CPU_SetPC(g_dwNewPC);
			g_nDelay = NO_DELAY;

			// Only attempt to recompile *after* a jump target!
			dwOp = *(DWORD *)g_pPCMemBase;
			if (!OP_IS_A_HACK(dwOp))
				SR_CompileCode(g_dwPC);
			break;
		case NO_DELAY:
			// Normal operation - just increment the PC
			INCREMENT_PC();
			dwOp = *(DWORD *)g_pPCMemBase;
			if (!OP_IS_A_HACK(dwOp))
				SR_CompileCode(g_dwPC);

			break;
	}
	
}



// Hacky function to use when debugging
void CPUSkip(void)
{
	if (g_bCPURunning || g_hCPUThread != NULL)
	{
		DBGConsole_Msg(0, "Already Running");
		return;
	}

	INCREMENT_PC();

}

void CPUStep(void)
{
	if (g_bCPURunning || g_hCPUThread != NULL)
	{
		DBGConsole_Msg(0, "Already Running");
		return;
	}

	// We do this in a slightly different order to ensure that
	// any interrupts are taken care of before we execute an op
	if (g_dwCPUStuffToDo)
	{	
		// Process Interrupts/Exceptions on a priority basis
		// Call most likely first!
		if (g_dwCPUStuffToDo & CPU_JUMP_EVEC)
		{
			R4300_JumpToInterruptVector(E_VEC);
			g_dwCPUStuffToDo &= ~CPU_JUMP_EVEC;

		}
		else
		if (g_dwCPUStuffToDo & CPU_CHECK_INTERRUPTS)
		{
			R4300_Interrupt_MI();
			g_dwCPUStuffToDo &= ~CPU_CHECK_INTERRUPTS;
		}
		else if (g_dwCPUStuffToDo & CPU_CHECK_COUNT_INTERRUPT)
		{
			// Do VBl or COMPARE interrupt
			CPU_HANDLE_COUNT_INTERRUPT();
			g_dwCPUStuffToDo &= ~CPU_CHECK_COUNT_INTERRUPT;
		}
		else if (g_dwCPUStuffToDo & CPU_TLB_EXCEPTION)
		{
			R4300_Exception_TLB();
			g_dwCPUStuffToDo &= ~CPU_TLB_EXCEPTION;
		}
		else if (g_dwCPUStuffToDo & CPU_CHECK_POSTPONED_INTERRUPTS)
		{
			R4300_Interrupt_CheckPostponed();
			g_dwCPUStuffToDo &= ~CPU_CHECK_POSTPONED_INTERRUPTS;
		}
		else if (g_dwCPUStuffToDo & CPU_BREAKPOINT)
		{
			R4300_Exception_Break();
			g_dwCPUStuffToDo &= ~CPU_BREAKPOINT;
		}
		else if (g_dwCPUStuffToDo & CPU_SYSCALL)
		{
			R4300_Exception_Syscall();
			g_dwCPUStuffToDo &= ~CPU_SYSCALL;
		}
		else if (g_dwCPUStuffToDo & CPU_STOP_RUNNING)
		{
			g_dwCPUStuffToDo &= ~CPU_STOP_RUNNING;
			g_bCPURunning = FALSE;
		}
		/* Not necessary
		else if (g_dwCPUStuffToDo & CPU_CHANGE_CORE)
		{
			g_dwCPUStuffToDo &= ~CPU_CHANGE_CORE;
		}*/
		// Clear g_dwCPUStuffToDo?
	}

	CPU_EXECUTE_OP_WITH_ENSURED_SINGLE_STEP();

	g_pRSPStepInstr();

	DBGConsole_UpdateDisplay();
}




// Keep executing instructions until there are other tasks to do (i.e. g_dwCPUStuffToDo is set)
// Process these tasks and loop
void CPU_GoWithRSP_NoDynarec(void)
{
	int test = 0;
	while (g_bCPURunning)
	{
      /*  if( test % 100000 == 0 )
            printf( "cmd %d\n", test );
        test++;*/

		while (g_dwCPUStuffToDo == 0)
		{		
			CPU_EXECUTE_OP_WITH_NO_DYNAREC();


			g_pRSPStepInstr();

		}
		
		if (g_dwCPUStuffToDo & CPU_JUMP_EVEC)
		{
			R4300_JumpToInterruptVector(E_VEC);
			g_dwCPUStuffToDo &= ~CPU_JUMP_EVEC;

		}
		else
		if (g_dwCPUStuffToDo & CPU_CHECK_INTERRUPTS)
		{
			R4300_Interrupt_MI();
			g_dwCPUStuffToDo &= ~CPU_CHECK_INTERRUPTS;
		}
		else if (g_dwCPUStuffToDo & CPU_CHECK_COUNT_INTERRUPT)
		{
			// Do VBl or COMPARE interrupt
			CPU_HANDLE_COUNT_INTERRUPT();
			g_dwCPUStuffToDo &= ~CPU_CHECK_COUNT_INTERRUPT;
		}


		else if (g_dwCPUStuffToDo & CPU_TLB_EXCEPTION)
		{
			R4300_Exception_TLB();
			g_dwCPUStuffToDo &= ~CPU_TLB_EXCEPTION;
		}
		else if (g_dwCPUStuffToDo & CPU_CHECK_POSTPONED_INTERRUPTS)
		{
			R4300_Interrupt_CheckPostponed();
			g_dwCPUStuffToDo &= ~CPU_CHECK_POSTPONED_INTERRUPTS;
		}
		else if (g_dwCPUStuffToDo & CPU_BREAKPOINT)
		{
			R4300_Exception_Break();
			g_dwCPUStuffToDo &= ~CPU_BREAKPOINT;
		}
		else if (g_dwCPUStuffToDo & CPU_SYSCALL)
		{
			R4300_Exception_Syscall();
			g_dwCPUStuffToDo &= ~CPU_SYSCALL;
		}
		else if (g_dwCPUStuffToDo & CPU_STOP_RUNNING)
		{
			g_dwCPUStuffToDo &= ~CPU_STOP_RUNNING;
			g_bCPURunning = FALSE;
		}
		else if (g_dwCPUStuffToDo & CPU_CHANGE_CORE)
		{
			g_dwCPUStuffToDo &= ~CPU_CHANGE_CORE;
			break;
		}

		// Clear g_dwCPUStuffToDo?
	}
}

// Keep executing instructions until there are other tasks to do (i.e. g_dwCPUStuffToDo is set)
// Process these tasks and loop
void CPU_GoWithRSP_Dynarec(void)
{
	while (g_bCPURunning)
	{

		while (g_dwCPUStuffToDo == 0)
		{		
			CPU_EXECUTE_OP_WITH_DYNAREC();

			g_pRSPStepInstr();

		}

		if (g_dwCPUStuffToDo & CPU_JUMP_EVEC)
		{
			R4300_JumpToInterruptVector(E_VEC);
			g_dwCPUStuffToDo &= ~CPU_JUMP_EVEC;

		}
		else
		if (g_dwCPUStuffToDo & CPU_CHECK_INTERRUPTS)
		{
			R4300_Interrupt_MI();
			g_dwCPUStuffToDo &= ~CPU_CHECK_INTERRUPTS;
		}
		else if (g_dwCPUStuffToDo & CPU_CHECK_COUNT_INTERRUPT)
		{
			// Do VBl or COMPARE interrupt
			CPU_HANDLE_COUNT_INTERRUPT();
			g_dwCPUStuffToDo &= ~CPU_CHECK_COUNT_INTERRUPT;
		}
		else if (g_dwCPUStuffToDo & CPU_TLB_EXCEPTION)
		{
			R4300_Exception_TLB();
			g_dwCPUStuffToDo &= ~CPU_TLB_EXCEPTION;
		}
		else if (g_dwCPUStuffToDo & CPU_CHECK_POSTPONED_INTERRUPTS)
		{
			R4300_Interrupt_CheckPostponed();
			g_dwCPUStuffToDo &= ~CPU_CHECK_POSTPONED_INTERRUPTS;
		}
		else if (g_dwCPUStuffToDo & CPU_BREAKPOINT)
		{
			R4300_Exception_Break();
			g_dwCPUStuffToDo &= ~CPU_BREAKPOINT;
		}
		else if (g_dwCPUStuffToDo & CPU_SYSCALL)
		{
			R4300_Exception_Syscall();
			g_dwCPUStuffToDo &= ~CPU_SYSCALL;
		}
		else if (g_dwCPUStuffToDo & CPU_STOP_RUNNING)
		{
			g_dwCPUStuffToDo &= ~CPU_STOP_RUNNING;
			g_bCPURunning = FALSE;
		}
		else if (g_dwCPUStuffToDo & CPU_CHANGE_CORE)
		{
			g_dwCPUStuffToDo &= ~CPU_CHANGE_CORE;
			break;
		}

		// Clear g_dwCPUStuffToDo?
	}
}

// Keep executing instructions until there are other tasks to do (i.e. g_dwCPUStuffToDo is set)
// Process these tasks and loop
void CPU_GoWithNoRSP_NoDynarec(void)
{
	while (g_bCPURunning)
	{

		while (g_dwCPUStuffToDo == 0)
		{		
			CPU_EXECUTE_OP_WITH_NO_DYNAREC();

		}

		if (g_dwCPUStuffToDo & CPU_CHECK_INTERRUPTS)
		{
			R4300_Interrupt_MI();
			g_dwCPUStuffToDo &= ~CPU_CHECK_INTERRUPTS;
		}
		else if (g_dwCPUStuffToDo & CPU_CHECK_COUNT_INTERRUPT)
		{
			// Do VBl or COMPARE interrupt
			CPU_HANDLE_COUNT_INTERRUPT();
			g_dwCPUStuffToDo &= ~CPU_CHECK_COUNT_INTERRUPT;
		} 
		else if (g_dwCPUStuffToDo & CPU_TLB_EXCEPTION)
		{
			R4300_Exception_TLB();
			g_dwCPUStuffToDo &= ~CPU_TLB_EXCEPTION;
		}
		else if (g_dwCPUStuffToDo & CPU_CHECK_POSTPONED_INTERRUPTS)
		{
			R4300_Interrupt_CheckPostponed();
			g_dwCPUStuffToDo &= ~CPU_CHECK_POSTPONED_INTERRUPTS;
		}
		else if (g_dwCPUStuffToDo & CPU_BREAKPOINT)
		{
			R4300_Exception_Break();
			g_dwCPUStuffToDo &= ~CPU_BREAKPOINT;
		}		
		else if (g_dwCPUStuffToDo & CPU_SYSCALL)
		{
			R4300_Exception_Syscall();
			g_dwCPUStuffToDo &= ~CPU_SYSCALL;
		}
		else if (g_dwCPUStuffToDo & CPU_STOP_RUNNING)
		{
			g_dwCPUStuffToDo &= ~CPU_STOP_RUNNING;
			g_bCPURunning = FALSE;
		}
		else if (g_dwCPUStuffToDo & CPU_CHANGE_CORE)
		{
			g_dwCPUStuffToDo &= ~CPU_CHANGE_CORE;

			// Break out of this loop. The new core is selected automatically
			break;
		}
		
		// Clear g_dwCPUStuffToDo?
	}
}

void CPU_GoWithNoRSP_Dynarec(void)
{
	while (g_bCPURunning)
	{

		while (g_dwCPUStuffToDo == 0)
		{		
			CPU_EXECUTE_OP_WITH_DYNAREC();
		}

		if (g_dwCPUStuffToDo & CPU_CHECK_INTERRUPTS)
		{
			R4300_Interrupt_MI();
			g_dwCPUStuffToDo &= ~CPU_CHECK_INTERRUPTS;
		}
		else if (g_dwCPUStuffToDo & CPU_CHECK_COUNT_INTERRUPT)
		{
			// Do VBl or COMPARE interrupt
			CPU_HANDLE_COUNT_INTERRUPT();
			g_dwCPUStuffToDo &= ~CPU_CHECK_COUNT_INTERRUPT;
		} 
		else if (g_dwCPUStuffToDo & CPU_TLB_EXCEPTION)
		{
			R4300_Exception_TLB();
			g_dwCPUStuffToDo &= ~CPU_TLB_EXCEPTION;
		}
		else if (g_dwCPUStuffToDo & CPU_CHECK_POSTPONED_INTERRUPTS)
		{
			R4300_Interrupt_CheckPostponed();
			g_dwCPUStuffToDo &= ~CPU_CHECK_POSTPONED_INTERRUPTS;
		}
		else if (g_dwCPUStuffToDo & CPU_BREAKPOINT)
		{
			R4300_Exception_Break();
			g_dwCPUStuffToDo &= ~CPU_BREAKPOINT;
		}		
		else if (g_dwCPUStuffToDo & CPU_SYSCALL)
		{
			R4300_Exception_Syscall();
			g_dwCPUStuffToDo &= ~CPU_SYSCALL;
		}
		else if (g_dwCPUStuffToDo & CPU_STOP_RUNNING)
		{
			g_dwCPUStuffToDo &= ~CPU_STOP_RUNNING;
			g_bCPURunning = FALSE;
		}
		else if (g_dwCPUStuffToDo & CPU_CHANGE_CORE)
		{
			g_dwCPUStuffToDo &= ~CPU_CHANGE_CORE;

			// Break out of this loop. The new core is selected automatically
			break;
		}
		
		// Clear g_dwCPUStuffToDo?


	}
}




void CPU_AddEvent(DWORD dwCount, DWORD dwEventType)
{
	if (dwCount == 0)
	{
//		DBGConsole_Msg(0, "CPU: Attempt to add event with 0 time");
		return;
	}


	// Insert event at Count cycles into list
	CPUEvent * pNewEvent = g_pCPUEventPool;
	if (pNewEvent)
	{
		g_pCPUEventPool = pNewEvent->pNext;
	}
	else
	{
		pNewEvent = new CPUEvent;
		// Handle failures?
		if (pNewEvent == NULL)
			return;
	}

	pNewEvent->dwCount = dwCount;
	pNewEvent->dwEventType = dwEventType;
	

	CPUEvent * pCheckEvent = g_pFirstCPUEvent;
	CPUEvent * pPrev = NULL;

	while (pCheckEvent != NULL &&
		   pNewEvent->dwCount > pCheckEvent->dwCount)
	{
		// Decrease time by time waiter for this event
		pNewEvent->dwCount -= pCheckEvent->dwCount;

		// Check the next event
		pPrev = pCheckEvent;
		pCheckEvent = pCheckEvent->pNext;
	}

	// pCheckEvent points to an event that occurs AFTER us, so we need to update it:
	if (pCheckEvent != NULL)
	{
		pCheckEvent->dwCount -= pNewEvent->dwCount;

		// Link into this list
		pNewEvent->pPrev = pCheckEvent->pPrev;
		pNewEvent->pNext = pCheckEvent;

		if (pNewEvent->pPrev != NULL)
			pNewEvent->pPrev->pNext = pNewEvent;
		else
			g_pFirstCPUEvent = pNewEvent;
		
		pNewEvent->pNext->pPrev = pNewEvent;
	}
	else if (pPrev != NULL)
	{
		// We've been inserted at the end of the list: Insert us after pPrev
		pNewEvent->pPrev = pPrev;
		pNewEvent->pNext = NULL;

		pNewEvent->pPrev->pNext = pNewEvent;
	}
	else
	{
		// We must be the first event in the list
		pNewEvent->pPrev = NULL;
		pNewEvent->pNext = NULL;
		g_pFirstCPUEvent = pNewEvent;
	}
}

// Remove the event from the head of the event list, and place in the pool queue
// Ignores count of item being removed!
void CPU_RemoveHeadEvent()
{
	if (g_pFirstCPUEvent != NULL)
	{
		CPUEvent * pEvent = g_pFirstCPUEvent;

		if (pEvent->pNext != NULL)
			pEvent->pNext->pPrev = NULL;

		g_pFirstCPUEvent = pEvent->pNext;

		// Add to head of pool:
		pEvent->pNext = g_pCPUEventPool;
		g_pCPUEventPool = pEvent;
	}

}
