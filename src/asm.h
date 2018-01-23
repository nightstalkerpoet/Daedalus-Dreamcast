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

#ifndef __ASM_H__
#define __ASM_H__

// Get the lo and high half indices for MIPS registers
#define lohalf(x)		(x*2 + 0)
#define hihalf(x)		(x*2 + 1)

// Emulated regs 0..31 here

// Intel register codes. Odd ordering is for intel bytecode
enum REGCODE {
	EAX_CODE = 0,
	ECX_CODE = 1,
	EDX_CODE = 2,
	EBX_CODE = 3,
	ESP_CODE = 4,
	EBP_CODE = 5,
	ESI_CODE = 6,
	EDI_CODE = 7
};

// Some more friendly macros:
#define SetVar32(VarName, Value) \
	pCode->EmitWORD(0x05c7); pCode->EmitDWORD((DWORD)&VarName); pCode->EmitDWORD(Value);


// I sometimes use this to force an exceptions so I can analyse the generated code
#define CRASH(x)			pCode->EmitWORD(0x05c7); pCode->EmitDWORD(0x00000000); pCode->EmitDWORD(x) //c7 05 xx+0 xx xx xx ef be ad de

#define CALL_EAX()			pCode->EmitWORD(0xd0ff)
#define RET()				pCode->EmitBYTE(0xC3)

#define PUSH(reg)			pCode->EmitBYTE(0x50 | reg)
#define POP(reg)			pCode->EmitBYTE(0x58 | reg)
		
#define SAVE_ESP() MOV_MEM_REG(&g_SavedESP, ESP_CODE)
#define SAVE_EBP() MOV_MEM_REG(&g_SavedEBP, EBP_CODE)
#define RESTORE_ESP() if (g_bESPInvalid) { MOV_REG_MEM(ESP_CODE, &g_SavedESP); g_bESPInvalid = FALSE; }
#define RESTORE_EBP() if (g_bEBPInvalid) { MOV_REG_MEM(EBP_CODE, &g_SavedEBP); g_bEBPInvalid = FALSE; }

					// e.g ADD_EAX_EBX = 0xc303
#define ADD(reg1, reg2)		pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x03)
#define ADC(reg1, reg2)		pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x13)
#define SUB(reg1, reg2)		pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x2b)
#define SBB(reg1, reg2)		pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x1b)

#define AND(reg1, reg2)		if (reg1 != reg2) {pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x23);}
#define MOV(reg1, reg2)		if (reg1 != reg2) {pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x8B);}
#define  OR(reg1, reg2)		if (reg1 != reg2) {pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x0B);}
#define XCHG(reg1,reg2)		if (reg1 != reg2) {pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x87);}
#define XOR(reg1, reg2)		pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x33)
#define CMP(reg1, reg2)     pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x3b)
#define TEST(reg1, reg2)    pCode->EmitWORD(((0xc0 | (reg1<<3) | reg2)<<8) | 0x85)

#define MOVSX(reg1, reg2)		pCode->EmitWORD(0xbf0f); pCode->EmitBYTE((0xc0 | (reg1<<3) | reg2))

#define SHLI(reg, sa)		pCode->EmitWORD(((0xe0 | reg)<<8) | 0xc1); pCode->EmitBYTE((BYTE)(sa))
#define SHRI(reg, sa)		pCode->EmitWORD(((0xe8 | reg)<<8) | 0xc1); pCode->EmitBYTE((BYTE)(sa))
#define SARI(reg, sa)		pCode->EmitWORD(((0xf8 | reg)<<8) | 0xc1); pCode->EmitBYTE((BYTE)(sa))

#define ADDI(reg, data) /*if (reg == EAX_CODE){ pCode->EmitBYTE(0x05); } else */{ pCode->EmitWORD(((0xc0 | reg)<<8) | 0x81);}	\
						pCode->EmitDWORD(data)

// Use short form (0x83d0) if data is just one byte!
#define ADCI(reg, data) \
	/*if (reg == EAX_CODE){ pCode->EmitBYTE(0x15); pCode->EmitDWORD(data)} else*/ \
	if (data <= 127 && data > -127)							\
	{ 														\
		pCode->EmitWORD(((0xd0 | reg)<<8) | 0x83);			\
		pCode->EmitBYTE((BYTE)(data));						\
	}														\
	else													\
	{														\
		pCode->EmitWORD(((0xd0 | reg)<<8) | 0x81);			\
		pCode->EmitDWORD(data);								\
	}


#define CMPI(reg, data) pCode->EmitWORD(((0xf8 | reg)<<8) | 0x81); pCode->EmitDWORD(data)

#define MUL(reg) pCode->EmitWORD(((0xe0 | reg)<<8) | 0xf7)
#define MUL_EAX_MEM(mem) pCode->EmitBYTE(0xf7); pCode->EmitBYTE(0x25); pCode->EmitDWORD((DWORD)(mem));


#define ADDI_b(reg, data) pCode->EmitWORD(((0xc0 | reg)<<8) | 0x83); pCode->EmitBYTE(data)
#define ADCI_b(reg, data) pCode->EmitWORD(((0xd0 | reg)<<8) | 0x83); pCode->EmitBYTE(data)

#define DEC(reg) pCode->EmitBYTE(0x48 | (reg))



#define MOV_REG_MEM(reg, mem) /*if (reg == EAX_CODE) { pCode->EmitBYTE(0xa1); } else*/ { pCode->EmitWORD( (((reg<<3) | 0x05)<<8) | 0x8b);} \
						      pCode->EmitDWORD((DWORD)(mem))


#define MOV_MEM_REG(mem, reg) /*if (reg == EAX_CODE) { pCode->EmitBYTE(0xa3); } else*/ { pCode->EmitWORD( (((reg<<3) | 0x05)<<8) | 0x89);} \
							  pCode->EmitDWORD((DWORD)(mem))


#define MOVI(reg, data)		pCode->EmitBYTE(0xB8 | (reg)); pCode->EmitDWORD((data))	


#define MOVI_MEM(mem, data) pCode->EmitWORD(0x05C7); pCode->EmitDWORD((DWORD)(mem)); pCode->EmitDWORD(data)

#define ANDI(reg, data) /*if (reg == EAX_CODE) { pCode->EmitBYTE(0x25); } else */{ pCode->EmitWORD(((0xe0 | reg)<<8) | 0x81); } \
						pCode->EmitDWORD((DWORD)(data))

#define XORI(reg, data) /*if (reg == EAX_CODE) { pCode->EmitBYTE(0x35); } else */{ pCode->EmitWORD(((0xf0 | reg)<<8) | 0x81); } \
						pCode->EmitDWORD((DWORD)(data))

#define ORI(reg, data) /*if (reg == EAX_CODE) { pCode->EmitBYTE(0x0D); } else*/ { pCode->EmitWORD(((0xc8 | reg)<<8) | 0x81); } \
					   pCode->EmitDWORD((DWORD)(data))


#define SETNS(reg)	pCode->EmitWORD(0x990f); pCode->EmitBYTE(0xc0 | (reg))
#define SETL(reg)	pCode->EmitWORD(0x9c0f); pCode->EmitBYTE(0xc0 | (reg))
#define SETB(reg)	pCode->EmitWORD(0x920f); pCode->EmitBYTE(0xc0 | (reg))
#define JA(off)		pCode->EmitWORD(((off)<<8) | 0x77)
#define JB(off)		pCode->EmitWORD(((off)<<8) | 0x72)
#define JAE(off)	pCode->EmitWORD(((off)<<8) | 0x73)
#define JE(off)		pCode->EmitWORD(((off)<<8) | 0x74)
#define JNE(off)	pCode->EmitWORD(((off)<<8) | 0x75)
#define JL(off)		pCode->EmitWORD(((off)<<8) | 0x7C)
#define JGE(off)	pCode->EmitWORD(((off)<<8) | 0x7D)
#define JLE(off)	pCode->EmitWORD(((off)<<8) | 0x7E)
#define JG(off)		pCode->EmitWORD(((off)<<8) | 0x7F)
#define JMP(off)	pCode->EmitWORD(((off)<<8) | 0xeb)
#define JMP_DIRECT(off)	pCode->EmitBYTE(0xe9); pCode->EmitDWORD((DWORD)(off))

#define CDQ			pCode->EmitBYTE(0x99)		//cdq						// 99	
		

#define AND_EAX(data) pCode->EmitBYTE(0x25);   pCode->EmitDWORD((DWORD)(data))	//and		eax, 0x12345678 // 25 xx xx xx xx
#define AND_EBX(data) pCode->EmitWORD(0xe381); pCode->EmitDWORD((DWORD)(data))	//and		ebx, 0x12345678 // 81 e3 xx xx xx xx


#define PXOR(mm)	pCode->EmitWORD(0xed0f); pCode->EmitBYTE(0xc0 + (mm)*9);
#define EMMS		pCode->EmitWORD(0x770f);


#define FLD_MEM(mem)  { pCode->EmitWORD(0x05d9);} \
						      pCode->EmitDWORD((DWORD)(mem))

#define FADD_MEM(mem) { pCode->EmitWORD(0x05d8);} \
						      pCode->EmitDWORD((DWORD)(mem))
#define FSUB_MEM(mem) { pCode->EmitWORD(0x25d8);} \
						      pCode->EmitDWORD((DWORD)(mem))
#define FMUL_MEM(mem) { pCode->EmitWORD(0x0dd8);} \
						      pCode->EmitDWORD((DWORD)(mem))
#define FDIV_MEM(mem) { pCode->EmitWORD(0x35d8);} \
						      pCode->EmitDWORD((DWORD)(mem))

#define FSTP_MEM(mem) { pCode->EmitWORD( 0x1dd9);} \
						      pCode->EmitDWORD((DWORD)(mem))

#define FSQRT pCode->EmitWORD(0xfad9);
#define FCHS pCode->EmitWORD(0xe0d9);

//////////////////////////////////////
#define FLD_MEMp(pCode, mem)  { pCode->EmitWORD(0x05d9);} \
						      pCode->EmitDWORD((DWORD)(mem))

#define FSTP_MEMp(pCode, mem) { pCode->EmitWORD( 0x1dd9);} \
						      pCode->EmitDWORD((DWORD)(mem))

#define FLD(pCode, i)	pCode->EmitWORD((WORD)((0xc0 + (i))<<8) |(0xd9)); 


#define FXCH(pCode, i)	pCode->EmitWORD((WORD)((0xc8 + (i))<<8) |(0xd9)); 

#define FFREE(pCode, i)	pCode->EmitWORD((WORD)((0xc0 + (i))<<8) |(0xdd)); 
#define FINCSTP(pCode) pCode->EmitWORD(0xf7d9);
#define FSQRTp(pCode) pCode->EmitWORD(0xfad9);
#define FCHSp(pCode) pCode->EmitWORD(0xe0d9);

// fadd st(0),st(i)
#define FADD(pCode, i)	pCode->EmitWORD((WORD)((0xc0 + (i))<<8) |(0xd8)); 

// fsub st(0),st(i)
#define FSUB(pCode, i)	pCode->EmitWORD((WORD)((0xe0 + (i))<<8) |(0xd8)); 

// fmul st(0),st(i)
#define FMUL(pCode, i)	pCode->EmitWORD((WORD)((0xc8 + (i))<<8) |(0xd8)); 

// fdiv st(0),st(i)
#define FDIV(pCode, i)	pCode->EmitWORD((WORD)((0xf0 + (i))<<8) |(0xd8)); 





#endif
