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



static void SR_FP_Init(CDynarecCode * pCode);
static void SR_FP_StackPicture();
static void SR_FP_ExchReg(DWORD dwSP1, DWORD dwSP2);
static void SR_FP_LoadReg(DWORD dwFS);
static void SR_FP_SaveReg(DWORD dwFS);
static void SR_FP_DiscardReg(DWORD dwFS);
static void SR_FP_EvictOldestRegister();
static void SR_FP_PopValueOffStack(DWORD dwSI);
static void SR_FP_PushRegs();
static void SR_FP_PopRegs();
static void SR_FP_Touch(DWORD dwFS);
static void SR_FP_Prepare_D_S_S(DWORD dwFD, DWORD dwFS, DWORD dwFT);
static void SR_FP_CopyRegToST0(DWORD dwFS);
static void SR_FP_AssignToST0(DWORD dwFD);

static void SR_FP_S_ADD(DWORD dwFD, DWORD dwFS, DWORD dwFT);
static void SR_FP_S_SUB(DWORD dwFD, DWORD dwFS, DWORD dwFT);
static void SR_FP_S_MUL(DWORD dwFD, DWORD dwFS, DWORD dwFT);
static void SR_FP_S_DIV(DWORD dwFD, DWORD dwFS, DWORD dwFT);
static void SR_FP_S_SQRT(DWORD dwFD, DWORD dwFS);
static void SR_FP_S_NEG(DWORD dwFD, DWORD dwFS);
static void SR_FP_S_ABS(DWORD dwFD, DWORD dwFS);


typedef struct tagFPRegInfo
{
	DWORD dwStackPos;		// ~0 indicates not loaded at present
							// 0 .. 7 indicates actual position on stack
	BOOL bDirty;			// TRUE if we need to write back to g_CPR[1][fp];

	DWORD dwTimestamp;		// Time of last use. Lower numbers indicate older 

} fpRegInfo;

static fpRegInfo g_FPReg[32];
static DWORD g_dwFPStackContents[8];	// What the current stack contains

static DWORD g_dwFPTimeNow = 0;

static CDynarecCode * g_pFPpCode = NULL;


// At the assembly level the recommended workaround for the second FIST bug is the same for the first; 
// inserting the FRNDINT instruction immediately preceding the FIST instruction. 

// Rounds to the nearest number
inline void FloatToInt(int *int_pointer, float f)
{
    /*KOS
	__asm  fld  f
  __asm  mov  edx,int_pointer
  __asm  FRNDINT
  __asm  fistp dword ptr [edx];
  */

}


void SR_FP_Init(CDynarecCode * pCode)
{
	DWORD i;

	for (i = 0; i < 32; i++)
	{
		g_FPReg[i].bDirty = FALSE;
		g_FPReg[i].dwStackPos = ~0;	// Not on stack at the moment;
	}

	for (i = 0; i < 8; i++)
	{
		g_dwFPStackContents[i] = ~0;
	}

	g_pFPpCode = pCode;
}

void SR_FP_FlushAllRegs()
{
	LONG i;

	DWORD dwSafeCount;

	//for (i = 0; i < 32; i++)
	dwSafeCount = 0;
	while (g_dwFPStackContents[0] != ~0)
	{
		DWORD dwFS = g_dwFPStackContents[0];
		
		//if (g_FPReg[i].dwStackPos != ~0)
		{
			if (g_FPReg[dwFS].bDirty)
				SR_FP_SaveReg(dwFS);
			else
				SR_FP_DiscardReg(dwFS);
		}

		// Make sure we don't keep looping forever!
		dwSafeCount++;
		if (dwSafeCount >= 8)
		{
			DPF(DEBUG_DYNREC, "!!!! Unable to flush FP regs!");
			break;
		}
	}

	if (dwSafeCount > 0)
	{
		DPF(DEBUG_DYNREC, "Flushed %d fp regs", dwSafeCount);	
	}

	// Assert all regs saved?
	for (i = 0; i < 32; i++)
	{
		_ASSERTE(g_FPReg[i].bDirty == FALSE);
	}
}

void SR_FP_StackPicture()
{

	DPF(DEBUG_DYNREC,
		"%02d %02d %02d %02d %02d %02d %02d %02d",
		g_dwFPStackContents[0] == ~0 ? -1 : g_dwFPStackContents[0],
		g_dwFPStackContents[1] == ~0 ? -1 : g_dwFPStackContents[1],
		g_dwFPStackContents[2] == ~0 ? -1 : g_dwFPStackContents[2],
		g_dwFPStackContents[3] == ~0 ? -1 : g_dwFPStackContents[3],
		g_dwFPStackContents[4] == ~0 ? -1 : g_dwFPStackContents[4],
		g_dwFPStackContents[5] == ~0 ? -1 : g_dwFPStackContents[5],
		g_dwFPStackContents[6] == ~0 ? -1 : g_dwFPStackContents[6],
		g_dwFPStackContents[7] == ~0 ? -1 : g_dwFPStackContents[7]);

}

// Swap positions of registers dwSP1 and dwSP2
// Assumes: both registers have valid contents!
// Assumes: dwSP1 is 0!
// Assumes: dwSP1 != dwSP2
void SR_FP_ExchReg(DWORD dwSP1, DWORD dwSP2)
{
	_ASSERTE(dwSP1 == 0);
	_ASSERTE(dwSP1 != dwSP2);

	// Swap stackpos of 
	DWORD dwFP1 = g_dwFPStackContents[dwSP1];
	DWORD dwFP2 = g_dwFPStackContents[dwSP2];

	// Update reg indicators:
	g_FPReg[dwFP2].dwStackPos = dwSP1;
	g_FPReg[dwFP1].dwStackPos = dwSP2;

	// Update stack indicators
	g_dwFPStackContents[dwSP2] = dwFP1;
	g_dwFPStackContents[dwSP1] = dwFP2;

	// Emit instructions for fxch!
	DPF(DEBUG_DYNREC,"fxch	st(%d)", dwSP2);

	FXCH(g_pFPpCode, dwSP2);

}

// If the register is not on the stack, it is loaded into position 0,
// Assumes: Any registers required for this operation have been "touched"
//          so that they won't be evicted
// Assumes: Register is not already loaded
void SR_FP_LoadReg(DWORD dwFS)
{
	_ASSERTE(g_FPReg[dwFS].dwStackPos == ~0);

	// Need to load into new position

	// Check if stack is full. If so, pop oldest
	if (g_dwFPStackContents[7] != ~0)
	{
		// This register will be evicted by the load - we have to either store or discard
		// the oldest register to prevent this...
		SR_FP_EvictOldestRegister();
	}

	// Move regs up stack - this is to simulate the fld
	SR_FP_PushRegs();

	// The contents of st(0) are the new register
	g_dwFPStackContents[0] = dwFS;
	
	// Load register into position 0 and set timestamp
	g_FPReg[dwFS].dwStackPos = 0;
	g_FPReg[dwFS].bDirty = FALSE;
	g_FPReg[dwFS].dwTimestamp = g_dwFPTimeNow;

	// Emit instruction for fld !!!!!!!
	DPF(DEBUG_DYNREC,"fld    dword ptr [FP%02d]", dwFS);
	FLD_MEMp(g_pFPpCode, &g_qwCPR[1][dwFS]);
	
}

// This removes the oldest register from the stack.
// Assumes: Any registers required for this operation have been "touched"
// Assumes: Stack is not empty
void SR_FP_EvictOldestRegister()
{
	DWORD dwOldest = 0xFFFFFFFF;
	DWORD dwOldestIndex = ~0;

	DWORD i;
	DWORD dwFi;

	for (i = 0; i < 7; i++)
	{
		dwFi = g_dwFPStackContents[i];

		// Ignore reg if empty
		if (dwFi == ~0)
			continue;

		if (g_FPReg[dwFi].dwTimestamp < dwOldest)
		{
			dwOldest = g_FPReg[dwFi].dwTimestamp;
			dwOldestIndex = i;
		}
	}

	// dwOldestIndex should NOT be ~0 here, unless the stack is empty
	_ASSERTE(dwOldestIndex != ~0);

	// Pop this value off the top of the stack.
	SR_FP_PopValueOffStack(dwOldestIndex);

}

// 
// If the register at the top of the stack is dirty, store it, else discard it
// Assumes: dwSI contains a register
void SR_FP_PopValueOffStack(DWORD dwSI)
{
	DWORD dwFS;
		
	_ASSERTE(g_dwFPStackContents[dwSI] != ~0);
	
	// Exchange the oldest register with the top of the stack (if it is not top already)
	if (dwSI != 0)
		SR_FP_ExchReg(0, dwSI);

	dwFS = g_dwFPStackContents[0];

	// Should be same value as assert above
	_ASSERTE(dwFS != ~0);

	if (g_FPReg[dwFS].bDirty)
	{
		SR_FP_SaveReg(dwFS);
	}
	else
	{
		SR_FP_DiscardReg(dwFS);
	}

}

// Save contents of FP reg
// Assumes: dwFS is currently on the stack
void SR_FP_SaveReg(DWORD dwFS)
{
	DWORD dwSI;
	_ASSERTE(dwFS != ~0);
	_ASSERTE(g_FPReg[dwFS].bDirty);

	// Move FD to top of stack
	dwSI = g_FPReg[dwFS].dwStackPos;

	_ASSERTE(dwSI != ~0);

	// Move to top of stack (if it's not there already)
	if (dwSI != 0)
		SR_FP_ExchReg(0, g_FPReg[dwFS].dwStackPos);

	DPF(DEBUG_DYNREC,"fstp   dword ptr [FP%02d]		// Save", dwFS);
	FSTP_MEMp(g_pFPpCode, &g_qwCPR[1][dwFS]);
	
	
	g_FPReg[dwFS].dwStackPos = ~0;
	g_dwFPStackContents[0] = ~0;
	g_FPReg[dwFS].bDirty = FALSE;

	SR_FP_PopRegs();
	
}


// As above, but just discards contents
// Assumes: dwFS is currently on the stack
void SR_FP_DiscardReg(DWORD dwFS)
{
	DWORD dwSI;
	_ASSERTE(dwFS != ~0);

	// Move FD to top of stack
	dwSI = g_FPReg[dwFS].dwStackPos;

	_ASSERTE(dwSI != ~0);

	// Move to top of stack (if it's not there already)
	if (dwSI != 0)
		SR_FP_ExchReg(0, g_FPReg[dwFS].dwStackPos);

	DPF(DEBUG_DYNREC,"ffree	st(0)");
	FFREE(g_pFPpCode, 0);
	DPF(DEBUG_DYNREC,"fincstp      //(discard FP%02d)", dwFS);
	FINCSTP(g_pFPpCode);
	
	g_FPReg[dwFS].dwStackPos = ~0;
	
	g_dwFPStackContents[0] = ~0;
	g_FPReg[dwFS].bDirty = FALSE;
	
	SR_FP_PopRegs();

}


// Push registers up the stack
// Assumes: st(7) is empty
void SR_FP_PushRegs()
{
	DWORD i;

	_ASSERTE(g_dwFPStackContents[7] == ~0);

	for (i = 7; i >= 1; i--)
		g_dwFPStackContents[i] = g_dwFPStackContents[i - 1];

	// Bottom is empty now
	g_dwFPStackContents[0] = ~0;

	for (i = 0; i < 32; i++)
		if (g_FPReg[i].dwStackPos != ~0)
			g_FPReg[i].dwStackPos++;

}

// Pop register from the stack.
// Assumes: St(0) is empty
void SR_FP_PopRegs()
{
	DWORD i;

	_ASSERTE(g_dwFPStackContents[0] == ~0);

	for (i = 0; i < 7; i++)
		g_dwFPStackContents[i] = g_dwFPStackContents[i + 1];

	// Top is empty now
	g_dwFPStackContents[7] = ~0;

	for (i = 0; i < 32; i++)
		if (g_FPReg[i].dwStackPos != ~0)
			g_FPReg[i].dwStackPos--;

}

// "touch" a register to update it to the current time. This prevents it from 
// being evicted this "cycle"
void SR_FP_Touch(DWORD dwFS)
{
	g_FPReg[dwFS].dwTimestamp = g_dwFPTimeNow;
}

// Ensure that the two source registers are available for 
// the calculation, and discard the contents of the 
// destination register if it is not used
void SR_FP_Prepare_D_S_S(DWORD dwFD, DWORD dwFS, DWORD dwFT)
{
	SR_FP_Touch(dwFS);
	SR_FP_Touch(dwFT);

	// If the destination register is not used in the calculation, discard
	if (g_FPReg[dwFD].dwStackPos != ~0 &&		// Register is on the stack
		dwFD != dwFS &&							// Register is not FS
		dwFD != dwFT)							// Register is not FT
		SR_FP_DiscardReg(dwFD);

	// Ensure register is loaded
	if (g_FPReg[dwFS].dwStackPos == ~0)
		SR_FP_LoadReg(dwFS);

	if (g_FPReg[dwFT].dwStackPos == ~0)
		SR_FP_LoadReg(dwFT);
}

// Ensure that the source register is available for 
// the calculation, and discard the contents of the 
// destination register if it is not used
void SR_FP_Prepare_D_S(DWORD dwFD, DWORD dwFS)
{
	SR_FP_Touch(dwFS);

	// If the destination register is not used in the calculation, discard
	if (g_FPReg[dwFD].dwStackPos != ~0 &&		// Register is on the stack
		dwFD != dwFS)							// Register is not FS
		SR_FP_DiscardReg(dwFD);

	// Ensure register is loaded
	if (g_FPReg[dwFS].dwStackPos == ~0)
		SR_FP_LoadReg(dwFS);

}


// Copies a register to st(0) in preparation for a calculation
// Assumes: dwFS is already loaded
// ---Assumes: st(7) is empty -- We evict a register if necessary
void SR_FP_CopyRegToST0(DWORD dwFS)
{
	DWORD dwSI;
	
	if (g_dwFPStackContents[7] != ~0)
		SR_FP_EvictOldestRegister();

	dwSI = g_FPReg[dwFS].dwStackPos;	// Keep copy of stack pos

	_ASSERTE(dwSI != ~0);
	//_ASSERTE(g_dwFPStackContents[7] == ~0);
	
	SR_FP_PushRegs();		// Make space at 0

	DPF(DEBUG_DYNREC,"fld    st(%d)    //(FP%02d)", dwSI, dwFS);
	FLD(g_pFPpCode, dwSI);
	
}

// Assigns st(0) to reg FD
// Assumes: st(0) is currently unassigned
//
// If FD is currently assigned, this code will exchange the result with that reg
// on the top, and discard the old contents of st(FD)
void SR_FP_AssignToST0(DWORD dwFD)
{
	_ASSERTE(g_dwFPStackContents[0] == ~0);

	if (g_FPReg[dwFD].dwStackPos == ~0)
	{
		g_dwFPStackContents[0] = dwFD;
		g_FPReg[dwFD].dwStackPos = 0;
		g_FPReg[dwFD].bDirty = TRUE;
		g_FPReg[dwFD].dwTimestamp = g_dwFPTimeNow;
	}
	else
	{
		// Can't swap with ourselves!
		_ASSERTE(g_FPReg[dwFD].dwStackPos != 0);

		DWORD dwSI = g_FPReg[dwFD].dwStackPos;

		// Emit instructions for fxch!
		DPF(DEBUG_DYNREC,"//Discarding old value!");
		DPF(DEBUG_DYNREC,"fxch	st(%d)", dwSI);
		FXCH(g_pFPpCode, dwSI);
		
		// Stackpos stays the same. Update dirty etc
		g_FPReg[dwFD].bDirty = TRUE;
		g_FPReg[dwFD].dwTimestamp = g_dwFPTimeNow;
		
		// Discard the top value
		DPF(DEBUG_DYNREC,"ffree	st(0)");
		FFREE(g_pFPpCode, 0);
		DPF(DEBUG_DYNREC,"fincstp      //(discard)");
		FINCSTP(g_pFPpCode);
				
		SR_FP_PopRegs();
		
	}	
	
}


void SR_FP_S_ADD(DWORD dwFD, DWORD dwFS, DWORD dwFT)
{
	SR_FP_Prepare_D_S_S(dwFD, dwFS, dwFT);

	SR_FP_CopyRegToST0(dwFS);

	// Add 
	DWORD dwST = g_FPReg[dwFT].dwStackPos;
	DPF(DEBUG_DYNREC,"fadd   st(0),st(%d)    //(FP%02d)", dwST, dwFT);
	FADD(g_pFPpCode, dwST);
	
	// Assign to dwFD
	SR_FP_AssignToST0(dwFD);
}

void SR_FP_S_SUB(DWORD dwFD, DWORD dwFS, DWORD dwFT)
{
	SR_FP_Prepare_D_S_S(dwFD, dwFS, dwFT);

	SR_FP_CopyRegToST0(dwFS);

	// Sub 
	DWORD dwST = g_FPReg[dwFT].dwStackPos;
	DPF(DEBUG_DYNREC,"fsub   st(0),st(%d)    //(FP%02d)", dwST, dwFT);
	FSUB(g_pFPpCode, dwST);
	
	// Assign to dwFD
	SR_FP_AssignToST0(dwFD);
}

void SR_FP_S_MUL(DWORD dwFD, DWORD dwFS, DWORD dwFT)
{
	SR_FP_Prepare_D_S_S(dwFD, dwFS, dwFT);

	SR_FP_CopyRegToST0(dwFS);

	// Mul 
	DWORD dwST = g_FPReg[dwFT].dwStackPos;
	DPF(DEBUG_DYNREC,"fmul   st(0),st(%d)    //(FP%02d)", dwST, dwFT);
	FMUL(g_pFPpCode, dwST);

	// Assign to dwFD
	SR_FP_AssignToST0(dwFD);
}

void SR_FP_S_DIV(DWORD dwFD, DWORD dwFS, DWORD dwFT)
{
	SR_FP_Prepare_D_S_S(dwFD, dwFS, dwFT);

	SR_FP_CopyRegToST0(dwFS);

	// Mul 
	DWORD dwST = g_FPReg[dwFT].dwStackPos;
	DPF(DEBUG_DYNREC,"fdiv   st(0),st(%d)    //(FP%02d)", dwST, dwFT);
	FDIV(g_pFPpCode, dwST);

	// Assign to dwFD
	SR_FP_AssignToST0(dwFD);
}



void SR_FP_S_SQRT(DWORD dwFD, DWORD dwFS)
{
	SR_FP_Prepare_D_S(dwFD, dwFS);
	
	SR_FP_CopyRegToST0(dwFS);
	
	// Sqrt
	DPF(DEBUG_DYNREC,"fsqrt");
	FSQRTp(g_pFPpCode);

	// Assign to dwFD
	SR_FP_AssignToST0(dwFD);
}

void SR_FP_S_NEG(DWORD dwFD, DWORD dwFS)
{
	SR_FP_Prepare_D_S(dwFD, dwFS);
	
	SR_FP_CopyRegToST0(dwFS);
	
	// fchs
	DPF(DEBUG_DYNREC,"fchs");
	FCHSp(g_pFPpCode);

	// Assign to dwFD
	SR_FP_AssignToST0(dwFD);
}

void SR_FP_S_MOV(DWORD dwFD, DWORD dwFS)
{
	SR_FP_Prepare_D_S(dwFD, dwFS);
	
	SR_FP_CopyRegToST0(dwFS);
	
	// move (just assign st(0) to FD)
	DPF(DEBUG_DYNREC,"'fmov'");

	// Assign to dwFD
	SR_FP_AssignToST0(dwFD);
}


void SR_FP_MTC1(DWORD dwFD)
{

	// If the value is currently on the stack, discard (even if dirty)
	if (g_FPReg[dwFD].dwStackPos != ~0)
		SR_FP_DiscardReg(dwFD);

	// TODO- copy value to FP (and load? - it's not worth doing this if the value
	//       is not used in subsequent instructions)
	DPF(DEBUG_DYNREC,"mtc1 REG -> FP%02d", dwFD);
	
}
void SR_FP_MFC1(DWORD dwFS)
{

	// If the value is currently on the stack, write it out
	// Usually it will not be used again (because the calculation is complete

	if (g_FPReg[dwFS].dwStackPos != ~0 && g_FPReg[dwFS].bDirty)
		SR_FP_SaveReg(dwFS);
	

	DPF(DEBUG_DYNREC,"mfc1 REG <- FP%02d", dwFS);
}
void SR_FP_LWC1(DWORD dwFD)
{
	// If the value is currently on the stack, discard (even if dirty)
	if (g_FPReg[dwFD].dwStackPos != ~0)
		SR_FP_DiscardReg(dwFD);

	// TODO- copy value to FP (and load? - it's not worth doing this if the value
	//       is not used in subsequent instructions)
	DPF(DEBUG_DYNREC,"lwc1 MEM -> FP%02d", dwFD);
}

void SR_FP_SWC1(DWORD dwFS)
{
	// If the value is currently on the stack, write it out
	// Usually it will not be used again (because the calculation is complete

	if (g_FPReg[dwFS].dwStackPos != ~0 && g_FPReg[dwFS].bDirty)
		SR_FP_SaveReg(dwFS);
	

	DPF(DEBUG_DYNREC,"swc1 MEM <- FP%02d", dwFS);
}
