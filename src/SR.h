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

#ifndef __SR_H__
#define __SR_H__

#include <vector>
#include "CPU.h"
#include "DynarecCode.h"



HRESULT SR_Init(DWORD dwSize);
void SR_Reset();
void SR_Fini();
CDynarecCode * SR_CompileCode(DWORD dwPC);
void SR_OptimiseCode(CDynarecCode *pCode, DWORD dwLevel);
void SR_Stats();

extern CDynarecCode **g_pDynarecCodeTable;
extern DWORD g_dwNumStaticEntries;

extern DWORD g_dwNumSRCompiled;
extern DWORD g_dwNumSROptimised;
extern DWORD g_dwNumSRFailed;



inline void SR_ExecuteCode(CDynarecCode * pCode)
{
	// Assume that pCode is always found. Possibly an unsafe assumption, but 
	// Good for performance
	pCode->dwCount++;

	// Check if it's worth recompiling this code
	if (pCode->dwCount == 5)
	{
		SR_OptimiseCode(pCode, 0);
	}

	// Execute the compiled code
	pCode->pF();

	// Ignore the last instruction...
	//	*(u32 *)&g_qwCPR[0][COUNT] = (u32)g_qwCPR[0][COUNT] + 1;//(pCode->dwNumOps-1);
	//	g_dwNextInterruptCount -= 1;//(pCode->dwNumOps-1);


}

inline void SR_ExecuteCode_AlreadyOptimised(CDynarecCode * pCode)
{
	// Execute the compiled code
	pCode->pF();
}




#endif
