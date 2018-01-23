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

// RSP Processor stuff
#include "main.h"

#include "CPU.h"
#include "RSP.h"
#include "RDP.h"
#include "Memory.h"
#include "Debug.h"
#include "OpCode.h"
#include "Registers.h"
#include "ultra_rcp.h"
#include "DBGConsole.h"
#include "R4300.h"

#include "ultra_r4300.h"

// We need similar registers to the main CPU

// 32 32-bit General Purpose Registers
DWORD	g_dwRSPGPR[32];

// 32 8*16bit Vector registers
static u16	g_wVR[32][8];
static u16	g_wTemp[2][8];		// Temporary registers for vector calculations
static u32	g_dwAccu[8];

static DWORD g_dwCarryFlags = 0;
static DWORD g_dwCompareFlags = 0;

static DWORD g_nRSPDelay = NO_DELAY;
static DWORD g_dwNewRSPPC = 0;

BOOL	g_bRSPHalted		= TRUE;

// We also have some vector registers here


typedef void (*RSPInstruction)(DWORD dwOp);


static void RSP_Unk(DWORD dwOp);
static void RSP_Special(DWORD dwOp);
static void RSP_RegImm(DWORD dwOp);
static void RSP_J(DWORD dwOp);
static void RSP_JAL(DWORD dwOp);
static void RSP_BEQ(DWORD dwOp);
static void RSP_BNE(DWORD dwOp);
static void RSP_BLEZ(DWORD dwOp);
static void RSP_BGTZ(DWORD dwOp);
static void RSP_ADDI(DWORD dwOp);
static void RSP_ADDIU(DWORD dwOp);
static void RSP_SLTI(DWORD dwOp);
static void RSP_SLTIU(DWORD dwOp);
static void RSP_ANDI(DWORD dwOp);
static void RSP_ORI(DWORD dwOp);
static void RSP_XORI(DWORD dwOp);
static void RSP_LUI(DWORD dwOp);
static void RSP_CoPro0(DWORD dwOp);
static void RSP_CoPro2(DWORD dwOp);
static void RSP_LB(DWORD dwOp);
static void RSP_LH(DWORD dwOp);
static void RSP_LW(DWORD dwOp);
static void RSP_LBU(DWORD dwOp);
static void RSP_LHU(DWORD dwOp);
static void RSP_SB(DWORD dwOp);
static void RSP_SH(DWORD dwOp);
static void RSP_SW(DWORD dwOp);
static void RSP_LWC2(DWORD dwOp);
static void RSP_SWC2(DWORD dwOp);


static void RSP_Special_Unk(DWORD dwOp);
static void RSP_Special_SLL(DWORD dwOp);
static void RSP_Special_SRL(DWORD dwOp);
static void RSP_Special_SRA(DWORD dwOp);
static void RSP_Special_SLLV(DWORD dwOp);
static void RSP_Special_SRLV(DWORD dwOp);
static void RSP_Special_SRAV(DWORD dwOp);
static void RSP_Special_JR(DWORD dwOp);
static void RSP_Special_JALR(DWORD dwOp);
static void RSP_Special_BREAK(DWORD dwOp);
static void RSP_Special_ADD(DWORD dwOp);
static void RSP_Special_ADDU(DWORD dwOp);
static void RSP_Special_SUB(DWORD dwOp);
static void RSP_Special_SUBU(DWORD dwOp);
static void RSP_Special_AND(DWORD dwOp);
static void RSP_Special_OR(DWORD dwOp);
static void RSP_Special_XOR(DWORD dwOp);
static void RSP_Special_NOR(DWORD dwOp);
static void RSP_Special_SLT(DWORD dwOp);
static void RSP_Special_SLTU(DWORD dwOp);

static void RSP_RegImm_Unk(DWORD dwOp);
static void RSP_RegImm_BLTZ(DWORD dwOp);
static void RSP_RegImm_BGEZ(DWORD dwOp);
static void RSP_RegImm_BLTZAL(DWORD dwOp);
static void RSP_RegImm_BGEZAL(DWORD dwOp);


static void RSP_Cop0_Unk(DWORD dwOp);
static void RSP_Cop0_MFC0(DWORD dwOp);
static void RSP_Cop0_MTC0(DWORD dwOp);

static void RSP_LWC2_Unk(DWORD dwOp);
static void RSP_LWC2_LBV(DWORD dwOp);
static void RSP_LWC2_LSV(DWORD dwOp);
static void RSP_LWC2_LLV(DWORD dwOp);
static void RSP_LWC2_LDV(DWORD dwOp);
static void RSP_LWC2_LQV(DWORD dwOp);
static void RSP_LWC2_LRV(DWORD dwOp);
static void RSP_LWC2_LPV(DWORD dwOp);
static void RSP_LWC2_LUV(DWORD dwOp);
static void RSP_LWC2_LHV(DWORD dwOp);
static void RSP_LWC2_LFV(DWORD dwOp);
static void RSP_LWC2_LWV(DWORD dwOp);
static void RSP_LWC2_LTV(DWORD dwOp);

static void RSP_SWC2_Unk(DWORD dwOp);
static void RSP_SWC2_SBV(DWORD dwOp);
static void RSP_SWC2_SSV(DWORD dwOp);
static void RSP_SWC2_SLV(DWORD dwOp);
static void RSP_SWC2_SDV(DWORD dwOp);
static void RSP_SWC2_SQV(DWORD dwOp);
static void RSP_SWC2_SRV(DWORD dwOp);
static void RSP_SWC2_SPV(DWORD dwOp);
static void RSP_SWC2_SUV(DWORD dwOp);
static void RSP_SWC2_SHV(DWORD dwOp);
static void RSP_SWC2_SFV(DWORD dwOp);
static void RSP_SWC2_SWV(DWORD dwOp);
static void RSP_SWC2_STV(DWORD dwOp);

static void RSP_Cop2_Unk(DWORD dwOp);
static void RSP_Cop2_MFC2(DWORD dwOp);
static void RSP_Cop2_CFC2(DWORD dwOp);
static void RSP_Cop2_MTC2(DWORD dwOp);
static void RSP_Cop2_CTC2(DWORD dwOp);
static void RSP_Cop2_Vector(DWORD dwOp);

static void RSP_Cop2V_Unk(DWORD dwOp);
static void RSP_Cop2V_VMULF(DWORD dwOp);
static void RSP_Cop2V_VMULU(DWORD dwOp);
static void RSP_Cop2V_VRNDP(DWORD dwOp);
static void RSP_Cop2V_VMULQ(DWORD dwOp);
static void RSP_Cop2V_VMUDL(DWORD dwOp);
static void RSP_Cop2V_VMUDM(DWORD dwOp);
static void RSP_Cop2V_VMUDN(DWORD dwOp);
static void RSP_Cop2V_VMUDH(DWORD dwOp);
static void RSP_Cop2V_VMACF(DWORD dwOp);
static void RSP_Cop2V_VMACU(DWORD dwOp);
static void RSP_Cop2V_VRNDN(DWORD dwOp);
static void RSP_Cop2V_VMACQ(DWORD dwOp);
static void RSP_Cop2V_VMADL(DWORD dwOp);
static void RSP_Cop2V_VMADM(DWORD dwOp);
static void RSP_Cop2V_VMADN(DWORD dwOp);
static void RSP_Cop2V_VMADH(DWORD dwOp);
static void RSP_Cop2V_VADD(DWORD dwOp);
static void RSP_Cop2V_VSUB(DWORD dwOp);
static void RSP_Cop2V_VSUT(DWORD dwOp);
static void RSP_Cop2V_VABS(DWORD dwOp);
static void RSP_Cop2V_VADDC(DWORD dwOp);
static void RSP_Cop2V_VSUBC(DWORD dwOp);
static void RSP_Cop2V_VADDB(DWORD dwOp);
static void RSP_Cop2V_VSUBB(DWORD dwOp);
static void RSP_Cop2V_VACCB(DWORD dwOp);
static void RSP_Cop2V_VSUCB(DWORD dwOp);
static void RSP_Cop2V_VSAD(DWORD dwOp);
static void RSP_Cop2V_VSAC(DWORD dwOp);
static void RSP_Cop2V_VSUM(DWORD dwOp);
static void RSP_Cop2V_VSAW(DWORD dwOp);
static void RSP_Cop2V_VLT(DWORD dwOp);
static void RSP_Cop2V_VEQ(DWORD dwOp);
static void RSP_Cop2V_VNE(DWORD dwOp);
static void RSP_Cop2V_VGE(DWORD dwOp);
static void RSP_Cop2V_VCL(DWORD dwOp);
static void RSP_Cop2V_VCH(DWORD dwOp);
static void RSP_Cop2V_VCR(DWORD dwOp);
static void RSP_Cop2V_VMRG(DWORD dwOp);
static void RSP_Cop2V_VAND(DWORD dwOp);
static void RSP_Cop2V_VNAND(DWORD dwOp);
static void RSP_Cop2V_VOR(DWORD dwOp);
static void RSP_Cop2V_VNOR(DWORD dwOp);
static void RSP_Cop2V_VXOR(DWORD dwOp);
static void RSP_Cop2V_VNXOR(DWORD dwOp);
static void RSP_Cop2V_VRCP(DWORD dwOp);
static void RSP_Cop2V_VRCPL(DWORD dwOp);
static void RSP_Cop2V_VRCPH(DWORD dwOp);
static void RSP_Cop2V_VMOV(DWORD dwOp);
static void RSP_Cop2V_VRSQ(DWORD dwOp);
static void RSP_Cop2V_VRSQL(DWORD dwOp);
static void RSP_Cop2V_VRSQH(DWORD dwOp);



/*
    CPU: Instructions encoded by opcode field.
    31---------26---------------------------------------------------0
    |  opcode   |                                                   |
    ------6----------------------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | *1    | *2    |   J   |  JAL  |  BEQ  |  BNE  | BLEZ  | BGTZ  |
001 | ADDI  | ADDIU | SLTI  | SLTIU | ANDI  |  ORI  | XORI  |  LUI  |
010 | *3    |  ---  |  *4   |  ---  |  ---  |  ---  |  ---  |  ---  |
011 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
100 |  LB   |  LH   |  ---  |  LW   |  LBU  |  LHU  |  ---  |  ---  |
101 |  SB   |  SH   |  ---  |  SW   |  ---  |  ---  |  ---  |  ---  |
110 |  ---  |  ---  | *LWC2 |  ---  |  ---  |  ---  |  ---  |  ---  |
111 |  ---  |  ---  | *SWC2 |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
     *1 = SPECIAL, see SPECIAL list    *2 = REGIMM, see REGIMM list
     *3 = COP0                         *4 = COP2
     *LWC2 = RSP Load instructions     *SWC2 = RSP Store instructions
*/


// Opcode Jump Table
RSPInstruction RSP_Instruction[64] = {
	RSP_Special, RSP_RegImm, RSP_J, RSP_JAL, RSP_BEQ, RSP_BNE, RSP_BLEZ, RSP_BGTZ,
	RSP_ADDI, RSP_ADDIU, RSP_SLTI, RSP_SLTIU, RSP_ANDI, RSP_ORI, RSP_XORI, RSP_LUI,
	RSP_CoPro0, RSP_Unk, RSP_CoPro2, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk,
	RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk,
	RSP_LB, RSP_LH, RSP_Unk, RSP_LW, RSP_LBU, RSP_LHU, RSP_Unk, RSP_Unk,
	RSP_SB, RSP_SH, RSP_Unk, RSP_SW, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk,
	RSP_Unk, RSP_Unk, RSP_LWC2, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk,
	RSP_Unk, RSP_Unk, RSP_SWC2, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk, RSP_Unk
};


/*
    RSP SPECIAL: Instr. encoded by function field when opcode field = SPECIAL.
    31---------26-----------------------------------------5---------0
    | = SPECIAL |                                         | function|
    ------6----------------------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 |  SLL  |  ---  |  SRL  |  SRA  | SLLV  |  ---  | SRLV  | SRAV  |
001 |  JR   |  JALR |  ---  |  ---  |  ---  | BREAK |  ---  |  ---  |
010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
011 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
100 |  ADD  | ADDU  |  SUB  | SUBU  |  AND  |  OR   |  XOR  |  NOR  |
101 |  ---  |  ---  |  SLT  | SLTU  |  ---  |  ---  |  ---  |  ---  |
110 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
111 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
 */

// SpecialOpCode Jump Table
RSPInstruction RSPSpecialInstruction[64] = {
	RSP_Special_SLL, RSP_Special_Unk,  RSP_Special_SRL, RSP_Special_SRA,  RSP_Special_SLLV, RSP_Special_Unk,   RSP_Special_SRLV, RSP_Special_SRAV,
	RSP_Special_JR,  RSP_Special_JALR, RSP_Special_Unk, RSP_Special_Unk,  RSP_Special_Unk,  RSP_Special_BREAK, RSP_Special_Unk,  RSP_Special_Unk,
	RSP_Special_Unk, RSP_Special_Unk,  RSP_Special_Unk, RSP_Special_Unk,  RSP_Special_Unk,  RSP_Special_Unk,   RSP_Special_Unk,  RSP_Special_Unk,
	RSP_Special_Unk, RSP_Special_Unk,  RSP_Special_Unk, RSP_Special_Unk,  RSP_Special_Unk,  RSP_Special_Unk,   RSP_Special_Unk,  RSP_Special_Unk,
	RSP_Special_ADD, RSP_Special_ADDU, RSP_Special_SUB, RSP_Special_SUBU, RSP_Special_AND,  RSP_Special_OR,    RSP_Special_XOR,  RSP_Special_NOR,
	RSP_Special_Unk, RSP_Special_Unk,  RSP_Special_SLT, RSP_Special_SLTU, RSP_Special_Unk,  RSP_Special_Unk,   RSP_Special_Unk,  RSP_Special_Unk,
	RSP_Special_Unk, RSP_Special_Unk,  RSP_Special_Unk, RSP_Special_Unk,  RSP_Special_Unk,  RSP_Special_Unk,   RSP_Special_Unk,  RSP_Special_Unk,
	RSP_Special_Unk, RSP_Special_Unk,  RSP_Special_Unk, RSP_Special_Unk,  RSP_Special_Unk,  RSP_Special_Unk,   RSP_Special_Unk,  RSP_Special_Unk
};


/*
    REGIMM: Instructions encoded by the rt field when opcode field = REGIMM.
    31---------26----------20-------16------------------------------0
    | = REGIMM  |          |   rt    |                              |
    ------6---------------------5------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | BLTZ  | BGEZ  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 01 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 |BLTZAL |BGEZAL |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
 */

RSPInstruction RSPRegImmInstruction[32] = {

	RSP_RegImm_BLTZ,   RSP_RegImm_BGEZ,   RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk,
	RSP_RegImm_Unk,    RSP_RegImm_Unk,    RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk,
	RSP_RegImm_BLTZAL, RSP_RegImm_BGEZAL, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk,
	RSP_RegImm_Unk,    RSP_RegImm_Unk,    RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk, RSP_RegImm_Unk
};



/*
    COP0: Instructions encoded by the fmt field when opcode = COP0.
    31--------26-25------21 ----------------------------------------0
    |  = COP0   |   fmt   |                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | MFC0  |  ---  |  ---  |  ---  | MTC0  |  ---  |  ---  |  ---  |
 01 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/

// COP0 Jump Table
RSPInstruction RSPCop0Instruction[32] = {
	RSP_Cop0_MFC0, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_MTC0, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk,
	RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk,
	RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk,
	RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk, RSP_Cop0_Unk,
};



/*
    RSP Load: Instr. encoded by rd field when opcode field = LWC2
    31---------26-------------------15-------11---------------------0
    |  110010   |                   |   rd   |                      |
    ------6-----------------------------5----------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 |  LBV  |  LSV  |  LLV  |  LDV  |  LQV  |  LRV  |  LPV  |  LUV  |
 01 |  LHV  |  LFV  |  LWV  |  LTV  |  ---  |  ---  |  ---  |  ---  |
 10 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/

// LWC2 Jump Table
RSPInstruction RSPLWC2Instruction[32] = {
	RSP_LWC2_LBV, RSP_LWC2_LSV, RSP_LWC2_LLV, RSP_LWC2_LDV, RSP_LWC2_LQV, RSP_LWC2_LRV, RSP_LWC2_LPV, RSP_LWC2_LUV,
	RSP_LWC2_LHV, RSP_LWC2_LFV, RSP_LWC2_LWV, RSP_LWC2_LTV, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk,
	RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk,
	RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk, RSP_LWC2_Unk,
};

/*
    RSP Store: Instr. encoded by rd field when opcode field = SWC2
    31---------26-------------------15-------11---------------------0
    |  111010   |                   |   rd   |                      |
    ------6-----------------------------5----------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 |  SBV  |  SSV  |  SLV  |  SDV  |  SQV  |  SRV  |  SPV  |  SUV  |
 01 |  SHV  |  SFV  |  SWV  |  STV  |  ---  |  ---  |  ---  |  ---  |
 10 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/

// SWC2 Jump Table
RSPInstruction RSPSWC2Instruction[32] = {
	RSP_SWC2_SBV, RSP_SWC2_SSV, RSP_SWC2_SLV, RSP_SWC2_SDV, RSP_SWC2_SQV, RSP_SWC2_SRV, RSP_SWC2_SPV, RSP_SWC2_SUV,
	RSP_SWC2_SHV, RSP_SWC2_SFV, RSP_SWC2_SWV, RSP_SWC2_STV, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk,
	RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk,
	RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk, RSP_SWC2_Unk,
};


/*
    COP2: Instructions encoded by the fmt field when opcode = COP2.
    31--------26-25------21 ----------------------------------------0
    |  = COP2   |   fmt   |                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | MFC2  |  ---  | CFC2  |  ---  | MTC2  |  ---  | CTC2  |  ---  |
 01 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 |  *1   |  *1   |  *1   |  *1   |  *1   |  *1   |  *1   |  *1   |
 11 |  *1   |  *1   |  *1   |  *1   |  *1   |  *1   |  *1   |  *1   |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
     *1 = Vector opcode
*/

RSPInstruction RSPCop2Instruction[32] = {
	RSP_Cop2_MFC2, RSP_Cop2_Unk, RSP_Cop2_CFC2, RSP_Cop2_Unk, RSP_Cop2_MTC2, RSP_Cop2_Unk, RSP_Cop2_CTC2, RSP_Cop2_Unk,
	RSP_Cop2_Unk, RSP_Cop2_Unk, RSP_Cop2_Unk, RSP_Cop2_Unk, RSP_Cop2_Unk, RSP_Cop2_Unk, RSP_Cop2_Unk, RSP_Cop2_Unk, 
	RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector,
	RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector, RSP_Cop2_Vector
};

/*
    Vector opcodes: Instr. encoded by the function field when opcode = COP2.
    31---------26---25------------------------------------5---------0
    |  = COP2   | 1 |                                     | function|
    ------6-------1--------------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | VMULF | VMULU | VRNDP | VMULQ | VMUDL | VMUDM | VMUDN | VMUDH |
001 | VMACF | VMACU | VRNDN | VMACQ | VMADL | VMADM | VMADN | VMADH |
010 | VADD  | VSUB  | VSUT? | VABS  | VADDC | VSUBC | VADDB?| VSUBB?|
011 | VACCB?| VSUCB?| VSAD? | VSAC? | VSUM? | VSAW  |  ---  |  ---  |
100 |  VLT  |  VEQ  |  VNE  |  VGE  |  VCL  |  VCH  |  VCR  | VMRG  |
101 | VAND  | VNAND |  VOR  | VNOR  | VXOR  | VNXOR |  ---  |  ---  |
110 | VRCP  | VRCPL | VRCPH | VMOV  | VRSQ  | VRSQL | VRSQH |  ---  |
110 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
    Comment: Those with a ? in the end of them may not exist
*/
RSPInstruction RSPCop2VInstruction[64] = {
	RSP_Cop2V_VMULF, RSP_Cop2V_VMULU, RSP_Cop2V_VRNDP, RSP_Cop2V_VMULQ, RSP_Cop2V_VMUDL, RSP_Cop2V_VMUDM, RSP_Cop2V_VMUDN, RSP_Cop2V_VMUDH,
	RSP_Cop2V_VMACF, RSP_Cop2V_VMACU, RSP_Cop2V_VRNDN, RSP_Cop2V_VMACQ, RSP_Cop2V_VMADL, RSP_Cop2V_VMADM, RSP_Cop2V_VMADN, RSP_Cop2V_VMADH,
	RSP_Cop2V_VADD,  RSP_Cop2V_VSUB,  RSP_Cop2V_VSUT,  RSP_Cop2V_VABS,  RSP_Cop2V_VADDC, RSP_Cop2V_VSUBC, RSP_Cop2V_VADDB, RSP_Cop2V_VSUBB,
	RSP_Cop2V_VACCB, RSP_Cop2V_VSUCB, RSP_Cop2V_VSAD,  RSP_Cop2V_VSAC,  RSP_Cop2V_VSUM,  RSP_Cop2V_VSAW,  RSP_Cop2V_Unk,   RSP_Cop2V_Unk,
	RSP_Cop2V_VLT,   RSP_Cop2V_VEQ,   RSP_Cop2V_VNE,   RSP_Cop2V_VGE,   RSP_Cop2V_VCL,   RSP_Cop2V_VCH,   RSP_Cop2V_VCR,   RSP_Cop2V_VMRG,
	RSP_Cop2V_VAND,  RSP_Cop2V_VNAND, RSP_Cop2V_VOR,   RSP_Cop2V_VNOR,  RSP_Cop2V_VXOR,  RSP_Cop2V_VNXOR, RSP_Cop2V_Unk,   RSP_Cop2V_Unk,
	RSP_Cop2V_VRCP,  RSP_Cop2V_VRCPL, RSP_Cop2V_VRCPH, RSP_Cop2V_VMOV,  RSP_Cop2V_VRSQ,  RSP_Cop2V_VRSQL, RSP_Cop2V_VRSQH, RSP_Cop2V_Unk,
	RSP_Cop2V_Unk,   RSP_Cop2V_Unk,   RSP_Cop2V_Unk,   RSP_Cop2V_Unk,   RSP_Cop2V_Unk,   RSP_Cop2V_Unk,   RSP_Cop2V_Unk,   RSP_Cop2V_Unk
};

/*
	vadd   $v1, $v2, $v3          0 1 2 3 4 5 6 7
	vadd   $v1, $v2, $v3[0q]      0 1 2 3 0 1 2 3
	vadd   $v1, $v2, $v3[1q]      4 5 6 7 4 5 6 7
	vadd   $v1, $v2, $v3[0h]      0 1 0 1 0 1 0 1
	vadd   $v1, $v2, $v3[1h]      2 3 2 3 2 3 2 3
	vadd   $v1, $v2, $v3[2h]      4 5 4 5 4 5 4 5
	vadd   $v1, $v2, $v3[3h]      6 7 6 7 6 7 6 7 
	vadd   $v1, $v2, $v3[0]       0 0 0 0 0 0 0 0
	vadd   $v1, $v2, $v3[1]       1 1 1 1 1 1 1 1
	vadd   $v1, $v2, $v3[2]       2 2 2 2 2 2 2 2
	vadd   $v1, $v2, $v3[3]       3 3 3 3 3 3 3 3
	vadd   $v1, $v2, $v3[4]       4 4 4 4 4 4 4 4
	vadd   $v1, $v2, $v3[5]       5 5 5 5 5 5 5 5
	vadd   $v1, $v2, $v3[6]       6 6 6 6 6 6 6 6
	vadd   $v1, $v2, $v3[7]       7 7 7 7 7 7 7 7
*/

#define LOAD_VECTORS(dwA, dwB, dwPattern)						\
	for (DWORD __i = 0; __i < 8; __i++) {								\
		g_wTemp[0][__i] = g_wVR[dwA][__i];							\
		g_wTemp[1][__i] = g_wVR[dwB][g_Pattern[dwPattern][__i]];	\
	}

static const DWORD g_Pattern[16][8] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },	//?
	{ 0, 0, 2, 2, 4, 4, 6, 6 },
	{ 1, 1, 3, 3, 5, 5, 7, 7 },

	{ 0, 0, 0, 0, 4, 4, 4, 4 },
	{ 1, 1, 1, 1, 5, 5, 5, 5 }, 
	{ 2, 2, 2, 2, 6, 6, 6, 6 },
	{ 3, 3, 3, 3, 7, 7, 7, 7 },

	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 3, 3, 3, 3, 3, 3, 3, 3 },
	{ 4, 4, 4, 4, 4, 4, 4, 4 },
	{ 5, 5, 5, 5, 5, 5, 5, 5 },
	{ 6, 6, 6, 6, 6, 6, 6, 6 },
	{ 7, 7, 7, 7, 7, 7, 7, 7 }
};

void RSP_Reset(void)
{
	for (DWORD dwReg = 0; dwReg < 32; dwReg++)
		g_dwRSPGPR[dwReg] = 0;

	StopRSP();
}


void RSPStepHalted() {}


void RSPStep(void)
{
	// Don't bother running any tasks while the RSP is in this shape!
	RSP_Special_BREAK(0);
	return;

	// Fetch next instruction
	DWORD dwPC = RSPPC();
	DWORD dwOp;
	dwOp = *(DWORD *)(g_pu8SpMemBase + ((dwPC&0x0FFF) | 0x1000));


	RSP_Instruction[op(dwOp)](dwOp);

	switch (g_nRSPDelay) {
		case DO_DELAY:
			// We've got a delayed instruction to execute. Increment
			// PC as normal, so that subsequent instruction is executed
			*(DWORD *)(g_pMemoryBuffers[MEM_SP_PC_REG]) += 4;
			g_nRSPDelay = EXEC_DELAY;
			break;
		case EXEC_DELAY:
			// We've just executed the delayed instr. Now carry out jump
			//  as stored in g_dwNewPC;		
			*(DWORD *)(g_pMemoryBuffers[MEM_SP_PC_REG]) = g_dwNewRSPPC;
			g_nRSPDelay = NO_DELAY;
			break;
		case NO_DELAY:
			// Normal operation - just increment the PC
			*(DWORD *)(g_pMemoryBuffers[MEM_SP_PC_REG]) += 4;
			break;
	}
}

void RSP_DumpVector(DWORD dwReg)
{
	LONG i;
	dwReg &= 0x1f;

	DBGConsole_Msg(0, "Vector%d", dwReg);
	for (i = 0; i < 8; i++)
	{
		DBGConsole_Msg(0, "%d: 0x%04x", i, g_wVR[dwReg][i]);
	}
}

void RSP_DumpVectors(DWORD dwReg1, DWORD dwReg2, DWORD dwReg3)
{
	LONG i;

	dwReg1 &= 0x1f;
	dwReg2 &= 0x1f;
	dwReg3 &= 0x1f;

	DBGConsole_Msg(0, "    Vec%2d\tVec%2d\tVec%2d", dwReg1, dwReg2, dwReg3);
	for (i = 0; i < 8; i++)
	{
		DBGConsole_Msg(0, "%d: 0x%04x\t0x%04x\t0x%04x",
			i, g_wVR[dwReg1][i], g_wVR[dwReg2][i], g_wVR[dwReg3][i]);
	}
}



void RSP_Unk(DWORD dwOp)		{ WARN_NOEXIST("RSP_Unk"); }
void RSP_Special(DWORD dwOp)	{ RSPSpecialInstruction[funct(dwOp)](dwOp); }
void RSP_RegImm(DWORD dwOp)		{ RSPRegImmInstruction[rt(dwOp)](dwOp); }
void RSP_CoPro0(DWORD dwOp)		{ RSPCop0Instruction[cop0fmt(dwOp)](dwOp); }		
void RSP_CoPro2(DWORD dwOp)		{ RSPCop2Instruction[rs(dwOp)](dwOp); }		
void RSP_LWC2(DWORD dwOp)		{ RSPLWC2Instruction[rd(dwOp)](dwOp); }
void RSP_SWC2(DWORD dwOp)		{ RSPSWC2Instruction[rd(dwOp)](dwOp); }

void RSP_J(DWORD dwOp) {			// Jump
	
	// Jump
	g_dwNewRSPPC = (RSPPC() & 0xF0000000) | (target(dwOp)<<2);
	g_nRSPDelay = DO_DELAY;


}
void RSP_JAL(DWORD dwOp) {				// Jump And Link

	g_dwRSPGPR[31] = (s32)(RSPPC() + 8);		// Store return address

	g_dwNewRSPPC = (RSPPC() & 0xF0000000) | (target(dwOp)<<2);
	g_nRSPDelay = DO_DELAY;
}

void RSP_BEQ(DWORD dwOp) {					// Branch on Equal
	//branch if rs == rt
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	if (g_dwRSPGPR[dwRS] == g_dwRSPGPR[dwRT]) {
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewRSPPC = RSPPC() + ((s32)wOffset<<2) + 4;
		g_nRSPDelay = DO_DELAY;
	}
}
void RSP_BNE(DWORD dwOp) {				// Branch on Not Equal
	//branch if rs == rt
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	if (g_dwRSPGPR[dwRS] != g_dwRSPGPR[dwRT]) {
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewRSPPC = RSPPC() + ((s32)wOffset<<2) + 4;
		g_nRSPDelay = DO_DELAY;
	}

 }
void RSP_BLEZ(DWORD dwOp) {			// Branch on Less than or Equal to Zero

	//branch if rs <= 0
	DWORD dwRS = rs(dwOp);

	if ((s32)g_dwRSPGPR[dwRS] <= 0) {
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewRSPPC = RSPPC() + (s32)wOffset*4 + 4;
		g_nRSPDelay = DO_DELAY;
	} 
}
void RSP_BGTZ(DWORD dwOp) {			// Branch on Greater Than to Zero

	//branch if rs > 0
	DWORD dwRS = rs(dwOp);

	if ((s32)g_dwRSPGPR[dwRS] > 0) {
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewRSPPC = RSPPC() + (s32)wOffset*4 + 4;
		g_nRSPDelay = DO_DELAY;
	} 

}
void RSP_ADDI(DWORD dwOp) {			// ADD Immediate
	
	//rt = rs + immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	s16 wData = (s16)imm(dwOp);

	g_dwRSPGPR[dwRT] = g_dwRSPGPR[dwRS] + (s32)wData;

	// Generates overflow exception
}
void RSP_ADDIU(DWORD dwOp) {				// ADD Immediate Unsigned -- Same as above, but doesn't generate exception
	
	//rt = rs + immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	s16 wData = (s16)imm(dwOp);

	g_dwRSPGPR[dwRT] = g_dwRSPGPR[dwRS] + (s32)wData;
}

void RSP_SLTI(DWORD dwOp) { WARN_NOIMPL("RSP: SLTI"); }
void RSP_SLTIU(DWORD dwOp) { WARN_NOIMPL("RSP: SLTIU"); }
void RSP_ANDI(DWORD dwOp) {				// AND Immediate
		
	//rt = rs & immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	u16 wData = (u16)imm(dwOp);

	g_dwRSPGPR[dwRT] = g_dwRSPGPR[dwRS] & (u32)wData;
}
void RSP_ORI(DWORD dwOp) {			// OR Immediate
	
	//rt = rs | immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	u16 wData = (u16)imm(dwOp);

	g_dwRSPGPR[dwRT] = g_dwRSPGPR[dwRS] | (u32)wData;
}

void RSP_XORI(DWORD dwOp) {						// XOR Immediate
	
	//rt = rs ^ immediate
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);
	u16 wData = (u16)imm(dwOp);

	g_dwRSPGPR[dwRT] = g_dwRSPGPR[dwRS] ^ (u32)wData;

}
void RSP_LUI(DWORD dwOp) {
	//dwRS == 0?
	DWORD dwRT = rt(dwOp);
	s16 wData = (s16)imm(dwOp);

	g_dwRSPGPR[dwRT] = (wData<<16);
	// Keep lower 16bits?
}

void RSP_LB(DWORD dwOp)
{

	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + (s32)wOffset) & 0x0FFF;

	char nData = Read8Bits(PHYS_TO_K1(0x04000000 + dwAddress));

	g_dwRSPGPR[dwRT] = (s32)(s16)nData;

}
void RSP_LH(DWORD dwOp) {				// Load Halfword

	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + (s32)wOffset) & 0x0FFF;

	WORD wData = Read16Bits(PHYS_TO_K1(0x04000000 + dwAddress));

	g_dwRSPGPR[dwRT] = (s32)(s16)wData;

}
void RSP_LW(DWORD dwOp) {						// Load Word

	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + (s32)wOffset) & 0x0FFF;

	DWORD dwData = Read32Bits(PHYS_TO_K1(0x04000000 + dwAddress));

	g_dwRSPGPR[dwRT] = (s32)dwData;

}
void RSP_LBU(DWORD dwOp) { 

	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + (s32)wOffset) & 0x0FFF;

	BYTE nData = Read8Bits(PHYS_TO_K1(0x04000000 + dwAddress));

	g_dwRSPGPR[dwRT] = (u32)(u16)nData;

}
void RSP_LHU(DWORD dwOp) {

	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + (s32)wOffset) & 0x0FFF;

	WORD wData = Read16Bits(PHYS_TO_K1(0x04000000 + dwAddress));

	g_dwRSPGPR[dwRT] = (u32)wData;

}
void RSP_SB(DWORD dwOp) {

	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + (s32)wOffset) & 0x0FFF;

	BYTE nData = (BYTE)g_dwRSPGPR[dwRT];

	Write8Bits(PHYS_TO_K1(0x04000000 + dwAddress), nData);

}

void RSP_SH(DWORD dwOp) {						// Store Halfword

	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + (s32)wOffset) & 0x0FFF;

	u16 wData =	(u16)g_dwRSPGPR[dwRT];

	Write16Bits(PHYS_TO_K1(0x04000000 + dwAddress), wData);
}

void RSP_SW(DWORD dwOp) { 


	DWORD dwBase = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);
	DWORD dwRT  = rt(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + (s32)wOffset) & 0x0FFF;

	DWORD dwData =	g_dwRSPGPR[dwRT];

	Write32Bits(PHYS_TO_K1(0x04000000 + dwAddress), dwData);

}

void RSP_Special_Unk(DWORD dwOp) {
	/*CPUHalt(); WARN_NOEXIST("RSP_Special_Unk");
	DPF(DEBUG_ALWAYS, "RSP: PC: 0x%08x Unknown Op: 0x%08x", RSPPC(), dwOp);
	*/
}

void RSP_Special_SLL(DWORD dwOp) {				// Shift word Left Logical

	//DWORD dwRS = rs(dwOp);	// Should be == 0
	DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_dwRSPGPR[dwRD] = g_dwRSPGPR[dwRT] << dwSA;
}

void RSP_Special_SRL(DWORD dwOp) {		// Shift word Right Logical

	//DWORD dwRS = rs(dwOp);	// Should be == 0
	DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_dwRSPGPR[dwRD] = g_dwRSPGPR[dwRT] >> dwSA;
}
void RSP_Special_SRA(DWORD dwOp) {	// Shift word Right Arithmetic

	//DWORD dwRS = rs(dwOp);	// Should be == 0
	DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_dwRSPGPR[dwRD] = ((s32)g_dwRSPGPR[dwRT]) >> dwSA;
}

void RSP_Special_SLLV(DWORD dwOp) {		// Shift word Left Logical Variable

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// Sign Extend
	g_dwRSPGPR[dwRD] = g_dwRSPGPR[dwRT] << (g_dwRSPGPR[dwRS]&0x1F);

}
void RSP_Special_SRLV(DWORD dwOp)
{

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	g_dwRSPGPR[dwRD] = g_dwRSPGPR[dwRT] >> (g_dwRSPGPR[dwRS]&0x1F);


}
void RSP_Special_SRAV(DWORD dwOp)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);


	g_dwRSPGPR[dwRD] = (s32)g_dwRSPGPR[dwRT] >> (g_dwRSPGPR[dwRS]&0x1F);

}

void RSP_Special_JR(DWORD dwOp) {			// Jump Register

	DWORD dwRS = rs(dwOp);

	g_dwNewRSPPC = 0x04000000 | (g_dwRSPGPR[dwRS] & 0x1fff);
	g_nRSPDelay = DO_DELAY;
}

void RSP_Special_JALR(DWORD dwOp)
{
	// Jump And Link
	DWORD dwRS = rs(dwOp);
	DWORD dwRD = rd(dwOp);


	g_dwNewRSPPC = 0x04000000 | (g_dwRSPGPR[dwRS] & 0x1fff);
	g_dwRSPGPR[dwRD] = (s32)(RSPPC() + 8);		// Store return address
	
	g_nRSPDelay = DO_DELAY;


}

void RSP_Special_BREAK(DWORD dwOp)
{
	Memory_SP_SetRegisterBits(SP_STATUS_REG, SP_STATUS_SIG2|SP_STATUS_BROKE|SP_STATUS_HALT);

	if(Memory_SP_GetRegister(SP_STATUS_REG) & SP_STATUS_INTR_BREAK)
	{
		Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SP);
		AddCPUJob(CPU_CHECK_INTERRUPTS);
	}

	//DBGConsole_Msg(0, "RSP: [WBREAK]");
	StopRSP();
}

void RSP_Special_ADD(DWORD dwOp) {			// ADD signed - may throw exception

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// 32 bit numbers for addition...
	g_dwRSPGPR[dwRD] = (s32)g_dwRSPGPR[dwRS] + (s32)g_dwRSPGPR[dwRT];
}
void RSP_Special_ADDU(DWORD dwOp) {			// ADD Unsigned - doesn't throw exception

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// 32 bit numbers for addition...
	g_dwRSPGPR[dwRD] = (s32)g_dwRSPGPR[dwRS] + (s32)g_dwRSPGPR[dwRT];
}

void RSP_Special_SUB(DWORD dwOp) {			// SUB signed - may throw exception

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// 32 bit numbers for addition...
	g_dwRSPGPR[dwRD] = (s32)g_dwRSPGPR[dwRS] - (s32)g_dwRSPGPR[dwRT];
}
void RSP_Special_SUBU(DWORD dwOp) {			// SUB Unsigned - may throw exception

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	// 32 bit numbers for addition...
	g_dwRSPGPR[dwRD] = (s32)g_dwRSPGPR[dwRS] - (s32)g_dwRSPGPR[dwRT];
}
void RSP_Special_AND(DWORD dwOp) { 			// logical AND

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);
	g_dwRSPGPR[dwRD] = g_dwRSPGPR[dwRS] & g_dwRSPGPR[dwRT];

}
void RSP_Special_OR(DWORD dwOp) {				// logical OR

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);
	g_dwRSPGPR[dwRD] = g_dwRSPGPR[dwRS] | g_dwRSPGPR[dwRT];

}
void RSP_Special_XOR(DWORD dwOp) {			// logical eXclusive OR


	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);
	g_dwRSPGPR[dwRD] = g_dwRSPGPR[dwRS] ^ g_dwRSPGPR[dwRT];



}
void RSP_Special_NOR(DWORD dwOp) {			// logical NOT OR

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);
	g_dwRSPGPR[dwRD] = ~(g_dwRSPGPR[dwRS] | g_dwRSPGPR[dwRT]);

}
void RSP_Special_SLT(DWORD dwOp) { WARN_NOIMPL("RSP: SLT"); }
void RSP_Special_SLTU(DWORD dwOp) { WARN_NOIMPL("RSP: SLTU"); }








void RSP_RegImm_Unk(DWORD dwOp){ WARN_NOEXIST("RSP_RegImm_Unk"); }
void RSP_RegImm_BLTZ(DWORD dwOp){				// Branch on Less than Zero
	//branch if rs < 0
	DWORD dwRS = rs(dwOp);

	if ((s32)g_dwRSPGPR[dwRS] < 0) {
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewRSPPC = RSPPC() + (s32)wOffset*4 + 4;
		g_nRSPDelay = DO_DELAY;
	} 
}
void RSP_RegImm_BGEZ(DWORD dwOp){				// Branch on Greater than or Equal to than Zero
	//branch if rs < 0
	DWORD dwRS = rs(dwOp);

	if ((s32)g_dwRSPGPR[dwRS] >= 0) {
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewRSPPC = RSPPC() + (s32)wOffset*4 + 4;
		g_nRSPDelay = DO_DELAY;
	}
}
void RSP_RegImm_BLTZAL(DWORD dwOp){ WARN_NOIMPL("RSP: BLTZAL"); }
void RSP_RegImm_BGEZAL(DWORD dwOp)
{
	//WARN_NOIMPL("RSP: BGEZAL");

	//branch if rs >= 0
	DWORD dwRS = rs(dwOp);

	// Apparently this always happens, even if branch not taken
	g_dwRSPGPR[31] = RSPPC() + 8;		// Store return address	
	if ((s32)g_dwRSPGPR[dwRS] >= 0) {
		s16 wOffset = (s16)imm(dwOp);
		g_dwNewRSPPC = RSPPC() + ((s32)wOffset*4) + 4;
		g_nRSPDelay = DO_DELAY;

	} 

}











void RSP_Cop0_Unk(DWORD dwOp) {WARN_NOEXIST("RSP_Cop0_Unk"); }
void RSP_Cop0_MFC0(DWORD dwOp)
{ 
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	if(dwRD < 8)	g_dwRSPGPR[dwRT] = Read32Bits(0x84040000 | (dwRD << 2));			// SP Regs
	else			g_dwRSPGPR[dwRT] = Read32Bits(0x84100000 | ((dwRD & ~0x08) << 2));	// DP Command Regs.

}
void RSP_Cop0_MTC0(DWORD dwOp)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	if(dwRD < 8) Write32Bits(0x84040000 | (dwRD << 2), g_dwRSPGPR[dwRT]);			// SP Regs
	else		 Write32Bits(0x84100000 | ((dwRD & ~0x08) << 2),g_dwRSPGPR[dwRT]);	// DP Command Regs.
}


void RSP_LWC2_Unk(DWORD dwOp) { WARN_NOEXIST("RSP_LWC2_Unk"); }
void RSP_LWC2_LBV(DWORD dwOp) { WARN_NOIMPL("RSP: LBV"); }
void RSP_LWC2_LRV(DWORD dwOp) { WARN_NOIMPL("RSP: LRV"); }
void RSP_LWC2_LPV(DWORD dwOp) { /*WARN_NOIMPL("RSP: LPV");zelda*/ }
void RSP_LWC2_LUV(DWORD dwOp) { /*WARN_NOIMPL("RSP: LUV");zelda*/ }
void RSP_LWC2_LHV(DWORD dwOp) { WARN_NOIMPL("RSP: LHV"); }
void RSP_LWC2_LFV(DWORD dwOp) { WARN_NOIMPL("RSP: LFV"); }
void RSP_LWC2_LWV(DWORD dwOp) { WARN_NOIMPL("RSP: LWV"); }


void RSP_LWC2_LTV(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: LTV");zelda*/
	LONG i;
	DWORD dwBase = base(dwOp);
	DWORD dwDest  = dest(dwOp);
	DWORD dwV = sel(dwOp);
	s16 wOffset = rsp_vecoffset(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + ((s32)wOffset<<4)) & 0x0FFF;

	dwV >>= 1;
    for (i = 0; i < 8; i++)
    {
		g_wVR[dwDest+i][(8-dwV+i)&7] = (g_pu8SpMemBase[(dwAddress+(i<<1)+0)^3] << 8) |
			                            g_pu8SpMemBase[(dwAddress+(i<<1)+1)^3];
    }
}

void RSP_SWC2_STV(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: STV");zelda*/
	LONG i;	
	DWORD dwBase = base(dwOp);
	DWORD dwDest  = dest(dwOp);
	DWORD dwV = sel(dwOp);
	s16 wOffset = rsp_vecoffset(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + ((s32)wOffset<<4)) & 0x0FFF;
	
	//CPU_Halt("STV");

	/*tr: The following was VERY VERY hard to figure out!!! */
    for (i = 0; i < 8; i++)
    {
		s32   o = dwV ? dwAddress + (16-dwV) : dwAddress;
		g_pu8SpMemBase[(((o & ~0x0f) | ((o + (i<<1)) & 0x0f)) + 0)^3] = g_wVR[dwDest+i][(8-(dwV>>1)+i)&7] >> 8;
		g_pu8SpMemBase[(((o & ~0x0f) | ((o + (i<<1)) & 0x0f)) + 1)^3] = g_wVR[dwDest+i][(8-(dwV>>1)+i)&7];
	}
}

void RSP_LWC2_LLV(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: LLV");zelda*/
	DWORD dwBase = base(dwOp);
	DWORD dwDest  = dest(dwOp);
	DWORD dwV = sel(dwOp);
	s16 wOffset = rsp_vecoffset(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + ((s32)wOffset<<2)) & 0x0FFF;

	dwV >>= 1;
    g_wVR[dwDest][dwV+0] = (g_pu8SpMemBase[(dwAddress+0)^3] << 8) | g_pu8SpMemBase[(dwAddress+1)^3];
    g_wVR[dwDest][dwV+1] = (g_pu8SpMemBase[(dwAddress+2)^3] << 8) | g_pu8SpMemBase[(dwAddress+3)^3];
}



/*
  +-----------+---------------------------------------------------+
  | LxV       | Load xxxxxx to vector                             |
  +-----------+---------+---------+---------+-------+-+-----------+
  |  110010   |  base   |  dest   |  xxxxx  |  sel  |0|   offset  |
  +-----6-----+----5----+----5----+----5----+---4---+1+-----6-----+
  Format:  LxV $v<dest>[sel], offset(base)
*/

void RSP_LWC2_LSV(DWORD dwOp)
{

	DWORD dwBase = base(dwOp);
	DWORD dwDest  = dest(dwOp);
	DWORD dwV = sel(dwOp);
	s16 wOffset = rsp_vecoffset(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + ((s32)wOffset<<1)) & 0x0FFF;

	// Divide by two - it points to 8bits in the registers, not 16bits
	dwV >>= 1;
    g_wVR[dwDest][dwV+0] = (g_pu8SpMemBase[(dwAddress+0)^3] << 8) | g_pu8SpMemBase[(dwAddress+1)^3];

}


/* The following will read from 0(s0) and 0(s0), 8(s0) and 8(s0) respectively
0x04001514: <ca191800> LDV       $vec25[0h] <- 0x0000(s0)
0x04001518: <ca181c00> LDV       $vec24[4h] <- 0x0000(s0)
0x0400151c: <ca171801> LDV       $vec23[0h] <- 0x0008(s0)
0x04001520: <ca171c01> LDV       $vec23[4h] <- 0x0008(s0)

*/
void RSP_LWC2_LDV(DWORD dwOp) {			// Load Doubleword into Vector

	DWORD dwBase = base(dwOp);
	DWORD dwDest  = dest(dwOp);
	DWORD dwV = sel(dwOp);
	s16 wOffset = rsp_vecoffset(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + ((s32)wOffset<<3)) & 0x0FFF;

	// Check for dwV>>1+x being > 7 here?
	dwV >>= 1;
    g_wVR[dwDest][dwV+0] = (g_pu8SpMemBase[(dwAddress+0)^3] << 8) | g_pu8SpMemBase[(dwAddress+1)^3];
    g_wVR[dwDest][dwV+1] = (g_pu8SpMemBase[(dwAddress+2)^3] << 8) | g_pu8SpMemBase[(dwAddress+3)^3];
    g_wVR[dwDest][dwV+2] = (g_pu8SpMemBase[(dwAddress+4)^3] << 8) | g_pu8SpMemBase[(dwAddress+5)^3];
    g_wVR[dwDest][dwV+3] = (g_pu8SpMemBase[(dwAddress+6)^3] << 8) | g_pu8SpMemBase[(dwAddress+7)^3];
}

void RSP_LWC2_LQV(DWORD dwOp)
{ 
	DWORD dwBase	= base(dwOp);
	DWORD dwDest	= dest(dwOp);
	DWORD dwV		= sel(dwOp);
	s16 wOffset = rsp_vecoffset(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + ((s32)wOffset<<4)) & 0x0FFF;

	// It should load into the whole vector (i.e 128bits)
	for(DWORD i=0; i < (16 - (dwAddress & 0xf)); i++)
	{
		if((dwV+i) & 1)
		{
				g_wVR[dwDest][((dwV+i)>>1)] &= 0xff00;
				g_wVR[dwDest][((dwV+i)>>1)] |= g_pu8SpMemBase[(dwAddress+i)^3];
		}
		else
		{
				g_wVR[dwDest][((dwV+i)>>1)] &= 0x00ff;
				g_wVR[dwDest][((dwV+i)>>1)] |= g_pu8SpMemBase[(dwAddress+i)^3] << 8;
		}
	}
}


void RSP_SWC2_Unk(DWORD dwOp) {  WARN_NOEXIST("RSP_SWC2_Unk"); }
void RSP_SWC2_SBV(DWORD dwOp) { WARN_NOIMPL("RSP: SBV"); }


//sprintf(str, "SSV       vec%02d[%d ] -> 0x%04x(%s)",
//                        
//                        dest(dwOp),
//                        sel(dwOp)>>1,
//                        offset(dwOp)<<1,
//                        RegNames[base(dwOp)]);
void RSP_SWC2_SSV(DWORD dwOp) 		// Store single from vector (16bits)
{	
	DWORD dwBase = base(dwOp);
	DWORD dwDest  = dest(dwOp);
	DWORD dwV = sel(dwOp);
	s16 wOffset = rsp_vecoffset(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + ((s32)wOffset<<1)) & 0x0FFF;

	dwV >>= 1;
    g_pu8SpMemBase[(dwAddress+0)^3] = (BYTE)(g_wVR[dwDest][dwV+0] >> 8);
	g_pu8SpMemBase[(dwAddress+1)^3] = (BYTE)(g_wVR[dwDest][dwV+0] & 0xFF);
}

void RSP_SWC2_SLV(DWORD dwOp) 		// Store word from vector (32bits)
{
	DWORD dwBase = base(dwOp);
	DWORD dwDest  = dest(dwOp);
	DWORD dwV = sel(dwOp);
	s16 wOffset = rsp_vecoffset(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + ((s32)wOffset<<2)) & 0x0FFF;

	dwV >>= 1;
	g_pu8SpMemBase[(dwAddress+0)^3] = (BYTE)(g_wVR[dwDest][dwV+0] >> 8);
	g_pu8SpMemBase[(dwAddress+1)^3] = (BYTE)(g_wVR[dwDest][dwV+0]);
	g_pu8SpMemBase[(dwAddress+2)^3] = (BYTE)(g_wVR[dwDest][dwV+1] >> 8);
	g_pu8SpMemBase[(dwAddress+3)^3] = (BYTE)(g_wVR[dwDest][dwV+1]);
}


void RSP_SWC2_SDV(DWORD dwOp) 			// Store doubleword from vector (64 bits)
{
	DWORD dwBase = base(dwOp);
	DWORD dwDest  = dest(dwOp);
	DWORD dwV = sel(dwOp);
	s16 wOffset = rsp_vecoffset(dwOp);

	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + ((s32)wOffset<<3)) & 0x0FFF;

	// Check for dwV>>1+x being > 7 here?
	dwV >>= 1;
	g_pu8SpMemBase[(dwAddress+0)^3] = (BYTE)(g_wVR[dwDest][dwV+0] >> 8);
	g_pu8SpMemBase[(dwAddress+1)^3] = (BYTE)(g_wVR[dwDest][dwV+0]);
	g_pu8SpMemBase[(dwAddress+2)^3] = (BYTE)(g_wVR[dwDest][dwV+1] >> 8);
	g_pu8SpMemBase[(dwAddress+3)^3] = (BYTE)(g_wVR[dwDest][dwV+1]);
	g_pu8SpMemBase[(dwAddress+4)^3] = (BYTE)(g_wVR[dwDest][dwV+2] >> 8);
	g_pu8SpMemBase[(dwAddress+5)^3] = (BYTE)(g_wVR[dwDest][dwV+2]);
	g_pu8SpMemBase[(dwAddress+6)^3] = (BYTE)(g_wVR[dwDest][dwV+3] >> 8);
	g_pu8SpMemBase[(dwAddress+7)^3] = (BYTE)(g_wVR[dwDest][dwV+3]);
}


//0xa4001428: <0xe8b82000> SQV       vec24[0a] -> 0x0000(a1)
void RSP_SWC2_SQV(DWORD dwOp)
{
	DWORD i;
	DWORD dwBase = base(dwOp);
	DWORD dwDest = dest(dwOp);
	DWORD dwV	 = sel(dwOp);
	s16 wOffset = rsp_vecoffset(dwOp);


	DWORD dwAddress = (u32)((s32)g_dwRSPGPR[dwBase] + ((s32)wOffset<<4)) & 0x0FFF;

	for(i = 0; i < (16 - (dwAddress & 0xf)); i++)
	{
		if((dwV+i) & 1)
		{
			g_pu8SpMemBase[(dwAddress+i)^3] = (BYTE)(g_wVR[dwDest][((dwV+i)>>1)]);
		}
		else
		{
			g_pu8SpMemBase[(dwAddress+i)^3] = (BYTE)(g_wVR[dwDest][((dwV+i)>>1)] >> 8);
		}
	}
}

void RSP_SWC2_SRV(DWORD dwOp) { WARN_NOIMPL("RSP: SRV"); }
void RSP_SWC2_SPV(DWORD dwOp) { WARN_NOIMPL("RSP: SPV"); }
void RSP_SWC2_SUV(DWORD dwOp) { WARN_NOIMPL("RSP: SUV"); }
void RSP_SWC2_SHV(DWORD dwOp) { WARN_NOIMPL("RSP: SHV"); }
void RSP_SWC2_SFV(DWORD dwOp) { WARN_NOIMPL("RSP: SFV"); }
void RSP_SWC2_SWV(DWORD dwOp) { WARN_NOIMPL("RSP: SWV"); }


void RSP_Cop2_Unk(DWORD dwOp)	{ WARN_NOEXIST("RSP_Cop2_Unk"); }
void RSP_Cop2_Vector(DWORD dwOp)	{ RSPCop2VInstruction[funct(dwOp)](dwOp); }
void RSP_Cop2_MFC2(DWORD dwOp)	{ WARN_NOIMPL("RSP: MFC2"); }
void RSP_Cop2_CFC2(DWORD dwOp)	{ WARN_NOIMPL("RSP: CFC2"); }
void RSP_Cop2_MTC2(DWORD dwOp)	{ /*WARN_NOIMPL("RSP: MTC2");zelda*/ }
void RSP_Cop2_CTC2(DWORD dwOp)	{ WARN_NOIMPL("RSP: CTC2"); }

void RSP_Cop2V_Unk(DWORD dwOp)		{ WARN_NOEXIST("RSP_Cop2V_Unk"); }
void RSP_Cop2V_VMULU(DWORD dwOp)	{ /*WARN_NOIMPL("RSP: VMULU");zelda*/ }
void RSP_Cop2V_VMACU(DWORD dwOp)	{ /*WARN_NOIMPL("RSP: VMACU");zelda*/ }
void RSP_Cop2V_VRNDP(DWORD dwOp)	{ WARN_NOIMPL("RSP: VRNDP"); }
void RSP_Cop2V_VMULQ(DWORD dwOp)	{ WARN_NOIMPL("RSP: VMULQ"); }
void RSP_Cop2V_VRNDN(DWORD dwOp)	{ WARN_NOIMPL("RSP: VRNDN"); }
void RSP_Cop2V_VMACQ(DWORD dwOp)	{ WARN_NOIMPL("RSP: VMACQ"); }

void RSP_Cop2V_VMULF(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VMULF");*/
	int i;
	DWORD dwDest = vdest(dwOp);
	DWORD dwVS1  = vs1(dwOp);
	DWORD dwVS2  = vs2(dwOp);
	DWORD dwPattern = ve1(dwOp);

	LOAD_VECTORS(dwVS1, dwVS2, dwPattern);

	for (i = 0; i < 8; i++) 
	{
		g_wVR[dwDest][i] = (s32)((s32)(s16)g_wTemp[0][i] * (s32)(s16)g_wTemp[1][i]) >> 16; 
	}
}

void RSP_Cop2V_VMACF(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VMACF");*/
	int i;
	DWORD dwDest = vdest(dwOp);
	DWORD dwVS1  = vs1(dwOp);
	DWORD dwVS2  = vs2(dwOp);
	DWORD dwPattern = ve1(dwOp);

	LOAD_VECTORS(dwVS1, dwVS2, dwPattern);

	for (i = 0; i < 8; i++) 
	{
		g_wVR[dwDest][i] += (s32)((s32)(s16)g_wTemp[0][i] * (s32)(s16)g_wTemp[1][i]) >> 16; 
	}
}


/* I have m/n swapped...
* VMUDL:  acc  = (src1 * src2) >> 16, dest = acc & 0xffff
* VMUDM:  acc  = (src1 * src2), dest = acc >> 16
* VMUDN:  acc  = (src1 * src2), dest = acc & 0xffff
* VMUDH:  acc  = (src1 * src2) >> 16, dest = acc >> 16

* VMADL:  acc += (src1 * src2) >> 16, dest = acc & 0xffff
* VMADM:  acc += (src1 * src2), dest = acc >> 16
* VMADN:  acc += (src1 * src2), dest = acc & 0xffff
* VMADH:  acc += (src1 * src2) >> 16, dest = acc >> 16
*/

//0xa40011d4: <0x4a058407> VMUDH     vec16  = ( acc  = vec16 * vec05[0a] >> 16 ) >> 16
//sprintf(str, "VMUDH     vec%02d  = ( acc  = vec%02d * vec%02d[%s] >> 16 ) >> 16",
//				vdest(dwOp),
//              vs1(dwOp),
//              vs2(dwOp),
//              g_szVSelName[ve1(dwOp)]);
void RSP_Cop2V_VMUDH(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VMUDH");*/                                                                                                             \
    int i;
	DWORD dwDest = vdest(dwOp);
	DWORD dwVS1  = vs1(dwOp);
	DWORD dwVS2  = vs2(dwOp);
	DWORD dwPattern = ve1(dwOp);

	LOAD_VECTORS(dwVS1, dwVS2, dwPattern);

	for (i = 0; i < 8; i++) 
	{
		g_dwAccu[i] = (s32)((s16)g_wTemp[0][i] * (s16)g_wTemp[1][i]) >> 16; 
		g_wVR[dwDest][i] = (s32)g_dwAccu[i] >> 16;
	}
}

// Same as VMUDH, but accumulates
//1648: <4BFF108F>  vmadh      vec02  = ( acc+= vec02 * vec31[7 ] >>16)>>16
void RSP_Cop2V_VMADH(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VMADH");*/
    int i;
	DWORD dwDest = vdest(dwOp);
	DWORD dwVS1  = vs1(dwOp);
	DWORD dwVS2  = vs2(dwOp);
	DWORD dwPattern = ve1(dwOp);

	LOAD_VECTORS(dwVS1, dwVS2, dwPattern);
	for (i = 0; i < 8; i++) 
	{
		g_dwAccu[i] += (s32)((s16)g_wVR[dwVS1][i] * (s16)g_wVR[dwVS2][i]) >> 16; 
		g_wVR[dwDest][i] = (s32)g_dwAccu[i] >> 16;
	}
}

//0xa4001210: <0x4b018606> VMUDN     vec24  = ( acc  = vec16 * vec01[0 ]       ) >> 16
void RSP_Cop2V_VMUDN(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VMUDN");*/
    int i;
	DWORD dwDest = vdest(dwOp);
	DWORD dwVS1  = vs1(dwOp);
	DWORD dwVS2  = vs2(dwOp);
	DWORD dwPattern = ve1(dwOp);

	LOAD_VECTORS(dwVS1, dwVS2, dwPattern);

	// unsigned??
	for (i = 0; i < 8; i++) 
	{
		g_dwAccu[i] = (s32)((s16)g_wTemp[0][i] * (s16)g_wTemp[1][i]); 
		g_wVR[dwDest][i] = (u16)((s32)g_dwAccu[i] >> 16);
	}
}

// Same as VMUDH, but accumulates
void RSP_Cop2V_VMADN(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VMADN");*/
    int i;
	DWORD dwDest = vdest(dwOp);
	DWORD dwVS1  = vs1(dwOp);
	DWORD dwVS2  = vs2(dwOp);
	DWORD dwPattern = ve1(dwOp);

	LOAD_VECTORS(dwVS1, dwVS2, dwPattern);

	// unsigned??
	for (i = 0; i < 8; i++) 
	{
		g_dwAccu[i] += (s32)((s16)g_wTemp[0][i] * (s16)g_wTemp[1][i]); 
		g_wVR[dwDest][i] = (u16)((s32)g_dwAccu[i] >> 16);
	}
}

//0xa40016b0: <0x4b04cc05> VMUDM     vec16  = ( acc  = vec25 * vec04[0 ] >> 16 )
void RSP_Cop2V_VMUDM(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VMUDM");*/
    int i;
	DWORD dwDest = vdest(dwOp);
	DWORD dwVS1  = vs1(dwOp);
	DWORD dwVS2  = vs2(dwOp);
	DWORD dwPattern = ve1(dwOp);

	LOAD_VECTORS(dwVS1, dwVS2, dwPattern);

	// unsigned??
	for (i = 0; i < 8; i++) 
	{
		g_dwAccu[i] = (s32)((s16)g_wTemp[0][i] * (s16)g_wTemp[1][i]) >> 16; 
		g_wVR[dwDest][i] = (u16)((s32)g_dwAccu[i]);
	}

}

//15E0: <4A05174D>  vmadm      vec29  = ( acc+= vec02 * vec05[0a] >>16)
void RSP_Cop2V_VMADM(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VMADM");*/
    int i;
	DWORD dwDest = vdest(dwOp);
	DWORD dwVS1  = vs1(dwOp);
	DWORD dwVS2  = vs2(dwOp);
	DWORD dwPattern = ve1(dwOp);

	LOAD_VECTORS(dwVS1, dwVS2, dwPattern);

	// unsigned??
	for (i = 0; i < 8; i++) 
	{
		g_dwAccu[i] += (s32)((s16)g_wTemp[0][i] * (s16)g_wTemp[1][i]) >> 16; 
		g_wVR[dwDest][i] = (u16)((s32)g_dwAccu[i]);
	}
}

//15D8: <4A05AF44>  vmudl      vec29  = ( acc = vec21 * vec05[0a]     )
void RSP_Cop2V_VMUDL(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VMUDL");*/
    int i;
	DWORD dwDest = vdest(dwOp);
	DWORD dwVS1  = vs1(dwOp);
	DWORD dwVS2  = vs2(dwOp);
	DWORD dwPattern = ve1(dwOp);

	LOAD_VECTORS(dwVS1, dwVS2, dwPattern);

	// unsigned??
	for (i = 0; i < 8; i++) 
	{
		g_dwAccu[i] = (s32)((s16)g_wTemp[0][i] * (s16)g_wTemp[1][i]); 
		g_wVR[dwDest][i] = (u16)((s32)g_dwAccu[i]);
	}
}


void RSP_Cop2V_VMADL(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VMADL");zelda*/
    int i;
	DWORD dwDest = vdest(dwOp);
	DWORD dwVS1  = vs1(dwOp);
	DWORD dwVS2  = vs2(dwOp);
	DWORD dwPattern = ve1(dwOp);

	LOAD_VECTORS(dwVS1, dwVS2, dwPattern);

	// unsigned??
	for (i = 0; i < 8; i++) 
	{
		g_dwAccu[i] += (s32)((s16)g_wTemp[0][i] * (s16)g_wTemp[1][i]); 
		g_wVR[dwDest][i] = (u16)((s32)g_dwAccu[i]);
	}

}



void RSP_Cop2V_VADD(DWORD dwOp)
{
	DWORD dwDest	= vdest(dwOp);
	DWORD dwA		= vs1(dwOp);
	DWORD dwB		= vs2(dwOp);

	DWORD dwPattern = ve1(dwOp);
	LOAD_VECTORS(dwA, dwB, dwPattern);
	for (DWORD dwV = 0; dwV < 8; dwV++)
	{
		g_wVR[dwDest][dwV] = g_wTemp[0][dwV] + g_wTemp[1][dwV];

		// Add 1 if carry flag set
	/*	if (g_dwCarryFlags & (1<<dwV))
			g_wVR[dwDest][dwV]++;
		g_dwCarryFlags &= 0xff00;*/

		// Clip?
        if((~g_wTemp[0][dwV] & ~g_wTemp[1][dwV] & g_wVR[dwDest][dwV]) & 0x8000)
                g_wVR[dwDest][dwV] = 0x7fff;
        else if((g_wTemp[0][dwV] & g_wTemp[1][dwV] & ~g_wVR[dwDest][dwV]) & 0x8000)
                g_wVR[dwDest][dwV] = 0x8000;
	}


	// To set the carry flags
	/*g_dwCarryFlags = 0;
	for (dwV = 0; dwV < 8; dwV++) {
		if(((s32)g_wTemp[0][dwV] + (s32)g_wTemp[1][dwV]) != (s32)g_wVR[dwDest][dwV])
			g_dwCarryFlags |= (1 << dwV);	// Set carry flag
	}*/
}

void RSP_Cop2V_VSUB(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VSUB");*/
	DWORD dwDest	= vdest(dwOp);
	DWORD dwA		= vs1(dwOp);
	DWORD dwB		= vs2(dwOp);

	DWORD dwPattern = ve1(dwOp);
	LOAD_VECTORS(dwA, dwB, dwPattern);
	for (DWORD dwV = 0; dwV < 8; dwV++)
	{
		g_wVR[dwDest][dwV] = g_wTemp[0][dwV] - g_wTemp[1][dwV];

		// Sub 1 if carry flag set
	/*	if (g_dwCarryFlags & (1<<dwV))
			g_wVR[dwDest][dwV]--;
		g_dwCarryFlags &= 0xff00;*/

		// Clip?
        if((~g_wTemp[0][dwV] & g_wTemp[1][dwV] & g_wVR[dwDest][dwV]) & 0x8000)
                g_wVR[dwDest][dwV] = 0x7fff;
        else if((g_wTemp[0][dwV] & ~g_wTemp[1][dwV] & ~g_wVR[dwDest][dwV]) & 0x8000)
                g_wVR[dwDest][dwV] = 0x8000;
	}



}




void RSP_Cop2V_VSUT(DWORD dwOp)	{ WARN_NOIMPL("RSP: VSUT"); }
void RSP_Cop2V_VABS(DWORD dwOp)	{ WARN_NOIMPL("RSP: VABS"); }
void RSP_Cop2V_VADDC(DWORD dwOp)	{ /*WARN_NOIMPL("RSP: VADDC");*/ }
void RSP_Cop2V_VSUBC(DWORD dwOp)	{ /*WARN_NOIMPL("RSP: VSUBC");*/ }
void RSP_Cop2V_VADDB(DWORD dwOp)	{ WARN_NOIMPL("RSP: VADDB"); }
void RSP_Cop2V_VSUBB(DWORD dwOp)	{ WARN_NOIMPL("RSP: VACCB"); }
void RSP_Cop2V_VACCB(DWORD dwOp)	{ WARN_NOIMPL("RSP: VACCB"); }
void RSP_Cop2V_VSUCB(DWORD dwOp)	{ WARN_NOIMPL("RSP: VSUCB"); }
void RSP_Cop2V_VSAD(DWORD dwOp)	{ WARN_NOIMPL("RSP: VSAD"); }
void RSP_Cop2V_VSAC(DWORD dwOp)	{ WARN_NOIMPL("RSP: VSAC"); }
void RSP_Cop2V_VSUM(DWORD dwOp)	{ WARN_NOIMPL("RSP: VSUM"); }
void RSP_Cop2V_VSAW(DWORD dwOp)	{ /*WARN_NOIMPL("RSP: VSAW");*/ }
void RSP_Cop2V_VEQ(DWORD dwOp)	{ WARN_NOIMPL("RSP: VEQ"); }
void RSP_Cop2V_VNE(DWORD dwOp)	{ /*WARN_NOIMPL("RSP: VNE");zelda*/ }
void RSP_Cop2V_VCL(DWORD dwOp)	{ /*WARN_NOIMPL("RSP: VCL");*/ }
void RSP_Cop2V_VCH(DWORD dwOp)	{ WARN_NOIMPL("RSP: VCH"); }
void RSP_Cop2V_VCR(DWORD dwOp)	{ WARN_NOIMPL("RSP: VCR"); }
void RSP_Cop2V_VMRG(DWORD dwOp)	{ /*WARN_NOIMPL("RSP: VMRG");zelda*/ }
void RSP_Cop2V_VRCP(DWORD dwOp)	{ WARN_NOIMPL("RSP: VRCP"); }
void RSP_Cop2V_VRCPL(DWORD dwOp)	{ WARN_NOIMPL("RSP: VRCPL"); }
void RSP_Cop2V_VRCPH(DWORD dwOp)	{ WARN_NOIMPL("RSP: VRCPH"); }
void RSP_Cop2V_VMOV(DWORD dwOp)	{ /*WARN_NOIMPL("RSP: VMOV");zelda*/ }
void RSP_Cop2V_VRSQ(DWORD dwOp)	{ WARN_NOIMPL("RSP: VRSQ"); }
void RSP_Cop2V_VRSQL(DWORD dwOp)	{ WARN_NOIMPL("RSP: VRSQL"); }
void RSP_Cop2V_VRSQH(DWORD dwOp)	{ WARN_NOIMPL("RSP: VRSQH"); }


/*
        PRINT_DEBUGGING_INFO("VGE", V_V_VE)

        STORE_REGISTER
        VECT_OP_COMP(>=)
        VECT_OP_COPY(rsp_reg.flag[1])
        rsp_reg.flag[0] = 0;

*/
void RSP_Cop2V_VGE(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VGE");zelda*/
	LONG 	i;																		
	DWORD dwDest	= vdest(dwOp);
	DWORD dwA		= (dwOp >> 11) & 0x1f;
	DWORD dwB		= (dwOp >> 16) & 0x1f;
	DWORD dwPattern = ve1(dwOp);
	LOAD_VECTORS(dwA, dwB, dwPattern);

	
	for (i = 0; i < 8; i++)																
	{																				
		if(g_wTemp[0][i] >= g_wTemp[1][i])							
			g_dwCompareFlags |= 1 << i; 											
		else																	
			g_dwCompareFlags &= ~(1 << i);											
			

		if(g_dwCarryFlags & (1 << (i+8)))						
		{														
			g_dwCompareFlags &= ~(1 << i);							
		}	
		
        if(g_dwCompareFlags & (1 << i))
			g_wVR[dwDest][i] = g_wTemp[0][i];
        else
			g_wVR[dwDest][i] = g_wTemp[1][i];
	}																				
	

}



void RSP_Cop2V_VLT(DWORD dwOp)
{
	/*WARN_NOIMPL("RSP: VLT");zelda*/
	LONG 	i;																		
	DWORD dwDest	= vdest(dwOp);
	DWORD dwA		= (dwOp >> 11) & 0x1f;
	DWORD dwB		= (dwOp >> 16) & 0x1f;
	DWORD dwPattern = ve1(dwOp);
	LOAD_VECTORS(dwA, dwB, dwPattern);
	
	for (i = 0; i < 8; i++)																
	{																				
		if(g_wTemp[0][i] < g_wTemp[1][i])							
			g_dwCompareFlags |= 1 << i; 											
		else																	
			g_dwCompareFlags &= ~(1 << i);											
		
		
		if(g_dwCarryFlags & (1 << i)) 
		{
			if(g_wTemp[0][i] == g_wTemp[1][i]) 
				g_dwCompareFlags |= 1 << i;
		}

        if(g_dwCompareFlags & (1 << i))
			g_wVR[dwDest][i] = g_wTemp[0][i];
        else
			g_wVR[dwDest][i] = g_wTemp[1][i];
	}																				

}



void RSP_Cop2V_VAND(DWORD dwOp)
{
	
	DWORD dwDest	= vdest(dwOp);
	DWORD dwA		= (dwOp >> 11) & 0x1f;
	DWORD dwB		= (dwOp >> 16) & 0x1f;
	DWORD dwPattern = ve1(dwOp);
	LOAD_VECTORS(dwA, dwB, dwPattern);

	for (DWORD dwV = 0; dwV < 8; dwV++)
	{
		//DPF(DEBUG_ALWAYS, "%d: A: 0x%04x B: 0x%04x", dwV, g_wTemp[0][dwV], g_wTemp[1][dwV]);
		g_wVR[dwDest][dwV] = g_wTemp[0][dwV] & g_wTemp[1][dwV];
		//DPF(DEBUG_ALWAYS, "%d: D: 0x%04x", dwV, g_wVR[dwDest][dwV]);
	}
}

void RSP_Cop2V_VNAND(DWORD dwOp)
{
	DWORD dwDest	= vdest(dwOp);
	DWORD dwA		= (dwOp >> 11) & 0x1f;
	DWORD dwB		= (dwOp >> 16) & 0x1f;
	DWORD dwPattern = ve1(dwOp);
	LOAD_VECTORS(dwA, dwB, dwPattern);
	for (DWORD dwV = 0; dwV < 8; dwV++) {
		g_wVR[dwDest][dwV] = ~(g_wTemp[0][dwV] & g_wTemp[1][dwV]);
	}
}

void RSP_Cop2V_VOR(DWORD dwOp)
{

	DWORD dwDest	= vdest(dwOp);
	DWORD dwA		= (dwOp >> 11) & 0x1f;
	DWORD dwB		= (dwOp >> 16) & 0x1f;
	DWORD dwPattern = ve1(dwOp);
	LOAD_VECTORS(dwA, dwB, dwPattern);
	for (DWORD dwV = 0; dwV < 8; dwV++) {
		g_wVR[dwDest][dwV] = g_wTemp[0][dwV] | g_wTemp[1][dwV];
	}
}

void RSP_Cop2V_VNOR(DWORD dwOp)
{
	DWORD dwDest	= vdest(dwOp);
	DWORD dwA		= (dwOp >> 11) & 0x1f;
	DWORD dwB		= (dwOp >> 16) & 0x1f;
	DWORD dwPattern = ve1(dwOp);
	LOAD_VECTORS(dwA, dwB, dwPattern);
	for (DWORD dwV = 0; dwV < 8; dwV++) {
		g_wVR[dwDest][dwV] = ~(g_wTemp[0][dwV] | g_wTemp[1][dwV]);
	}
}

void RSP_Cop2V_VXOR(DWORD dwOp)
{
	DWORD dwDest	= vdest(dwOp);
	DWORD dwA		= (dwOp >> 11) & 0x1f;
	DWORD dwB		= (dwOp >> 16) & 0x1f;
	DWORD dwPattern = ve1(dwOp);
	LOAD_VECTORS(dwA, dwB, dwPattern);
	for (DWORD dwV = 0; dwV < 8; dwV++) {
		g_wVR[dwDest][dwV] = g_wTemp[0][dwV] ^ g_wTemp[1][dwV];
	}
}

void RSP_Cop2V_VNXOR(DWORD dwOp)
{

	DWORD dwDest	= vdest(dwOp);
	DWORD dwA		= (dwOp >> 11) & 0x1f;
	DWORD dwB		= (dwOp >> 16) & 0x1f;
	DWORD dwPattern = ve1(dwOp);
	LOAD_VECTORS(dwA, dwB, dwPattern);
	for (DWORD dwV = 0; dwV < 8; dwV++) {
		g_wVR[dwDest][dwV] = ~(g_wTemp[0][dwV] ^ g_wTemp[1][dwV]);
	}
}

