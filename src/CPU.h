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

#ifndef __CPU_H__
#define __CPU_H__

#include <kos.h>
#include <vector>
#include "Memory.h"
#include "R4300_Regs.h"

#include "DBGConsole.h"
#include "R4300.h"
#include "opcode.h"

#define NO_DELAY		0
#define EXEC_DELAY		1
#define DO_DELAY		2


void CPU_Reset(DWORD dwNewPC);

void CPUStep(void);
void CPUSkip(void);


BOOL StartCPUThread(LPSTR szReason, LONG nLen);
void StopCPUThread();

void CPU_Halt(LPCTSTR szReason);

void CPU_SelectCore();

#define CPU_BREAKPOINT						0x00000001
#define CPU_SYSCALL							0x00000002
#define CPU_COP1_UNUSABLE					0x00000004

#define CPU_CHECK_POSTPONED_INTERRUPTS		0x00000040
#define CPU_STOP_RUNNING					0x00000080
#define CPU_CHECK_COUNT_INTERRUPT			0x00000100
#define CPU_TLB_EXCEPTION					0x00000200
#define CPU_JUMP_EVEC						0x00000400

#define CPU_CHECK_INTERRUPTS				0x00001000
#define CPU_CHANGE_CORE						0x00002000


// Arbitrary unique numbers for different timing related events:
#define CPU_EVENT_VBL						1
#define CPU_EVENT_COMPARE					2

// External declarations
extern BOOL	 g_bCPURunning;
extern DWORD g_dwCPUStuffToDo;
extern DWORD g_dwVIs;
extern BOOL	g_bReloadAiPlugin;

#define DESIRED_VI_INTERVAL (1.0f / 60.0f)
extern float g_fCurrentRate;


void CPU_AddEvent(DWORD dwCount, DWORD dwEventType);


inline void AddCPUJob(DWORD dwJob)
{
	g_dwCPUStuffToDo |= dwJob;
}

inline void CPU_SetPC(DWORD dwPC)
{
	g_dwPC = dwPC;
	g_pPCMemBase = (BYTE *)ReadAddress(g_dwPC);
}

inline void INCREMENT_PC()
{
	g_dwPC += 4;
	g_pPCMemBase += 4;
	//	g_pPCMemBase = (BYTE *)ReadAddress(g_dwPC);
}

inline void DECREMENT_PC()
{
	g_dwPC -= 4;
	g_pPCMemBase -= 4;
	//	g_pPCMemBase = (BYTE *)ReadAddress(g_dwPC);
}

inline void CPU_Delay()
{
	g_nDelay = DO_DELAY;
}


typedef struct tagCPUEvent
{
	struct tagCPUEvent * pNext;
	struct tagCPUEvent * pPrev;
	DWORD dwCount;
	DWORD dwEventType;
	// Other details
} CPUEvent;


extern CPUEvent * g_pFirstCPUEvent;


typedef struct
{
	DWORD dwOriginalOp;
	BOOL  bEnabled;
	BOOL  bTemporaryDisable;		// Set to TRUE when BP is activated - this lets us type 
									// go immediately after a bp and execution will resume.
									// otherwise it would keep activating the bp
									// The patch bp r4300 instruction clears this 
									// when it is next executed
} DBG_BreakPoint;

extern std::vector< DBG_BreakPoint > g_BreakPoints;

void CPU_SetCompare(u64 qwNewValue);

// Add a break point at address dwAddress
void CPU_AddBreakPoint(DWORD dwAddress);
// Enable/Disable the breakpoint as the specified address
void CPU_EnableBreakPoint(DWORD dwAddress, BOOL bEnable);



#endif
