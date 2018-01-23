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

#ifndef __OPCODE_H__
#define __OPCODE_H__

void SprintOpCodeInfo(char *str, DWORD dwAddr, DWORD oc);

void SprintRSPOpCodeInfo(char *str, DWORD dwAddr, DWORD oc);

#define make_op(x)   ((x) << 26)

#define op(x)    ((x & 0xFC000000)>>26)
#define rs(x)    ((x & 0x03E00000)>>21)
#define rt(x)    ((x & 0x001F0000)>>16)
#define rd(x)    ((x & 0x0000F800)>>11)
#define sa(x)    ((x & 0x000007C0)>>6)

#define imm(x)   ((x & 0x0000FFFF))
#define funct(x) ((x & 0x0000003F))
#define target(x)((x & 0x3ffffff))

#define cop0fmt(x)((x & 0x03E00000)>>21) 
#define cop1fmt(x)((x & 0x03E00000)>>21) 
#define cop2fmt(x)((x & 0x03E00000)>>21) 

#define bc(x) ((x>>16)&0x3)					// For BC1 Stuff

#define ft(x)     ((x & 0x001F0000)>>16)
#define fs(x)     ((x & 0x0000F800)>>11)
#define fd(x)	  ((x & 0x000007C0)>>6) 

// For LWC2 etc
#define base(x)	  rs(x)
#define dest(x)   rt(x)
#define sel(x)    ((x & 0x00000780)>>7)
#define offset(x) funct(x)
// This sign extends wOffset, as it's only 7bits long (not 8)
#define rsp_vecoffset(oc) (s16)(s8) ((oc) & 0x40 ? (((oc) & 0x7F)|0x80) : ((oc) & 0x7F))

#define vdest(x) ((x>>6)&0x1F)
#define vs1(x) ((x>>11)&0x1F)
#define vs2(x) ((x>>16)&0x1F)
#define ve1(x) ((x>>21)&0x0F)
#define vmovoff(x) ((sa(x)>>1)&0x0F)

/*
    CPU: Instructions encoded by opcode field.
    31---------26---------------------------------------------------0
    |  opcode   |                                                   |
    ------6----------------------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | *1    | *2    | J     | JAL   | BEQ   | BNE   | BLEZ  | BGTZ  |
001 | ADDI  | ADDIU | SLTI  | SLTIU | ANDI  | ORI   | XORI  | LUI   |
010 | *3    | *4    |  ---  |  ---  | BEQL  | BNEL  | BLEZL | BGTZL |
011 | DADDI |DADDIU | LDL   | LDR   |  xxx  |  xxx  |  xxx  |  xxx  | top 4 bits == 7
100 | LB    | LH    | LWL   | LW    | LBU   | LHU   | LWR   | LWU   |
101 | SB    | SH    | SWL   | SW    | SDL   | SDR   | SWR   | CACHE |
110 | LL    | LWC1  |  ---  |  ---  | LLD   | LDC1  | LDC2  | LD    |
111 | SC    | SWC1  |  xxx  |  ---  | SCD   | SDC1  | SDC2  | SD    |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
     *1 = SPECIAL, see SPECIAL list    *2 = REGIMM, see REGIMM list
     *3 = COP0                         *4 = COP1
*/

#define OP_IS_A_HACK(x) (((x) >> 28) == 0x7)


enum OpCodeValue {
	OP_SPECOP		= 0,
	OP_REGIMM		= 1,
	OP_J			= 2,
	OP_JAL			= 3,
	OP_BEQ			= 4,
	OP_BNE			= 5,
	OP_BLEZ		= 6,
	OP_BGTZ		= 7,
	OP_ADDI		= 8,
	OP_ADDIU		= 9,
	OP_SLTI		= 10,
	OP_SLTIU		= 11,
	OP_ANDI		= 12,
	OP_ORI			= 13,
	OP_XORI		= 14,
	OP_LUI			= 15,
	OP_COPRO0		= 16,
	OP_COPRO1		= 17,
/*	OP_SRHACK1		= 18,
	OP_SRHACK2		= 19,*/
	OP_BEQL		= 20,
	OP_BNEL		= 21,
	OP_BLEZL		= 22,
	OP_BGTZL		= 23,
	OP_DADDI		= 24,
	OP_DADDIU		= 25,
	OP_LDL			= 26,
	OP_LDR			= 27,
	OP_PATCH		= 28,
	OP_SRHACK_UNOPT = 29,
	OP_SRHACK_OPT   = 30,
	OP_SRHACK_NOOPT = 31,
/*	OP_UNK4		= 29,
	OP_UNK5		= 30,
	OP_UNK6		= 31,*/
	OP_LB			= 32,
	OP_LH			= 33,
	OP_LWL			= 34,
	OP_LW 			= 35,
	OP_LBU			= 36,
	OP_LHU			= 37,
	OP_LWR			= 38,
	OP_LWU			= 39,
	OP_SB			= 40,
	OP_SH			= 41,
	OP_SWL			= 42,
	OP_SW			= 43,
	OP_SDL			= 44,
	OP_SDR			= 45,
	OP_SWR			= 46,
	OP_CACHE		= 47,
	OP_LL			= 48,
	OP_LWC1			= 49,
	OP_UNK7			= 50,
	OP_UNK8			= 51,
	OP_LLD			= 52,
	OP_LDC1			= 53,
	OP_LDC2 		= 54,
	OP_LD			= 55,
	OP_SC 			= 56,
	OP_SWC1 		= 57,
	OP_DBG_BKPT		= 58,
	OP_UNK10 		= 59,
	OP_SCD 			= 60,
	OP_SDC1 		= 61,
	OP_SDC2 		= 62,
	OP_SD			= 63
};

/*
    SPECIAL: Instr. encoded by function field when opcode field = SPECIAL.
    31---------26------------------------------------------5--------0
    | = SPECIAL |                                         | function|
    ------6----------------------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | SLL   |  ---  | SRL   | SRA   | SLLV  |  ---  | SRLV  | SRAV  |
001 | JR    | JALR  |  ---  |  ---  |SYSCALL| BREAK |  ---  | SYNC  |
010 | MFHI  | MTHI  | MFLO  | MTLO  | DSLLV |  ---  | DSRLV | DSRAV |
011 | MULT  | MULTU | DIV   | DIVU  | DMULT | DMULTU| DDIV  | DDIVU |
100 | ADD   | ADDU  | SUB   | SUBU  | AND   | OR    | XOR   | NOR   |
101 |  ---  |  ---  | SLT   | SLTU  | DADD  | DADDU | DSUB  | DSUBU |
110 | TGE   | TGEU  | TLT   | TLTU  | TEQ   |  ---  | TNE   |  ---  |
111 | DSLL  |  ---  | DSRL  | DSRA  |DSLL32 |  ---  |DSRL32 |DSRA32 |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/

enum SpecialOpCodeValue {
	OP_SLL					= 0,
	OP_UNK11				= 1,
	OP_SRL					= 2,
	OP_SRA					= 3,
	OP_SLLV				= 4,
	OP_UNK12				= 5,
	OP_SRLV				= 6,
	OP_SRAV				= 7,
	OP_JR					= 8,
	OP_JALR				= 9,
	OP_UNK13				= 10,
	OP_UNK14				= 11,
	OP_SYSCALL				= 12,
	OP_BREAK				= 13,
	OP_UNK15				= 14,
	OP_SYNC				= 15,
	OP_MFHI				= 16,
	OP_MTHI				= 17,
	OP_MFLO				= 18,
	OP_MTLO				= 19,
	OP_DSLLV				= 20,
	OP_UNK16				= 21,
	OP_DSRLV				= 22,
	OP_DSRAV				= 23,
	OP_MULT				= 24,
	OP_MULTU				= 25,
	OP_DIV					= 26,
	OP_DIVU				= 27,
	OP_DMULT				= 28,
	OP_DMULTU				= 29,
	OP_DDIV				= 30,
	OP_DDIVU				= 31,
	OP_ADD					= 32,
	OP_ADDU				= 33,
	OP_SUB					= 34,
	OP_SUBU				= 35,
	OP_AND					= 36,
	OP_OR					= 37,
	OP_XOR					= 38,
	OP_NOR					= 39,
	OP_UNK17				= 40,
	OP_UNK18				= 41,
	OP_SLT					= 42,
	OP_SLTU				= 43,
	OP_DADD				= 44,
	OP_DADDU				= 45,
	OP_DSUB				= 46,
	OP_DSUBU				= 47,
	OP_TGE					= 48,
	OP_TGEU				= 49,
	OP_TLT					= 50,
	OP_TLTU				= 51,
	OP_TEQ					= 52,
	OP_UNK19				= 53,
	OP_TNE					= 54,
	OP_UNK20				= 55,
	OP_DSLL				= 56,
	OP_UNK21				= 57,
	OP_DSRL				= 58,
	OP_DSRA				= 59,
	OP_DSLL32				= 60,
	OP_UNK22				= 61,
	OP_DSRL32				= 62,
	OP_DSRA32				= 63
};

/*
    REGIMM: Instructions encoded by the rt field when opcode field = REGIMM.
    31---------26----------20-------16------------------------------0
    | = REGIMM  |          |   rt    |                              |
    ------6---------------------5------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | BLTZ  | BGEZ  | BLTZL | BGEZL |  ---  |  ---  |  ---  |  ---  |
 01 | TGEI  | TGEIU | TLTI  | TLTIU | TEQI  |  ---  | TNEI  |  ---  |
 10 | BLTZAL| BGEZAL|BLTZALL|BGEZALL|  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
 */

enum RegImmOpCodeValue {
	OP_BLTZ				= 0,
	OP_BGEZ				= 1,
	OP_BLTZL				= 2,
	OP_BGEZL				= 3,
	OP_TGEI				= 8,
	OP_TGEIU				= 9,
	OP_TLTI				= 10,
	OP_TLTIU				= 11,
	OP_TEQI				= 12,
	OP_TNEI				= 14,
	OP_BLTZAL				= 16,
	OP_BGEZAL				= 17,
	OP_BLTZALL				= 18,
	OP_BGEZALL				= 19
};

/*
    COP0: Instructions encoded by the fmt field when opcode = COP0.
    31--------26-25------21 ----------------------------------------0
    |  = COP0   |   fmt   |                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | MFC0  |  ---  |  ---  |  ---  | MTC0  |  ---  |  ---  |  ---  |
 01 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 | *1    |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
     *1 = TLB instr, see TLB list
*/



enum Cop0OpCodeValue {
	OP_MFC0	= 0,
	OP_MTC0	= 4,
	OP_TLB		= 16
};

/*
    TLB: Instructions encoded by the function field when opcode
         = COP0 and fmt = TLB.
    31--------26-25------21 -------------------------------5--------0
    |  = COP0   |  = TLB  |                               | function|
    ------6----------5-----------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 |  ---  | TLBR  | TLBWI |  ---  |  ---  |  ---  | TLBWR |  ---  |
001 | TLBP  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
011 | ERET  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
100 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
101 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
110 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
111 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|

*/



enum TLBOpCodeValue {
	OP_TLBR = 1,
	OP_TLBWI = 2,
	OP_TLBWR = 6,
	OP_TLBBP = 8,
	OP_ERET = 24
};


/*
    COP1: Instructions encoded by the fmt field when opcode = COP1.
    31--------26-25------21 ----------------------------------------0
    |  = COP1   |   fmt   |                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | MFC1  | DMFC1 | CFC1  |  ---  | MTC1  | DMTC1 | CTC1  |  ---  |
 01 | *1    |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 | *2    | *3    |  ---  |  ---  | *4    | *5    |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
     *1 = BC instructions, see BC1 list
     *2 = S instr, see FPU list            *3 = D instr, see FPU list
     *4 = W instr, see FPU list            *5 = L instr, see FPU list
*/


enum Cop1OpCodeValue {
	OP_MFC1	= 0,
	OP_DMFC1	= 1,
	OP_CFC1	= 2,
	OP_MTC1	= 4,
	OP_DMTC1	= 5,
	OP_CTC1	= 6,

	BCInstr = 8,
	OP_SInstr = 16,
	OP_DInstr = 17,
	OP_WInstr = 20,
	OP_LInstr = 21

};
#define	OP_CVT_S 32
#define	OP_CVT_D 33

/*
    FPU: Instructions encoded by the function field when opcode = COP1
         and fmt = S
    31--------26-25------21 -------------------------------5--------0
    |  = COP1   |  = S    |                               | function|
    ------6----------5-----------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | ADD   | SUB   | MUL   | DIV   | SQRT  | ABS   | MOV   | NEG   |
001 |ROUND.L|TRUNC.L| CEIL.L|FLOOR.L|ROUND.W|TRUNC.W| CEIL.W|FLOOR.W|
010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
011 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
100 |  ---  | CVT.D |  ---  |  ---  | CVT.W | CVT.L |  ---  |  ---  |
101 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
110 | C.F   | C.UN  | C.EQ  | C.UEQ | C.OLT | C.ULT | C.OLE | C.ULE |
111 | C.SF  | C.NGLE| C.SEQ | C.NGL | C.LT  | C.NGE | C.LE  | C.NGT |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/

/*
    FPU: Instructions encoded by the function field when opcode = COP1
         and fmt = D
    31--------26-25------21 -------------------------------5--------0
    |  = COP1   |  = D    |                               | function|
    ------6----------5-----------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | ADD   | SUB   | MUL   | DIV   | SQRT  | ABS   | MOV   | NEG   |
001 |ROUND.L|TRUNC.L| CEIL.L|FLOOR.L|ROUND.W|TRUNC.W| CEIL.W|FLOOR.W|
010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
011 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
100 | CVT.S |  ---  |  ---  |  ---  | CVT.W | CVT.L |  ---  |  ---  |
101 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
110 | C.F   | C.UN  | C.EQ  | C.UEQ | C.OLT | C.ULT | C.OLE | C.ULE |
111 | C.SF  | C.NGLE| C.SEQ | C.NGL | C.LT  | C.NGE | C.LE  | C.NGT |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/

/*
    FPU: Instructions encoded by the function field when opcode = COP1
         and fmt = W
    31--------26-25------21 -------------------------------5--------0
    |  = COP1   |  = W    |                               | function|
    ------6----------5-----------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
001 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
011 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
100 | CVT.S | CVT.D |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
101 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
110 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
111 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|

*/

static char *Cop1WOpCodeNames[64] = {
	"-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?",
	"-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?",
	"-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?",
	"-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?",
	"CVT.S.W", "CVT.D.W", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?",
	"-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?",
	"-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?",
	"-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?", "-C1W?"
};
static char *Cop1LOpCodeNames[64] =
{
	"-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?",
	"-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?",
	"-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?",
	"-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?",
	"CVT.S.L", "CVT.D.L", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?",
	"-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?",
	"-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?",
	"-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?", "-C1L?"
};



/*
    BC1: Instructions encoded by the nd and tf fields when opcode
         = COP1 and fmt = BC
    31--------26-25------21 ---17--16-------------------------------0
    |  = COP1   |  = BC   |    |nd|tf|                              |
    ------6----------5-----------1--1--------------------------------
    |---0---|---1---| tf
  0 | BC1F  | BC1T  |
  1 | BC1FL | BC1TL |
 nd |-------|-------|
*/

static char *Cop1BC1OpCodeNames[4] = {
	"BC1F", "BC1T",
	"BC1FL", "BC1TL"
};

enum Cop1BC1OpCodeValue {
	OP_BC1F = 0,
	OP_BC1T = 1,
	OP_BC1FL = 2,
	OP_BC1TL = 3
};

//RSP Stuff:
/*
    RSP CPU: Instructions encoded by opcode field.
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


/*
    RSP COP0: Instructions encoded by the fmt field when opcode = COP0.
    31--------26-25------21 ----------------------------------------0
    |  010000   |   fmt   |                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | MFC0  |  ---  |  ---  |  ---  | MTC0  |  ---  |  ---  |  ---  |
 01 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/

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



#endif
