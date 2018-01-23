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

#ifndef __PATCH_H__
#define __PATCH_H__

#include <vector>
#include "opcode.h"		// For SRHACK_???? etc
#include "sr.h"			// For g_p
#include "cpu.h"		// Breakpoint stuff

#define PATCH_RET_NOT_PROCESSED 0
#define PATCH_RET_JR_RA 1
#define PATCH_RET_ERET 2

// Returns return type to determine what kind of post-processing should occur
typedef DWORD (*PatchFunction)();

#define VAR_ADDRESS(name)  (g_##name##_v.dwLocation)
#define VAR_FOUND(name)  (g_##name##_v.bFound)


enum PATCH_CROSSREF_TYPE
{
	PX_JUMP,
	PX_VARIABLE_HI,
	PX_VARIABLE_LO
};

struct _PatchSymbol;



typedef struct
{
	DWORD dwOffset;				// Opcode offset
	PATCH_CROSSREF_TYPE nType;	// Type - J/JAL/LUI/ORI
	struct _PatchSymbol * pSymbol;		// Only one of these is valid
	struct _PatchVariable * pVariable;
} PatchCrossRef;

#define PATCH_PARTIAL_CRC_LEN 4

typedef struct
{
	DWORD nNumOps;
	PatchCrossRef * pCrossRefs;
	PatchFunction pFunction;

	u32		firstop;			// First op in signature (used for rapid scanning)
	u32		partial_crc;		// Crc after 4 ops (ignoring symbol bits)
	u32		crc;				// Crc for entire buffer (ignoring symbol bits)
} PatchSignature;


typedef struct _PatchSymbol
{
	BOOL bFound;				// Has this symbol been found?
	DWORD dwLocation;			// What is the address of the symbol?
	LPCTSTR szName;				// Symbol name

	DWORD dwReplacedOp;			// The op that was replaced when we were patched in

	PatchSignature * pSignatures; // Crossrefs for this code (Function symbols only)

	PatchFunction pFunction;	// The installed patch Emulated function call (Function symbols only)


} PatchSymbol;

typedef struct _PatchVariable
{
	BOOL bFound;				// Has this symbol been found?
	DWORD dwLocation;			// What is the address of the symbol?
	LPCTSTR szName;				// Symbol name

	// For now have these as separate fields. We probably want to 
	// put them into a union
	BOOL bFoundHi; BOOL bFoundLo; 
	WORD wHiPart; WORD wLoPart;

} PatchVariable;
///////////////////////////////////////////////////////////////

#define BEGIN_PATCH_XREFS(label) \
static PatchCrossRef g_##label##_x[] = {

#define PATCH_XREF_FUNCTION(offset, symbol) \
	{ offset, PX_JUMP, &g_##symbol##_s, NULL },

#define PATCH_XREF_VAR(offsethi, offsetlo, symbol) \
{ offsethi, PX_VARIABLE_HI, NULL, &g_##symbol##_v }, \
{ offsetlo, PX_VARIABLE_LO, NULL, &g_##symbol##_v },


#define PATCH_XREF_VAR_LO(offset, symbol) \
{ offset, PX_VARIABLE_LO, NULL, &g_##symbol##_v },

#define PATCH_XREF_VAR_HI(offset, symbol) \
{ offset, PX_VARIABLE_HI, NULL, &g_##symbol##_v },

#define END_PATCH_XREFS() \
	{ ~0, PX_JUMP, NULL, NULL }		\
};

/////////////////////////////////////////////////////////////////

#define BEGIN_PATCH_SIGNATURE_LIST(name) \
static PatchSignature	g_##name##_sig[] = {

#define PATCH_SIGNATURE_LIST_ENTRY(label, numops, firstop, partial, crc) \
{ numops, g_##label##_x, Patch_##label, firstop, partial, crc },

#define END_PATCH_SIGNATURE_LIST() \
{ NULL, NULL, NULL, 0, 0 }         \
};
///////////////////////////////////////////////////////////////

#define PATCH_SYMBOL_FUNCTION(name) \
PatchSymbol g_##name##_s = { FALSE, 0, #name, 0, g_##name##_sig, NULL };

#define PATCH_SYMBOL_VARIABLE(name) \
PatchVariable g_##name##_v = { FALSE, 0, #name, FALSE, 0, FALSE, 0};


#define NUM_PATCH_FUNCTIONS 107
#define NUM_PATCH_VARIABLES 51

#define BEGIN_PATCH_SYMBOL_TABLE(name) \
PatchSymbol * name[NUM_PATCH_FUNCTIONS] = {

#define PATCH_FUNCTION_ENTRY(name)  \
	&g_##name##_s,

#define END_PATCH_SYMBOL_TABLE() \
};

#define BEGIN_PATCH_VARIABLE_TABLE(name) \
PatchVariable * name[NUM_PATCH_VARIABLES] = {

#define PATCH_VARIABLE_ENTRY(name)  \
	&g_##name##_v,

#define END_PATCH_VARIABLE_TABLE() \
};



extern PatchSymbol * g_PatchSymbols[NUM_PATCH_FUNCTIONS];
extern PatchVariable * g_PatchVariables[NUM_PATCH_VARIABLES];





inline DWORD GetCorrectOp(DWORD dwOp)
{
	if (op(dwOp) == OP_SRHACK_UNOPT ||
		op(dwOp) == OP_SRHACK_OPT ||
		op(dwOp) == OP_SRHACK_NOOPT)
	{
		DWORD dwEntry = dwOp & 0x03ffffff;
		if (dwEntry < g_dwNumStaticEntries)
			dwOp = g_pDynarecCodeTable[dwEntry]->dwOriginalOp;
	}
	else if (op(dwOp) == OP_DBG_BKPT)
	{
		DWORD dwBreakPoint = dwOp & 0x03ffffff;

		if (dwBreakPoint < g_BreakPoints.size())
			dwOp = g_BreakPoints[dwBreakPoint].dwOriginalOp;
	}

	// Hacks may have replaced patch, so drop through to below:
	if (op(dwOp) == OP_PATCH)
	{
		DWORD dwPatch = dwOp & 0x00FFFFFF;

		if (dwPatch < ARRAYSIZE(g_PatchSymbols))
			dwOp = g_PatchSymbols[dwPatch]->dwReplacedOp;
	}

	return dwOp;
}

inline BOOL IsJumpTarget(DWORD dwOp)
{
	if (op(dwOp) == OP_SRHACK_UNOPT ||
		op(dwOp) == OP_SRHACK_OPT ||
		op(dwOp) == OP_SRHACK_NOOPT)
	{
		DWORD dwEntry = dwOp & 0x03ffffff;
		if (dwEntry < g_dwNumStaticEntries)
			dwOp = g_pDynarecCodeTable[dwEntry]->dwOriginalOp;

	}

	// Hacks may have replaced patch
	if (op(dwOp) == OP_PATCH)
	{
		return TRUE;
	}

	return FALSE;

}

void Patch_Reset();
void Patch_ApplyPatches();
LPCSTR Patch_GetJumpAddressName(DWORD dwJumpTarget);
DWORD Patch_GetSymbolAddress(LPCSTR szName);

void Patch_DumpOsThreadInfo();
void Patch_DumpOsQueueInfo();
void Patch_DumpOsEventInfo();

#endif
