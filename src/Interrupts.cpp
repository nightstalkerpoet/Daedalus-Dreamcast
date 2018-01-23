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

// Handle interrupts etc
#include "main.h"

#include "Debug.h"

#include "CPU.h"
#include "Interrupt.h"
#include "ultra_rcp.h"
#include "ultra_r4300.h"
#include "R4300.h"

LONG g_nTLBExceptionReason = 0;

DWORD g_dwNumExceptions = 0;


void R4300_Interrupt_CheckPostponed()
{
	DWORD dwIntrBits = Memory_MI_GetRegister(MI_INTR_REG);
	DWORD dwIntrMask = Memory_MI_GetRegister(MI_INTR_MASK_REG);

	if ((dwIntrBits & dwIntrMask) != 0)
	{
		g_qwCPR[0][C0_CAUSE] |= CAUSE_IP3;
	}

	if(g_qwCPR[0][C0_SR] & g_qwCPR[0][C0_CAUSE] & CAUSE_IPMASK)		// Are interrupts pending/wanted
	{
		if(g_qwCPR[0][C0_SR] & SR_IE)								// Are interrupts enabled
		{
			if((g_qwCPR[0][C0_SR] & (SR_EXL|SR_ERL)) == 0x0000)		// Ensure ERL/EXL are not set
			{
				R4300_JumpToInterruptVector(E_VEC);								// Go
				return;
			}
		} 
	}
} 

void R4300_Exception_Break(void)
{

	// Clear CAUSE_EXCMASK
	g_qwCPR[0][C0_CAUSE] &= ~CAUSE_EXCMASK;
	g_qwCPR[0][C0_CAUSE] |= EXC_BREAK;

	R4300_JumpToInterruptVector(E_VEC);
}


void R4300_Exception_Syscall(void)
{
	// Clear CAUSE_EXCMASK
	g_qwCPR[0][C0_CAUSE] &= ~CAUSE_EXCMASK;
	g_qwCPR[0][C0_CAUSE] |= EXC_SYSCALL;

	R4300_JumpToInterruptVector(E_VEC);
}

void R4300_Exception_TLB()
{
//	CPU_Halt("TLB");

	switch (g_nTLBExceptionReason)
	{
	case  EXCEPTION_TLB_REFILL_LOAD:
		g_qwCPR[0][C0_CAUSE] &= ~CAUSE_EXCMASK;
		g_qwCPR[0][C0_CAUSE] |= EXC_RMISS;

		R4300_JumpToInterruptVector(UT_VEC);
		break;
	case EXCEPTION_TLB_REFILL_STORE:

		g_qwCPR[0][C0_CAUSE] &= ~CAUSE_EXCMASK;
		g_qwCPR[0][C0_CAUSE] |= EXC_WMISS;

		R4300_JumpToInterruptVector(UT_VEC);
		break;

	case EXCEPTION_TLB_INVALID_LOAD:
		g_qwCPR[0][C0_CAUSE] &= ~CAUSE_EXCMASK;
		g_qwCPR[0][C0_CAUSE] |= EXC_RMISS;

		R4300_JumpToInterruptVector(E_VEC);
		break;
	case EXCEPTION_TLB_INVALID_STORE:
		g_qwCPR[0][C0_CAUSE] &= ~CAUSE_EXCMASK;
		g_qwCPR[0][C0_CAUSE] |= EXC_WMISS;

		R4300_JumpToInterruptVector(E_VEC);
		break;

	}
}

void R4300_Interrupt_Compare()
{
	DPF(DEBUG_INTR_COMPARE, "COMPARE: Interrupt Occuring");

	// Compare causes IP7 (CAUSE_IP8) (Timer event) to be set in CAUSE register (see R4300 DataSheet, Rev 0.3, p13)
	g_qwCPR[0][C0_CAUSE] |= CAUSE_IP8;
   
	// If IP7 or IE are not set in STATUS, ignore interrupt
	if ( (g_qwCPR[0][C0_SR] & (SR_IBIT8|SR_IE)) != (SR_IBIT8|SR_IE) )
	{
		DPF(DEBUG_INTR_COMPARE, "COMPARE: IP7 or IE is clear - Ignoring Interrupt");
		return;
	}
    
	// If EXL or ERL is set, ignore
	if ( g_qwCPR[0][C0_SR] & (SR_EXL|SR_ERL) )
	{
		DPF(DEBUG_INTR_COMPARE, "COMPARE: EXL or ERL is set - Ignoring Interrupt");
		return;
	}

 	DPF(DEBUG_INTR_COMPARE, "COMPARE: Interrupt");

	// Clear CAUSE_EXCMASK
	g_qwCPR[0][C0_CAUSE] &= ~CAUSE_EXCMASK;

	R4300_JumpToInterruptVector(E_VEC);
  
} 

void R4300_Interrupt_MI()
{
	// if MI_INTR_MASK_REG is not set (disabled), don't cause interrupt
	if ((Memory_MI_GetRegister(MI_INTR_MASK_REG) & 
		 Memory_MI_GetRegister(MI_INTR_REG)) == 0)
	{
		// Nothing to do
		return;
	}

	// VI causes IP2 to be set in CAUSE register (see R4300 DataSheet, Rev 0.3, p13)
	g_qwCPR[0][C0_CAUSE] |= CAUSE_IP3;

	// If IP2 or IE are not set in STATUS, ignore interrupt
	if ( (g_qwCPR[0][C0_SR] & (SR_IBIT3|SR_IE)) != (SR_IBIT3|SR_IE) )
	{
		return;
	}
  
	// If EXL or ERL is set, ignore
	if ( g_qwCPR[0][C0_SR] & (SR_EXL|SR_ERL) )
	{
		return;
	}

	// Clear CAUSE_EXCMASK
	g_qwCPR[0][C0_CAUSE] &= ~CAUSE_EXCMASK;

	R4300_JumpToInterruptVector(E_VEC);
}


// dwIntrMask is one or more of MI_INTR_VI etc
static void R4300_Interrupt_GenericMI(DWORD dwIntrMask)
{
	// Set VI (Video Interface) flag in memory regs (MI_INTR_REGS)
	Memory_MI_SetRegisterBits(MI_INTR_REG, dwIntrMask);

	R4300_Interrupt_MI();
}




void
R4300_JumpToInterruptVector(DWORD dwExceptionVector)
{
	g_dwNumExceptions++;


	// If EXL is already set, we jump 
	if (g_qwCPR[0][C0_SR] & SR_EXL)
	{
		CPU_Halt("Exception in exception");
		DBGConsole_Msg(0, "[MException within exception");

		// Force default exception handler
		dwExceptionVector = E_VEC;

	}
	else
	{
		g_qwCPR[0][C0_SR] |= SR_EXL;		// Set the EXeption Level (EXL) in STATUS reg (in Kernel mode)
		g_qwCPR[0][C0_EPC] = g_dwPC;		// Save the PC in ExceptionPC (EPC)

		if (g_nDelay == EXEC_DELAY)
		{
			g_qwCPR[0][C0_CAUSE] |= CAUSE_BD;	// Set the BD (Branch Delay) flag in the CAUSE reg
			g_qwCPR[0][C0_EPC]   = g_dwPC - 4;	// We want the immediately preceeding instruction
		}
		else
		{
			g_qwCPR[0][C0_CAUSE] &= ~CAUSE_BD;	// Clear the BD (Branch Delay) flag
			g_qwCPR[0][C0_EPC]   = g_dwPC;		// We want the current instruction
		}
	}

	if (g_qwCPR[0][C0_SR] & SR_BEV)
	{
		// Use Boot Exception Vectors...
		CPU_SetPC(dwExceptionVector);		// Set the Program Counter to the interrupt vector
	}
	else
	{
		// Use normal exception vectors..
		CPU_SetPC(dwExceptionVector);		// Set the Program Counter to the interrupt vector
	}
	// Clear CPU_DO_DELAY????
	g_nDelay = NO_DELAY;				// Ensure we don't execute delayed instruction

}
