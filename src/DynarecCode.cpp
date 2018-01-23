// DynarecCode.cpp: implementation of the CDynarecCode class.
//
//////////////////////////////////////////////////////////////////////

#include "main.h"
#include "DynarecCode.h"

#include "debug.h"
#include "registers.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDynarecCode::CDynarecCode(DWORD dwPC, DWORD dwOp)
{
	pCodeBuffer = NULL;
	dwBuffSize = 0;
	dwCurrentPos = 0;

	dwStartPC = dwPC;
	dwEndPC = 0;
	dwOriginalOp = dwOp;
	dwNumOps = 0;
	dwNumOptimised = 0;

	dwCount = 0;
	dwOptimiseLevel = 0;

	dwWarn = 0;

	ResetStats();

	
	bSpCachedInESI = FALSE;		// Is sp cached in ESI?
	dwSetSpPostUpdate = 0;	// Set Sp base counter after this update

	dwBranchTarget = ~0;		// Target address if we take the branch

	dwEntry = 0;				// Entry into global array


}

CDynarecCode::~CDynarecCode()
{

}


void CDynarecCode::ResetStats()
{

	ZeroMemory(dwRegReads, sizeof(dwRegReads));
	ZeroMemory(dwRegUpdates, sizeof(dwRegUpdates));
	ZeroMemory(dwRegBaseUse, sizeof(dwRegBaseUse));

	ZeroMemory(dwSRegReads, sizeof(dwSRegReads));
	ZeroMemory(dwSRegUpdates, sizeof(dwSRegUpdates));

	ZeroMemory(dwDRegReads, sizeof(dwDRegReads));
	ZeroMemory(dwDRegUpdates, sizeof(dwDRegUpdates));

}


void CDynarecCode::DisplayRegStats()
{
	DWORD i;
	BOOL bDisplayedHdr;
	
	// Integer registers
	DPF(DEBUG_DYNREC, "");
	DPF(DEBUG_DYNREC, " Reg:   r   w   b");
	for (i = 0; i < 32; i++)
	{
		if (dwRegReads[i] > 0 ||
			dwRegUpdates[i] > 0 ||
			dwRegBaseUse[i] > 0)
		{
			DPF(DEBUG_DYNREC, "  %s: %3d %3d %3d",
				RegNames[i], dwRegReads[i],
							 dwRegUpdates[i],
							 dwRegBaseUse[i]);
		}

		//Work out what we would optimize
	}

	// Single registers:
	bDisplayedHdr = FALSE;
	for (i = 0; i < 32; i++)
	{
		if (dwSRegReads[i] > 0 ||
			dwSRegUpdates[i] > 0)
		{
			if (!bDisplayedHdr)
			{
				DPF(DEBUG_DYNREC, "");
				DPF(DEBUG_DYNREC, " SReg:   r   w");
				bDisplayedHdr = TRUE;
			}

			DPF(DEBUG_DYNREC, " FP%02d: %3d %3d",
				i, dwSRegReads[i], dwSRegUpdates[i]);
		}

		//Work out what we would optimize
	}

	// Double registers:
	bDisplayedHdr = FALSE;
	for (i = 0; i < 32; i++)
	{
		if (dwDRegReads[i] > 0 ||
			dwDRegUpdates[i] > 0)
		{
			if (!bDisplayedHdr)
			{
				DPF(DEBUG_DYNREC, "");
				DPF(DEBUG_DYNREC, " DReg:   r   w");
				bDisplayedHdr = TRUE;
			}
		
			DPF(DEBUG_DYNREC, "   FP%02d: %3d %3d",
				i, dwDRegReads[i], dwDRegUpdates[i]);
		}

		//Work out what we would optimize
	}
	DPF(DEBUG_DYNREC, "");
}

