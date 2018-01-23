/*
Copyright (C) 2001 Lkb

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

// these #defines conflict with <map>
#undef op
#undef base
#undef rs


#pragma warning(disable:4786)		//identifier was truncated to '255' characters in the debug information


#include <map>

using namespace std;

// ModR/M true/false
// Immediate 0 = none 8 = byte 16 = word 32 = dword -1 = word/dword

#define X86_LENGTH 0xff
#define X86_MODRM 0x100
#define X86_VARIABLE 0x200
#define X86_POINTER 0x400
#define X86_UNDEFINED 0x800
#define X86_PREFIX 0x1000
#define X86_IF_MODRM_OP_0_IMM_BYTE 0x2000
#define X86_IF_MODRM_OP_0_IMM_VARIABLE 0x4000

// ModR/M
#define M X86_MODRM
// Variable 16/32 Immediate
#define V X86_VARIABLE
// Variable 32/48 Pointer
#define P X86_POINTER
// Extended
#define X U
// Undefined
#define U X86_UNDEFINED

#define PR X86_PREFIX
// Coprocessor
#define C M

// Hacks for Intel PDF Group 3
#define M0B X86_IF_MODRM_OP_0_IMM_BYTE
#define M0V X86_IF_MODRM_OP_0_IMM_VARIABLE

#define P3 U

// 00-05 ADD				08-0D OR
// 10-15 ADC				18-1D SBB
// 20-25 ADD				28-2D SUB
// 30-35 XOR				38-3D CMP
// 40-47 INC ExX			48-4F DEC ExX
// 50-57 PUSH ExX			58-5F POP ExX
//				70-7F Jcc
// 90-97 XCHG EAX, ExX
// B0-B7 MOV ExX, imm8		B8-BF MOV ExX, immV
//							D8-DF X87

DWORD g_nX86OpcodeMap[] =
{
	//		0		1		2		3		4		5		6		7			8		9		A		B		C		D		E		F
	/*0*/	M,		M,		M,		M,		1,		V,		0,		0,			M,		M,		M,		M,		M,		M,		0,		X,		// 0f = special
	/*1*/	M,		M,		M,		M,		1,		V,		0,		0,			M,		M,		M,		M,		M,		M,		0,		0,
	/*2*/	M,		M,		M,		M,		1,		V,		PR,		0,			M,		M,		M,		M,		M,		M,		PR,		0,
	/*3*/	M,		M,		M,		M,		1,		V,		PR,		0,			M,		M,		M,		M,		M,		M,		PR,		0,
	
	/*4*/	0,		0,		0,		0,		0,		0,		0,		0,			0,		0,		0,		0,		0,		0,		0,		0,
	/*5*/	0,		0,		0,		0,		0,		0,		0,		0,			0,		0,		0,		0,		0,		0,		0,		0,

	/*6*/	0,		0,		M,		M,		PR,		PR,		PR,		PR,			V,		M | V,	1,		M | 1,	0,		0,		0,		0,
	/*7*/	1,		1,		1,		1,		1,		1,		1,		1,			1,		1,		1,		1,		1,		1,		1,		1,
	/*8*/	M | 1,	M | V,	M | 1,	M | 1,	M,		M,		M,		M,			M,		M,		M,		M,		M,		M,		M,		M,
	/*9*/	0,		0,		0,		0,		0,		0,		0,		0,			0,		0,		P,		0,		0,		0,		0,		0,
	/*A*/	1,		V,		1,		V,		0,		0,		0,		0,			1,		V,		0,		0,		0,		0,		0,		0,
	/*B*/	1,		1,		1,		1,		1,		1,		1,		1,			V,		V,		V,		V,		V,		V,		V,		V,
	/*C*/	M | 1,	M | 1,	2,		0,		M,		M,		M | 1,	M | V,		3,		0,		2,		0,		0,		1,		0,		0,
	/*D*/	1,		V,		1,		V,		1,		1,		U,		0,			C,		C,		C,		C,		C,		C,		C,		C,
	/*E*/	1,		1,		1,		1,		1,		1,		1,		1,			V,		V,		P,		1,		0,		0,		0,		0,
	/*F*/	PR,		U,		PR,		PR,		0,		0,		M|M0B,	M|M0V,		0,		0,		0,		0,		0,		0,		M,		M
};

DWORD g_nX86TwoByteOpcodeMap[] =
{
	//		0		1		2		3		4		5		6		7			8		9		A		B		C		D		E		F
	/*0*/	M,		M,		M,		M,		U,		U,		0,		U,			0,		0,		U,		U,		U,		U,		U,		U,
	/*1*/	P3,		P3,		P3,		P3,		P3,		P3,		P3,		P3,			M,		U,		U,		U,		U,		U,		U,		U,
	/*2*/	M,		M,		M,		M,		U,		U,		U,		U,			P3,		P3,		P3,		P3,		P3,		P3,		P3,		P3,
	/*3*/	0,		0,		0,		0,		0,		0,		U,		U,			U,		U,		U,		U,		U,		U,		U,		U,
	
	/*4*/	M,		M,		M,		M,		M,		M,		M,		M,			M,		M,		M,		M,		M,		M,		M,		M,
	
	/*5*/	P3,		P3,		P3,		P3,		P3,		P3,		P3,		P3,			P3,		P3,		U,		U,		P3,		P3,		P3,		P3,
	/*6*/	P3,		P3,		P3,		P3,		P3,		P3,		P3,		P3,			P3,		P3,		P3,		P3,		U,		U,		P3,		P3,
	/*7*/	P3,		P3,		P3,		P3,		P3,		P3,		P3,		P3,			P3,		P3,		P3,		P3,		P3,		P3,		P3,		P3,

	/*8*/	V,		V,		V,		V,		V,		V,		V,		V,			V,		V,		V,		V,		V,		V,		V,		V,
	/*9*/	M,		M,		M,		M,		M,		M,		M,		M,			M,		M,		M,		M,		M,		M,		M,		M,

	/*A*/	0,		0,		0,		M,		M | 1,	M,		U,		U,			0,		0,		0,		M,		M | 1,	M,		M,		M,
	/*B*/	M,		M,		M,		M,		M,		M,		M,		M,			U,		U,		M | 1,	M,		M,		M,		M,		M,

	/*C*/	M,		M,		P3,		U,		P3,		P3,		P3,		M,			0,		0,		0,		0,		0,		0,		0,		0,
	/*D*/	U,		P3,		P3,		P3,		U,		P3,		U,		P3,			P3,		P3,		P3,		P3,		P3,		P3,		P3,		P3,
	/*E*/	P3,		P3,		P3,		P3,		P3,		P3,		U,		P3,			P3,		P3,		P3,		P3,		P3,		P3,		P3,		P3,
	/*F*/	U,		P3,		P3,		P3,		U,		P3,		P3,		P3,			P3,		P3,		P3,		U,		P3,		P3,		P3,		U
};

#undef M
#undef V
#undef P
#undef X
#undef U
#undef C
#undef M0B
#undef M0V
#undef P3

#define X86_PREFIX_LENGTH_MASK 0xf
#define X86_OPCODE_LENGTH_MASK 0xf0
#define X86_MODRM_LENGTH_MASK 0xf00
#define X86_IMMEDIATE_LENGTH_MASK 0xf000

#define X86_ADDRESS32 0x10000
#define X86_OPERAND32 0x10000

/*
Prefixes 0-4
Opcode 1-2
ModR/M + SIB 0-2
Displacement 0-4
Immediate 0-4
*/

static DWORD ReadUnsigned(void* p, int nBytes)
{
	switch(nBytes)
	{
		case 1:
			return (DWORD)(*(BYTE*)p);
		case 2:
			return (DWORD)(*(WORD*)p);
		//case 3:
		//	return (DWORD)(*(WORD*)p | (((BYTE*)p)[2] << 16));
		default:
			return (DWORD)(*(DWORD*)p);
	}
}

static LONG ReadSigned(void* p, int nBytes)
{
	switch(nBytes)
	{
		case 1:
			return (LONG)(*(signed char*)p);
		case 2:
			return (LONG)(*(SHORT*)p);
		//case 3:
		//	return (DWORD)(*(WORD*)p | (((BYTE*)p)[2] << 16));
		default:
			return (LONG)(*(LONG*)p);
	}
}

static void WriteValue(void* p, int nBytes, DWORD nValue)
{
	switch(nBytes)
	{
		case 1:
			*(BYTE*)p = (BYTE)nValue;
			break;
		case 2:
			*(WORD*)p = (WORD)nValue;
			break;
		//case 3:
		//	return (DWORD)(*(WORD*)p | (((BYTE*)p)[2] << 16));
		default:
			*(DWORD*)p = (DWORD)nValue;
			break;
	}
}

struct X86InstructionInfo
{
private:
	BYTE* m_pInstruction;
	BYTE* m_pOpcode;
	BYTE* m_pModRM;
	BYTE* m_pDisplacement;
	BYTE* m_pImmediate;
	BYTE* m_pEnd;
	DWORD m_nOperandSize;
	DWORD m_nAddressSize;

public:
	BOOL Fill(BYTE* pCode)
	{
		ZeroMemory(this, sizeof(X86InstructionInfo));
		m_nOperandSize = 4;
		m_nAddressSize = 4;
		m_pInstruction = pCode;
		DWORD nOpcode;
		DWORD nOpcodeFlags;
			
		while(TRUE)
		{
			nOpcode = *pCode++;
			nOpcodeFlags = g_nX86OpcodeMap[nOpcode];
			if(nOpcode == 0x66)
			{
				m_nOperandSize = (m_nOperandSize == 2) ? 4 : 2;
			}
			else if(nOpcode == 0x67)
			{
				m_nAddressSize = (m_nOperandSize == 2) ? 4 : 2;
			}
			else if(nOpcodeFlags & X86_PREFIX)
			{
				// nop
			}
			else
			{			
				break;
			}
		}
		m_pOpcode = pCode - 1;
		if(nOpcode == 0x0f)
		{
			nOpcode = *pCode++;
			nOpcodeFlags = g_nX86TwoByteOpcodeMap[nOpcode];
		}

		if(nOpcodeFlags & X86_UNDEFINED)
			return FALSE;

		if(nOpcodeFlags & X86_MODRM)
		{
			m_pModRM = pCode;

			DWORD nModRM = *pCode++;
			
			DWORD nModAndRM = nModRM & 0xc7;
			DWORD nMod = nModRM & 0xc0;
			DWORD nRM = nModRM & 0x7;
			if((m_nAddressSize == 4) && (nRM == 4) && (nMod != 0xc0))
			{
				DWORD nSIB = *pCode++;
				DWORD nBase = nSIB & 7;
				if((nBase == 5) && (nMod == 0))
				{
					m_pDisplacement = pCode;
					pCode += m_nAddressSize;
				}
			}
			
			if((nModAndRM == 5) || (nMod == 0x80))
			{
				m_pDisplacement = pCode;
				pCode += m_nAddressSize;
			}
			else if(nMod == 0x40)
			{
				m_pDisplacement = pCode;
				pCode += 1;
			}
		}

		m_pImmediate = pCode;

		if(nOpcodeFlags & X86_IF_MODRM_OP_0_IMM_BYTE)
		{
			if((*m_pModRM & 0x38) == 0)		// Was 0x1c
			{
				pCode++;
			}
		}

		if(nOpcodeFlags & X86_IF_MODRM_OP_0_IMM_VARIABLE)
		{
			if((*m_pModRM & 0x38) == 0)		// Was 0x1c
			{
				pCode += m_nOperandSize;
			}
		}

		if(nOpcodeFlags & X86_VARIABLE)
		{
			pCode += m_nOperandSize;
		}

		if(nOpcodeFlags & X86_POINTER)
		{
			pCode += m_nOperandSize + 2;
		}

		pCode += (nOpcodeFlags & X86_LENGTH);

		m_pEnd = pCode;

		return TRUE;
	}

	inline LONG GetSignedImmediate()
	{
		return ReadSigned(m_pImmediate, m_pEnd - m_pImmediate);
	}

	inline DWORD GetUnsignedImmediate()
	{
		return ReadUnsigned(m_pImmediate, m_pEnd - m_pImmediate);
	}

	inline BYTE* GetImmediateAddress()
	{
		return m_pImmediate;
	}

	inline DWORD GetImmediateSize()
	{
		return m_pEnd - m_pImmediate;
	}

	inline BYTE GetOpcode(int n)
	{
		return m_pOpcode[n];
	}

	inline BYTE* GetAddress()
	{
		return m_pInstruction;
	}

	inline BYTE* GetByteAfter()
	{
		return m_pEnd;
	}

	// this function doesn't check is the ModR/M is present
	inline BYTE GetModRMByte()
	{
		return m_pModRM[0];
	}

	inline BYTE GetModRMRegOp()
	{
		return GetModRMByte() & 0x1c;
	}
};

struct SRInlinerFunctionBranchInfo
{
public:
	BYTE* pStart;
	DWORD nSize; // total size including last instruction
	BYTE* pLastInstruction;
	//BOOL bEndsWithReturn;

	SRInlinerFunctionBranchInfo(BYTE* p_pStart = NULL) : pStart(p_pStart), nSize(0), pLastInstruction(0) //, bEndsWithReturn(0)
	{
	}

	void End(X86InstructionInfo& x86info) //, BOOL bIsReturn = FALSE)
	{
		pLastInstruction = x86info.GetAddress();
		nSize = x86info.GetByteAfter() - pStart;
		//bEndsWithReturn = bIsReturn;
	}
};

/*
PUSH retaddr // only if multiple returns or absolute jumps
<code>
ADD ESP, 
	4 // only if multiple returns or absolute jumps
	+ stackpop // only if RET pops from stack
ret:
*/

// up to 1 MB per inlined function
#define SRINLINER_PATCH_SIZE_MASK 0xf000000
#define SRINLINER_PATCH_TYPE_ADD_FUNC_SUB_DRCODE 0x00000000
#define SRINLINER_PATCH_TYPE_ADD_DRCODE_SUB_FUNC 0x40000000

#define SRINLINER_PATCH_TYPE_WRITEBYTE 0x80000000
// value is the byte to write

#define SRINLINER_PATCH_TYPE_INSERTORREMOVE 0xc0000000
// value is signed: >0 insert nnn bytes <0 remove nnn bytes

#define SRINLINER_PATCH_GET_TYPE(p) ((p) & 0xf0000000)
#define SRINLINER_PATCH_GET_VALUE(p) (((p) >> 20) & 0xf)
#define SRINLINER_PATCH_GET_OFFSET(p) ((p) & 0x000fffff)

#define SRINLINER_PATCH_MAKE_VALUE(v) (((v) & 0xf) << 20)

// WriteByte and InsertOrRemove patches not implemented and currently not required

struct SRInlinerFunctionData
{
	BOOL bPushReturnAddress;

	BYTE* pFunction;
	DWORD nCodeSize;
	
	DWORD nPopAmount;

	vector<DWORD> patches;
};

DWORD g_nSR_Inliner_AnalyzeFunction_MaxSize = 4096;

BOOL SR_Inliner_AnalyzeFunction(void* pFunction, SRInlinerFunctionData* pData)
{
	vector<SRInlinerFunctionBranchInfo*> analyzedBranches;
	vector<SRInlinerFunctionBranchInfo*> todoBranches;
	vector<SRInlinerFunctionBranchInfo*>::iterator analyzedBranchesIter;
	vector<SRInlinerFunctionBranchInfo*>::iterator todoBranchesIter;
	todoBranches.push_back(new SRInlinerFunctionBranchInfo((BYTE*)pFunction));
	DWORD nNonTrackedJumps = 0;
	DWORD nReturns = 0;
	DWORD nInstructions = 0;

	//DBGConsole_Msg(0, "SR Inliner: Analyzing %08x...", pFunction);

	//???
	//GetDesktopWindow();

	void* pBreakOn = NULL; //R4300_Cop0_MTC0;

    /*KOS
	if(pFunction == pBreakOn)
		DebugBreak();
    */

	while(TRUE)
	{
		// remove repeated entries (if we don't do this we will get into an infinite loop when processing loops)
		
		for ( analyzedBranchesIter = analyzedBranches.begin( ); analyzedBranchesIter < analyzedBranches.end( ); analyzedBranchesIter++ )
		{
			for ( todoBranchesIter = todoBranches.begin( ); todoBranchesIter < todoBranches.end( ); todoBranchesIter++ )
			{
				if(((*todoBranchesIter)->pStart >= (*analyzedBranchesIter)->pStart) && ((*todoBranchesIter)->pStart < ((*analyzedBranchesIter)->pStart + (*analyzedBranchesIter)->nSize)))
				{
					delete *todoBranchesIter;
					todoBranchesIter = todoBranches.erase(todoBranchesIter);
				}
			}
		}

		if(todoBranches.size() == 0)
			break;

		SRInlinerFunctionBranchInfo* currentBranch = todoBranches.back();
		todoBranches.pop_back();
		analyzedBranches.push_back(currentBranch);
		BYTE* pCode = currentBranch->pStart;
		while(TRUE)
		{
			nInstructions++;
			X86InstructionInfo x86info;
			if(!x86info.Fill(pCode))
			{
				DBGConsole_Msg(0, "SR Inliner: Failed! Undefined opcode!");
				return FALSE;
			}

			BYTE* pJumpTo = NULL;
			BOOL bRelativeJump = FALSE;
			BOOL bJumpAlways = FALSE;
			BOOL bJumpIsCall = FALSE;

			if((x86info.GetOpcode(0) & 0xf0) == 0x70)
			{
				// Jcc rel8
				pJumpTo = x86info.GetByteAfter() + x86info.GetSignedImmediate();
				bRelativeJump = TRUE;
			}
			else if((x86info.GetOpcode(0) == 0xe9) || (x86info.GetOpcode(0) == 0xeb))
			{
				// JMP rel
				pJumpTo = x86info.GetByteAfter() + x86info.GetSignedImmediate();
				bRelativeJump = TRUE;
				bJumpAlways = TRUE;
			}
			else if((x86info.GetOpcode(0) == 0xea) || ((x86info.GetOpcode(0) == 0xff) && ((x86info.GetModRMRegOp() == 4) || (x86info.GetModRMRegOp() == 5))))
			{
				// JMP abs
				// TODO: implement (requires parsing of ModR/M+SIB)
				nNonTrackedJumps++;
				currentBranch->End(x86info);
				break;
			}
			else if((x86info.GetOpcode(0) == 0xc3) || (x86info.GetOpcode(0) == 0xcb) || (x86info.GetOpcode(0) == 0xc2) || (x86info.GetOpcode(0) == 0xca))
			{
				// RET
				nReturns++;
				currentBranch->End(x86info);
				break;
			}
			else if(x86info.GetOpcode(0) == 0xe8)
			{
				// CALL rel16/32
				pJumpTo = x86info.GetByteAfter() + x86info.GetSignedImmediate();
				bRelativeJump = TRUE;
				bJumpIsCall = TRUE;
			}
			else if(x86info.GetOpcode(0) == 0x0f)
			{
				if((x86info.GetOpcode(1) & 0xf0) == 0x80)
				{
					// Jcc rel16/32
					pJumpTo = x86info.GetByteAfter() + x86info.GetSignedImmediate();
					bRelativeJump = TRUE;
				}
			}

			if(pJumpTo != NULL)
			{
				if((nInstructions == 1) && bJumpAlways)
				{
					// pFunction points to a jump table entry
					nInstructions--;
					currentBranch->pStart = pJumpTo;
					pFunction = pJumpTo;
					pCode = pJumpTo;
                    /*KOS
					if(pFunction == pBreakOn)
						DebugBreak();
                    */

					continue;
				}
				BOOL bPatchJump = FALSE;
				if(bJumpIsCall)
				{
					if(bRelativeJump)
					{
						bPatchJump = TRUE;
					}
				}
				else
				{
					if((pJumpTo < pFunction) || (pJumpTo >= ((BYTE*)pFunction + g_nSR_Inliner_AnalyzeFunction_MaxSize)))
					{
						if(bRelativeJump)
						{
							bPatchJump = TRUE;
						}
						nNonTrackedJumps++;
					}
					else
					{
						if(!bRelativeJump)
						{
							bPatchJump = TRUE;
						}
						todoBranches.push_back(new SRInlinerFunctionBranchInfo(pJumpTo));
					}
				}
				if(bPatchJump)
				{
					// Relative jumps/calls that point to memory not inlined must be patched
					// Absolute jumps that point to inlined memory must be patched since there may be relative jumps to patch in the code they point to
					pData->patches.push_back((x86info.GetImmediateAddress() - (BYTE*)pFunction) | (bRelativeJump ? SRINLINER_PATCH_TYPE_ADD_FUNC_SUB_DRCODE : SRINLINER_PATCH_TYPE_ADD_DRCODE_SUB_FUNC) | SRINLINER_PATCH_MAKE_VALUE(x86info.GetImmediateSize()));
				}
				if(bJumpAlways)
				{
					currentBranch->End(x86info);
					break;
				}
			}

			pCode = x86info.GetByteAfter();
		}
	}

	BYTE* pFirstByte = (BYTE*)0xffffffff;
	BYTE* pByteAfterEnd = (BYTE*)0;
	BYTE* pLastInstruction = (BYTE*)0;
	for ( analyzedBranchesIter = analyzedBranches.begin( ); analyzedBranchesIter != analyzedBranches.end( ); analyzedBranchesIter++ )
	{
		pFirstByte = (BYTE*)min((DWORD)((*analyzedBranchesIter)->pStart), (DWORD)pFirstByte);
		pByteAfterEnd = (BYTE*)max((DWORD)((*analyzedBranchesIter)->pStart) + (*analyzedBranchesIter)->nSize, (DWORD)pByteAfterEnd);
		pLastInstruction = (BYTE*)max((DWORD)((*analyzedBranchesIter)->pLastInstruction), (DWORD)pLastInstruction);
		delete *analyzedBranchesIter;
	}

	for ( todoBranchesIter = todoBranches.begin( ); todoBranchesIter != todoBranches.end( ); todoBranchesIter++ )
	{
		delete *todoBranchesIter;
	}

	if(pFirstByte != pFunction)
	{
		DBGConsole_Msg(0, "SR Inliner: Failed! Code before entry point!");
		return FALSE;
	}

	vector<DWORD>::iterator patchesIter;
	for ( patchesIter = pData->patches.begin( ); patchesIter < pData->patches.end( ); patchesIter++ )
	{
		vector<DWORD>::iterator patchesIter2;
		for ( patchesIter2 = patchesIter + 1; patchesIter2 < pData->patches.end( ); patchesIter2++ )
		{
			if(SRINLINER_PATCH_GET_OFFSET(*patchesIter) == SRINLINER_PATCH_GET_OFFSET(*patchesIter2))
			{
				patchesIter2 = pData->patches.erase(patchesIter2);
			}
		}
	}

	DWORD nLastInstruction = *pLastInstruction;
	DWORD nPopAmount = 0; // if this remains 0, it means that the last instruction is a JMP, not a RET
	if((nLastInstruction == 0xc3) || (nLastInstruction == 0xcb))
	{
		pByteAfterEnd = pLastInstruction;
		nPopAmount = 4;
	}
	else if((nLastInstruction == 0xc2) || (nLastInstruction == 0xca))
	{
		pByteAfterEnd = pLastInstruction;
		nPopAmount = 4 + *(WORD*)(pLastInstruction + 1);
	}

	pData->pFunction = pFirstByte;
	pData->nCodeSize = pByteAfterEnd - pFirstByte;
	pData->bPushReturnAddress = (nReturns > 1) || ((nReturns == 1) && (nPopAmount == 0)) || (nNonTrackedJumps > 0);
	pData->nPopAmount = nPopAmount ? (nPopAmount - (pData->bPushReturnAddress ? 0 : 4)) : 0;
	return TRUE;
}

map<void*, SRInlinerFunctionData*> g_SRInlinerMap;

SRInlinerFunctionData* SR_Inliner_LookupOrAnalyzeFunction(void* pFunction)
{
	map<void*, SRInlinerFunctionData*>::const_iterator iter = g_SRInlinerMap.find(pFunction);

	if(iter == g_SRInlinerMap.end())
	{
		SRInlinerFunctionData* pData = new SRInlinerFunctionData();

		if(!SR_Inliner_AnalyzeFunction(pFunction, pData))
		{
			delete pData;
			pData = NULL;
		}
		
		g_SRInlinerMap.insert(map<void*, SRInlinerFunctionData*>::value_type(pFunction, pData));

		return pData;
	}

	return (*iter).second;
}

BOOL SR_Emit_Inline_Analyzed_Function(CDynarecCode *pCode, SRInlinerFunctionData& funcData)
{
	if(funcData.pFunction == NULL)
		return FALSE;

	// Remember to comment again
// 0001:000106b0       ?R4300_Cop1_D_LE@@YIXK@Z   004116b0 f   R4300.obj
//  001:0000fab0       ?R4300_Cop1_S_LE@@YIXK@Z   00410ab0 f   R4300.obj
	/*DBGConsole_Msg(0, "SR Inliner: Emitting %08x at PC %08X. range %08x->%08x",
		funcData.pFunction, g_dwPC,
		pCode->pCodeBuffer + pCode->dwCurrentPos,
		pCode->pCodeBuffer + pCode->dwCurrentPos + funcData.nCodeSize );*/

	if(funcData.bPushReturnAddress)
	{
		// PUSH imm32
		pCode->EmitBYTE(0x68);
		pCode->EmitDWORD((DWORD)(pCode->pCodeBuffer) + pCode->dwCurrentPos + 4 + funcData.nCodeSize + 6);
	}
	BYTE* pInlinedBody = pCode->pCodeBuffer + pCode->dwCurrentPos;
	
	memcpy(pCode->pCodeBuffer + pCode->dwCurrentPos, funcData.pFunction, funcData.nCodeSize);
	pCode->dwCurrentPos += funcData.nCodeSize;



	if(funcData.nPopAmount > 0)
	{
		ADDI(ESP_CODE, funcData.nPopAmount);
	}

	vector<DWORD>::const_iterator patchesIter;

	for ( patchesIter = funcData.patches.begin( ); patchesIter != funcData.patches.end( ); patchesIter++ )
	{
		DWORD nPatch = *patchesIter;
		DWORD nPatchParam = SRINLINER_PATCH_GET_VALUE(nPatch);
		BYTE* pPatch = pInlinedBody + SRINLINER_PATCH_GET_OFFSET(nPatch);
		
		int nValue = ReadSigned(pPatch, nPatchParam);

		switch(SRINLINER_PATCH_GET_TYPE(nPatch))
		{
			case SRINLINER_PATCH_TYPE_ADD_FUNC_SUB_DRCODE:
				nValue = nValue + funcData.pFunction - pInlinedBody;
				break;
			case SRINLINER_PATCH_TYPE_ADD_DRCODE_SUB_FUNC:
				nValue = nValue + pInlinedBody - funcData.pFunction;
				break;
		}
		//DBGConsole_Msg(0, "SR Inliner: Patching %08X from %08X to %08X", pPatch, ReadSigned(pPatch, nPatchParam), nValue);
		
		WriteValue(pPatch, nPatchParam, nValue);
	}

	return TRUE;
}

BOOL SR_Emit_Inline_Function(CDynarecCode *pCode, void* pFunction)
{
	SRInlinerFunctionData* pData;
	pData = SR_Inliner_LookupOrAnalyzeFunction(pFunction);
	if(pData == NULL)
		return FALSE;
	
	return SR_Emit_Inline_Analyzed_Function(pCode, *pData);
}

/*
Patch SR_Emit_Generic_R4300 in SR.cpp with this to activate SR Inliner with standard dynarec
NOTE: You may have to increase the dynarec buffer size to avoid exhausting it

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

	if(!SR_Emit_Inline_Function(pCode, pF))
	{
		// Call function
		pCode->EmitBYTE(0xB8); pCode->EmitDWORD((DWORD)pF);	// Copy p to eax
		CALL_EAX();
	}

}
*/
