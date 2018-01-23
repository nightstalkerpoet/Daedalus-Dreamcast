// DynarecCode.h: interface for the CDynarecCode class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DYNARECCODE_H__A08EE1AF_55F6_4EA0_BD36_FA1F711265D5__INCLUDED_)
#define AFX_DYNARECCODE_H__A08EE1AF_55F6_4EA0_BD36_FA1F711265D5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef void (* DynarecFunctionType)();

class CDynarecCode  
{
public:
	CDynarecCode(DWORD dwPC, DWORD dwOp);
	virtual ~CDynarecCode();


	void ResetStats();
	void DisplayRegStats();
public:
	union
	{
		DynarecFunctionType pF;			// Our compiled function
		BYTE *pCodeBuffer;
	};
	DWORD dwBuffSize;
	DWORD dwCurrentPos;			// CurrentWriting position in buffer

	DWORD dwStartPC;			// The starting PC of this code fragment
	DWORD dwEndPC;
	DWORD dwOriginalOp;			// The original opcode
	DWORD dwNumOps;				// Number of operations compiled into i386
	DWORD dwNumOptimised;		// Number of operations that have been optimised

	DWORD dwCount;				// The number of times the compiled code has been executed
	DWORD dwOptimiseLevel;		// The level of optimisation applied

	DWORD dwWarn;				// Set if one of the Emit instructions does not have enough space

	DWORD dwRegReads[32];		// #times used at src
	DWORD dwRegUpdates[32];		// #times used as dest
	DWORD dwRegBaseUse[32];		// #times used as base

	DWORD dwSRegReads[32];		// #times used at src
	DWORD dwSRegUpdates[32];	// #times used as dest
	
	DWORD dwDRegReads[32];		// #times used at src
	DWORD dwDRegUpdates[32];	// #times used as dest

	
	BOOL  bSpCachedInESI;		// Is sp cached in ESI?
	DWORD dwSetSpPostUpdate;	// Set Sp base counter after this update

	DWORD dwBranchTarget;		// Target address if we take the branch

	DWORD dwEntry;				// Entry into global array
	

	inline void EmitBYTE(BYTE byte)
	{
		pCodeBuffer[dwCurrentPos] = byte;
		dwCurrentPos++;
	}
	inline void EmitWORD(WORD word)
	{
		*(WORD *)(&pCodeBuffer[dwCurrentPos]) = word;
		dwCurrentPos += 2;
	}
	inline void EmitDWORD(DWORD dword)
	{
        printf( "pCodeBuffer %d (%d)\n", pCodeBuffer, dwCurrentPos );
		*(DWORD *)(&pCodeBuffer[dwCurrentPos]) = dword;
		dwCurrentPos += 4;
	}



};

#endif // !defined(AFX_DYNARECCODE_H__A08EE1AF_55F6_4EA0_BD36_FA1F711265D5__INCLUDED_)
