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

// Static Recompilation
#include "main.h"

#include "ultra_r4300.h"		// C0_COUNT

#include "CPU.h"
#include "Interrupt.h"
#include "SR.h"

#include "Registers.h"			// For REG_?? / RegNames

#include "debug.h"
#include "asm.h"
#include "R4300.h"
#include "DBGConsole.h"

#include <cstdlib>

DWORD g_dwNumStaticEntries = 0;
DWORD g_dwStaticCodeTableSize = 0;
DWORD g_dwStaticCodeTableInitialSize = 30000;	// Size of first buffer
CDynarecCode **g_pDynarecCodeTable = NULL;

static BYTE	* g_pGlobalBuffer = NULL;
static DWORD g_dwGlobalBufferPtr = 0;
static DWORD g_dwGlobalBufferSize = 0;

DWORD g_dwNumSRCompiled = 0;
DWORD g_dwNumSROptimised = 0;
DWORD g_dwNumSRFailed = 0;

#define SR_FLAGS_BRANCHES			0x00000001
#define SR_FLAGS_BRANCHES_TO_START	0x00000002
#define SR_FLAGS_HANDLE_BRANCH		0x00000004

typedef struct tagCachedRegInfo
{
	DWORD dwCachedIReg;		// ~0 if uncached, else ???_CODE of intel reg we're cached in
	BOOL  bValid;			// If cached, TRUE if value in intel register is valid
	BOOL  bDirty;			// If cached, TRUE if value has been modified

	BOOL  bLoUnk;			// TRUE if we don't know the status of this reg (so we have to load from memory)
	DWORD dwLoValue;		// If bUnk is false, this is the last assigned value

} CachedRegInfo;

typedef struct tagHiRegInfo
{
	BOOL	bDirty;			// Do we need to write information about this register back to memory?

	BOOL	bUnk;			// If TRUE, we don't know the value of this register
	BOOL	bSignExt;		// If TRUE, value is sign-extension of low value, dwValue is invalid
	DWORD	dwValue;		// If bUnk is FALSE, this is the value of the register (through setmipshi..)
} HiRegInfo;

static CachedRegInfo g_MIPSRegInfo[32];
static HiRegInfo g_MIPSHiRegInfo[32];
static DWORD g_IntelRegInfo[8];

#include "SR_FP.inl"


#define SR_Stat_D_S_S(d, s1, s2) \
	pCode->dwRegUpdates[d]++;    \
	pCode->dwRegReads[s1]++;     \
	pCode->dwRegReads[s2]++;	 

#define SR_Stat_D_S(d, s)	\
	pCode->dwRegUpdates[d]++;    \
	pCode->dwRegReads[s]++; 

#define SR_Stat_D(d) \
	pCode->dwRegUpdates[d]++;

#define SR_Stat_S(s) \
	pCode->dwRegReads[s]++;

#define SR_Stat_S_S(s1, s2)		\
	pCode->dwRegReads[s1]++;	\
	pCode->dwRegReads[s2]++;

#define SR_Stat_Base(b) \
	pCode->dwRegBaseUse[b]++;  

#define SR_Stat_Load_B_D(b, d)	\
	pCode->dwRegBaseUse[b]++;	\
	pCode->dwRegUpdates[d]++;  

#define SR_Stat_Load_B(b)	\
	pCode->dwRegBaseUse[b]++;  
	
#define SR_Stat_Save_B_S(b, s)	\
	pCode->dwRegBaseUse[b]++;	\
	pCode->dwRegReads[s]++;  

#define SR_Stat_Save_B(b)	\
	pCode->dwRegBaseUse[b]++;

//////////
#define SR_Stat_Long_D(d)
#define SR_Stat_Word_D(d)

//////////
#define SR_Stat_Single_Unk()

#define SR_Stat_Single_D_S_S(d, s1, s2) \
	pCode->dwSRegUpdates[d]++;    \
	pCode->dwSRegReads[s1]++;     \
	pCode->dwSRegReads[s2]++;	 

#define SR_Stat_Single_D_S(d, s) \
	pCode->dwSRegUpdates[d]++;    \
	pCode->dwSRegReads[s]++;

#define SR_Stat_Single_S_S(s1, s2) \
	pCode->dwSRegReads[s1]++;     \
	pCode->dwSRegReads[s2]++;	 
	
#define SR_Stat_Single_D(d) \
	pCode->dwSRegUpdates[d]++;

#define SR_Stat_Single_S(s) \
	pCode->dwSRegReads[s]++;	 

/////////////////////////////
#define SR_Stat_Double_Unk()

#define SR_Stat_Double_D_S_S(d, s1, s2) \
	pCode->dwDRegUpdates[d]++;    \
	pCode->dwDRegReads[s1]++;     \
	pCode->dwDRegReads[s2]++;	 

#define SR_Stat_Double_D_S(d, s) \
	pCode->dwDRegUpdates[d]++;    \
	pCode->dwDRegReads[s]++;	 

#define SR_Stat_Double_S_S(s1, s2) \
	pCode->dwDRegReads[s1]++;     \
	pCode->dwDRegReads[s2]++;	 

#define SR_Stat_Double_D(d) \
	pCode->dwDRegUpdates[d]++;

#define SR_Stat_Double_S(s) \
	pCode->dwDRegReads[s]++;	 


// Return TRUE if compiled ok, FALSE otherwise
typedef BOOL (*SR_EmitInstructionType)(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_DBG_BreakPoint(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SRHack_UnOpt(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SRHack_Opt(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SRHack_NoOpt(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_J(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_JAL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_BEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_BNE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_BLEZ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_BGTZ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_ADDI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_ADDIU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SLTI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SLTIU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_ANDI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_ORI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_XORI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LUI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_CoPro0(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_CoPro1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_BEQL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_BNEL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_BLEZL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_BGTZL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_DADDI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_DADDIU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LDL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LDR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_LB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LH(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LWL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LW(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LBU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LHU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LWR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LWU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_SB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SH(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SWL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SW(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SDL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SDR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SWR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_CACHE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_LL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LWC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LLD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LDC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LDC2(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_LD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_SC(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SWC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SCD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SDC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SDC2(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_SD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Special_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SLL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SRL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SRA(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SLLV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SRLV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SRAV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Special_JR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_JALR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SYSCALL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_BREAK(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SYNC(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Special_MFHI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_MTHI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_MFLO(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_MTLO(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DSLLV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DSRLV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DSRAV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Special_MULT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_MULTU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DIV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DIVU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DMULT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DMULTU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DDIV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DDIVU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Special_ADD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_ADDU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SUB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SUBU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_AND(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_OR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_XOR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_NOR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Special_SLT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_SLTU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DADD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DADDU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DSUB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DSUBU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Special_TGE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_TGEU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_TLT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_TLTU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_TEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_TNE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Special_DSLL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DSRL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DSRA(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DSLL32(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DSRL32(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Special_DSRA32(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);


static BOOL SR_Emit_RegImm_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_BLTZ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_BGEZ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_BLTZL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_BGEZL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_RegImm_TGEI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_TGEIU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_TLTI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_TLTIU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_TEQI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_TNEI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_RegImm_BLTZAL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_BGEZAL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_BLTZALL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_RegImm_BGEZALL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);


static BOOL SR_Emit_Cop0_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop0_MFC0(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop0_MTC0(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop0_TLB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_MFC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_DMFC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_CFC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_MTC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_DMTC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_CTC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_BCInstr(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_SInstr(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_DInstr(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_WInstr(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_LInstr(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);


static BOOL SR_BC1_BC1F(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_BC1_BC1T(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_BC1_BC1FL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_BC1_BC1TL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);


static BOOL SR_Emit_Cop1_S_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_ADD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_SUB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_MUL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_DIV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_SQRT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_ABS(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_MOV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_NEG(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_S_ROUND_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_TRUNC_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_CEIL_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_FLOOR_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_ROUND_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_TRUNC_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_CEIL_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_FLOOR_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_S_CVT_D(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_CVT_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_CVT_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_S_F(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_UN(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_EQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_UEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_OLT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_ULT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_OLE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_ULE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_S_SF(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_NGLE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_SEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_NGL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_LT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_NGE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_LE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_S_NGT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_D_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_ADD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_SUB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_MUL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_DIV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_SQRT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_ABS(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_MOV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_NEG(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_D_ROUND_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_TRUNC_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_CEIL_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_FLOOR_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_ROUND_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_TRUNC_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_CEIL_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_FLOOR_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_D_CVT_S(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_CVT_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_CVT_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_D_F(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_UN(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_EQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_UEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_OLT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_ULT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_OLE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_ULE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);

static BOOL SR_Emit_Cop1_D_SF(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_NGLE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_SEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_NGL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_LT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_NGE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_LE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);
static BOOL SR_Emit_Cop1_D_NGT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags);



// Opcode Jump Table
SR_EmitInstructionType SR_EmitInstruction[64] = {
	SR_Emit_Special, SR_Emit_RegImm, SR_Emit_J, SR_Emit_JAL, SR_Emit_BEQ, SR_Emit_BNE, SR_Emit_BLEZ, SR_Emit_BGTZ,
	SR_Emit_ADDI, SR_Emit_ADDIU, SR_Emit_SLTI, SR_Emit_SLTIU, SR_Emit_ANDI, SR_Emit_ORI, SR_Emit_XORI, SR_Emit_LUI,
	SR_Emit_CoPro0, SR_Emit_CoPro1, SR_Emit_Unk, SR_Emit_Unk, SR_Emit_BEQL, SR_Emit_BNEL, SR_Emit_BLEZL, SR_Emit_BGTZL,
	SR_Emit_DADDI, SR_Emit_DADDIU, SR_Emit_LDL, SR_Emit_LDR, /*SR_Emit_Patch*/SR_Emit_Unk, SR_Emit_SRHack_UnOpt, SR_Emit_SRHack_Opt, SR_Emit_SRHack_NoOpt,
	SR_Emit_LB, SR_Emit_LH, SR_Emit_LWL, SR_Emit_LW, SR_Emit_LBU, SR_Emit_LHU, SR_Emit_LWR, SR_Emit_LWU,
	SR_Emit_SB, SR_Emit_SH, SR_Emit_SWL, SR_Emit_SW, SR_Emit_SDL, SR_Emit_SDR, SR_Emit_SWR, SR_Emit_CACHE,
	SR_Emit_LL, SR_Emit_LWC1, SR_Emit_Unk, SR_Emit_Unk, SR_Emit_LLD, SR_Emit_LDC1, SR_Emit_LDC2, SR_Emit_LD,
	SR_Emit_SC, SR_Emit_SWC1, SR_Emit_DBG_BreakPoint, SR_Emit_Unk, SR_Emit_SCD, SR_Emit_SDC1, SR_Emit_SDC2, SR_Emit_SD
};

// SpecialOpCode Jump Table
SR_EmitInstructionType SR_EmitSpecialInstruction[64] = {
	SR_Emit_Special_SLL, SR_Emit_Special_Unk, SR_Emit_Special_SRL, SR_Emit_Special_SRA, SR_Emit_Special_SLLV, SR_Emit_Special_Unk, SR_Emit_Special_SRLV, SR_Emit_Special_SRAV,
	SR_Emit_Special_JR, SR_Emit_Special_JALR, SR_Emit_Special_Unk, SR_Emit_Special_Unk, SR_Emit_Special_SYSCALL, SR_Emit_Special_BREAK, SR_Emit_Special_Unk, SR_Emit_Special_SYNC,
	SR_Emit_Special_MFHI, SR_Emit_Special_MTHI, SR_Emit_Special_MFLO, SR_Emit_Special_MTLO, SR_Emit_Special_DSLLV, SR_Emit_Special_Unk, SR_Emit_Special_DSRLV, SR_Emit_Special_DSRAV,
	SR_Emit_Special_MULT, SR_Emit_Special_MULTU, SR_Emit_Special_DIV, SR_Emit_Special_DIVU, SR_Emit_Special_DMULT, SR_Emit_Special_DMULTU, SR_Emit_Special_DDIV, SR_Emit_Special_DDIVU,
	SR_Emit_Special_ADD, SR_Emit_Special_ADDU, SR_Emit_Special_SUB, SR_Emit_Special_SUBU, SR_Emit_Special_AND, SR_Emit_Special_OR, SR_Emit_Special_XOR, SR_Emit_Special_NOR,
	SR_Emit_Special_Unk, SR_Emit_Special_Unk, SR_Emit_Special_SLT, SR_Emit_Special_SLTU, SR_Emit_Special_DADD, SR_Emit_Special_DADDU, SR_Emit_Special_DSUB, SR_Emit_Special_DSUBU,
	SR_Emit_Special_TGE, SR_Emit_Special_TGEU, SR_Emit_Special_TLT, SR_Emit_Special_TLTU, SR_Emit_Special_TEQ, SR_Emit_Special_Unk, SR_Emit_Special_TNE, SR_Emit_Special_Unk,
	SR_Emit_Special_DSLL, SR_Emit_Special_Unk, SR_Emit_Special_DSRL, SR_Emit_Special_DSRA, SR_Emit_Special_DSLL32, SR_Emit_Special_Unk, SR_Emit_Special_DSRL32, SR_Emit_Special_DSRA32
};

// RegImmOpCode Jump Table
SR_EmitInstructionType SR_EmitRegImmInstruction[32] = {
	SR_Emit_RegImm_BLTZ,   SR_Emit_RegImm_BGEZ,   SR_Emit_RegImm_BLTZL,   SR_Emit_RegImm_BGEZL,   SR_Emit_RegImm_Unk,  SR_Emit_RegImm_Unk, SR_Emit_RegImm_Unk,  SR_Emit_RegImm_Unk,
	SR_Emit_RegImm_TGEI,   SR_Emit_RegImm_TGEIU,  SR_Emit_RegImm_TLTI,    SR_Emit_RegImm_TLTIU,   SR_Emit_RegImm_TEQI, SR_Emit_RegImm_Unk, SR_Emit_RegImm_TNEI, SR_Emit_RegImm_Unk,
	SR_Emit_RegImm_BLTZAL, SR_Emit_RegImm_BGEZAL, SR_Emit_RegImm_BLTZALL, SR_Emit_RegImm_BGEZALL, SR_Emit_RegImm_Unk,  SR_Emit_RegImm_Unk, SR_Emit_RegImm_Unk,  SR_Emit_RegImm_Unk,
	SR_Emit_RegImm_Unk,    SR_Emit_RegImm_Unk,    SR_Emit_RegImm_Unk,     SR_Emit_RegImm_Unk,     SR_Emit_RegImm_Unk,  SR_Emit_RegImm_Unk, SR_Emit_RegImm_Unk,  SR_Emit_RegImm_Unk
};

// COP0 Jump Table
SR_EmitInstructionType SR_EmitCop0Instruction[32] = {
	SR_Emit_Cop0_MFC0, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_MTC0, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk,
	SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk,
	SR_Emit_Cop0_TLB, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk,
	SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk, SR_Emit_Cop0_Unk,
};


// COP1 Jump Table
SR_EmitInstructionType SR_EmitCop1Instruction[32] = {
	SR_Emit_Cop1_MFC1,    SR_Emit_Cop1_DMFC1,  SR_Emit_Cop1_CFC1, SR_Emit_Cop1_Unk, SR_Emit_Cop1_MTC1,   SR_Emit_Cop1_DMTC1,  SR_Emit_Cop1_CTC1, SR_Emit_Cop1_Unk,
	SR_Emit_Cop1_BCInstr, SR_Emit_Cop1_Unk,    SR_Emit_Cop1_Unk,  SR_Emit_Cop1_Unk, SR_Emit_Cop1_Unk,    SR_Emit_Cop1_Unk,    SR_Emit_Cop1_Unk,  SR_Emit_Cop1_Unk,
	SR_Emit_Cop1_SInstr,  SR_Emit_Cop1_DInstr, SR_Emit_Cop1_Unk,  SR_Emit_Cop1_Unk, SR_Emit_Cop1_WInstr, SR_Emit_Cop1_LInstr, SR_Emit_Cop1_Unk,  SR_Emit_Cop1_Unk,
	SR_Emit_Cop1_Unk,     SR_Emit_Cop1_Unk,    SR_Emit_Cop1_Unk,  SR_Emit_Cop1_Unk, SR_Emit_Cop1_Unk,    SR_Emit_Cop1_Unk,    SR_Emit_Cop1_Unk,  SR_Emit_Cop1_Unk
};

SR_EmitInstructionType SR_EmitCop1BC1Instruction[4] = {
	SR_BC1_BC1F, SR_BC1_BC1T, SR_BC1_BC1FL, SR_BC1_BC1TL
};


// Single Jump Table
SR_EmitInstructionType SR_EmitCop1SInstruction[64] = {
	SR_Emit_Cop1_S_ADD,     SR_Emit_Cop1_S_SUB,     SR_Emit_Cop1_S_MUL,    SR_Emit_Cop1_S_DIV,     SR_Emit_Cop1_S_SQRT,    SR_Emit_Cop1_S_ABS,     SR_Emit_Cop1_S_MOV,    SR_Emit_Cop1_S_NEG,
	SR_Emit_Cop1_S_ROUND_L, SR_Emit_Cop1_S_TRUNC_L,	SR_Emit_Cop1_S_CEIL_L, SR_Emit_Cop1_S_FLOOR_L, SR_Emit_Cop1_S_ROUND_W, SR_Emit_Cop1_S_TRUNC_W, SR_Emit_Cop1_S_CEIL_W, SR_Emit_Cop1_S_FLOOR_W,
	SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,    SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,    SR_Emit_Cop1_S_Unk, 
	SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,    SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,    SR_Emit_Cop1_S_Unk, 
	SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_CVT_D,   SR_Emit_Cop1_S_Unk,    SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_CVT_W,   SR_Emit_Cop1_S_CVT_L,   SR_Emit_Cop1_S_Unk,    SR_Emit_Cop1_S_Unk,
	SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,    SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,     SR_Emit_Cop1_S_Unk,    SR_Emit_Cop1_S_Unk, 
	SR_Emit_Cop1_S_F,       SR_Emit_Cop1_S_UN,      SR_Emit_Cop1_S_EQ,     SR_Emit_Cop1_S_UEQ,     SR_Emit_Cop1_S_OLT,     SR_Emit_Cop1_S_ULT,     SR_Emit_Cop1_S_OLE,    SR_Emit_Cop1_S_ULE,
	SR_Emit_Cop1_S_SF,      SR_Emit_Cop1_S_NGLE,    SR_Emit_Cop1_S_SEQ,    SR_Emit_Cop1_S_NGL,     SR_Emit_Cop1_S_LT,      SR_Emit_Cop1_S_NGE,     SR_Emit_Cop1_S_LE,     SR_Emit_Cop1_S_NGT
};

// Double Jump Table
SR_EmitInstructionType SR_EmitCop1DInstruction[64] = {
	SR_Emit_Cop1_D_ADD,     SR_Emit_Cop1_D_SUB,     SR_Emit_Cop1_D_MUL, SR_Emit_Cop1_D_DIV, SR_Emit_Cop1_D_SQRT, SR_Emit_Cop1_D_ABS, SR_Emit_Cop1_D_MOV, SR_Emit_Cop1_D_NEG,
	SR_Emit_Cop1_D_ROUND_L, SR_Emit_Cop1_D_TRUNC_L,	SR_Emit_Cop1_D_CEIL_L, SR_Emit_Cop1_D_FLOOR_L, SR_Emit_Cop1_D_ROUND_W, SR_Emit_Cop1_D_TRUNC_W, SR_Emit_Cop1_D_CEIL_W, SR_Emit_Cop1_D_FLOOR_W,
	SR_Emit_Cop1_D_Unk,     SR_Emit_Cop1_D_Unk,     SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, 
	SR_Emit_Cop1_D_Unk,     SR_Emit_Cop1_D_Unk,     SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, 
	SR_Emit_Cop1_D_CVT_S,   SR_Emit_Cop1_D_Unk,     SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_CVT_W, SR_Emit_Cop1_D_CVT_L, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk,
	SR_Emit_Cop1_D_Unk,     SR_Emit_Cop1_D_Unk,     SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, SR_Emit_Cop1_D_Unk, 
	SR_Emit_Cop1_D_F,       SR_Emit_Cop1_D_UN,      SR_Emit_Cop1_D_EQ, SR_Emit_Cop1_D_UEQ, SR_Emit_Cop1_D_OLT, SR_Emit_Cop1_D_ULT, SR_Emit_Cop1_D_OLE, SR_Emit_Cop1_D_ULE,
	SR_Emit_Cop1_D_SF,      SR_Emit_Cop1_D_NGLE,    SR_Emit_Cop1_D_SEQ, SR_Emit_Cop1_D_NGL, SR_Emit_Cop1_D_LT, SR_Emit_Cop1_D_NGE, SR_Emit_Cop1_D_LE, SR_Emit_Cop1_D_NGT
};

// Assumes: mreg is cached
void EnsureCachedValidLo(CDynarecCode * pCode, DWORD mreg)	
{
	DWORD iCachedReg = g_MIPSRegInfo[mreg].dwCachedIReg;
	
	if (!g_MIPSRegInfo[mreg].bValid)	
	{
		DPF(DEBUG_DYNREC, "  ++ Cached value for %s is invalid - loading...\\/", RegNames[mreg]);
		MOV_REG_MEM((WORD)iCachedReg, (((BYTE *)&g_qwGPR[0]) + (lohalf(mreg)*4)) );	
		g_MIPSRegInfo[mreg].bValid = TRUE;							
	}
	else
	{
		DPF(DEBUG_DYNREC, "  ++ Using cached value for %s...\\/", RegNames[mreg]);
	}
}


// 
void LoadMIPSLo(CDynarecCode * pCode, WORD ireg, DWORD mreg)	
{
	DWORD iCachedReg = g_MIPSRegInfo[mreg].dwCachedIReg;
	
	if (iCachedReg != ~0)
	{	
		EnsureCachedValidLo(pCode, mreg);																
		MOV(ireg, (WORD)iCachedReg);											
	}																																		
	else if (mreg == REG_r0)											
	{
		DPF(DEBUG_DYNREC, "  ++ Clearing reg for r0 load...\\/");		
		// We have to use MOV and not XOR to avoid setting the flags
		//XOR(ireg, ireg);											
		MOVI(ireg, 0);
	}
	else																
	{
		MOV_REG_MEM(ireg, (((BYTE *)&g_qwGPR[0]) + (lohalf(mreg)*4)) );	
	}
}


// TODO - if the reg is cached, check signed/zero/set flags?!?
void LoadMIPSHi(CDynarecCode * pCode, WORD ireg, DWORD mreg)
{
	if (mreg == REG_r0)
	{
		DPF(DEBUG_DYNREC, "  ++ Clearing reg for r0 load...\\/");
		//XOR(ireg, ireg);
		MOVI(ireg, 0);
	}
	else
	{
		if (!g_MIPSHiRegInfo[mreg].bUnk)
		{
			DPF(DEBUG_DYNREC, "  ++ We could optimise hi reg load here (loading 0x%08x)!!!", g_MIPSHiRegInfo[mreg].dwValue);
			MOVI(ireg, g_MIPSHiRegInfo[mreg].dwValue);
		}
		else
		{
			MOV_REG_MEM(ireg, (((BYTE *)&g_qwGPR[0]) + (hihalf(mreg)*4)) );
		}
	}
}

void StoreMIPSLo(CDynarecCode * pCode, DWORD mreg, WORD ireg)
{
	DWORD iCachedReg = g_MIPSRegInfo[mreg].dwCachedIReg;
	
	if (iCachedReg != ~0)
	{
		DPF(DEBUG_DYNREC, "  ++ Updating cached value for %s...\\/", RegNames[mreg]);
		MOV((WORD)iCachedReg, ireg);
		g_MIPSRegInfo[mreg].bDirty = TRUE;
		g_MIPSRegInfo[mreg].bValid = TRUE;
	}
	else
	{
		MOV_MEM_REG((((BYTE *)&g_qwGPR[0]) + (lohalf(mreg)*4)), (ireg) );
	}
}

// TODO - Keep record of signed/zero/set flags?
void StoreMIPSHi(CDynarecCode * pCode, DWORD mreg, WORD ireg)
{
	MOV_MEM_REG((((BYTE *)&g_qwGPR[0]) + (hihalf(mreg)*4)), ireg );

	// We know longer know the contents of the high register...
	if (!g_MIPSHiRegInfo[mreg].bUnk)
	{
		DPF(DEBUG_DYNREC, "  ++ We've successfully avoided a hiwriteback!");
	}
	g_MIPSHiRegInfo[mreg].bUnk = TRUE;
	g_MIPSHiRegInfo[mreg].bDirty = FALSE;	
	
}


void SetMIPSLo(CDynarecCode * pCode, DWORD mreg, DWORD data)
{
	DWORD iCachedReg = g_MIPSRegInfo[mreg].dwCachedIReg;
	
	if (iCachedReg != ~0)
	{
		DPF(DEBUG_DYNREC, "  ++ Setting cached value for %s...\\/", RegNames[mreg]);
		MOVI((WORD)iCachedReg, (data));
		g_MIPSRegInfo[mreg].bDirty = TRUE;
		g_MIPSRegInfo[mreg].bValid = TRUE;
	}
	else
	{
		MOVI_MEM( (((BYTE *)&g_qwGPR[0]) + (lohalf(mreg)*4)), data);
	}
}
	
void SetMIPSHi(CDynarecCode * pCode, DWORD mreg, DWORD data)
{
	//MOVI_MEM( (((BYTE *)&g_qwGPR[0]) + (hihalf(mreg)*4)), (data));

	DPF(DEBUG_DYNREC, "  ++ We could stall writeback here!!!");
	g_MIPSHiRegInfo[mreg].bUnk = FALSE;
	g_MIPSHiRegInfo[mreg].dwValue = data;
	g_MIPSHiRegInfo[mreg].bDirty = TRUE;
}


static BOOL SR_EnsureTableSpace();
static void SR_AddCompiledCode(CDynarecCode *pCode, DWORD * pdwBase);

static void SR_FlushMipsRegs(CDynarecCode * pCode);
static void SR_RestoreIntelRegs(CDynarecCode * pCode);

static void SR_PreEmitCachedRegCheck(CDynarecCode *pCode);
static void SR_PostEmitCachedRegCheck(CDynarecCode *pCode, BOOL bOptimised);

static void SR_InitMIPSRegInfo();
static void SR_Stat_Analyze(CDynarecCode * pCode);
static DWORD SR_CheckStuff(DWORD dwNumCycles);


/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

HRESULT SR_Init(DWORD dwSize)
{
	g_dwStaticCodeTableInitialSize = dwSize;
	
	g_dwNumStaticEntries = 0;
	g_dwStaticCodeTableSize = 0;
	g_pDynarecCodeTable = NULL;

	// Reserve a huge range of memory. We do this because we can't simply
	// allocate a new buffer and copy the existing code across (this would
	// mess up all the existing function pointers and jumps etc).
	// Note that this call does not actually allocate any storage - we're not
	// actually asking Windows to allocate 256Mb!
	/*KOS
	g_pGlobalBuffer = (BYTE*)VirtualAlloc(NULL,
										 256 * 1024 * 1024, 
										 MEM_RESERVE, 
										 PAGE_EXECUTE_READWRITE);

	if (g_pGlobalBuffer == NULL)
		return E_OUTOFMEMORY;*/

	g_dwGlobalBufferPtr = 0;
	g_dwGlobalBufferSize = 0;

	return S_OK;
}

void SR_Reset()
{
	SR_Fini();
	SR_Init(g_dwStaticCodeTableInitialSize);
}

void SR_Fini()
{
	if (g_pDynarecCodeTable != NULL)
	{

		for (DWORD i = 0; i < g_dwNumStaticEntries; i++)
		{
			CDynarecCode * pCode = g_pDynarecCodeTable[i];
			if (pCode != NULL)
			{
				#ifdef DAEDALUS_LOG
				if (pCode->dwCount > 10000)
				{
					DPF(DEBUG_DYNREC, "Dynarec: 0x%08x - %-4d ops (executed %d times)",
						pCode->dwStartPC, pCode->dwNumOps, pCode->dwCount);
				}
				#endif

				delete pCode;
				g_pDynarecCodeTable[i] = NULL;
			}
		}

		delete []g_pDynarecCodeTable;
		g_pDynarecCodeTable = NULL;
	}

	g_dwNumStaticEntries = 0;
	g_dwStaticCodeTableSize = 0;

	if (g_pGlobalBuffer != NULL)
	{
		// Decommit all the pages first
		/*KOS
		VirtualFree(g_pGlobalBuffer, 256 * 1024 * 1024, MEM_DECOMMIT);
		// Now release
		VirtualFree(g_pGlobalBuffer, 0, MEM_RELEASE);
		*/
		g_pGlobalBuffer = NULL;
	}

}

void SR_Stats()
{
	DWORD dwCount = 0;
	DWORD dwTotalInstrs = 0;
	DWORD dwTotalOptimised = 0;
	DWORD dwTotalInstrsExe = 0;
	DWORD dwMaxLen = 0;
	DWORD dwMaxCount = 0;

	DWORD dwTotalBlocksOptimised = 0;
	DWORD dwTotalOptInstrs = 0;
	DWORD dwTotalOptInstrsOptimised = 0;
	DWORD dwTotalOptInputBytes = 0;
	DWORD dwTotalOptOutputBytes = 0;
		
	DWORD dwTotalBlocksUnoptimised = 0;
	DWORD dwTotalUnoptInstrs = 0;
	DWORD dwTotalUnoptInstrsOptimised = 0;
	DWORD dwTotalUnoptInputBytes = 0;
	DWORD dwTotalUnoptOutputBytes = 0;

	for (DWORD i = 0; i < g_dwNumStaticEntries; i++)
	{
		CDynarecCode * pCode = g_pDynarecCodeTable[i];
		if (pCode != NULL)
		{
			DPF(DEBUG_DYNREC, "0x%08x, %s, %d ops, %d optimised",
				pCode->dwStartPC, pCode->dwOptimiseLevel ? "optimised": "unoptimised", 
				pCode->dwNumOps, pCode->dwNumOptimised);
			
			dwTotalInstrs += pCode->dwNumOps;
			dwTotalOptimised += pCode->dwNumOptimised;
			dwTotalInstrsExe += (pCode->dwNumOps * pCode->dwCount);
			dwCount += pCode->dwCount;

			if (pCode->dwOptimiseLevel == 0)
			{
				dwTotalUnoptInputBytes += pCode->dwNumOps * 4;
				dwTotalUnoptOutputBytes += pCode->dwCurrentPos;
				dwTotalUnoptInstrs += pCode->dwNumOps;
				dwTotalUnoptInstrsOptimised += pCode->dwNumOptimised;
				dwTotalBlocksUnoptimised++;
			}
			else
			{
				dwTotalOptInputBytes += pCode->dwNumOps * 4;
				dwTotalOptOutputBytes += pCode->dwCurrentPos;
				dwTotalOptInstrs += pCode->dwNumOps;
				dwTotalOptInstrsOptimised += pCode->dwNumOptimised;
				dwTotalBlocksOptimised++;
			}

			if (pCode->dwNumOps > dwMaxLen)
				dwMaxLen = pCode->dwNumOps;

			if (pCode->dwCount > dwMaxCount)
				dwMaxCount = pCode->dwCount;
		}
	}

	DBGConsole_Msg(0, "Dynarec Stats");		
	DBGConsole_Msg(0, "-------------");		
	DBGConsole_Msg(0, "%d Entries (%#.3f %% optimised)", g_dwNumStaticEntries, (float)dwTotalBlocksOptimised * 100.0f / (float)g_dwNumStaticEntries);
	DBGConsole_Msg(0, "%d Ops compiled in total (%#.3f %% optimised)", dwTotalInstrs, (float)dwTotalOptimised * 100.0f / (float)dwTotalInstrs);
	DBGConsole_Msg(0, "%d Ops executed in total", dwTotalInstrsExe);
	DBGConsole_Msg(0, "%d Calls", dwCount);
	DBGConsole_Msg(0, "%#.3f Average ops/call", (float)dwTotalInstrsExe / (float)dwCount);
	DBGConsole_Msg(0, "%d Longest run", dwMaxLen);
	DBGConsole_Msg(0, "%d Largest Count", dwMaxCount);
	DBGConsole_Msg(0, "");
	DBGConsole_Msg(0, "Unoptimised");
	DBGConsole_Msg(0, "---------");
	DBGConsole_Msg(0, "%d Input Bytes", dwTotalUnoptInputBytes);
	DBGConsole_Msg(0, "%d Output Bytes", dwTotalUnoptOutputBytes);
	DBGConsole_Msg(0, "%#.3f Average Expansion Ratio", (float)dwTotalUnoptOutputBytes/(float)dwTotalUnoptInputBytes);
	DBGConsole_Msg(0, "%#.3f%% optimised inplace", (float)dwTotalUnoptInstrsOptimised * 100.0f / (float)dwTotalUnoptInstrs);
	DBGConsole_Msg(0, "");
	DBGConsole_Msg(0, "Optimised");
	DBGConsole_Msg(0, "---------");
	DBGConsole_Msg(0, "%d Input Bytes", dwTotalOptInputBytes);
	DBGConsole_Msg(0, "%d Output Bytes", dwTotalOptOutputBytes);
	DBGConsole_Msg(0, "%#.3f Average Expansion Ratio", (float)dwTotalOptOutputBytes/(float)dwTotalOptInputBytes);
	DBGConsole_Msg(0, "%#.3f%% optimised inplace", (float)dwTotalOptInstrsOptimised * 100.0f / (float)dwTotalOptInstrs);
}



// Ensures that there is space in the table for one more entry
BOOL SR_EnsureTableSpace()
{
	CDynarecCode ** pNewBuffer;

	// Check if current space is sufficient
	if ((g_dwNumStaticEntries+1) < g_dwStaticCodeTableSize)
		return TRUE;

	// Double the current buffer size, or set to initial size if currently empty
	DWORD dwNewSize = g_dwStaticCodeTableSize * 2;

	if (dwNewSize == 0)
		dwNewSize = g_dwStaticCodeTableInitialSize;

	pNewBuffer = new CDynarecCode *[dwNewSize];
	if (pNewBuffer == NULL)
		return FALSE;

	DBGConsole_Msg(0, "Resizing DR buffer to %d entries", dwNewSize);


	if (g_dwNumStaticEntries > 0)
		memcpy(pNewBuffer, g_pDynarecCodeTable, g_dwNumStaticEntries * sizeof(CDynarecCode *));

	SAFE_DELETEARRAY(g_pDynarecCodeTable);
	g_pDynarecCodeTable = pNewBuffer;
	g_dwStaticCodeTableSize = dwNewSize;

	return TRUE;
}


// Add the compiled code to g_pDynarecCodeTable. Insert hacked instruction
// into RDRAM to allow us to quickly branch to our compiled code
void SR_AddCompiledCode(CDynarecCode *pCode, DWORD * pdwBase)
{
	DWORD dwNewInstr;
	
	if (!SR_EnsureTableSpace())
	{
		DBGConsole_Msg(0, "Warning - static code table size is too small!");
		// Should really abort, because we will keep trying to compile this op
		return;
	}

	pCode->dwEntry = g_dwNumStaticEntries;


	pCode->dwOriginalOp = *pdwBase;
	if (pCode->dwNumOps == 0)  // Don't bother using compiled code - this entry makes us just execute the original mips
	{
		g_dwNumSRFailed++;
		dwNewInstr = make_op(OP_SRHACK_NOOPT) | pCode->dwEntry;
	}
	else                      // Use the compiled code
		dwNewInstr = make_op(OP_SRHACK_UNOPT) | pCode->dwEntry;
	*pdwBase = dwNewInstr;

	g_pDynarecCodeTable[g_dwNumStaticEntries] = pCode;
	g_dwNumStaticEntries++;
}



#define PROLOGUE()								\

// EPILOGUE - set up g_pPCMemBase
#define EPILOGUE(dwPC)							\
	MOVI(EAX_CODE, dwPC);						\
	MOV_MEM_REG(&g_dwPC, EAX_CODE);				\
	MOV(ECX_CODE, EAX_CODE);					\
	SHRI(EAX_CODE, 18);							\
	/* call dword ptr [g_ReadAddressLookupTable + eax*4] */ \
	pCode->EmitBYTE(0xFF);						\
	pCode->EmitBYTE(0x14);						\
	pCode->EmitBYTE(0x85);						\
	pCode->EmitDWORD((DWORD)g_ReadAddressLookupTable);		\
	MOV_MEM_REG(&g_pPCMemBase, EAX_CODE);		\
	RET();  


void SR_AllocateCodeBuffer(CDynarecCode * pCode)
{
	// Round up to 16 byte boundry
	g_dwGlobalBufferPtr = (g_dwGlobalBufferPtr + 15) & (~15);

	// This is a bit of a hack. We assume that no single entry will generate more than 
	// 32k of storage. If there appear to be problems with this assumption, this
	// value can be enlarged
    printf( "Perhaps Memory needed\n" );
	if (g_dwGlobalBufferPtr + 32768 > g_dwGlobalBufferSize)
	{
        printf( "Memory needed\n" );
        if( g_pGlobalBuffer != NULL )
        {
            printf( "Da ist der Speicher jetzt aber weg\n" );
            exit(-1);
        }
		// Increase by 1MB
		LPVOID pNewAddress;

		g_dwGlobalBufferSize += 512 * 512;
		/*KOS
		pNewAddress = VirtualAlloc(g_pGlobalBuffer, g_dwGlobalBufferSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		*/
		g_pGlobalBuffer = (BYTE *)malloc( g_dwGlobalBufferSize );
		if (pNewAddress != g_pGlobalBuffer)
		{
			DBGConsole_Msg(0, "SR Buffer base address has changed 0x%08x 0x%08x",
				g_pGlobalBuffer, pNewAddress);
		}
		else
		{
			DBGConsole_Msg(0, "Allocated %dMB of storage for dynarec buffer",
				g_dwGlobalBufferSize / (1024*1024));
		}

	}
    printf( "CodeBuffer assigned %d (%d) - %d\n", g_pGlobalBuffer, g_dwGlobalBufferPtr, g_dwGlobalBufferSize );
	pCode->pCodeBuffer = &g_pGlobalBuffer[g_dwGlobalBufferPtr];
	pCode->dwBuffSize = 0;
	pCode->dwCurrentPos = 0;   // Start of buffer
}

void SR_SetCodeBuffer(CDynarecCode * pCode)
{
	g_dwGlobalBufferPtr += pCode->dwCurrentPos;
}

CDynarecCode * SR_CompileCode(DWORD dwPC)
{
	DWORD *pdwPCMemBase;

	g_dwNumSRCompiled++;
	
	if (InternalReadAddress(dwPC, (void**)&pdwPCMemBase) == MEM_UNUSED)
		return NULL;

	CDynarecCode *pCode = new CDynarecCode(dwPC, pdwPCMemBase[0]);
	if(pCode == NULL)
		return NULL;
	
	// Initialise various fields
	SR_AllocateCodeBuffer(pCode);


	//#ifdef DAEDALUS_LOG
	//DPF(DEBUG_DYNREC, "Compiling block starting at 0x%08x:", pCode->dwStartPC);
	//#endif

	DWORD dwOp;
	DWORD dwFlags;
	BOOL bEmitOk;

	DWORD *pdwPC = pdwPCMemBase;
	// Loop forever, incrementing the PC each loop. We break out of the loop when
	// we reach an operation that we can't compile
	for ( ;; pdwPC++)
	{
		dwOp = *pdwPC;

		dwFlags = 0;			// Reset flags to 0 - Emit instructions don't do this

		// Emit the actual instruction
		bEmitOk = SR_EmitInstruction[op(dwOp)](pCode, dwOp, &dwFlags);

		// Exit if instruction wasn't emitted ok
		if (pCode->dwWarn != 0)
			break;

		if (!bEmitOk)
			break;

		pCode->dwNumOps++;

		// Exit if one or more of the flags were set (later we can selectively add
		// code to handle counter adjustments here)
		if (dwFlags != 0)
			break;
	}

	#ifdef DAEDALUS_LOG
	char opstr[300];
	DWORD dwCurrPC = pCode->dwStartPC+pCode->dwNumOps*4;
	SprintOpCodeInfo(opstr, dwCurrPC, dwOp);
	DPF(DEBUG_DYNREC_STOPPAGE, "Stopped On: 0x%08x: %s", dwCurrPC, opstr);
	#endif


	// Ensure all registers are stored
	if (pCode->dwNumOps > 0)
	{
		pCode->dwEndPC = pCode->dwStartPC + ((pCode->dwNumOps-1)*4);

		EPILOGUE(pCode->dwEndPC);
	}

	// Check if there was an error along the way (e.g. memory allocation failed)
	if (pCode->dwWarn != 0)
	{
		DBGConsole_Msg(DEBUG_DYNREC, "Static Recompilation failed!");
		SAFE_DELETEARRAY(pCode->pCodeBuffer);
		delete pCode;
		return NULL;
	}


	SR_AddCompiledCode(pCode, pdwPCMemBase);

	SR_SetCodeBuffer(pCode);

	return pCode;
}


void SR_OptimiseCode(CDynarecCode *pCode, DWORD dwLevel)
{
	// Optimise the code more aggressively
	DWORD dwOp;
	DWORD dwFlags;
	BOOL bEmitOk;
	DWORD *pdwPCMemBase = (DWORD *)ReadAddress(pCode->dwStartPC);
	CDynarecCode * pCodeTarget = NULL;

	g_dwNumSROptimised++;

	/*if (dwLevel > 15)
	{
		DBGConsole_Msg(0, "Optimisation recursion is too deep!");
		return;
	}*/
	// Avoid re-optimizing
	if (pCode->dwOptimiseLevel >= 1)
		return;

	pCode->dwOptimiseLevel = 1;

	// Branch target set from previous unoptimized compilation!
	if (pCode->dwBranchTarget != ~0)
	{
		// Get the opcode of the target pc..
		DWORD * pdwTarget;

		if (InternalReadAddress(pCode->dwBranchTarget, (void **)&pdwTarget))
		{
			DWORD dwOp = pdwTarget[0];

			if (op(dwOp) == OP_SRHACK_UNOPT ||
				op(dwOp) == OP_SRHACK_OPT)
			{
				DWORD dwEntry = dwOp & 0x00FFFFFF;
				if (dwEntry < g_dwNumStaticEntries)
				{
					pCodeTarget = g_pDynarecCodeTable[dwEntry];

					DPF(DEBUG_DYNREC, " Target entry is %d", dwEntry);
					DPF(DEBUG_DYNREC, "   StartAddress: 0x%08x, OptimizeLevel: %d", pCodeTarget->dwStartPC, pCodeTarget->dwOptimiseLevel);

					if (pCodeTarget->dwOptimiseLevel == 0)
					{
						SR_OptimiseCode(pCodeTarget, dwLevel+1);
					}
				}
			}
			else if (op(dwOp) != OP_SRHACK_NOOPT &&
				     op(dwOp) != OP_PATCH)
			{
				// Uncompiled code - compile now?
				pCodeTarget = SR_CompileCode(pCode->dwBranchTarget);
				if (pCodeTarget != NULL && pCodeTarget->dwNumOps > 0)
					SR_OptimiseCode(pCodeTarget, dwLevel+1);

				if (pCodeTarget->dwOptimiseLevel == 0)
					pCodeTarget = NULL;

			}
			else
			{
			//	DBGConsole_Msg(0, "0x%08x Huh??: 0x%08x (%d)", pCode->dwBranchTarget, dwOp, op(dwOp));
	
			}
		}
	}

	// Initialise various fields
	SR_AllocateCodeBuffer(pCode);		// Restart using new buffer

	pCode->dwNumOps = 0;			// The emit instructions expect this to be the number of ops since dwStartPC
	pCode->dwNumOptimised = 0;
	//pCode->dwCRC = 0;
	pCode->dwWarn = 0;
	pCode->dwBranchTarget = ~0;
	

	// Set the sp when the update count equals this value
	pCode->bSpCachedInESI = FALSE;
	pCode->dwSetSpPostUpdate = ~0;


	#ifdef DAEDALUS_LOG
	DPF(DEBUG_DYNREC, "");
	DPF(DEBUG_DYNREC, "");
	DPF(DEBUG_DYNREC, "************************************");
	DPF(DEBUG_DYNREC, "Optimising block starting at 0x%08x (%d hits, %d deep):", pCode->dwStartPC, pCode->dwCount, dwLevel);
	#endif


	PROLOGUE();

	// Clear the fp status
	SR_FP_Init(pCode);
	SR_InitMIPSRegInfo();

	// Display stats for this block before we optimise
	#ifdef DAEDALUS_LOG
	pCode->DisplayRegStats();
	#endif
	
	SR_Stat_Analyze(pCode);

	pCode->ResetStats();

	
	if (pCode->dwSetSpPostUpdate != ~0)
	{
		PUSH(ESI_CODE);
	}


	// Loop forever, incrementing the PC each loop. We break out of the loop when
	// we reach an operation that we can't compile
	dwFlags = 0;

	BOOL bKeepCompiling = TRUE;
	BOOL bEmitDelayedOp = FALSE;

	BOOL bHandleBranch = FALSE;
	
	while ( bKeepCompiling || bEmitDelayedOp )
	{
		g_dwFPTimeNow++;

		dwOp = pdwPCMemBase[pCode->dwNumOps];

		if (!pCode->bSpCachedInESI && pCode->dwRegUpdates[REG_sp] == pCode->dwSetSpPostUpdate)
		{
			DPF(DEBUG_DYNREC, "  Setting sp in ESI after update %d", pCode->dwRegUpdates[REG_sp]);
			
			pCode->bSpCachedInESI = TRUE;
			
			// Calculate SP address and store in esi:
			// Will use cached value if present
			LoadMIPSLo(pCode, EAX_CODE, REG_sp);

			MOV(ECX_CODE, EAX_CODE);			// For fastcall

			// Get top bits (offset into table) in ECX
			SHRI(EAX_CODE, 18);

			// call dword ptr [g_ReadAddressLookupTable + eax*4]
			pCode->EmitBYTE(0xFF);
			pCode->EmitBYTE(0x14);
			pCode->EmitBYTE(0x85);		// 0x8d for ecx
			pCode->EmitDWORD((DWORD)g_ReadAddressLookupTable);		// Won't work if g_ReadAddressLookupTable recreated

			MOV(ESI_CODE, EAX_CODE);

		}


		dwFlags = 0;			// Reset flags to 0 - Emit instructions don't do this


		DWORD dwNumOptimisedCheck = pCode->dwNumOptimised;


		SR_PreEmitCachedRegCheck(pCode);

			//SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

			// Emit the actual instruction
			bEmitOk = SR_EmitInstruction[op(dwOp)](pCode, dwOp, &dwFlags);

			// Exit if instruction wasn't emitted ok
			if (!bEmitOk || pCode->dwWarn != 0)
			{
				bKeepCompiling = FALSE;
				break;
			}
		
			BOOL bOptimised = pCode->dwNumOptimised != dwNumOptimisedCheck;
		
			#ifdef DAEDALUS_LOG
			char opstr[300];
			DWORD dwCurrPC = pCode->dwStartPC+pCode->dwNumOps*4;
			SprintOpCodeInfo(opstr, dwCurrPC, dwOp);
			DPF(DEBUG_DYNREC, "0x%08x: %c %s", dwCurrPC, bOptimised ? '*' : ' ', opstr);
			#endif
		
		SR_PostEmitCachedRegCheck(pCode, bOptimised);

		
		pCode->dwNumOps++;

		// Exit if we've emitted a delayed op
		if (bEmitDelayedOp)
			break;

		// Exit if one or more of the flags were set (later we can selectively add
		// code to handle counter adjustments here)


		if (dwFlags & SR_FLAGS_BRANCHES)
		{
			bEmitDelayedOp = TRUE;
			if (dwFlags & SR_FLAGS_HANDLE_BRANCH)
				bHandleBranch = TRUE;
		}
		else if (dwFlags != 0)
		{
			// Don't stop compiling!
			break;
		}
	}



	// Flush all FP regs that are currently cached
	SR_FP_FlushAllRegs();

	
	if (pCode->dwSetSpPostUpdate != ~0)
	{
		POP(ESI_CODE);
	}


	SR_FlushMipsRegs(pCode);

	SR_RestoreIntelRegs(pCode);


	
	// Ok, so we've now flushed all the registers
	// If we're executing an internal branch, execute it now
	if (bEmitDelayedOp)
	{
		DPF(DEBUG_DYNREC, "Emitting code to update delay info...");
		// Need to clear g_nDelay here, or set to EXEC_DELAY if it is currently DO_DELAY

		BOOL bHandled = FALSE;
		//
		if (bHandleBranch)
		{
			LONG nJump;

			if (pCodeTarget != NULL)
			{
				// If NO_DELAY, skip all this, and return
				MOV_REG_MEM(EAX_CODE, &g_nDelay);
				TEST(EAX_CODE, EAX_CODE);			// NO_DELAY == 0, 
				
				JE(5+5+2+2+2+
				   10+5+
				   10);			// ->branch_not_taken

				MOVI(ECX_CODE, pCode->dwNumOps);		// 5
				pCode->EmitBYTE(0xB8); pCode->EmitDWORD((DWORD)SR_CheckStuff);	// Copy p to eax - 5
				CALL_EAX();								// 2
				TEST(EAX_CODE, EAX_CODE);				// 2
				JNE(10+5);		//-> stuff_to_do				// 2

				// Set delay instruction to NO_DELAY - have we handled the op?
				SetVar32(g_nDelay, NO_DELAY);		// 10 bytes
				// Branch to the top of the code..
				nJump = pCodeTarget->pCodeBuffer - (pCode->pCodeBuffer + pCode->dwCurrentPos + 5);		// Plus 5 for the jump
				JMP_DIRECT((DWORD)nJump);		// 5 bytes

// stuff_to_do
				// If g_nDelay was DO_DELAY, we come here
				SetVar32(g_nDelay, EXEC_DELAY);		// 10 bytes
// branch_not_taken
				// No delay, because the jump was not taken
				// If g_nDelay == NO_DELAY, then we jump here

				bHandled = TRUE;
			}
		}
		else
		{
			DPF(DEBUG_DYNREC, "bHandleBranch == FALSE");
		}


		if (!bHandled)
		{
			DPF(DEBUG_DYNREC, "Unhanded branch");

			MOV_REG_MEM(EAX_CODE, &g_nDelay);
			CMPI(EAX_CODE, NO_DELAY);			// Delay set?
			
			JE(10);
			// Just let the core handle the jump for now...change from DO_DELAY to EXEC_DELAY
			SetVar32(g_nDelay, EXEC_DELAY);		// 10 bytes
		}
	}


	pCode->dwEndPC = pCode->dwStartPC + ((pCode->dwNumOps-1)*4);

	EPILOGUE(pCode->dwEndPC);


	// Check if there was an error along the way (e.g. memory allocation failed)
	if (pCode->dwWarn != 0)
	{
		DBGConsole_Msg(DEBUG_DYNREC, "!!Optimisation Recompilation failed!");		
		//delete pCode;
		return;
	}

	SR_SetCodeBuffer(pCode);


	{
		//DWORD dwOp = pdwPCMemBase[0];
		//if (!OP_IS_A_HACK(dwOp))
		//	g_pdwOriginalMips[pCode->dwInstrNum] = dwOp;

		pdwPCMemBase[0] = make_op(OP_SRHACK_OPT) | pCode->dwEntry;
	}
	


	DPF(DEBUG_DYNREC, "************************************");
	DPF(DEBUG_DYNREC, "");

}

// Initialise the status of what we know about all the mips registers
void SR_InitMIPSRegInfo()
{
	DWORD i;

	for (i = 0; i < 32; i++)
	{
		g_MIPSRegInfo[i].dwCachedIReg = ~0;
		g_MIPSRegInfo[i].bValid = FALSE;
		g_MIPSRegInfo[i].bDirty = FALSE;

		g_MIPSRegInfo[i].bLoUnk = TRUE;
		g_MIPSRegInfo[i].dwLoValue = 0;		// Ignored

		g_MIPSHiRegInfo[i].bDirty = FALSE;
		g_MIPSHiRegInfo[i].bUnk = TRUE;
		g_MIPSHiRegInfo[i].dwValue = 0;		// Ignored
	}

	// Init intel regs
	for (i = 0; i < 8; i++)
	{
		g_IntelRegInfo[i] = ~0;
	}
}


void SR_Stat_Analyze(CDynarecCode * pCode)
{
	DWORD i;
	
	// Determine which base reg to cache
	DWORD dwMaxBaseUseCount = 0;
	DWORD dwMaxBaseUseIndex = ~0;
	// Determine which general reg to cache
	DWORD dwMaxUseCount[3] = { 0,0,0 };
	DWORD dwMaxUseIndex[3] = { ~0,~0,~0 };

	// Ingore reg0 - start at reg 1
	for (i = 1; i < 32; i++)
	{
		// Base reg suitability?
		if (pCode->dwRegBaseUse[i] > 1 &&
			pCode->dwRegUpdates[i] == 0)
		{
			DPF(DEBUG_DYNREC, "  Mmm - %s looks nice for base caching with %d uses", 
				RegNames[i], pCode->dwRegBaseUse[i]);

			if (pCode->dwRegBaseUse[i] > dwMaxBaseUseCount)
			{
				dwMaxBaseUseCount = pCode->dwRegBaseUse[i];
				dwMaxBaseUseIndex = i;
			}
		}

		// Read reg suitability?
		DWORD dwUses = pCode->dwRegReads[i] +
					   pCode->dwRegUpdates[i] + 
					   pCode->dwRegBaseUse[i];

		// TODO: When more load/store ops are optimised, add base usage to count.
		// TODO: Writes are more expensive with more generic ops, becase we have to
		//       re-load the register is its contents have changed in the call!
		if (dwUses > 1)			
		{
			DPF(DEBUG_DYNREC, "  Mmm - %s looks nice for general caching with %d reads, %d writes, %d base uses", 
				RegNames[i], pCode->dwRegReads[i], pCode->dwRegUpdates[i], pCode->dwRegBaseUse[i]);

			if (dwUses > dwMaxUseCount[0])
			{
				dwMaxUseCount[2] = dwMaxUseCount[1];
				dwMaxUseIndex[2] = dwMaxUseIndex[1];
				
				dwMaxUseCount[1] = dwMaxUseCount[0];
				dwMaxUseIndex[1] = dwMaxUseIndex[0];
				
				dwMaxUseCount[0] = dwUses;
				dwMaxUseIndex[0] = i;
			}
			else if (dwUses > dwMaxUseCount[1])
			{
				dwMaxUseCount[2] = dwMaxUseCount[1];
				dwMaxUseIndex[2] = dwMaxUseIndex[1];
				
				dwMaxUseCount[1] = dwUses;
				dwMaxUseIndex[1] = i;
			}
			else if (dwUses > dwMaxUseCount[2])
			{			
				dwMaxUseCount[2] = dwUses;
				dwMaxUseIndex[2] = i;
			}
		}	
	}


	
	if (dwMaxBaseUseIndex != ~0)
	{
		DPF(DEBUG_DYNREC, "  Best register for base pointer caching is %s", RegNames[dwMaxBaseUseIndex]);
	}
	// Only bother if the register is used as a base more than once
	if (pCode->dwRegBaseUse[REG_sp] > 1)
	{
		// Double the "preference" for base caching of sp, because it avoids
		// having to call ReadAddress
		if (dwMaxUseIndex[2] != ~0 && (pCode->dwRegBaseUse[REG_sp]*2) < dwMaxUseCount[2])
		{
			DPF(DEBUG_DYNREC, "  Caching %s rather than sp/base",
				RegNames[dwMaxUseIndex[2]], pCode->dwRegUpdates[REG_sp]);
		}
		else
		{
			// If the register changes several times, ignore
			if (pCode->dwRegUpdates[REG_sp] > 1)
			{
				DPF(DEBUG_DYNREC, "  Unable to cache SP - %d updates", pCode->dwRegUpdates[REG_sp]);
			}
			else
			{
				DPF(DEBUG_DYNREC, "  Will cache sp in ESI after update %d", pCode->dwRegUpdates[REG_sp]);

				pCode->dwSetSpPostUpdate = pCode->dwRegUpdates[REG_sp];
			}
		}
	}


	
	if (dwMaxUseIndex[0] != ~0)
	{
		DPF(DEBUG_DYNREC, "  Best register for general caching is %s", RegNames[dwMaxUseIndex[0]]);

		// Cache read reg in EDI:
		// Save edi for future use
		PUSH(EDI_CODE);

		g_MIPSRegInfo[dwMaxUseIndex[0]].dwCachedIReg = EDI_CODE;
		g_MIPSRegInfo[dwMaxUseIndex[0]].bDirty = FALSE;
		g_MIPSRegInfo[dwMaxUseIndex[0]].bValid = FALSE;
		//g_MIPSRegInfo[dwMaxUseIndex[0]].bLoUnk = TRUE;
		//g_MIPSRegInfo[dwMaxUseIndex[0]].dwLoValue = 0;

		g_IntelRegInfo[EDI_CODE] = dwMaxUseIndex[0];
	}

	// Do this here, as EBX < ESI < EDI 
	if (pCode->dwSetSpPostUpdate == ~0 && dwMaxUseIndex[2] != ~0)
	{
		DPF(DEBUG_DYNREC, "  3rd best register for general caching is %s", RegNames[dwMaxUseIndex[2]]);

		// Cache read reg in ESI:
		// Save esi for future use
		PUSH(ESI_CODE);

		g_MIPSRegInfo[dwMaxUseIndex[2]].dwCachedIReg = ESI_CODE;
		g_MIPSRegInfo[dwMaxUseIndex[2]].bDirty = FALSE;
		g_MIPSRegInfo[dwMaxUseIndex[2]].bValid = FALSE;
		//g_MIPSRegInfo[dwMaxUseIndex[2]].bLoUnk = TRUE;
		//g_MIPSRegInfo[dwMaxUseIndex[2]].dwLoValue = 0;

		g_IntelRegInfo[ESI_CODE] = dwMaxUseIndex[2];


	}

	if (dwMaxUseIndex[1] != ~0)
	{
		DPF(DEBUG_DYNREC, "  2nd best register for general caching is %s", RegNames[dwMaxUseIndex[1]]);

		// Cache read reg in EBX:
		// Save ebx for future use
		PUSH(EBX_CODE);
	
		g_MIPSRegInfo[dwMaxUseIndex[1]].dwCachedIReg = EBX_CODE;
		g_MIPSRegInfo[dwMaxUseIndex[1]].bDirty = FALSE;
		g_MIPSRegInfo[dwMaxUseIndex[1]].bValid = FALSE;
		//g_MIPSRegInfo[dwMaxUseIndex[1]].bLoUnk = TRUE;
		//g_MIPSRegInfo[dwMaxUseIndex[1]].dwLoValue = 0;
		
		g_IntelRegInfo[EBX_CODE] = dwMaxUseIndex[1];
	}
}

void SR_FlushMipsRegs(CDynarecCode * pCode)
{
	DWORD i;

	// TODO: We'd need to flush hi regs here!
	for (i = 1; i < 32; i++)
	{
		if (g_MIPSHiRegInfo[i].bDirty && !g_MIPSHiRegInfo[i].bUnk)
		{
			// Writeback
			DPF(DEBUG_DYNREC, "Writing back info for %s/hi: 0x%08x", RegNames[i], g_MIPSHiRegInfo[i].dwValue);

			MOVI_MEM( (((BYTE *)&g_qwGPR[0]) + (hihalf(i)*4)), g_MIPSHiRegInfo[i].dwValue);

			g_MIPSHiRegInfo[i].bDirty = FALSE;
		}
	}

	DWORD iReg;
	// Pop regs from lowest to highest...
	for (iReg = 0; iReg < 8; iReg++)
	{
		// If we have cached a register, we may need to flush it
		if (g_IntelRegInfo[iReg] != ~0)
		{
			DWORD dwMReg = g_IntelRegInfo[iReg];

			if (g_MIPSRegInfo[dwMReg].bDirty)
			{
				if (g_MIPSRegInfo[dwMReg].bValid)
				{
					DPF(DEBUG_DYNREC, "Cached reg %s is dirty: flushing", RegNames[dwMReg]);
					
					MOV_MEM_REG((((BYTE *)&g_qwGPR[0]) + (lohalf(dwMReg)*4)), (WORD)iReg );

					// We could update the modcount here?
				}
				g_MIPSRegInfo[dwMReg].bDirty = FALSE;
			}
		}

	}
}



void SR_RestoreIntelRegs(CDynarecCode * pCode)
{
	DWORD iReg;
	// Pop regs from lowest to highest...
	for (iReg = 0; iReg < 8; iReg++)
	{
		// If we have cached a register, we may need to flush it
		if (g_IntelRegInfo[iReg] != ~0)
		{
			DWORD dwMReg = g_IntelRegInfo[iReg];

			g_MIPSRegInfo[dwMReg].bValid = FALSE;

			// Always restore!
			POP((WORD)iReg);
		}
	}
}



static DWORD g_dwWriteCheck[32];

void SR_PreEmitCachedRegCheck(CDynarecCode *pCode)
{
	DWORD i;

	// Record how many updates we've had
	for (i = 0; i < 32; i++)
	{
		g_dwWriteCheck[i] = pCode->dwRegUpdates[i];
	}
}

void SR_PostEmitCachedRegCheck(CDynarecCode *pCode, BOOL bOptimised)
{
	DWORD i;
	// If the operation was NOT optimised, and our cached register was written to, 
	// then we have to reload the value:
	if (!bOptimised)
	{
		// Also, check hi regs! g_MIPSHiRegInfo[i].bUnk
		for (i = 0; i < 32; i++)
		{
			if (g_dwWriteCheck[i] != pCode->dwRegUpdates[i])
			{
				DPF(DEBUG_DYNREC, "  Reg %s was updated", RegNames[i]);

				// See if the lo value is invalid
				if (g_MIPSRegInfo[i].dwCachedIReg != ~0)
				{
					g_MIPSRegInfo[i].bValid = FALSE;
				}

				// Se if the hi value we have is invalid
				if (!g_MIPSHiRegInfo[i].bUnk)
				{
					g_MIPSHiRegInfo[i].bUnk = TRUE;
					g_MIPSHiRegInfo[i].bDirty = FALSE;
				}
			}
		}
	}
}

// Returns non-zero if we should continue
DWORD SR_CheckStuff(DWORD dwNumCycles)
{
	// Increment count register
	if (g_pFirstCPUEvent->dwCount > dwNumCycles)
	{
		*(DWORD *)&g_qwCPR[0][C0_COUNT] = *(DWORD *)&g_qwCPR[0][C0_COUNT]+dwNumCycles;
		g_pFirstCPUEvent->dwCount-= dwNumCycles;

		return g_dwCPUStuffToDo;		// Don't return 0 - return flags if they're set
	}
	else
	{
		dwNumCycles = g_pFirstCPUEvent->dwCount - 1;
		
		*(DWORD *)&g_qwCPR[0][C0_COUNT] = *(DWORD *)&g_qwCPR[0][C0_COUNT]+ dwNumCycles;
		g_pFirstCPUEvent->dwCount = 1;

		return 1;
	}

}



















BOOL SR_Emit_Inline_Function(CDynarecCode *pCode, void* pFunction);

static void SR_Emit_Generic_R4300(CDynarecCode *pCode, DWORD dwOp, CPU_Instruction pF)
{

	// Flush all fp registers before a generic call
	if (pCode->dwOptimiseLevel >= 1)
	{
		SR_FlushMipsRegs(pCode);		// Valid flag updated after op "executed"
		SR_FP_FlushAllRegs();
	}

	// Set up dwOp
	MOVI(ECX_CODE, dwOp);

	// Lkb: I think it's better to only inline functions when dwOptimiseLevel
	// is greater than zero. This avoid inlining for functions that are only
	// ever hit once, saving buffer space and "one-off" overheads.
	if (pCode->dwOptimiseLevel < 1)
	{
		// Call function
		pCode->EmitBYTE(0xB8); pCode->EmitDWORD((DWORD)pF);	// Copy p to eax
		CALL_EAX();
	}
	else
	{
		if(!SR_Emit_Inline_Function(pCode, (void *)pF))
		{
			// Call function
			pCode->EmitBYTE(0xB8); pCode->EmitDWORD((DWORD)pF);	// Copy p to eax
			CALL_EAX();
		}
	}

}




BOOL SR_Emit_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	return FALSE;
}

BOOL SR_Emit_DBG_BreakPoint(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwBreakPoint = dwOp & 0x03FFFFFF;

	dwOp = g_BreakPoints[dwBreakPoint].dwOriginalOp;
	// Use the original opcode
	return SR_EmitInstruction[op(dwOp)](pCode, dwOp, pdwFlags);

}


BOOL SR_Emit_SRHack_UnOpt(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwEntry = dwOp & 0x03FFFFFF;
	if (g_pDynarecCodeTable[dwEntry] == NULL)
	{
		DBGConsole_Msg(DEBUG_DYNREC, "No compiled code here!.");
		return FALSE;
	}

	dwOp = g_pDynarecCodeTable[dwEntry]->dwOriginalOp;

	// Recurse, using the original opcode
	return SR_EmitInstruction[op(dwOp)](pCode, dwOp, pdwFlags);
}

BOOL SR_Emit_SRHack_Opt(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwEntry = dwOp & 0x03FFFFFF;
	if (g_pDynarecCodeTable[dwEntry] == NULL)
	{
		DBGConsole_Msg(DEBUG_DYNREC, "No compiled code here!.");
		return FALSE;
	}

	dwOp = g_pDynarecCodeTable[dwEntry]->dwOriginalOp;

	// Recurse, using the original opcode
	return SR_EmitInstruction[op(dwOp)](pCode, dwOp, pdwFlags);
}


BOOL SR_Emit_SRHack_NoOpt(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwEntry = dwOp & 0x03FFFFFF;

	dwOp = g_pDynarecCodeTable[dwEntry]->dwOriginalOp;
	// Recurse, using the original opcode
	return SR_EmitInstruction[op(dwOp)](pCode, dwOp, pdwFlags);
}

BOOL SR_Emit_Special(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	return SR_EmitSpecialInstruction[funct(dwOp)](pCode, dwOp, pdwFlags);
}

BOOL SR_Emit_RegImm(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	return SR_EmitRegImmInstruction[rt(dwOp)](pCode, dwOp, pdwFlags);
}

BOOL SR_Emit_CoPro0(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	return SR_EmitCop0Instruction[cop0fmt(dwOp)](pCode, dwOp, pdwFlags);
}

BOOL SR_Emit_CoPro1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{ 
	return SR_EmitCop1Instruction[cop0fmt(dwOp)](pCode, dwOp, pdwFlags);
}

#include "sr_branch.inl"
#include "sr_cop1.inl"
#include "sr_cop1_s.inl"
#include "sr_load.inl"
#include "sr_math.inl"
#include "sr_math_imm.inl"
#include "sr_store.inl"


BOOL SR_Emit_LL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_LLD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_LDC2(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_SC(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_SCD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_SDC2(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }




BOOL SR_Emit_BEQL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// We can't execute this op yet, as we need to ensure that dwPC is set correctly
	// if branch is not taken
	return FALSE;

}
BOOL SR_Emit_BNEL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// We can't execute this op yet, as we need to ensure that dwPC is set correctly
	// if branch is not taken
	return FALSE;
}

BOOL SR_Emit_BLEZL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// We can't execute this op yet, as we need to ensure that dwPC is set correctly
	// if branch is not taken
	return FALSE;
}

BOOL SR_Emit_BGTZL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// We can't execute this op yet, as we need to ensure that dwPC is set correctly
	// if branch is not taken
	return FALSE;
}



BOOL SR_Emit_DADDI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);

	SR_Stat_D_S(dwRT, dwRS);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_DADDI);
	return TRUE;
}

BOOL SR_Emit_DADDIU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);

	SR_Stat_D_S(dwRT, dwRS);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_DADDIU);
	return TRUE;
}

BOOL SR_Emit_LDL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_LDL);
	return TRUE;
}


BOOL SR_Emit_LDR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_LDR);
	return TRUE;
}



BOOL SR_Emit_LWL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);


	SR_Emit_Generic_R4300(pCode, dwOp, R4300_LWL);
	return TRUE;
}



BOOL SR_Emit_LWR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_LWR);
	return TRUE;
}

BOOL SR_Emit_LWU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_LWU);
	return TRUE;
}

BOOL SR_Emit_LD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_LD);
	return TRUE;
}



BOOL SR_Emit_SWL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Save_B_S(dwBase, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_SWL);
	return TRUE;
}


BOOL SR_Emit_SDL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Save_B_S(dwBase, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_SDL);
	return TRUE;
}

BOOL SR_Emit_SDR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Save_B_S(dwBase, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_SDR);
	return TRUE;
}

BOOL SR_Emit_SWR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Save_B_S(dwBase, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_SWR);
	return TRUE;
}

BOOL SR_Emit_CACHE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Ignore
	return TRUE;
}



BOOL SR_Emit_LDC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwBase = rs(dwOp);
	DWORD dwFT   = ft(dwOp);

	SR_Stat_Load_B(dwBase);
	SR_Stat_Single_D(dwFT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_LDC1);
	return TRUE;
}



BOOL SR_Emit_SDC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwBase = rs(dwOp);
	DWORD dwFT    = ft(dwOp);

	SR_Stat_Save_B(dwBase);
	SR_Stat_Single_S(dwFT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_SDC1);
	return TRUE;
}

BOOL SR_Emit_SD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Save_B_S(dwBase, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_SD);
	return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////


BOOL SR_Emit_Special_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }


BOOL SR_Emit_Special_SLLV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_SLLV);
	return TRUE;
}

BOOL SR_Emit_Special_SRLV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_SRLV);
	return TRUE;
}

BOOL SR_Emit_Special_SRAV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_SRAV);
	return TRUE;
}

BOOL SR_Emit_Special_DSLLV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSLLV);
	return TRUE;
}

BOOL SR_Emit_Special_DSRLV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSRLV);
	return TRUE;
}

BOOL SR_Emit_Special_DSRAV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSRAV);
	return TRUE;
}


BOOL SR_Emit_Special_SYSCALL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_Special_BREAK(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_Special_SYNC(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }



// Move to MultHi/Lo is not very common
BOOL SR_Emit_Special_MTHI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);

	SR_Stat_S(dwRS);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_MTHI);
	return TRUE;
}


BOOL SR_Emit_Special_MTLO(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);

	SR_Stat_S(dwRS);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_MTLO);
	return TRUE;
}


BOOL SR_Emit_Special_MULT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	SR_Stat_S_S(dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_MULT);
	return TRUE;
}



BOOL SR_Emit_Special_DIV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	SR_Stat_S_S(dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DIV);
	return TRUE;
}


BOOL SR_Emit_Special_DIVU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	SR_Stat_S_S(dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DIVU);
	return TRUE;
}

BOOL SR_Emit_Special_DMULT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	SR_Stat_S_S(dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DMULT);
	return TRUE;
}

BOOL SR_Emit_Special_DMULTU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	SR_Stat_S_S(dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DMULTU);
	return TRUE;
}

BOOL SR_Emit_Special_DDIV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	SR_Stat_S_S(dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DDIV);
	return TRUE;
}

BOOL SR_Emit_Special_DDIVU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	SR_Stat_S_S(dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DDIVU);
	return TRUE;
}

BOOL SR_Emit_Special_TGE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_Special_TGEU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_Special_TLT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_Special_TLTU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_Special_TEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_Special_TNE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }






BOOL SR_Emit_Special_DSLL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	//DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S(dwRD, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSLL);
	return TRUE;
}

BOOL SR_Emit_Special_DSRL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	//DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S(dwRD, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSRL);
	return TRUE;
}

BOOL SR_Emit_Special_DSRA(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	//DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S(dwRD, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSRA);
	return TRUE;
}

BOOL SR_Emit_Special_DSLL32(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	//DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S(dwRD, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSLL32);
	return TRUE;
}

BOOL SR_Emit_Special_DSRL32(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	//DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S(dwRD, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSRL32);
	return TRUE;
}

BOOL SR_Emit_Special_DSRA32(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	//DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S(dwRD, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSRA32);
	return TRUE;
}






BOOL SR_Emit_Special_DADD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DADD);
	return TRUE;
}

BOOL SR_Emit_Special_DADDU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DADDU);
	return TRUE;
}

BOOL SR_Emit_Special_DSUB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSUB);
	return TRUE;
}

BOOL SR_Emit_Special_DSUBU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_DSUBU);
	return TRUE;
}




BOOL SR_Emit_Special_NOR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_NOR);
	return TRUE;
}



////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

BOOL SR_Emit_RegImm_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }

BOOL SR_Emit_RegImm_BLTZL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_RegImm_BGEZL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }

BOOL SR_Emit_RegImm_TGEI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_RegImm_TGEIU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_RegImm_TLTI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_RegImm_TLTIU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_RegImm_TEQI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_RegImm_TNEI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }

BOOL SR_Emit_RegImm_BLTZAL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_RegImm_BGEZAL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_RegImm_BLTZALL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
BOOL SR_Emit_RegImm_BGEZALL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

BOOL SR_Emit_Cop0_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }

BOOL SR_Emit_Cop0_MFC0(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Don't emit is this is refering to COUNT, as it's not maintained
	// during the execution of recompiled code
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	//g_qwGPR[dwRT] = (s64)(s32)g_qwCPR[0][dwFS];

	switch (dwFS)
	{
		case C0_COUNT:
			// Could emit instruction to set count here?
			return FALSE;
	}
	
	SR_Stat_D(dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop0_MFC0);
	return TRUE;
}

BOOL SR_Emit_Cop0_MTC0(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){

	// Don't emit is this is refering to COUNT, as it's not maintained
	// during the execution of recompiled code
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	//g_qwCPR[0][dwFS] = g_qwGPR[dwRT];

	switch (dwFS)
	{
		case C0_COMPARE:
		case C0_COUNT:
			// Could emit instruction to set count here?
			return FALSE;
	}
	
	SR_Stat_S(dwRT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop0_MTC0);
	return TRUE;
}

BOOL SR_Emit_Cop0_TLB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////


BOOL SR_Emit_Cop1_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }

BOOL SR_Emit_Cop1_BCInstr(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	return SR_EmitCop1BC1Instruction[bc(dwOp)](pCode, dwOp, pdwFlags);
}

BOOL SR_Emit_Cop1_SInstr(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	return SR_EmitCop1SInstruction[funct(dwOp)](pCode, dwOp, pdwFlags);
}

BOOL SR_Emit_Cop1_DInstr(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	return SR_EmitCop1DInstruction[funct(dwOp)](pCode, dwOp, pdwFlags);
}

BOOL SR_Emit_Cop1_WInstr(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{

	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_D_S(dwFD, dwFS);

	switch (funct(dwOp))
	{
		case OP_CVT_S:
			SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_W_CVT_S);
			return TRUE;

		case OP_CVT_D:
			SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_W_CVT_D);
			return TRUE;
	}
	return FALSE;
}



BOOL SR_Emit_Cop1_DMFC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	SR_Stat_D(dwRT);
	SR_Stat_Single_S(dwFS);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_DMFC1);

	return TRUE;
}


BOOL SR_Emit_Cop1_DMTC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	SR_Stat_S(dwRT);
	SR_Stat_Single_D(dwFS);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_DMTC1);
	return TRUE;
}




BOOL SR_Emit_Cop1_LInstr(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }


BOOL SR_BC1_BC1F(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	
	*pdwFlags |= SR_FLAGS_BRANCHES;

	// Set PC = StartPC + NumOps * 4
	SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_BC1_BC1F);
	return TRUE;
}

BOOL SR_BC1_BC1T(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	
	*pdwFlags |= SR_FLAGS_BRANCHES;

	// Set PC = StartPC + NumOps * 4
	SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_BC1_BC1T);
	return TRUE;
}

BOOL SR_BC1_BC1FL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Can't yet execute Likely Instructions
	return FALSE;
	*pdwFlags |= SR_FLAGS_BRANCHES;

	// Set PC = StartPC + NumOps * 4
	SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_BC1_BC1FL);
	return TRUE;
}

BOOL SR_BC1_BC1TL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Can't yet execute Likely Instructions
	return FALSE;
	*pdwFlags |= SR_FLAGS_BRANCHES;

	// Set PC = StartPC + NumOps * 4
	SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_BC1_BC1TL);
	return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////



BOOL SR_Emit_Cop1_S_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }

BOOL SR_Emit_Cop1_S_ABS(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_D_S(dwFD, dwFS);
	
	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_ABS);
	return TRUE;
}


BOOL SR_Emit_Cop1_S_ROUND_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Long_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_ROUND_L);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_TRUNC_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Long_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_TRUNC_L);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_CEIL_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Long_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_CEIL_L);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_FLOOR_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Long_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_FLOOR_L);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_ROUND_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Word_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_ROUND_W);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_TRUNC_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Word_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_TRUNC_W);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_CEIL_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Word_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_CEIL_W);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_FLOOR_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Word_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_FLOOR_W);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_CVT_D(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Double_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_CVT_D);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_CVT_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Word_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_CVT_W);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_CVT_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_S(dwFS);
	SR_Stat_Long_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_CVT_L);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_F(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_F);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_UN(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_UN);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_EQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	
	SR_Stat_Single_S_S(dwFS, dwFT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_EQ);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_UEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_UEQ);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_OLT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_OLT);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_ULT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_ULT);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_OLE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_OLE);
	return TRUE;
}


BOOL SR_Emit_Cop1_S_ULE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_ULE);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_SF(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_SF);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_NGLE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_NGLE);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_SEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_SEQ);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_NGL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_NGL);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_LT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	
	SR_Stat_Single_S_S(dwFS, dwFT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_LT);
	return TRUE;
}
BOOL SR_Emit_Cop1_S_NGE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_NGE);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_LE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	
	SR_Stat_Single_S_S(dwFS, dwFT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_LE);
	return TRUE;
}

BOOL SR_Emit_Cop1_S_NGT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Single_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_NGT);
	return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

BOOL SR_Emit_Cop1_D_Unk(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){ return FALSE; }

BOOL SR_Emit_Cop1_D_ADD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_D_S_S(dwFD, dwFS, dwFT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_ADD);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_SUB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_D_S_S(dwFD, dwFS, dwFT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_SUB);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_MUL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_D_S_S(dwFD, dwFS, dwFT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_MUL);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_DIV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_D_S_S(dwFD, dwFS, dwFT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_DIV);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_SQRT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_D_S(dwFD, dwFS);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_SQRT);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_ABS(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_D_S(dwFD, dwFS);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_ABS);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_MOV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_D_S(dwFD, dwFS);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_MOV);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_NEG(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_D_S(dwFD, dwFS);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_NEG);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_ROUND_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Long_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_ROUND_L);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_TRUNC_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Long_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_TRUNC_L);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_CEIL_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Long_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_CEIL_L);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_FLOOR_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Long_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_FLOOR_L);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_ROUND_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Word_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_ROUND_W);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_TRUNC_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Word_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_TRUNC_W);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_CEIL_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Word_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_CEIL_W);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_FLOOR_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Word_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_FLOOR_W);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_CVT_S(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Single_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_CVT_S);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_CVT_W(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Word_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_CVT_W);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_CVT_L(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Double_S(dwFS);
	SR_Stat_Long_D(dwFD);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_CVT_L);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_F(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_F);
	return TRUE;
}
BOOL SR_Emit_Cop1_D_UN(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_UN);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_EQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	
	SR_Stat_Double_S_S(dwFS, dwFT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_EQ);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_UEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_UEQ);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_OLT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_OLT);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_ULT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_ULT);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_OLE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_OLE);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_ULE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_ULE);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_SF(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_SF);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_NGLE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_NGLE);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_SEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_SEQ);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_NGL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_NGL);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_LT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	
	SR_Stat_Double_S_S(dwFS, dwFT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_LT);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_NGE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_NGE);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_LE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	
	SR_Stat_Double_S_S(dwFS, dwFT);

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_LE);
	return TRUE;
}

BOOL SR_Emit_Cop1_D_NGT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_Double_Unk();

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_D_NGT);
	return TRUE;
}




