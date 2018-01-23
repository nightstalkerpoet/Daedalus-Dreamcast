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

#ifndef __DAEDALUS_R4300_H__
#define __DAEDALUS_R4300_H__

#include "windef.h"

typedef void (*CPU_Instruction)(DWORD dwOp);



#define WARN_NOEXIST(inf) { outputMessage( MSG_ERROR, "Instruction Unknown: %s", inf ); CPU_Halt("Instruction Unknown"); }
#define WARN_NOIMPL(op) { outputMessage( MSG_ERROR, "Instruction Not Implemented: %s", op ); CPU_Halt("Instruction Not Implemented"); }



#define ToInt(x)	*((int *)&x)
#define ToQuad(x)	*((u64 *)&x)
#define ToFloat(x)	*((float *)&x)
#define ToDouble(x)	*((long double *)&x)

void R4300_SetSR(DWORD dwNewValue);

#ifdef _DEBUG
inline void CHECK_R0() {}
/*inline void CHECK_R0()
{
	if (g_qwGPR[0] != 0)
	{
		DBGConsole_Msg(0, "Warning: Attempted write to r0!");
		g_qwGPR[0] = 0;			// Ensure r0 is always zero (easier than trapping writes?)
	}
}*/

#else
inline void CHECK_R0() {}
#endif

extern CPU_Instruction R4300Instruction[64];
extern CPU_Instruction R4300SpecialInstruction[64];


void R4300_Unk(DWORD dwOp);
void R4300_Special(DWORD dwOp);
void R4300_RegImm(DWORD dwOp);
void R4300_J(DWORD dwOp);
void R4300_JAL(DWORD dwOp);
void R4300_BEQ(DWORD dwOp);
void R4300_BNE(DWORD dwOp);
void R4300_BLEZ(DWORD dwOp);
void R4300_BGTZ(DWORD dwOp);
void R4300_ADDI(DWORD dwOp);
void R4300_ADDIU(DWORD dwOp);
void R4300_SLTI(DWORD dwOp);
void R4300_SLTIU(DWORD dwOp);
void R4300_ANDI(DWORD dwOp);
void R4300_ORI(DWORD dwOp);
void R4300_XORI(DWORD dwOp);
void R4300_LUI(DWORD dwOp);
void R4300_CoPro0(DWORD dwOp);
void R4300_CoPro1(DWORD dwOp);
void R4300_CoPro1_Disabled(DWORD dwOp);
void R4300_SRHack_UnOpt(DWORD dwOp);
void R4300_SRHack_Opt(DWORD dwOp);
void R4300_SRHack_NoOpt(DWORD dwOp);
void R4300_BEQL(DWORD dwOp);
void R4300_BNEL(DWORD dwOp);
void R4300_BLEZL(DWORD dwOp);
void R4300_BGTZL(DWORD dwOp);
void R4300_DADDI(DWORD dwOp);
void R4300_DADDIU(DWORD dwOp);
void R4300_LDL(DWORD dwOp);
void R4300_LDR(DWORD dwOp);
void R4300_Patch(DWORD dwOp);
void R4300_LB(DWORD dwOp);
void R4300_LH(DWORD dwOp);
void R4300_LWL(DWORD dwOp);
void R4300_LW(DWORD dwOp);
void R4300_LBU(DWORD dwOp);
void R4300_LHU(DWORD dwOp);
void R4300_LWR(DWORD dwOp);
void R4300_LWU(DWORD dwOp);
void R4300_SB(DWORD dwOp);
void R4300_SH(DWORD dwOp);
void R4300_SWL(DWORD dwOp);
void R4300_SW(DWORD dwOp);
void R4300_SDL(DWORD dwOp);
void R4300_SDR(DWORD dwOp);
void R4300_SWR(DWORD dwOp);
void R4300_CACHE(DWORD dwOp);
void R4300_LL(DWORD dwOp);
void R4300_LWC1(DWORD dwOp);
void R4300_LLD(DWORD dwOp);
void R4300_LDC1(DWORD dwOp);
void R4300_LDC2(DWORD dwOp);
void R4300_LD(DWORD dwOp);
void R4300_SC(DWORD dwOp);
void R4300_SWC1(DWORD dwOp);
void R4300_DBG_Bkpt(DWORD dwOp);
void R4300_SCD(DWORD dwOp);
void R4300_SDC1(DWORD dwOp);
void R4300_SDC2(DWORD dwOp);
void R4300_SD(DWORD dwOp);


void R4300_Special_Unk(DWORD dwOp);
void R4300_Special_SLL(DWORD dwOp);
void R4300_Special_SRL(DWORD dwOp);
void R4300_Special_SRA(DWORD dwOp);
void R4300_Special_SLLV(DWORD dwOp);
void R4300_Special_SRLV(DWORD dwOp);
void R4300_Special_SRAV(DWORD dwOp);
void R4300_Special_JR_CheckBootAddress(DWORD dwOp);		// Special version to check for jump to game boot address
void R4300_Special_JR(DWORD dwOp);
void R4300_Special_JALR(DWORD dwOp);
void R4300_Special_SYSCALL(DWORD dwOp);
void R4300_Special_BREAK(DWORD dwOp);
void R4300_Special_SYNC(DWORD dwOp);
void R4300_Special_MFHI(DWORD dwOp);
void R4300_Special_MTHI(DWORD dwOp);
void R4300_Special_MFLO(DWORD dwOp);
void R4300_Special_MTLO(DWORD dwOp);
void R4300_Special_DSLLV(DWORD dwOp);
void R4300_Special_DSRLV(DWORD dwOp);
void R4300_Special_DSRAV(DWORD dwOp);
void R4300_Special_MULT(DWORD dwOp);
void R4300_Special_MULTU(DWORD dwOp);
void R4300_Special_DIV(DWORD dwOp);
void R4300_Special_DIVU(DWORD dwOp);
void R4300_Special_DMULT(DWORD dwOp);
void R4300_Special_DMULTU(DWORD dwOp);
void R4300_Special_DDIV(DWORD dwOp);
void R4300_Special_DDIVU(DWORD dwOp);
void R4300_Special_ADD(DWORD dwOp);
void R4300_Special_ADDU(DWORD dwOp);
void R4300_Special_SUB(DWORD dwOp);
void R4300_Special_SUBU(DWORD dwOp);
void R4300_Special_AND(DWORD dwOp);
void R4300_Special_OR(DWORD dwOp);
void R4300_Special_XOR(DWORD dwOp);
void R4300_Special_NOR(DWORD dwOp);
void R4300_Special_SLT(DWORD dwOp);
void R4300_Special_SLTU(DWORD dwOp);
void R4300_Special_DADD(DWORD dwOp);
void R4300_Special_DADDU(DWORD dwOp);
void R4300_Special_DSUB(DWORD dwOp);
void R4300_Special_DSUBU(DWORD dwOp);
void R4300_Special_TGE(DWORD dwOp);
void R4300_Special_TGEU(DWORD dwOp);
void R4300_Special_TLT(DWORD dwOp);
void R4300_Special_TLTU(DWORD dwOp);
void R4300_Special_TEQ(DWORD dwOp);
void R4300_Special_TNE(DWORD dwOp);
void R4300_Special_DSLL(DWORD dwOp);
void R4300_Special_DSRL(DWORD dwOp);
void R4300_Special_DSRA(DWORD dwOp);
void R4300_Special_DSLL32(DWORD dwOp);
void R4300_Special_DSRL32(DWORD dwOp);
void R4300_Special_DSRA32(DWORD dwOp);


void R4300_RegImm_Unk(DWORD dwOp);
void R4300_RegImm_BLTZ(DWORD dwOp);
void R4300_RegImm_BGEZ(DWORD dwOp);
void R4300_RegImm_BLTZL(DWORD dwOp);
void R4300_RegImm_BGEZL(DWORD dwOp);
void R4300_RegImm_TGEI(DWORD dwOp);
void R4300_RegImm_TGEIU(DWORD dwOp);
void R4300_RegImm_TLTI(DWORD dwOp);
void R4300_RegImm_TLTIU(DWORD dwOp);
void R4300_RegImm_TEQI(DWORD dwOp);
void R4300_RegImm_TNEI(DWORD dwOp);
void R4300_RegImm_BLTZAL(DWORD dwOp);
void R4300_RegImm_BGEZAL(DWORD dwOp);
void R4300_RegImm_BLTZALL(DWORD dwOp);
void R4300_RegImm_BGEZALL(DWORD dwOp);


void R4300_Cop0_Unk(DWORD dwOp);
void R4300_Cop0_MFC0(DWORD dwOp);
void R4300_Cop0_MTC0(DWORD dwOp);
void R4300_Cop0_TLB(DWORD dwOp);


void R4300_TLB_Unk(DWORD dwOp);
void R4300_TLB_TLBR(DWORD dwOp);
void R4300_TLB_TLBWI(DWORD dwOp);
void R4300_TLB_TLBWR(DWORD dwOp);
void R4300_TLB_TLBP(DWORD dwOp);
void R4300_TLB_ERET(DWORD dwOp);



void R4300_Cop1_Unk(DWORD dwOp);
void R4300_Cop1_MFC1(DWORD dwOp);
void R4300_Cop1_DMFC1(DWORD dwOp);
void R4300_Cop1_CFC1(DWORD dwOp);
void R4300_Cop1_MTC1(DWORD dwOp);
void R4300_Cop1_DMTC1(DWORD dwOp);
void R4300_Cop1_CTC1(DWORD dwOp);
void R4300_Cop1_BCInstr(DWORD dwOp);
void R4300_Cop1_SInstr(DWORD dwOp);
void R4300_Cop1_DInstr(DWORD dwOp);
void R4300_Cop1_WInstr(DWORD dwOp);
void R4300_Cop1_LInstr(DWORD dwOp);

void R4300_BC1_BC1F(DWORD dwOp);
void R4300_BC1_BC1T(DWORD dwOp);
void R4300_BC1_BC1FL(DWORD dwOp);
void R4300_BC1_BC1TL(DWORD dwOp);


void R4300_Cop1_S_Unk(DWORD dwOp);
void R4300_Cop1_S_ADD(DWORD dwOp);
void R4300_Cop1_S_SUB(DWORD dwOp);
void R4300_Cop1_S_MUL(DWORD dwOp);
void R4300_Cop1_S_DIV(DWORD dwOp);
void R4300_Cop1_S_SQRT(DWORD dwOp);
void R4300_Cop1_S_ABS(DWORD dwOp);
void R4300_Cop1_S_MOV(DWORD dwOp);
void R4300_Cop1_S_NEG(DWORD dwOp);
void R4300_Cop1_S_ROUND_L(DWORD dwOp);
void R4300_Cop1_S_TRUNC_L(DWORD dwOp);
void R4300_Cop1_S_CEIL_L(DWORD dwOp);
void R4300_Cop1_S_FLOOR_L(DWORD dwOp);
void R4300_Cop1_S_ROUND_W(DWORD dwOp);
void R4300_Cop1_S_TRUNC_W(DWORD dwOp);
void R4300_Cop1_S_CEIL_W(DWORD dwOp);
void R4300_Cop1_S_FLOOR_W(DWORD dwOp);
void R4300_Cop1_S_CVT_D(DWORD dwOp);
void R4300_Cop1_S_CVT_W(DWORD dwOp);
void R4300_Cop1_S_CVT_L(DWORD dwOp);
void R4300_Cop1_S_F(DWORD dwOp);
void R4300_Cop1_S_UN(DWORD dwOp);
void R4300_Cop1_S_EQ(DWORD dwOp);
void R4300_Cop1_S_UEQ(DWORD dwOp);
void R4300_Cop1_S_OLT(DWORD dwOp);
void R4300_Cop1_S_ULT(DWORD dwOp);
void R4300_Cop1_S_OLE(DWORD dwOp);
void R4300_Cop1_S_ULE(DWORD dwOp);
void R4300_Cop1_S_SF(DWORD dwOp);
void R4300_Cop1_S_NGLE(DWORD dwOp);
void R4300_Cop1_S_SEQ(DWORD dwOp);
void R4300_Cop1_S_NGL(DWORD dwOp);
void R4300_Cop1_S_LT(DWORD dwOp);
void R4300_Cop1_S_NGE(DWORD dwOp);
void R4300_Cop1_S_LE(DWORD dwOp);
void R4300_Cop1_S_NGT(DWORD dwOp);


void R4300_Cop1_W_CVT_S(DWORD dwOp);
void R4300_Cop1_W_CVT_D(DWORD dwOp);

void R4300_Cop1_L_CVT_S(DWORD dwOp);
void R4300_Cop1_L_CVT_D(DWORD dwOp);

void R4300_Cop1_D_Unk(DWORD dwOp);
void R4300_Cop1_D_ADD(DWORD dwOp);
void R4300_Cop1_D_SUB(DWORD dwOp);
void R4300_Cop1_D_MUL(DWORD dwOp);
void R4300_Cop1_D_DIV(DWORD dwOp);
void R4300_Cop1_D_SQRT(DWORD dwOp);
void R4300_Cop1_D_ABS(DWORD dwOp);
void R4300_Cop1_D_MOV(DWORD dwOp);
void R4300_Cop1_D_NEG(DWORD dwOp);
void R4300_Cop1_D_ROUND_L(DWORD dwOp);
void R4300_Cop1_D_TRUNC_L(DWORD dwOp);
void R4300_Cop1_D_CEIL_L(DWORD dwOp);
void R4300_Cop1_D_FLOOR_L(DWORD dwOp);
void R4300_Cop1_D_ROUND_W(DWORD dwOp);
void R4300_Cop1_D_TRUNC_W(DWORD dwOp);
void R4300_Cop1_D_CEIL_W(DWORD dwOp);
void R4300_Cop1_D_FLOOR_W(DWORD dwOp);
void R4300_Cop1_D_CVT_S(DWORD dwOp);
void R4300_Cop1_D_CVT_W(DWORD dwOp);
void R4300_Cop1_D_CVT_L(DWORD dwOp);
void R4300_Cop1_D_F(DWORD dwOp);
void R4300_Cop1_D_UN(DWORD dwOp);
void R4300_Cop1_D_EQ(DWORD dwOp);
void R4300_Cop1_D_UEQ(DWORD dwOp);
void R4300_Cop1_D_OLT(DWORD dwOp);
void R4300_Cop1_D_ULT(DWORD dwOp);
void R4300_Cop1_D_OLE(DWORD dwOp);
void R4300_Cop1_D_ULE(DWORD dwOp);
void R4300_Cop1_D_SF(DWORD dwOp);
void R4300_Cop1_D_NGLE(DWORD dwOp);
void R4300_Cop1_D_SEQ(DWORD dwOp);
void R4300_Cop1_D_NGL(DWORD dwOp);
void R4300_Cop1_D_LT(DWORD dwOp);
void R4300_Cop1_D_NGE(DWORD dwOp);
void R4300_Cop1_D_LE(DWORD dwOp);
void R4300_Cop1_D_NGT(DWORD dwOp);



#endif
