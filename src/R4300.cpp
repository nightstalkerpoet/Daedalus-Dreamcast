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
#include <kos.h>
#include <math.h>
#include "main.h"
#include "windef.h"
#include <cstdlib>
#include "debug.h"
#include "CPU.h"
#include "OS.h"
#include "ROM.h"
#include "RSP.h"
#include "SR.h"
#include "Registers.h"			// For REG_?? defines
#include "R4300.h"
#include "R4300_Jump.inl"		// Jump table
#include "OpCode.h"
#include "patch.h"				// For g_PatchSymbols
#include "DBGConsole.h"

#include "ultra_r4300.h"


inline void SpeedHack(BOOL bCond, DWORD dwOp)
{
	if (bCond)
	{
		DWORD dwNextOp;

		dwNextOp = *(DWORD *)(g_pPCMemBase+4);

		// If nop, then this is a busy-wait for an interrupt
		if (dwNextOp == 0)
		{ 
			*(DWORD*)&g_qwCPR[0][C0_COUNT] += (g_pFirstCPUEvent->dwCount - 1);
			g_pFirstCPUEvent->dwCount = 1;
		}
		// This is:
		// 0x7f0d01e8: BNEL      v0 != v1 --> 0x7f0d01e8
		// 0x7f0d01ec: ADDIU     v0 = v0 + 0x0004
		else if (dwOp == 0x5443ffff && dwNextOp == 0x24420004)
		{
			g_qwGPR[REG_v0] = g_qwGPR[REG_v1] - 4;
		}
		else
		{
			static BOOL bWarned = FALSE;

			if (!bWarned)
			{
				DBGConsole_Msg(0, "Missed Speedhack 0x%08x", g_dwPC);
				bWarned = TRUE;
			}
		}
	}
}
/*
	else if (g_dwPC == 0x7f0d01e8)			\
	{								\
		*(DWORD*)&g_qwCPR[0][C0_COUNT] += (g_pFirstCPUEvent->dwCount - 1); \
		g_pFirstCPUEvent->dwCount = 1; \
	}					\
*/


inline u64 LoadFPR_Long(DWORD dwReg)
{
	/*if (g_qwCPR[0][C0_SR] & SR_FR)
	{
		//DBGConsole_Msg(0,"Fulllen");
		return g_qwCPR[1][dwReg];
	}
	else*/
	{
		return (g_qwCPR[1][dwReg+1]<<32) | (g_qwCPR[1][dwReg+0]&0xFFFFFFFF);
	}
}

inline void StoreFPR_Long(DWORD dwReg, u64 qwValue)
{
	/*if (g_qwCPR[0][C0_SR] & SR_FR)
	{
		//DBGConsole_Msg(0,"Fulllen");
		g_qwCPR[1][dwReg] = qwValue;
	}
	else*/
	{
		// TODO - Don't think these need sign extending...
		ToInt(g_qwCPR[1][dwReg+0]) = (u32)(qwValue & 0xFFFFFFFF);
		ToInt(g_qwCPR[1][dwReg+1]) = (u32)(qwValue>>32);
	}
}

inline f64 LoadFPR_Double(DWORD dwReg)
{
	u64 qwValue = LoadFPR_Long(dwReg);

	return ToDouble(qwValue);
}

inline void StoreFPR_Double(DWORD dwReg, f64 fValue)
{
	StoreFPR_Long(dwReg, ToQuad(fValue));
}


inline LONG LoadFPR_Word(DWORD dwReg)
{
	return ToInt(g_qwCPR[1][dwReg]);
}

inline void StoreFPR_Word(DWORD dwReg, LONG nValue)
{
	ToInt(g_qwCPR[1][dwReg]) = nValue;
}


inline f32 LoadFPR_Single(DWORD dwReg)
{
	return ToFloat(g_qwCPR[1][dwReg]);
}

inline void StoreFPR_Single(DWORD dwReg, f32 fValue)
{
	ToFloat(g_qwCPR[1][dwReg]) = fValue;
}


void R4300_SetCop1Enable(BOOL bEnabled)
{
	//static BOOL bLast = FALSE;

	if (OS_HLE_DoFPStuff())
	{
		//SR_CU1
		if (bEnabled)
		{
			//if (!bLast) 
			//	DBGConsole_Msg(0, "Enabling FP");
			R4300Instruction[OP_COPRO1] = R4300_CoPro1;
		}
		else
		{
			//if (bLast)
			//	DBGConsole_Msg(0, "Disabling FP");
			R4300Instruction[OP_COPRO1] = R4300_CoPro1_Disabled;
		}
	}
	else
	{
		// Just enable by default
		R4300Instruction[OP_COPRO1] = R4300_CoPro1;
	}
	//bLast = bEnabled;
}

void R4300_SetSR(DWORD dwNewValue)
{
	static BOOL bWarned = FALSE;
	/*if (((u32)g_qwCPR[0][C0_SR] & SR_FR) != (dwNewValue & SR_FR))
	{
		if (dwNewValue & SR_FR)
			DBGConsole_Msg(0, "FP changed to 64bit register mode");
		else
			DBGConsole_Msg(0, "FP changed to 32bit register mode");
	}

	
	if (((u32)g_qwCPR[0][C0_SR] & SR_UX) != (dwNewValue & SR_UX))
	{
		if (dwNewValue & SR_UX)
			DBGConsole_Msg(0, "CPU changed to 64bit register mode");
		else
			DBGConsole_Msg(0, "CPU changed to 32bit register mode");
	}*/


	g_qwCPR[0][C0_SR] = (s64)(s32)dwNewValue;
	if ((g_qwCPR[0][C0_SR] & SR_FR) && !bWarned)
	{
		DBGConsole_Msg(0, "Warning, using full-length fp registers\nThis feature has been disabled");
		bWarned = TRUE;
	}

	if (g_qwCPR[0][C0_SR] & SR_CU1)
		R4300_SetCop1Enable(TRUE);
	else
		R4300_SetCop1Enable(FALSE);

}

// rount_int, floor_int and ceil_int were borrowed from
// someone's post on flipcode a while back!
// I added trunc and the double versions


// These seem to mess up at the moment - so I've resorted to the old method
inline s32 round_int_s(float x)
{
	return (s32)x;

/*
	s32      i;
	static const float round_to_nearest = 0.5f;
	__asm
	{
		fld      x
		fadd     st, st (0)
		fadd     round_to_nearest
		fistp    i
		sar      i, 1
	}
	
	return (i);*/
}

// Round to zero
inline s32 trunc_int_s(float x)
{
	return (s32)x;
/*	s32      i;
	__asm
	{
		fld      x
		fistp    i
	}
	
	return (i);*/
}

inline s32 floor_int_s (float x)
{
	return (s32)x;
/*	s32      i;
	static const float round_toward_m_i = -0.5f;
	__asm
	{
		fld      x
		fadd     st, st (0)
		fadd     round_toward_m_i
		fistp    i
		sar      i, 1
	}
	
	return (i);*/
}

inline s32 ceil_int_s(float x)
{
	return (s32)x;
/*	s32      i;
	static const float round_toward_p_i = -0.5f;
	__asm
	{
		fld      x
		fadd     st, st (0)
		fsubr    round_toward_p_i
		fistp    i
		sar      i, 1
	}
	
	return (-i);*/
}


inline s64 round_int_d(long double x)
{
	return (s64)x;
/*
	s64      i;
	static const double round_to_nearest = 0.5;
	__asm
	{
		fld      x
		fadd     st, st (0)
		fadd     round_to_nearest
		fistp    i
	}
	i >>= 1;
	
	return (i);*/
}

inline s64 trunc_int_d(long double x)
{
	return (s64)x;
/*
	s64      i;
	__asm
	{
		fld      x
		fistp    i
	}
	return (i);*/
}

inline s64 floor_int_d (long double x)
{
	return (s64)x;
/*
	s64      i;
	static const double round_toward_m_i = -0.5;
	__asm
	{
		fld      x
		fadd     st, st (0)
		fadd     round_toward_m_i
		fistp    i
	}
	i >>= 1;
	
	return (i);*/
}

inline s64 ceil_int_d(long double x)
{
	return (s64)x;
/*	s64      i;
	static const double round_toward_p_i = -0.5;
	__asm
	{
		fld      x
		fadd     st, st (0)
		fsubr    round_toward_p_i
		fistp    i
	}
	i >>= 1;
	
	return (-i);*/
}



/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////



void R4300_Unk(DWORD dwOp)     { WARN_NOEXIST("R4300_Unk"); }
void R4300_Special(DWORD dwOp) { R4300SpecialInstruction[funct(dwOp)](dwOp); }
void R4300_RegImm(DWORD dwOp)  { R4300RegImmInstruction[rt(dwOp)](dwOp);     }
void R4300_CoPro0(DWORD dwOp)  { R4300Cop0Instruction[cop0fmt(dwOp)](dwOp);  }
void R4300_CoPro1(DWORD dwOp)  { R4300Cop1Instruction[cop0fmt(dwOp)](dwOp);  }

void R4300_CoPro1_Disabled(DWORD dwOp)
{
	// Cop1 Unusable
	DBGConsole_Msg(0, "Thread accessing Cop1 for the first time");
	R4300_SetCop1Enable(TRUE);

	OS_HLE_EnableThreadFP();

	R4300Cop1Instruction[cop0fmt(dwOp)](dwOp);

}


// These are the only unimplemented R4300 instructions now:
void R4300_LL(DWORD dwOp) {  WARN_NOIMPL("LL"); }
void R4300_LLD(DWORD dwOp) {  WARN_NOIMPL("LLD"); }

void R4300_SC(DWORD dwOp) {  WARN_NOIMPL("SC"); }
void R4300_SCD(DWORD dwOp) {  WARN_NOIMPL("SCD"); }


void R4300_LDC2(DWORD dwOp) {  WARN_NOIMPL("LDC2"); }
void R4300_SDC2(DWORD dwOp) {  WARN_NOIMPL("SDC2"); }

void R4300_RegImm_TGEI(DWORD dwOp) {  WARN_NOIMPL("TGEI"); }
void R4300_RegImm_TGEIU(DWORD dwOp) {  WARN_NOIMPL("TGEIU"); }
void R4300_RegImm_TLTI(DWORD dwOp) {  WARN_NOIMPL("TLTI"); }
void R4300_RegImm_TLTIU(DWORD dwOp) {  WARN_NOIMPL("TLTIU"); }
void R4300_RegImm_TEQI(DWORD dwOp) {  WARN_NOIMPL("TEQI"); }
void R4300_RegImm_TNEI(DWORD dwOp) {  WARN_NOIMPL("TNEI"); }

void R4300_RegImm_BLTZALL(DWORD dwOp) {  WARN_NOIMPL("BLTZALL"); }
void R4300_RegImm_BGEZALL(DWORD dwOp) {  WARN_NOIMPL("BGEZALL"); }

void R4300_Special_TGE(DWORD dwOp) {  WARN_NOIMPL("TGE"); }
void R4300_Special_TGEU(DWORD dwOp) {  WARN_NOIMPL("TGEU"); }
void R4300_Special_TLT(DWORD dwOp) {  WARN_NOIMPL("TLT"); }
void R4300_Special_TLTU(DWORD dwOp) {  WARN_NOIMPL("TLTU"); }
void R4300_Special_TEQ(DWORD dwOp) {  WARN_NOIMPL("TEQ"); }
void R4300_Special_TNE(DWORD dwOp) {  WARN_NOIMPL("TNE"); }


void R4300_DBG_Bkpt(DWORD dwOp)
{
	// Entry is in lower 26 bits...
	DWORD dwBreakPoint = dwOp & 0x03FFFFFF;

	if (g_BreakPoints[dwBreakPoint].bEnabled &&
		!g_BreakPoints[dwBreakPoint].bTemporaryDisable)
	{
		// Set the temporary disable so we don't execute bp immediately again
		g_BreakPoints[dwBreakPoint].bTemporaryDisable = TRUE;
		CPU_Halt("BreakPoint");
		DBGConsole_Msg(0, "[RBreakPoint at 0x%08x]", g_dwPC);

		// Decrement, so we move onto this instruction next
		DECREMENT_PC();
	}
	else
	{
		// If this was on, disable it
		g_BreakPoints[dwBreakPoint].bTemporaryDisable = FALSE;

		dwOp = g_BreakPoints[dwBreakPoint].dwOriginalOp;
		R4300Instruction[op(dwOp)](dwOp);


	}
	
}
void R4300_SRHack_UnOpt(DWORD dwOp)
{
	// Entry is in lower 26 bits...
	DWORD dwEntry = dwOp & 0x03FFFFFF;

	CDynarecCode * pCode = g_pDynarecCodeTable[dwEntry];
	
	if (g_nDelay == NO_DELAY)
	{
		SR_ExecuteCode(pCode);
	}
	else
	{
		dwOp = pCode->dwOriginalOp;
		R4300Instruction[op(dwOp)](dwOp);
	}

}

void R4300_SRHack_Opt(DWORD dwOp)
{
	// Entry is in lower 26 bits...
	DWORD dwEntry = dwOp & 0x03FFFFFF;

	CDynarecCode * pCode = g_pDynarecCodeTable[dwEntry];
	
	if (g_nDelay == NO_DELAY)
	{
		SR_ExecuteCode_AlreadyOptimised(pCode);
	}
	else
	{
		dwOp = pCode->dwOriginalOp;
		R4300Instruction[op(dwOp)](dwOp);
	}

}


// Couldn't compile this code....
void R4300_SRHack_NoOpt(DWORD dwOp)
{
	// Entry is in lower 26 bits...
	DWORD dwEntry = dwOp & 0x03FFFFFF;

	CDynarecCode * pCode = g_pDynarecCodeTable[dwEntry];

	dwOp = pCode->dwOriginalOp;

	R4300Instruction[op(dwOp)](dwOp);

}

extern DWORD g_dwHackedReplacedOp;

void R4300_Patch(DWORD dwOp)
{
	// Read patch info from g_PatchSymbols[]
	DWORD dwPatchNum = dwOp & 0x00FFFFFF;

	DWORD dwRet = g_PatchSymbols[dwPatchNum]->pFunction();
	if (dwRet == PATCH_RET_JR_RA)
	{
		// TODO - We could call SR_CompileCode here to compile the subsequnt code
		CPU_SetPC((u32)g_qwGPR[REG_ra]);
		DECREMENT_PC();		// We increment PC after this call..
		g_nDelay = NO_DELAY;

	}
	else if (dwRet == PATCH_RET_ERET)
	{
		// Bit of a hack for now - we should have a seperate function...
		R4300_TLB_ERET(dwOp);	
	}
	else
	{
		DWORD dwOp = g_PatchSymbols[dwPatchNum]->dwReplacedOp;
		R4300Instruction[op(dwOp)](dwOp);
	}


}

void R4300_J(DWORD dwOp) 				// Jump
{
	g_dwNewPC = (g_dwPC & 0xF0000000) | (target(dwOp)<<2);

	SpeedHack(g_dwNewPC == g_dwPC, dwOp);
	CPU_Delay();

}

void R4300_JAL(DWORD dwOp) 				// Jump And Link
{
	g_qwGPR[REG_ra] = (s64)(s32)(g_dwPC + 8);		// Store return address
	g_dwNewPC = (g_dwPC & 0xF0000000) | (target(dwOp)<<2);


	CPU_Delay();
	
}

void R4300_BEQ(DWORD dwOp) 		// Branch on Equal
{
	//branch if rs == rt
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	if ((u32)g_qwGPR[dwRS] == (u32)g_qwGPR[dwRT])
	{
		s16 wOffset = (s16)imm(dwOp);

		SpeedHack(wOffset == -1, dwOp);
			
		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();
	}
	/*else
	{
		if (g_qwGPR[dwRS] == g_qwGPR[dwRT])
			DBGConsole_Msg(0, "BEQ Missed");
	}*/
}
extern DWORD dwLastCount;
void R4300_BNE(DWORD dwOp)             // Branch on Not Equal
{
	//branch if rs <> rt
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	if ((u32)g_qwGPR[dwRS] != (u32)g_qwGPR[dwRT])
	{
		s16 wOffset = (s16)imm(dwOp);

		SpeedHack(wOffset == -1, dwOp);

		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();
	} 
	/*else
	{
		if (g_qwGPR[dwRS] != g_qwGPR[dwRT])
			DBGConsole_Msg(0, "BNE Missed");
	}*/

}

void R4300_BLEZ(DWORD dwOp) 			// Branch on Less than of Equal to Zero
{
	//branch if rs <= 0
	DWORD dwRS = rs(dwOp);

	//if ((s64)g_qwGPR[dwRS] <= 0) {
	if ((s32)g_qwGPR[dwRS] <= 0)
	{
		s16 wOffset = (s16)imm(dwOp);

		SpeedHack(wOffset == -1, dwOp);

		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();

	} 
	/*else
	{
		if ((s64)g_qwGPR[dwRS] <= 0)
			DBGConsole_Msg(0, "BLEZ Missed");
	}*/
}

void R4300_BGTZ(DWORD dwOp) 			// Branch on Greater than Zero
{
	//branch if rs > 0
	DWORD dwRS = rs(dwOp);

	//if ((s64)g_qwGPR[dwRS] > 0)
	if ((s32)g_qwGPR[dwRS] > 0)
	{
		s16 wOffset = (s16)imm(dwOp);

		SpeedHack(wOffset == -1, dwOp);

		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();

	} 
	/*else
	{
		if ((s64)g_qwGPR[dwRS] > 0)
			DBGConsole_Msg(0, "BGTZ Missed");
	}*/
}


void R4300_DADDI(DWORD dwOp) 			// Doubleword ADD Immediate
{	
	//rt = rs + immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	s16 wData = (s16)imm(dwOp);

	// Check for overflow
	// Reserved Instruction exception
	g_qwGPR[dwRT] = (s64)g_qwGPR[dwRS] + (s32)wData;
	CHECK_R0();

}

void R4300_DADDIU(DWORD dwOp) 			// Doubleword ADD Immediate Unsigned
{	
	//rt = rs + immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	s16 wData = (s16)imm(dwOp);
		
	// Reserved Instruction exception
	g_qwGPR[dwRT] = (s64)g_qwGPR[dwRS] + (s32)wData;
	CHECK_R0();
}


void R4300_ADDI(DWORD dwOp) 
{
	//rt = rs + immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	s16 wData = (s16)imm(dwOp);

	// Generates overflow exception
	g_qwGPR[dwRT] = (s64)(s32)((s32)g_qwGPR[dwRS] + (s32)wData);

	CHECK_R0();
}

void R4300_ADDIU(DWORD dwOp) 		// Add Immediate Unsigned
{
	//rt = rs + immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	s16 wData = (s16)imm(dwOp);

	g_qwGPR[dwRT] = (s64)(s32)((s32)g_qwGPR[dwRS] + (s32)wData);
	CHECK_R0();

}

void R4300_SLTI(DWORD dwOp) 			// Set on Less Than Immediate
{	
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	s16 wData = (s16)imm(dwOp);

	// Cast to s32s to ensure sign is taken into account
	if ((s64)g_qwGPR[dwRS] < (s64)(s32)wData)
	{
		g_qwGPR[dwRT] = 1;
		CHECK_R0();
	}
	else
	{
		g_qwGPR[dwRT] = 0;
	}
}

void R4300_SLTIU(DWORD dwOp) 		// Set on Less Than Immediate Unsigned 	
{	
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	s16 wData = (s16)imm(dwOp);

	// Cast to s32s to ensure sign is taken into account
	if ((u64)g_qwGPR[dwRS] < (u64)(s64)(s32)wData)
	{
		g_qwGPR[dwRT] = 1;
		CHECK_R0();
	}
	else
	{
		g_qwGPR[dwRT] = 0;
	}
}


void R4300_ANDI(DWORD dwOp) 				// AND Immediate
{
	//rt = rs & immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	u16 wData = (u16)imm(dwOp);

	g_qwGPR[dwRT] = g_qwGPR[dwRS] & (u64)wData;
	CHECK_R0();
}


void R4300_ORI(DWORD dwOp) 				// OR Immediate
{				
	//rt = rs | immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	u16 wData = (u16)imm(dwOp);

	g_qwGPR[dwRT] = g_qwGPR[dwRS] | (u64)wData;
	CHECK_R0();
}

void R4300_XORI(DWORD dwOp) 				// XOR Immediate
{
	//rt = rs ^ immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	u16 wData = (u16)imm(dwOp);

	g_qwGPR[dwRT] = g_qwGPR[dwRS] ^ (u64)wData;
	CHECK_R0();
}

void R4300_LUI(DWORD dwOp) 				// Load Upper Immediate
{
	DWORD dwRT = rt(dwOp);
	s32 nData = (s32)(s16)imm(dwOp);

	g_qwGPR[dwRT] = (s64)(s32)(nData<<16);
	CHECK_R0();
}

void R4300_BEQL(DWORD dwOp) 			// Branch on Equal Likely
{
	//branch if rs == rt
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	if (g_qwGPR[dwRS] == g_qwGPR[dwRT])
	{
		s16 wOffset = (s16)imm(dwOp);

		SpeedHack(wOffset == -1, dwOp);

		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();
	}
	else
	{
		// Don't execute subsequent instruction
		INCREMENT_PC();
	}
}

void R4300_BNEL(DWORD dwOp) 			// Branch on Not Equal Likely
{
	//branch if rs <> rt
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	if (g_qwGPR[dwRS] != g_qwGPR[dwRT])
	{
		s16 wOffset = (s16)imm(dwOp);

		SpeedHack(wOffset == -1, dwOp);

		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();	
	}
	else
	{
		// Don't execute subsequent instruction
		INCREMENT_PC();
	}
}

void R4300_BLEZL(DWORD dwOp) 		// Branch on Less than or Equal to Zero Likely
{
	//branch if rs <= 0
	DWORD dwRS = rs(dwOp);

	if ((s64)g_qwGPR[dwRS] <= 0)
	{
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();

	}
	else
	{
		// Don't execute subsequent instruction
		INCREMENT_PC();
	}
}

void R4300_BGTZL(DWORD dwOp) 		// Branch on Greater than Zero Likely
{
	//branch if rs > 0
	DWORD dwRS = rs(dwOp);

	if ((s64)g_qwGPR[dwRS] > 0)
	{
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();
	}
	else
	{
		// Don't execute subsequent instruction
		INCREMENT_PC();
	}
}





void R4300_LB(DWORD dwOp) 			// Load Byte
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);

	g_qwGPR[dwRT] = (s64)(s8)Read8Bits(dwAddress);
	CHECK_R0();
}

void R4300_LBU(DWORD dwOp) 			// Load Byte Unsigned -- Zero extend byte...
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
	
	g_qwGPR[dwRT] = (s64)(u8)Read8Bits(dwAddress);
	CHECK_R0();
}


void R4300_LH(DWORD dwOp) 		// Load Halfword
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);
	
	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
	g_qwGPR[dwRT] = (s64)(s16)Read16Bits(dwAddress);
	CHECK_R0();
}

void R4300_LHU(DWORD dwOp)			// Load Halfword Unsigned -- Zero extend word
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);

	g_qwGPR[dwRT] = (u64)(u16)Read16Bits(dwAddress);
	CHECK_R0();
}


void R4300_LWL(DWORD dwOp) 			// Load Word Left
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset); 
	u32 nMemory = Read32Bits(dwAddress & ~0x3);

	u32 nReg = (u32)g_qwGPR[dwRT];

	switch (dwAddress % 4)
	{
        case 0: nReg = nMemory; break;
        case 1: nReg = ((nReg & 0x000000FF) | (nMemory << 8));  break;
        case 2: nReg = ((nReg & 0x0000FFFF) | (nMemory << 16)); break;
        case 3: nReg = ((nReg & 0x00FFFFFF) | (nMemory << 24)); break;
    }

	g_qwGPR[dwRT] = (s64)(s32)nReg;
	CHECK_R0();
}

// Starcraft - not tested!
void R4300_LDL(DWORD dwOp)
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset); 
	u64 nMemory = Read64Bits(dwAddress & ~0x7);		

	u64 nReg = g_qwGPR[dwRT];

	switch (dwAddress % 8)
	{
        case 0: nReg = nMemory; break;
        case 1: nReg = ((nReg & 0x00000000000000FFLL) | (nMemory << 8));  break;
        case 2: nReg = ((nReg & 0x000000000000FFFFLL) | (nMemory << 16)); break;
        case 3: nReg = ((nReg & 0x0000000000FFFFFFLL) | (nMemory << 24)); break;
        case 4: nReg = ((nReg & 0x00000000FFFFFFFFLL) | (nMemory << 32)); break;
        case 5: nReg = ((nReg & 0x000000FFFFFFFFFFLL) | (nMemory << 40)); break;
        case 6: nReg = ((nReg & 0x0000FFFFFFFFFFFFLL) | (nMemory << 48)); break;
        case 7: nReg = ((nReg & 0x00FFFFFFFFFFFFFFLL) | (nMemory << 56)); break;
   }

	g_qwGPR[dwRT] = nReg;
	CHECK_R0();
}


void R4300_LWR(DWORD dwOp) 			// Load Word Right
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset); 
	u32 nMemory = Read32Bits(dwAddress & ~0x3);
	
	u32 nReg = (u32)g_qwGPR[dwRT];

	
	switch (dwAddress % 4)
	{
        case 0: nReg = (nReg & 0xFFFFFF00) | (nMemory >> 24); break;
        case 1: nReg = (nReg & 0xFFFF0000) | (nMemory >> 16); break;
        case 2: nReg = (nReg & 0xFF000000) | (nMemory >>  8); break;
        case 3: nReg = nMemory; break;
    }

	g_qwGPR[dwRT] = (s64)(s32)nReg;
	CHECK_R0();
}

// Starcraft - not tested!
void R4300_LDR(DWORD dwOp)
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset); 
	u64 nMemory = Read64Bits(dwAddress & ~0x7);
	
	u64 nReg = g_qwGPR[dwRT];

	
	switch (dwAddress % 8)
	{
        case 0: nReg = (nReg & 0xFFFFFFFFFFFFFF00LL) | (nMemory >> 56); break;
        case 1: nReg = (nReg & 0xFFFFFFFFFFFF0000LL) | (nMemory >> 48); break;
        case 2: nReg = (nReg & 0xFFFFFFFFFF000000LL) | (nMemory >> 40); break;
        case 3: nReg = (nReg & 0xFFFFFFFF00000000LL) | (nMemory >> 32); break;
        case 4: nReg = (nReg & 0xFFFFFF0000000000LL) | (nMemory >> 24); break;
        case 5: nReg = (nReg & 0xFFFF000000000000LL) | (nMemory >> 16); break;
        case 6: nReg = (nReg & 0xFF00000000000000LL) | (nMemory >>  8); break;
        case 7: nReg = nMemory; break;
    }

	g_qwGPR[dwRT] = nReg;
	CHECK_R0();
}


void R4300_LW(DWORD dwOp) 			// Load Word
{
	const DWORD dwBase = rs(dwOp);
	const s16 wOffset = (s16)imm(dwOp);
	const DWORD dwRT  = rt(dwOp);

	const DWORD	dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
	g_qwGPR[dwRT] = (s64)(s32)Read32Bits(dwAddress);


	CHECK_R0();
}



void R4300_LWU(DWORD dwOp) 			// Load Word Unsigned
{
	const DWORD dwBase = rs(dwOp);
	const s16 wOffset = (s16)imm(dwOp);
	const DWORD dwRT  = rt(dwOp);

	const DWORD	dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
	g_qwGPR[dwRT] = (u64)(u32)Read32Bits(dwAddress);


	CHECK_R0();

}

void R4300_SW(DWORD dwOp) 			// Store Word
{
	const DWORD dwBase  = rs(dwOp);
	const s16 wOffset = (s16)imm(dwOp);
	const DWORD dwRT    = rt(dwOp);

	const DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);

	Write32Bits(dwAddress, (u32)g_qwGPR[dwRT]);
}

void R4300_SH(DWORD dwOp) 			// Store Halfword
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
	Write16Bits(dwAddress, (u16)g_qwGPR[dwRT]);
}

void R4300_SB(DWORD dwOp) 			// Store Byte
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
	Write8Bits(dwAddress, (u8)g_qwGPR[dwRT]);
}



void R4300_SWL(DWORD dwOp) 			// Store Word Left
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
  
	DWORD dwMemory = Read32Bits(dwAddress & ~0x3);
	DWORD dwReg = (u32)g_qwGPR[dwRT];
	DWORD dwNew = 0;

	switch(dwAddress % 4)
	{
	case 0:	dwNew = dwReg; break;			// Aligned
	case 1:	dwNew = (dwMemory & 0xFF000000) | (dwReg >> 8 ); break;
	case 2:	dwNew = (dwMemory & 0xFFFF0000) | (dwReg >> 16); break;
	case 3:	dwNew = (dwMemory & 0xFFFFFF00) | (dwReg >> 24); break;
	}
	Write32Bits(dwAddress & ~0x3, dwNew);
}


void R4300_SWR(DWORD dwOp) 			// Store Word Right 
{	
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
  
	DWORD dwMemory = Read32Bits(dwAddress & ~0x3);
	DWORD dwReg = (u32)g_qwGPR[dwRT];
	DWORD dwNew = 0;

	switch(dwAddress % 4)
	{
	case 0:	dwNew = (dwMemory & 0x00FFFFFF) | (dwReg << 24); break;
	case 1:	dwNew = (dwMemory & 0x0000FFFF) | (dwReg << 16); break;
	case 2:	dwNew = (dwMemory & 0x000000FF) | (dwReg << 8); break;
	case 3:	dwNew = dwReg; break;			// Aligned
	}
	Write32Bits(dwAddress & ~0x3, dwNew);

}


void R4300_SDL(DWORD dwOp)//CYRUS64
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);
	
	DWORD dwAddress = (u64)((s64)g_qwGPR[dwBase] + (s64)wOffset);
	
	u64 nMemory = Read64Bits(dwAddress & ~0x7);
	u64 nReg = g_qwGPR[dwRT];
	u64 nNew = 0;
	
	switch(dwAddress % 8)
	{
	case 0:	nNew = nReg; break;			// Aligned
	case 1:	nNew = (nMemory & 0xFF00000000000000LL) | (nReg >> 8); break;
	case 2:	nNew = (nMemory & 0xFFFF000000000000LL) | (nReg >> 16); break;
	case 3:	nNew = (nMemory & 0xFFFFFF0000000000LL) | (nReg >> 24); break;
	case 4:	nNew = (nMemory & 0xFFFFFFFF00000000LL) | (nReg >> 32); break;
	case 5:	nNew = (nMemory & 0xFFFFFFFFFF000000LL) | (nReg >> 40); break;
	case 6:	nNew = (nMemory & 0xFFFFFFFFFFFF0000LL) | (nReg >> 48); break;
	default:nNew = (nMemory & 0xFFFFFFFFFFFFFF00LL) | (nReg >> 56); break;
	}
	Write64Bits(dwAddress & ~0x7, nNew);
}

void R4300_SDR(DWORD dwOp)//CYRUS64
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);
	
	DWORD dwAddress = (u64)((s64)g_qwGPR[dwBase] + (s64)wOffset);
	
	u64 nMemory = Read64Bits(dwAddress & ~0x7);
	u64 nReg = g_qwGPR[dwRT];
	u64 nNew = 0;
	
	//DBGConsole_Msg(0,"Address:		0x%08x", dwAddress );
	
	switch(dwAddress % 4)
	{
	case 0:	nNew = (nMemory & 0x00FFFFFFFFFFFFFFLL) | (nReg << 56); break;
	case 1:	nNew = (nMemory & 0x0000FFFFFFFFFFFFLL) | (nReg << 48); break;
	case 2:	nNew = (nMemory & 0x000000FFFFFFFFFFLL) | (nReg << 40); break;
	case 3:	nNew = (nMemory & 0x00000000FFFFFFFFLL) | (nReg << 32); break;
	case 4:	nNew = (nMemory & 0x0000000000FFFFFFLL) | (nReg << 24); break;
	case 5:	nNew = (nMemory & 0x000000000000FFFFLL) | (nReg << 16); break;
	case 6:	nNew = (nMemory & 0x00000000000000FFLL) | (nReg << 8); break;
	default:nNew = nReg; break;			// Aligned
	}
	Write64Bits(dwAddress & ~0x7, nNew);
	
	
}



void R4300_CACHE(DWORD dwOp)
{
	return;

	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwCacheOp  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);

	// Do Nothing
	DWORD dwCache = dwCacheOp & 0x3;
	DWORD dwAction = (dwCacheOp >> 2) & 0x7;

	static const char * szCaches[] =
	{
		"I", "D", "SI", "SD"
	};	

	DBGConsole_Msg(0, "CACHE %s/%d, 0x%08x", szCaches[dwCache], dwAction, dwAddress);
}

void R4300_LWC1(DWORD dwOp) 				// Load Word to Copro 1 (FPU)
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwFT = ft(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);

	//g_qwCPR[1][dwFT] = (s64)(s32)Read32Bits(dwAddress);
	StoreFPR_Word(dwFT, Read32Bits(dwAddress));
}


void R4300_LDC1(DWORD dwOp)				// Load Doubleword to Copro 1 (FPU)
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwFT = ft(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
	StoreFPR_Long(dwFT, Read64Bits(dwAddress));
}


void R4300_LD(DWORD dwOp) 				// Load Doubleword
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
	g_qwGPR[dwRT] = Read64Bits(dwAddress);
	CHECK_R0();
}


void R4300_SWC1(DWORD dwOp) 			// Store Word From Copro 1
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwFT = ft(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
	//Write32Bits(dwAddress, (u32)g_qwCPR[1][dwFT]);
	Write32Bits(dwAddress, LoadFPR_Word(dwFT));
}

void R4300_SDC1(DWORD dwOp)		// Store Doubleword From Copro 1
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwFT = ft(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);

	Write64Bits(dwAddress, LoadFPR_Long(dwFT));
}


void R4300_SD(DWORD dwOp)			// Store Doubleword
{
	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
	Write64Bits(dwAddress, g_qwGPR[dwRT]);
}



/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

void R4300_Special_Unk(DWORD dwOp) { WARN_NOEXIST("R4300_Special_Unk"); }
void R4300_Special_SLL(DWORD dwOp) 		// Shift word Left Logical
{
	// NOP!
	if (dwOp == 0) return;

	DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = (s64)(s32)((((u32)g_qwGPR[dwRT]) << dwSA) & 0xFFFFFFFF);
	CHECK_R0();
}

void R4300_Special_SRL(DWORD dwOp) 		// Shift word Right Logical
{
	DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = (s64)(s32)(((u32)g_qwGPR[dwRT]) >> dwSA);
	CHECK_R0();
}

void R4300_Special_SRA(DWORD dwOp) 		// Shift word Right Arithmetic
{	
	DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = (s64)(s32)(((s32)g_qwGPR[dwRT]) >> dwSA);
	CHECK_R0();
}

void R4300_Special_SLLV(DWORD dwOp) 		// Shift word Left Logical Variable
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = (s64)(s32)(((u32)(g_qwGPR[dwRT]) << (g_qwGPR[dwRS]&0x1F)) & 0xFFFFFFFF);
	CHECK_R0();
}

void R4300_Special_SRLV(DWORD dwOp) 		// Shift word Right Logical Variable
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = (s64)(s32)((u32)g_qwGPR[dwRT] >> (g_qwGPR[dwRS]&0x1F));
	CHECK_R0();
}

void R4300_Special_SRAV(DWORD dwOp) 		// Shift word Right Arithmetic Variable
{
	//DWORD dwSA = sa(dwOp);	// Should be == 0
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = (s64)(s32)((s32)g_qwGPR[dwRT] >> (g_qwGPR[dwRS]&0x1F));
	CHECK_R0();
}



void R4300_Special_JR_CheckBootAddress(DWORD dwOp) 			// Jump Register
{	
	DWORD dwRS = rs(dwOp);
	g_dwNewPC = (u32)g_qwGPR[dwRS];

	// This is a hack to detect when the rom's boot code
	// has finished with the CRC check, and is *just* about to 
	// start executing the game code. We apply patches here,
	// otherwise the CRC would fail (which is bad :-)
	if (g_dwNewPC == g_ROM.u32BootAddres)
	{
		Write32Bits(0x80000318, g_dwRamSize);
		DBGConsole_Msg(0, "[RJumping to Game Boot Address]");
		Patch_ApplyPatches();

		// Restore normal op handling - this reduces the 
		// overhead of this check for the rest of the
		// execution of the rom.
		R4300SpecialInstruction[OP_JR] = R4300_Special_JR;
	}

	CPU_Delay();
	
}

void R4300_Special_JR(DWORD dwOp) 			// Jump Register
{	
	DWORD dwRS = rs(dwOp);
	g_dwNewPC = (u32)g_qwGPR[dwRS];

	CPU_Delay();
	
}


void R4300_Special_JALR(DWORD dwOp) 		// Jump and Link register
{	
	// Jump And Link
	DWORD dwRS = rs(dwOp);
	DWORD dwRD = rd(dwOp);

	g_dwNewPC = (u32)g_qwGPR[dwRS];
	g_qwGPR[dwRD] = (s64)(s32)(g_dwPC + 8);		// Store return address
	CHECK_R0();
	
	CPU_Delay();

}


void R4300_Special_SYSCALL(DWORD dwOp)
{
	AddCPUJob(CPU_SYSCALL);
}

void R4300_Special_BREAK(DWORD dwOp) 	// BREAK
{
	DPF(DEBUG_BREAK, "BREAK Called. PC: 0x%08x. COUNT: 0x%08x", g_dwPC, g_qwCPR[0][C0_COUNT]);

	AddCPUJob(CPU_BREAKPOINT);
}

void R4300_Special_SYNC(DWORD dwOp)
{
	// Just ignore
}

void R4300_Special_MFHI(DWORD dwOp) 			// Move From MultHI
{
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = g_qwMultHI;
	CHECK_R0();
}

void R4300_Special_MTHI(DWORD dwOp) 			// Move To MultHI
{
	DWORD dwRS = rs(dwOp);

	g_qwMultHI = g_qwGPR[dwRS];
}

void R4300_Special_MFLO(DWORD dwOp) 			// Move From MultLO
{
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = g_qwMultLO;
	CHECK_R0();

}

void R4300_Special_MTLO(DWORD dwOp) 			// Move To MultLO
{
	DWORD dwRS = rs(dwOp);

	g_qwMultLO = g_qwGPR[dwRS];
}

void R4300_Special_DSLLV(DWORD dwOp) 
{
	// Used by DKR
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// Reserved Instruction exception
	g_qwGPR[dwRD] = (u64)g_qwGPR[dwRT] << (g_qwGPR[dwRS]&0x1F);
	CHECK_R0();
}

void R4300_Special_DSRLV(DWORD dwOp)
{

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// Reserved Instruction exception
	g_qwGPR[dwRD] = (u64)g_qwGPR[dwRT] >> (g_qwGPR[dwRS]&0x1F);
	CHECK_R0();
}

// Aeroguage uses!
void R4300_Special_DSRAV(DWORD dwOp)
{

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// Reserved Instruction exception
	g_qwGPR[dwRD] = (s64)g_qwGPR[dwRT] >> (g_qwGPR[dwRS]&0x1F);
	CHECK_R0();

}



void R4300_Special_MULT(DWORD dwOp) 			// MULTiply Signed
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	s64 dwResult = ((s64)(s32)g_qwGPR[dwRS]) * ((s64)(s32)g_qwGPR[dwRT]);
	g_qwMultLO = (s64)(s32)(dwResult);
	g_qwMultHI = (s64)(s32)(dwResult >> 32);

}

void R4300_Special_MULTU(DWORD dwOp) 		// MULTiply Unsigned
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	u64 dwResult = ((u64)(u32)g_qwGPR[dwRS]) * ((u64)(u32)g_qwGPR[dwRT]);
	g_qwMultLO = (s64)(s32)(dwResult);
	g_qwMultHI = (s64)(s32)(dwResult >> 32);
}

void R4300_Special_DIV(DWORD dwOp) 			//DIVide
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	s32 nDividend = (s32)g_qwGPR[dwRS];
	s32 nDivisor  = (s32)g_qwGPR[dwRT];

	if (nDivisor)
	{
		g_qwMultLO = (s64)(s32)(nDividend / nDivisor);
		g_qwMultHI = (s64)(s32)(nDividend % nDivisor);
	}
}

void R4300_Special_DIVU(DWORD dwOp) 			// DIVide Unsigned
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	DWORD dwDividend = (u32)g_qwGPR[dwRS];
	DWORD dwDivisor  = (u32)g_qwGPR[dwRT];

	if (dwDivisor) {
		g_qwMultLO = (s64)(s32)(dwDividend / dwDivisor);
		g_qwMultHI = (s64)(s32)(dwDividend % dwDivisor);
	}
}

void R4300_Special_DMULT(DWORD dwOp) 		// Double Multiply
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	// Reserved Instruction exception
	g_qwMultLO = (s64)g_qwGPR[dwRS] * (s64)g_qwGPR[dwRT];
	g_qwMultHI = 0;
}

void R4300_Special_DMULTU(DWORD dwOp) 			// Double Multiply Unsigned
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	// Reserved Instruction exception
	g_qwMultLO = (u64)g_qwGPR[dwRS] * (u64)g_qwGPR[dwRT];
	g_qwMultHI = 0;
}

void R4300_Special_DDIV(DWORD dwOp) 				// Double Divide
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	s64 qwDividend = g_qwGPR[dwRS];
	s64 qwDivisor = g_qwGPR[dwRT];

	// Reserved Instruction exception
	if (qwDivisor)
	{
		g_qwMultLO = qwDividend / qwDivisor;
		g_qwMultHI = qwDividend % qwDivisor;
	}
}

void R4300_Special_DDIVU(DWORD dwOp) 			// Double Divide Unsigned
{	
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	u64 qwDividend = g_qwGPR[dwRS];
	u64 qwDivisor = g_qwGPR[dwRT];

	// Reserved Instruction exception
	if (qwDivisor)
	{
		g_qwMultLO = qwDividend / qwDivisor;
		g_qwMultHI = qwDividend % qwDivisor;
	}
}

void R4300_Special_ADD(DWORD dwOp) 			// ADD signed - may throw exception
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// Can generate overflow exception
	g_qwGPR[dwRD] = (s64)(s32)((s32)g_qwGPR[dwRS] + (s32)g_qwGPR[dwRT]);
	CHECK_R0();
}

void R4300_Special_ADDU(DWORD dwOp) 			// ADD Unsigned - doesn't throw exception
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = (s64)(s32)((s32)g_qwGPR[dwRS] + (s32)g_qwGPR[dwRT]);
	CHECK_R0();
}

void R4300_Special_SUB(DWORD dwOp) 			// SUB Signed - may throw exception
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// Can generate overflow exception
	g_qwGPR[dwRD] = (s64)(s32)((s32)g_qwGPR[dwRS]-(s32)g_qwGPR[dwRT]);
	CHECK_R0();
}


void R4300_Special_SUBU(DWORD dwOp) 			// SUB Unsigned - doesn't throw exception
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = (s64)((s32)g_qwGPR[dwRS] - (s32)g_qwGPR[dwRT]);
	CHECK_R0();
}


void R4300_Special_AND(DWORD dwOp) 				// logical AND
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = g_qwGPR[dwRS] & g_qwGPR[dwRT];
	CHECK_R0();

}
void R4300_Special_OR(DWORD dwOp) 				// logical OR
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = g_qwGPR[dwRS] | g_qwGPR[dwRT];
	CHECK_R0();
}

void R4300_Special_XOR(DWORD dwOp) 				// logical XOR
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = g_qwGPR[dwRS] ^ g_qwGPR[dwRT];
	CHECK_R0();
}

void R4300_Special_NOR(DWORD dwOp) 				// logical Not OR
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_qwGPR[dwRD] = ~(g_qwGPR[dwRS] | g_qwGPR[dwRT]);
	CHECK_R0();

}

void R4300_Special_SLT(DWORD dwOp) 				// Set on Less Than
{	
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// Cast to s32s to ensure sign is taken into account
	if ((s64)g_qwGPR[dwRS] < (s64)g_qwGPR[dwRT])
	{
		g_qwGPR[dwRD] = 1;
		CHECK_R0();
	}
	else
	{
		g_qwGPR[dwRD] = 0;
	}
}

void R4300_Special_SLTU(DWORD dwOp) 				// Set on Less Than Unsigned
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// Treated as unsigned....
	if ((u64)g_qwGPR[dwRS] < (u64)g_qwGPR[dwRT])
	{
		g_qwGPR[dwRD] = 1;
		CHECK_R0();
	}
	else
	{
		g_qwGPR[dwRD] = 0;
	}

}


void R4300_Special_DADD(DWORD dwOp)//CYRUS64 
{  
	DWORD dwRT=rt(dwOp);
	DWORD dwRD=rd(dwOp);
	DWORD dwRS=rs(dwOp);
	g_qwGPR[dwRD] = g_qwGPR[dwRT] + g_qwGPR[dwRS];
	
	CHECK_R0();
}

void R4300_Special_DADDU(DWORD dwOp)//CYRUS64
{
	DWORD dwRT=rt(dwOp);
	DWORD dwRD=rd(dwOp);
	DWORD dwRS=rs(dwOp);
	g_qwGPR[dwRD] = g_qwGPR[dwRT] + g_qwGPR[dwRS];
	
	CHECK_R0();
}



void R4300_Special_DSUB(DWORD dwOp)
{
	DWORD dwRT=rt(dwOp);
	DWORD dwRD=rd(dwOp);
	DWORD dwRS=rs(dwOp);
	g_qwGPR[dwRD] = g_qwGPR[dwRT] - g_qwGPR[dwRS];
	
	CHECK_R0();
}
void R4300_Special_DSUBU(DWORD dwOp)
{
	DWORD dwRT=rt(dwOp);
	DWORD dwRD=rd(dwOp);
	DWORD dwRS=rs(dwOp);
	g_qwGPR[dwRD] = g_qwGPR[dwRT] - g_qwGPR[dwRS];
	
	CHECK_R0();
}

void R4300_Special_DSLL(DWORD dwOp) 
{
	// Used by Goldeneye
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);
	DWORD dwSA = sa(dwOp);

	// Reserved Instruction exception
	g_qwGPR[dwRD] = g_qwGPR[dwRT] << dwSA;

	CHECK_R0();
}

void R4300_Special_DSRL(DWORD dwOp) 
{
	// Used by Goldeneye
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);
	DWORD dwSA = sa(dwOp);

	// Reserved Instruction exception
    g_qwGPR[dwRD] = (u64)g_qwGPR[dwRT] >> dwSA;

	CHECK_R0();
}

void R4300_Special_DSRA(DWORD dwOp)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);
	DWORD dwSA = sa(dwOp);

	// Reserved Instruction exception
	g_qwGPR[dwRD] = (s64)g_qwGPR[dwRT] >> dwSA;
	CHECK_R0();

}

void R4300_Special_DSLL32(DWORD dwOp) 			// Double Shift Left Logical 32
{	
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);
	DWORD dwSA = sa(dwOp);

	// Reserved Instruction exception
	g_qwGPR[dwRD] = g_qwGPR[dwRT] << (32 + dwSA);
	CHECK_R0();
}

void R4300_Special_DSRL32(DWORD dwOp) 			// Double Shift Right Logical 32
{
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);
	DWORD dwSA = sa(dwOp);

	// Reserved Instruction exception
	g_qwGPR[dwRD] = (u64)g_qwGPR[dwRT] >> (32 + dwSA);
	CHECK_R0();
}

void R4300_Special_DSRA32(DWORD dwOp) 			// Double Shift Right Arithmetic 32
{
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);
	DWORD dwSA = sa(dwOp);

	// Reserved Instruction exception
	g_qwGPR[dwRD] = (s64)g_qwGPR[dwRT] >> (32 + dwSA);
	CHECK_R0();
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////




void R4300_RegImm_Unk(DWORD dwOp) {  WARN_NOEXIST("R4300_RegImm_Unk"); }


void R4300_RegImm_BLTZ(DWORD dwOp) 			// Branch on Less than Zero
{
	//branch if rs < 0
	DWORD dwRS = rs(dwOp);

	if ((s64)g_qwGPR[dwRS] < 0)
	{
		s16 wOffset = (s16)imm(dwOp);

		SpeedHack(wOffset == -1, dwOp);

		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();

	} 
}

void R4300_RegImm_BLTZL(DWORD dwOp) 			// Branch on Less than Zero Likely
{
	//branch if rs < 0
	DWORD dwRS = rs(dwOp);

	if ((s64)g_qwGPR[dwRS] < 0)
	{
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();
	}
	else
	{
		// Don't execute subsequent instruction
		INCREMENT_PC();
	}

}
void R4300_RegImm_BLTZAL(DWORD dwOp) 		// Branch on Less than Zero And Link
{
	//branch if rs >= 0
	DWORD dwRS = rs(dwOp);

	// Apparently this always happens, even if branch not taken
	g_qwGPR[REG_ra] = (s64)(s32)(g_dwPC + 8);		// Store return address	
	if ((s64)g_qwGPR[dwRS] < 0)
	{
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();
	} 
}

void R4300_RegImm_BGEZ(DWORD dwOp) 			// Branch on Greater than or Equal to Zero
{
	//branch if rs >= 0
	DWORD dwRS = rs(dwOp);

	if ((s64)g_qwGPR[dwRS] >= 0)
	{
		s16 wOffset = (s16)imm(dwOp);

		SpeedHack(wOffset == -1, dwOp);

		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();
	} 
}

void R4300_RegImm_BGEZL(DWORD dwOp) 			// Branch on Greater than or Equal to Zero Likely
{
	//branch if rs >= 0
	DWORD dwRS = rs(dwOp);

	if ((s64)g_qwGPR[dwRS] >= 0)
	{
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();
	}
	else
	{
		// Don't execute subsequent instruction
		INCREMENT_PC();
	}
}

void R4300_RegImm_BGEZAL(DWORD dwOp) 		// Branch on Greater than or Equal to Zero And Link
{
	//branch if rs >= 0
	DWORD dwRS = rs(dwOp);

	// This always happens, even if branch not taken
	g_qwGPR[REG_ra] = (s64)(s32)(g_dwPC + 8);		// Store return address	
	if ((s64)g_qwGPR[dwRS] >= 0)
	{
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewPC = g_dwPC + ((s32)wOffset<<2) + 4;
		CPU_Delay();
	} 
}



/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

void R4300_Cop0_Unk(DWORD dwOp) { WARN_NOEXIST("R4300_Cop0_Unk"); }
void R4300_TLB_Unk(DWORD dwOp)  { WARN_NOEXIST("R4300_TLB_Unk"); }

void R4300_Cop0_TLB(DWORD dwOp) {  R4300TLBInstruction[funct(dwOp)](dwOp); }


void R4300_Cop0_MFC0(DWORD dwOp)
{
	// Copy from FS to RT
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	if (dwFS == C0_RAND)
	{
		DWORD dwWired = (u32)g_qwCPR[0][C0_WIRED] & 0x1F;

		// Select a value between dwWired and 31
		// We should use TLB least-recently used here too?
		g_qwGPR[dwRT] = (rand()%(32-dwWired)) + dwWired;
	}
	else
	{
		g_qwGPR[dwRT] = (s64)(s32)g_qwCPR[0][dwFS];
	}

	CHECK_R0();

	DPF(DEBUG_REGS, "MFC0 - Read. Currently %s is\t\t0x%08x", Cop0RegNames[dwFS], (u32)g_qwGPR[dwRT]);
}

// Move Word To CopReg
void R4300_Cop0_MTC0(DWORD dwOp)
{
	// Copy from RT to FS
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	u64 qwNewValue = g_qwGPR[dwRT];

	switch (dwFS) {
		case C0_CONTEXT:
			DBGConsole_Msg(0, "Setting context register to 0x%08x", (DWORD)qwNewValue);
			g_qwCPR[0][dwFS] = qwNewValue;
			break;

		case C0_WIRED:
			// Set to top limit on write to wired
			g_qwCPR[0][C0_RAND] = 32-1;
			DBGConsole_Msg(0, "Setting Wired register to 0x%08x", (DWORD)qwNewValue);
			g_qwCPR[0][dwFS] = qwNewValue;
			break;
		case C0_RAND:
		case C0_BADVADDR:
		case C0_PRID:
		case C0_CACHE_ERR:			// Furthermore, this reg must return 0 on reads.

			// All these registers are read only - make sure that software doesn't write to them
			DPF(DEBUG_REGS, "MTC0. Software attempted to write to read only reg %s: 0x%08x", Cop0RegNames[dwFS], (u32)qwNewValue);
			break;

		case C0_CAUSE:
			// Only IP1:0 (Interrupt Pending) bits are software writeable.
			// On writes, set all others to 0. Is this correct?
			//  Other bits are CE (copro error) BD (branch delay), the other
			// Interrupt pendings and EscCode.
			qwNewValue &= (CAUSE_SW1|CAUSE_SW2);

			DPF(DEBUG_CAUSE, "CAUSE set to 0x%08x (was: 0x%08x)", (u32)qwNewValue, (u32)g_qwGPR[dwRT]);
			g_qwCPR[0][C0_CAUSE] = qwNewValue;
			break;
		case C0_SR:
			// Software can enable/disable interrupts here. We check if Interrupt Enable is
			//  set, and if there are any pending interrupts. If there are, then we set the
			//  CHECK_POSTPONED_INTERRUPTS flag to make sure we check for interrupts that have
			//  occurred since we disabled interrupts

			R4300_SetSR((u32)qwNewValue);
			break;

		case C0_COUNT:
			{
				// See comments below for COMPARE.
				// When this register is set, we need to check whether the next timed interrupt will
				//  be due to vertical blank or COMPARE
				DPF(DEBUG_COUNT, "COUNT set to 0x%08x.", (u32)qwNewValue);
				g_qwCPR[0][C0_COUNT] = qwNewValue;


				DBGConsole_Msg(0, "Count set - setting int");
				// Add an event for this compare:
				/*if ((u32)g_qwCPR[0][C0_COMPARE] > (u32)g_qwCPR[0][C0_COUNT])
				{
					CPU_AddEvent((u32)g_qwCPR[0][C0_COMPARE] - (u32)g_qwCPR[0][C0_COUNT],
								CPU_EVENT_COMPARE);
				}*/

				break;
			}
		case C0_COMPARE:
			{
				// When the value of COUNT equals the value of COMPARE, IP7 of the CAUSE register is set 
				// (Timer interrupt). Writing to this register clears the timer interrupt pending flag.
				CPU_SetCompare(qwNewValue);
			}
			break;


		// Need to check CONFIG register writes - not all fields are writable.
		// This also sets Endianness mode.

		// WatchHi/WatchLo are used to create a Watch Trap. This may not need implementing, but we should
		// Probably provide a warning on writes, just so that we know
		default:
			// No specific handling needs for writes to these registers.
			DPF(DEBUG_REGS, "MTC0. Set %s to 0x%08x", Cop0RegNames[dwFS], (u32)qwNewValue);
			g_qwCPR[0][dwFS] = qwNewValue;
			break;
	}
}


void R4300_TLB_TLBR(DWORD dwOp) 				// TLB Read
{
	DWORD dwIndex = (u32)(g_qwCPR[0][C0_INX] & 0x1F);

	g_qwCPR[0][C0_PAGEMASK] = g_TLBs[dwIndex].mask;
	g_qwCPR[0][C0_ENTRYHI]  = g_TLBs[dwIndex].hi   & (~g_TLBs[dwIndex].pagemask);
	g_qwCPR[0][C0_ENTRYLO0] = g_TLBs[dwIndex].pfne | g_TLBs[dwIndex].g;
	g_qwCPR[0][C0_ENTRYLO1] = g_TLBs[dwIndex].pfno | g_TLBs[dwIndex].g;

	DPF(DEBUG_TLB, "TLBR: INDEX: 0x%04x. PAGEMASK: 0x%08x.", dwIndex, (u32)g_qwCPR[0][C0_PAGEMASK]);
	DPF(DEBUG_TLB, "      ENTRYHI: 0x%08x. ENTRYLO1: 0x%08x. ENTRYLO0: 0x%08x", (u32)g_qwCPR[0][C0_ENTRYHI], (u32)g_qwCPR[0][C0_ENTRYLO1], (u32)g_qwCPR[0][C0_ENTRYLO0]);
	//DBGConsole_Msg(0, "TLBR: INDEX: 0x%04x. PAGEMASK: 0x%08x.", dwIndex, (u32)g_qwCPR[0][C0_PAGEMASK]);
	//DBGConsole_Msg(0, "      ENTRYHI: 0x%08x. ENTRYLO1: 0x%08x. ENTRYLO0: 0x%08x", (u32)g_qwCPR[0][C0_ENTRYHI], (u32)g_qwCPR[0][C0_ENTRYLO1], (u32)g_qwCPR[0][C0_ENTRYLO0]);
}


void R4300_TLB_TLBWI(DWORD dwOp)			// TLB Write Index
{
	DWORD i = (u32)g_qwCPR[0][C0_INX] & 0x1F;

	DPF(DEBUG_TLB, "TLBWI: INDEX: 0x%04x. PAGEMASK: 0x%08x.", i, (u32)g_qwCPR[0][C0_PAGEMASK]);
	DPF(DEBUG_TLB, "       ENTRYHI: 0x%08x. ENTRYLO1: 0x%08x. ENTRYLO0: 0x%08x", (u32)g_qwCPR[0][C0_ENTRYHI], (u32)g_qwCPR[0][C0_ENTRYLO1], (u32)g_qwCPR[0][C0_ENTRYLO0]);
	//DBGConsole_Msg(0, "TLBWI: INDEX: 0x%04x. PAGEMASK: 0x%08x.", i, (u32)g_qwCPR[0][C0_PAGEMASK]);
	//DBGConsole_Msg(0, "       ENTRYHI: 0x%08x. ENTRYLO1: 0x%08x. ENTRYLO0: 0x%08x", (u32)g_qwCPR[0][C0_ENTRYHI], (u32)g_qwCPR[0][C0_ENTRYLO1], (u32)g_qwCPR[0][C0_ENTRYLO0]);

	// From the R4300i Instruction manual:
	// The G bit of the TLB is written with the logical AND of the G bits of the EntryLo0 and EntryLo1 regs
	// The TLB entry is loaded with the contents of the EntryHi and EntryLo regs.

	// TLB[INDEX] <- PageMask || (EntryHi AND NOT PageMask) || EntryLo1 || EntryLo0

	g_TLBs[i].pagemask	= (u32)g_qwCPR[0][C0_PAGEMASK];
	g_TLBs[i].hi		= (u32)g_qwCPR[0][C0_ENTRYHI];
	g_TLBs[i].pfno		= (u32)g_qwCPR[0][C0_ENTRYLO1];		// Clear Global bit?
	g_TLBs[i].pfne		= (u32)g_qwCPR[0][C0_ENTRYLO0];		// Clear Global bit?

	g_TLBs[i].g = (u32)g_qwCPR[0][C0_ENTRYLO1] &
		                (u32)g_qwCPR[0][C0_ENTRYLO0] &
						TLBLO_G;

	// Build the masks:
	g_TLBs[i].mask     =  g_TLBs[i].pagemask | (~TLBHI_VPN2MASK);
	g_TLBs[i].mask2    =  g_TLBs[i].mask>>1;
	g_TLBs[i].vpnmask  = ~g_TLBs[i].mask;
	g_TLBs[i].vpn2mask =  g_TLBs[i].vpnmask>>1;

	g_TLBs[i].addrcheck= g_TLBs[i].hi & g_TLBs[i].vpnmask;

	g_TLBs[i].pfnehi = ((g_TLBs[i].pfne<<TLBLO_PFNSHIFT) & g_TLBs[i].vpn2mask);
	g_TLBs[i].pfnohi = ((g_TLBs[i].pfno<<TLBLO_PFNSHIFT) & g_TLBs[i].vpn2mask);

	switch (g_TLBs[i].pagemask)
	{
		case TLBPGMASK_4K:	// 4k 
			DPF(DEBUG_TLB, "       4k Pagesize");
            g_TLBs[i].checkbit = 0x00001000;   // bit 12
            break;
        case TLBPGMASK_16K: //  16k pagesize 
			DPF(DEBUG_TLB, "       16k Pagesize");
            g_TLBs[i].checkbit = 0x00004000;   // bit 14
            break;
        case TLBPGMASK_64K: //  64k pagesize
			DPF(DEBUG_TLB, "       64k Pagesize");
            g_TLBs[i].checkbit = 0x00010000;   // bit 16
            break;
        case 0x0007e000: // 256k pagesize
			DPF(DEBUG_TLB, "       256k Pagesize");
            g_TLBs[i].checkbit = 0x00040000;   // bit 18
            break;
        case 0x001fe000: //   1M pagesize
			DPF(DEBUG_TLB, "       1M Pagesize");
            g_TLBs[i].checkbit = 0x00100000;   // bit 20
            break;
        case 0x007fe000: //   4M pagesize
			DPF(DEBUG_TLB, "       4M Pagesize");
            g_TLBs[i].checkbit = 0x00400000;   // bit 22
            break;
        case 0x01ffe000: //  16M pagesize
			DPF(DEBUG_TLB, "       16M Pagesize");
            g_TLBs[i].checkbit = 0x01000000;   // bit 24
            break;
        default: // should not happen! 
			DPF(DEBUG_TLB, "       Unknown Pagesize");
            g_TLBs[i].checkbit = 0;
            break;
	}
}


void R4300_TLB_TLBWR(DWORD dwOp)
{
	DWORD i;
	DWORD iLeastRecent;
	DWORD dwLeastRecent;
	
	// Select a value for index between dwWired and 31
	DWORD dwWired = (u32)g_qwCPR[0][C0_WIRED] & 0x1F;
	//i = (rand()%(32-dwWired)) + dwWired;

	// Don't choose random value - choose the least recently used page to 
	// replace
	dwLeastRecent = ~0;
	for (i = dwWired; i < 32; i++)
	{
		if (g_TLBs[i].lastaccessed < dwLeastRecent)
		{
			iLeastRecent = i;
			dwLeastRecent = g_TLBs[i].lastaccessed;
		}
	}

	i = iLeastRecent;

	//DBGConsole_Msg(0, "TLBWR: Using %d for C0_RAND", i);

	DPF(DEBUG_TLB, "TLBWR: INDEX: 0x%04x. PAGEMASK: 0x%08x.", i, (u32)g_qwCPR[0][C0_PAGEMASK]);
	DPF(DEBUG_TLB, "       ENTRYHI: 0x%08x. ENTRYLO1: 0x%08x. ENTRYLO0: 0x%08x", (u32)g_qwCPR[0][C0_ENTRYHI], (u32)g_qwCPR[0][C0_ENTRYLO1], (u32)g_qwCPR[0][C0_ENTRYLO0]);
	//DBGConsole_Msg(0, "TLBWR: INDEX: 0x%04x. PAGEMASK: 0x%08x.", i, (u32)g_qwCPR[0][C0_PAGEMASK]);
	//DBGConsole_Msg(0, "       ENTRYHI: 0x%08x. ENTRYLO1: 0x%08x. ENTRYLO0: 0x%08x", (u32)g_qwCPR[0][C0_ENTRYHI], (u32)g_qwCPR[0][C0_ENTRYLO1], (u32)g_qwCPR[0][C0_ENTRYLO0]);

	// From the R4300i Instruction manual:
	// The G bit of the TLB is written with the logical AND of the G bits of the EntryLo0 and EntryLo1 regs
	// The TLB entry is loaded with the contents of the EntryHi and EntryLo regs.

	// TLB[INDEX] <- PageMask || (EntryHi AND NOT PageMask) || EntryLo1 || EntryLo0

	g_TLBs[i].pagemask	= (u32)g_qwCPR[0][C0_PAGEMASK];
	g_TLBs[i].hi		= (u32)g_qwCPR[0][C0_ENTRYHI];
	g_TLBs[i].pfno		= (u32)g_qwCPR[0][C0_ENTRYLO1];		// Clear Global bit?
	g_TLBs[i].pfne		= (u32)g_qwCPR[0][C0_ENTRYLO0];		// Clear Global bit?

	g_TLBs[i].g = (u32)g_qwCPR[0][C0_ENTRYLO1] &
		                (u32)g_qwCPR[0][C0_ENTRYLO0] &
						TLBLO_G;

	// Build the masks:
	g_TLBs[i].mask      =  g_TLBs[i].pagemask | (~TLBHI_VPN2MASK); // 0x1fff
	g_TLBs[i].mask2     =  g_TLBs[i].mask>>1;
	g_TLBs[i].vpnmask   = ~g_TLBs[i].mask;
	g_TLBs[i].vpn2mask  =  g_TLBs[i].vpnmask>>1;

	g_TLBs[i].addrcheck = g_TLBs[i].hi & g_TLBs[i].vpnmask;

	g_TLBs[i].pfnehi = ((g_TLBs[i].pfne<<TLBLO_PFNSHIFT) & g_TLBs[i].vpn2mask);
	g_TLBs[i].pfnohi = ((g_TLBs[i].pfno<<TLBLO_PFNSHIFT) & g_TLBs[i].vpn2mask);


	switch (g_TLBs[i].pagemask)
	{
		case TLBPGMASK_4K:	// 4k 
			DPF(DEBUG_TLB, "       4k Pagesize");
            g_TLBs[i].checkbit = 0x00001000;   // bit 12
            break;
        case TLBPGMASK_16K: //  16k pagesize 
			DPF(DEBUG_TLB, "       16k Pagesize");
            g_TLBs[i].checkbit = 0x00004000;   // bit 14
            break;
        case TLBPGMASK_64K: //  64k pagesize
			DPF(DEBUG_TLB, "       64k Pagesize");
            g_TLBs[i].checkbit = 0x00010000;   // bit 16
            break;
        case 0x0007e000: // 256k pagesize
			DPF(DEBUG_TLB, "       256k Pagesize");
            g_TLBs[i].checkbit = 0x00040000;   // bit 18
            break;
        case 0x001fe000: //   1M pagesize
			DPF(DEBUG_TLB, "       1M Pagesize");
            g_TLBs[i].checkbit = 0x00100000;   // bit 20
            break;
        case 0x007fe000: //   4M pagesize
			DPF(DEBUG_TLB, "       4M Pagesize");
            g_TLBs[i].checkbit = 0x00400000;   // bit 22
            break;
        case 0x01ffe000: //  16M pagesize
			DPF(DEBUG_TLB, "       16M Pagesize");
            g_TLBs[i].checkbit = 0x01000000;   // bit 24
            break;
        default: // should not happen! 
			DPF(DEBUG_TLB, "       Unknown Pagesize");
            g_TLBs[i].checkbit = 0;
            break;
	}
}


void R4300_TLB_TLBP(DWORD dwOp) 				// TLB Probe
{
	BOOL bFound = FALSE;

	DWORD dwEntryHi = (u32)g_qwCPR[0][C0_ENTRYHI];

	DPF(DEBUG_TLB, "TLBP: ENTRYHI: 0x%08x", dwEntryHi);
	//DBGConsole_Msg(0, "TLBP: ENTRYHI: 0x%08x", dwEntryHi);

    for(DWORD dwIndex = 0; dwIndex<32; dwIndex++)
	{
		if( ((g_TLBs[dwIndex].hi & TLBHI_VPN2MASK) ==
		     (dwEntryHi          & TLBHI_VPN2MASK)) &&
			(
				(g_TLBs[dwIndex].g) ||
				((g_TLBs[dwIndex].hi & TLBHI_PIDMASK) ==
				 (dwEntryHi          & TLBHI_PIDMASK))
			) ) {
				DPF(DEBUG_TLB, "   Found matching TLB Entry - 0x%04x", dwIndex);
				g_qwCPR[0][C0_INX] = dwIndex;
				bFound = TRUE;
				break;
            }       
    }

	if (!bFound)
	{
		//DBGConsole_Msg(DEBUG_TLB, "   No matching TLB Entry Found for 0x%08x", dwEntryHi);
		DPF(DEBUG_TLB, "   No matching TLB Entry Found for 0x%08x", dwEntryHi);
		g_qwCPR[0][C0_INX] = TLBINX_PROBE;

		//CPUHalt();
	}
	else
	{
		//DBGConsole_Msg(DEBUG_TLB, "   Matching TLB Entry Found for 0x%08x", dwEntryHi);
		//CPUHalt();
	}

}

void R4300_TLB_ERET(DWORD dwOp)
{
	if(g_qwCPR[0][C0_SR] & SR_ERL)
	{
		// Returning from an error trap
		DPF(DEBUG_ERET, "ERET: Returning from error trap");
		CPU_SetPC((u32)(g_qwCPR[0][C0_ERROR_EPC]));
		g_qwCPR[0][C0_SR] &= ~SR_ERL;
	} else {
		DPF(DEBUG_ERET, "ERET: Returning from interrupt/exception");
		// Returning from an exception
		CPU_SetPC((u32)(g_qwCPR[0][C0_EPC]));
		g_qwCPR[0][C0_SR] &= ~SR_EXL;
	}
	// Point to previous instruction (as we increment the pointer immediately afterwards
	DECREMENT_PC();

	// Ensure we don't execute this in the delay slot
	g_nDelay = NO_DELAY;
	//AddCPUJob(CPU_NO_DELAY);

	g_bLLbit = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////



void R4300_Cop1_Unk(DWORD dwOp)     { WARN_NOEXIST("R4300_Cop1_Unk"); }
void R4300_Cop1_BCInstr(DWORD dwOp) { R4300Cop1BC1Instruction[bc(dwOp)](dwOp); }
void R4300_Cop1_SInstr(DWORD dwOp)  { R4300Cop1SInstruction[funct(dwOp)](dwOp); }
void R4300_Cop1_DInstr(DWORD dwOp)  { R4300Cop1DInstruction[funct(dwOp)](dwOp); }

void R4300_Cop1_LInstr(DWORD dwOp)
{
	switch (funct(dwOp))
	{
		case OP_CVT_S:
			R4300_Cop1_L_CVT_S(dwOp);
			return;
		case OP_CVT_D:
			R4300_Cop1_L_CVT_D(dwOp);
			return;
	}
}


void R4300_Cop1_WInstr(DWORD dwOp)
{
	switch (funct(dwOp))
	{
		case OP_CVT_S:
			R4300_Cop1_W_CVT_S(dwOp);
			return;
		case OP_CVT_D:
			R4300_Cop1_W_CVT_D(dwOp);
			return;
	}
	WARN_NOEXIST("R4300_Cop1_WInstr_Unk");
}



void R4300_Cop1_MTC1(DWORD dwOp)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	// Manual says top bits undefined after load
	StoreFPR_Word(dwFS, (s32)g_qwGPR[dwRT]);
}


void R4300_Cop1_MFC1(DWORD dwOp)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	// MFC1 in the manual says this is a sign-extended result
	g_qwGPR[dwRT] = (s64)(s32)LoadFPR_Word(dwFS);
	CHECK_R0();

}

void R4300_Cop1_DMTC1(DWORD dwOp)
{

	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	// Manual says top bits undefined after load
	StoreFPR_Long(dwFS, g_qwGPR[dwRT]);

}

void R4300_Cop1_DMFC1(DWORD dwOp)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	// MFC1 in the manual says this is a sign-extended result
	g_qwGPR[dwRT] = LoadFPR_Long(dwFS);
	CHECK_R0();
}




void R4300_Cop1_CFC1(DWORD dwOp) 		// move Control word From Copro 1
{
	DWORD dwFS = fs(dwOp);
	DWORD dwRT = rt(dwOp);

	// Only defined for reg 0 or 31
	if (dwFS == 0 || dwFS == 31)
	{
		g_qwGPR[dwRT] = (s64)(s32)g_qwCCR[1][dwFS];
		CHECK_R0();
	}
	else
	{
	}
	DPF(DEBUG_REGS, "CFC1. Currently CCR:%d is\t\t0x%08x", dwFS, (u32)g_qwGPR[dwRT]);
}

void R4300_Cop1_CTC1(DWORD dwOp) 		// move Control word To Copro 1
{
	DWORD dwFS = fs(dwOp);
	DWORD dwRT = rt(dwOp);

	// Only defined for reg 0 or 31
	// TODO - Maybe an exception was raised?
	if (dwFS == 0)
	{
		g_qwCCR[1][dwFS] = g_qwGPR[dwRT];

	}
	else if (dwFS == 31)
	{
		/*DWORD dwOldRM = g_qwCCR[1][31] & 0x3;
		DWORD dwNewRM = g_qwGPR[dwRT] & 0x3;

		if ((dwOldRM ^ dwNewRM) != 0)
		{
			switch (dwNewRM)
			{
			case 0: DBGConsole_Msg(0, "Rounding mode is now RN"); break;
			case 1: DBGConsole_Msg(0, "Rounding mode is now RZ"); break;
			case 2: DBGConsole_Msg(0, "Rounding mode is now RP"); break;
			case 3: DBGConsole_Msg(0, "Rounding mode is now RM"); break;
			}
		}*/

		g_qwCCR[1][dwFS] = g_qwGPR[dwRT];
	}
	else
	{

	}

	DPF(DEBUG_REGS, "CTC1. Set CCR:%d to\t\t0x%08x", dwFS, (u32)g_qwGPR[dwRT]);

	// Now generate lots of exceptions :-)
}




void R4300_BC1_BC1F(DWORD dwOp)		// Branch on FPU False
{
	if(!(g_qwCCR[1][31] & FPCSR_C))
	{
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewPC = g_dwPC + (s32)wOffset*4 + 4;
		CPU_Delay();

	} 

}
void R4300_BC1_BC1T(DWORD dwOp)	// Branch on FPU True
{
	if(g_qwCCR[1][31] & FPCSR_C)
	{
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewPC = g_dwPC + (s32)wOffset*4 + 4;
		CPU_Delay();
	} 
}
void R4300_BC1_BC1FL(DWORD dwOp)	// Branch on FPU False Likely

{
	if(!(g_qwCCR[1][31] & FPCSR_C))
	{
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewPC = g_dwPC + (s32)wOffset*4 + 4;
		CPU_Delay();
	}
	else
	{
		// Don't execute subsequent instruction
		INCREMENT_PC();
	}

}
void R4300_BC1_BC1TL(DWORD dwOp)		// Branch on FPU True Likely
{
	if(g_qwCCR[1][31] & FPCSR_C)
	{
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewPC = g_dwPC + (s32)wOffset*4 + 4;
		CPU_Delay();
	}
	else
	{
		// Don't execute subsequent instruction
		INCREMENT_PC();
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// WORD FP Instrs /////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////


void R4300_Cop1_W_CVT_S(DWORD dwOp)
{	
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	LONG nTemp = LoadFPR_Word(dwFS);

	StoreFPR_Single(dwFD, (f32)nTemp);
}

void R4300_Cop1_W_CVT_D(DWORD dwOp)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	LONG nTemp = LoadFPR_Word(dwFS);

	StoreFPR_Double(dwFD, (f64)nTemp);
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// LONG FP Instrs /////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////


void R4300_Cop1_L_CVT_S(DWORD dwOp)
{	
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	s64 nTemp = LoadFPR_Long(dwFS);

	StoreFPR_Single(dwFD, (f32)nTemp);
}

void R4300_Cop1_L_CVT_D(DWORD dwOp)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	s64 nTemp = LoadFPR_Long(dwFS);

	StoreFPR_Double(dwFD, (f64)nTemp);
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Single FP Instrs //////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////


void R4300_Cop1_S_Unk(DWORD dwOp) { WARN_NOEXIST("R4300_Cop1_S_Unk"); }


void R4300_Cop1_S_ADD(DWORD dwOp)
{
	// fd = fs+ft
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f32 fX = LoadFPR_Single(dwFS);
	f32 fY = LoadFPR_Single(dwFT);

	StoreFPR_Single(dwFD, fX + fY);
}

void R4300_Cop1_S_SUB(DWORD dwOp)
{
	// fd = fs-ft
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f32 fX = LoadFPR_Single(dwFS);
	f32 fY = LoadFPR_Single(dwFT);

	StoreFPR_Single(dwFD, fX - fY);
}

void R4300_Cop1_S_MUL(DWORD dwOp)
{
	// fd = fs*ft
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f32 fX = LoadFPR_Single(dwFS);
	f32 fY = LoadFPR_Single(dwFT);

	StoreFPR_Single(dwFD, fX * fY);
}

void R4300_Cop1_S_DIV(DWORD dwOp)
{
	// fd = fs/ft
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f32 fResult;
	f32 fDividend = LoadFPR_Single(dwFS);
	f32 fDivisor  = LoadFPR_Single(dwFT);

	fResult = fDividend / fDivisor;

	StoreFPR_Single(dwFD, fResult);
}

void R4300_Cop1_S_SQRT(DWORD dwOp)
{
	// fd = sqrt(fs)
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	StoreFPR_Single(dwFD, fsqrt(fX));
}


void R4300_Cop1_S_NEG(DWORD dwOp)
{
	// fd = -(fs)
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	StoreFPR_Single(dwFD, -fX);
}

void R4300_Cop1_S_MOV(DWORD dwOp)
{
	// fd = fs
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f32 fValue = LoadFPR_Single(dwFS);

	// Just copy bits directly?

	StoreFPR_Single(dwFD, fValue);
}

void R4300_Cop1_S_ABS(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	StoreFPR_Single(dwFD, fabsf(fX));
}


void R4300_Cop1_S_TRUNC_W(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	// Trunc - round to zero
	s32 x = trunc_int_s(fX);


	StoreFPR_Word(dwFD, x);

}

void R4300_Cop1_S_TRUNC_L(DWORD dwOp)
{

	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	// Trunc - round to zero
	s64 x = trunc_int_d((long double)fX);

	StoreFPR_Long(dwFD, x);
}


void R4300_Cop1_S_ROUND_W(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	// Round!
	s32 x = round_int_s(fX);

	StoreFPR_Word(dwFD, x);
}
void R4300_Cop1_S_ROUND_L(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	// Round!
	s64 x = round_int_d((long double)fX);

	StoreFPR_Long(dwFD, x);
}


void R4300_Cop1_S_CEIL_W(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	s32 x = ceil_int_s(fX);


	StoreFPR_Word(dwFD, x);

}
void R4300_Cop1_S_CEIL_L(DWORD dwOp)
{

	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	s64 x = ceil_int_d((long double)fX);

	StoreFPR_Long(dwFD, x);
}

void R4300_Cop1_S_FLOOR_W(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	// Floor!
	s32 x = floor_int_s(fX);

	StoreFPR_Word(dwFD, x);
}


void R4300_Cop1_S_FLOOR_L(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	// Floor!
	s64 x = floor_int_d((long double)fX);

	StoreFPR_Long(dwFD, x);

}


// Convert single to long - this is used by WarGods
void R4300_Cop1_S_CVT_L(DWORD dwOp)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	StoreFPR_Long(dwFD, (s64)fX);
}



// Convert single to word...
void R4300_Cop1_S_CVT_W(DWORD dwOp)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f32 fX = LoadFPR_Single(dwFS);

	StoreFPR_Word(dwFD, (s32)fX);
}

// Convert single to f64...
void R4300_Cop1_S_CVT_D(DWORD dwOp)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f32 fX = LoadFPR_Single(dwFS);


	StoreFPR_Double(dwFD, (f64)fX);
}


void R4300_Cop1_S_EQ(DWORD dwOp) 				// Compare for Equality
{
	// fs == ft?
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);
	f32 fY = LoadFPR_Single(dwFT);

	if ( fX == fY )
		g_qwCCR[1][31] |= FPCSR_C;
	else
		g_qwCCR[1][31] &= ~FPCSR_C;
}

void R4300_Cop1_S_LT(DWORD dwOp) 				// Compare for Equality
{
	// fs < ft?
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);
	f32 fY = LoadFPR_Single(dwFT);


	if ( fX < fY )
		g_qwCCR[1][31] |= FPCSR_C;
	else
		g_qwCCR[1][31] &= ~FPCSR_C;
}



void R4300_Cop1_C_S_Generic(DWORD dwOp)
{
	// fs < ft?
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);

	bool less;
	bool equal;
	bool unordered;
	bool cond;
	bool cond0 = (dwOp   ) & 0x1;
	bool cond1 = (dwOp>>1) & 0x1;
	bool cond2 = (dwOp>>2) & 0x1;
	bool cond3 = (dwOp>>3) & 0x1;

	f32 fX = LoadFPR_Single(dwFS);
	f32 fY = LoadFPR_Single(dwFT);

	if (_isnan(fX) || _isnan(fY))
	{
		//DBGConsole_Msg(0, "is nan");
		less = false;
		equal = false;
		unordered = true;

		// exception
		if (cond3)
		{
			// Exception
		}
	}
	else
	{
		less  = (fX < fY);
		equal = (fX == fY);
		unordered = false;	
	}

	cond = ((cond0 && unordered) ||
			(cond1 && equal) ||
			(cond2 && less));

	if ( cond )
		g_qwCCR[1][31] |= FPCSR_C;
	else
		g_qwCCR[1][31] &= ~FPCSR_C;

}

// Same as above, but unordered?
// cond3 = 1
// cond2 = 1
// cond1 = 0
// cond0 = 1
void R4300_Cop1_S_NGE(DWORD dwOp)
{
	// fs < ft?
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);

	bool less;
	bool unordered;
	bool cond;

	f32 fX = LoadFPR_Single(dwFS);
	f32 fY = LoadFPR_Single(dwFT);

	if (_isnan(fX) || _isnan(fY))
	{
		less = false;
		unordered = true;

		// exception
	}
	else
	{
		less = ( fX < fY );
		unordered = false;
	}

	cond = (unordered || less);

	if ( cond )
		g_qwCCR[1][31] |= FPCSR_C;
	else
		g_qwCCR[1][31] &= ~FPCSR_C;

}

void R4300_Cop1_S_LE(DWORD dwOp) 				// Compare for Equality
{
	// fs <= ft?
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);

	f32 fX = LoadFPR_Single(dwFS);
	f32 fY = LoadFPR_Single(dwFT);

	// SRInliner doesn't like two rets (I think). 
	// VC6++ with SP4 generates 2 rets with the commented code
	if ( fX <= fY )
		g_qwCCR[1][31] |= FPCSR_C;
	else
		g_qwCCR[1][31] &= ~FPCSR_C;
	

}


void R4300_Cop1_S_F(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_UN(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_UEQ(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_OLT(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_ULT(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_OLE(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_ULE(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_SF(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_NGLE(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_SEQ(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_NGL(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }
void R4300_Cop1_S_NGT(DWORD dwOp) { R4300_Cop1_C_S_Generic(dwOp); }




/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////


void R4300_Cop1_D_Unk(DWORD dwOp)		{ WARN_NOEXIST("R4300_Cop1_D_Unk"); }


void R4300_Cop1_D_ABS(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	StoreFPR_Double(dwFD, fabs(fX));
}

void R4300_Cop1_D_ADD(DWORD dwOp)
{
	// fd = fs+ft
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f64 fX = LoadFPR_Double(dwFS);
	f64 fY = LoadFPR_Double(dwFT);

	StoreFPR_Double(dwFD, fX + fY);
}

void R4300_Cop1_D_SUB(DWORD dwOp)
{
	// fd = fs-ft
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f64 fX = LoadFPR_Double(dwFS);
	f64 fY = LoadFPR_Double(dwFT);

	StoreFPR_Double(dwFD, fX - fY);
}

void R4300_Cop1_D_MUL(DWORD dwOp)
{
	// fd = fs*ft
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f64 fX = LoadFPR_Double(dwFS);
	f64 fY = LoadFPR_Double(dwFT);

	StoreFPR_Double(dwFD, fX * fY);
}

void R4300_Cop1_D_DIV(DWORD dwOp)
{
	// fd = fs/ft
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f64 fDividend = LoadFPR_Double(dwFS);
	f64 fDivisor = LoadFPR_Double(dwFT);

    //if(ToDouble(qwDivisor) != 0)
	{
		StoreFPR_Double(dwFD,  fDividend / fDivisor);
    } 
	// Throw exception?
}

void R4300_Cop1_D_SQRT(DWORD dwOp)
{
	// fd = sqrt(fs)
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	StoreFPR_Double(dwFD, sqrt(fX));
}


void R4300_Cop1_D_NEG(DWORD dwOp)
{
	// fd = -(fs)
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	StoreFPR_Double(dwFD, -fX);
}

void R4300_Cop1_D_MOV(DWORD dwOp)
{
	// fd = fs
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f64 fX = LoadFPR_Double(dwFS);
	
	StoreFPR_Double(dwFD, fX);

	// TODO - Just copy bits - don't interpret!
}

void R4300_Cop1_D_TRUNC_W(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	s32 x = trunc_int_s((f32)fX);

	StoreFPR_Word(dwFD, x);
}
void R4300_Cop1_D_TRUNC_L(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	s64 x = trunc_int_d(fX);

	StoreFPR_Long(dwFD, x);

}


void R4300_Cop1_D_ROUND_W(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	s32 x = round_int_s((f32)fX);

	StoreFPR_Word(dwFD, x);
}
void R4300_Cop1_D_ROUND_L(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	s64 x = round_int_d(fX);

	StoreFPR_Long(dwFD, x);

}


void R4300_Cop1_D_CEIL_W(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	s32 x = ceil_int_s((f32)fX);

	StoreFPR_Word(dwFD, x);
}
void R4300_Cop1_D_CEIL_L(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	s64 x = ceil_int_d(fX);

	StoreFPR_Long(dwFD, x);

}

void R4300_Cop1_D_FLOOR_W(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	s32 x = floor_int_s((f32)fX);

	StoreFPR_Word(dwFD, x);
}
void R4300_Cop1_D_FLOOR_L(DWORD dwOp)
{
	DWORD dwFD = fd(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	s64 x = floor_int_d(fX);

	StoreFPR_Long(dwFD, x);

}

void R4300_Cop1_D_CVT_S(DWORD dwOp)
{	
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f64 fX = LoadFPR_Double(dwFS);
	
	// Convert using current rounding mode?

	StoreFPR_Single(dwFD, (f32)fX);
}

// Convert f64 to word...
void R4300_Cop1_D_CVT_W(DWORD dwOp)
{	
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	// Convert using current rounding mode?

	StoreFPR_Word(dwFD, (s32)fX);
}

void R4300_Cop1_D_CVT_L(DWORD dwOp)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	f64 fX = LoadFPR_Double(dwFS);

	// Convert using current rounding mode?

	StoreFPR_Long(dwFD, (s64)fX);

}




void R4300_Cop1_C_D_Generic(DWORD dwOp)
{
	// fs < ft?
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);

	bool less;
	bool equal;
	bool unordered;
	bool cond;
	bool cond0 = (dwOp   ) & 0x1;
	bool cond1 = (dwOp>>1) & 0x1;
	bool cond2 = (dwOp>>2) & 0x1;
	bool cond3 = (dwOp>>3) & 0x1;

	f64 fX = LoadFPR_Double(dwFS);
	f64 fY = LoadFPR_Double(dwFT);

	if (_isnan(fX) || _isnan(fY))
	{
		//DBGConsole_Msg(0, "is nan");
		less = false;
		equal = false;
		unordered = true;

		// exception
		if (cond3)
		{
			// Exception
		}
	}
	else
	{
		less  = (fX < fY);
		equal = (fX == fY);
		unordered = false;	
	}

	cond = ((cond0 && unordered) ||
			(cond1 && equal) ||
			(cond2 && less));

	if ( cond )
		g_qwCCR[1][31] |= FPCSR_C;
	else
		g_qwCCR[1][31] &= ~FPCSR_C;

}


void R4300_Cop1_D_EQ(DWORD dwOp)				// Compare for Equality
{
	// fs == ft?
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);
	f64 fY = LoadFPR_Double(dwFT);

	if( fX == fY )
		g_qwCCR[1][31] |= FPCSR_C;
	else
		g_qwCCR[1][31] &= ~FPCSR_C;
}

void R4300_Cop1_D_LE(DWORD dwOp)				// Compare for Less Than or Equal
{

	// fs <= ft?
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);
	f64 fY = LoadFPR_Double(dwFT);

	if( fX <= fY )
		g_qwCCR[1][31] |= FPCSR_C;
	else
		g_qwCCR[1][31] &= ~FPCSR_C;

}


void R4300_Cop1_D_LT(DWORD dwOp)
{
	// fs < ft?
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);

	f64 fX = LoadFPR_Double(dwFS);
	f64 fY = LoadFPR_Double(dwFT);

	if( fX < fY )
		g_qwCCR[1][31] |= FPCSR_C;
	else
		g_qwCCR[1][31] &= ~FPCSR_C;

}


void R4300_Cop1_D_F(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_UN(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_UEQ(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_OLT(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_ULT(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_OLE(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_ULE(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_SF(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_NGLE(DWORD dwOp)	{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_SEQ(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_NGL(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_NGE(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
void R4300_Cop1_D_NGT(DWORD dwOp)		{ R4300_Cop1_C_D_Generic(dwOp); }
