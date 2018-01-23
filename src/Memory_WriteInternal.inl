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




static DWORD InternalWriteInvalid(DWORD dwAddress, void ** pTranslated)
{
	//CPUHalt();
	//DBGConsole_Msg(0, "Illegal Write at 0x%08x", dwAddress);
	//DPF(DEBUG_MEMWARN, "Illegal Memory Access Tried to Write To 0x%08x PC: 0x%08x", dwAddress, g_dwPC);

	*pTranslated = g_pMemoryBuffers[MEM_UNUSED];

	return MEM_UNUSED;
}



static DWORD InternalWriteMapped(DWORD dwAddress, void ** pTranslated)
{
	DWORD i;
	BOOL bMatched;
	DWORD iMatched;

	bMatched = FALSE;
	for (i = 0; i < 32; i++)
	{
		struct TLBEntry & tlb = g_TLBs[i];

		// Check that the VPN numbers match
		if ((dwAddress & tlb.vpnmask) == (tlb.hi & tlb.vpnmask))
		{
			if (!tlb.g)
			{
				if ( (tlb.hi & TLBHI_PIDMASK) !=
					 ((u32)g_qwCPR[0][C0_ENTRYHI] & TLBHI_PIDMASK) )
				{
					// Entries ASID must match.
					continue;
				}
			}

			bMatched = TRUE;
			iMatched = i;
			break;
		}
	}


	if (!bMatched)
	{
		// TLBRefill

		// No valid TLB entry - throw TLB Refill Exception
		if (g_bWarnMemoryErrors)
		{
			DBGConsole_Msg(0, "Internal TLB Refill", dwAddress);
		}
		return InternalWriteInvalid(dwAddress, pTranslated);
	}

	//DBGConsole_Msg(0, "ReadMapped (%d)", iMatched);
	struct TLBEntry & tlb = g_TLBs[iMatched];
		
	// Global
	if (dwAddress & tlb.checkbit) // Odd entry
	{
		if (tlb.pfno & TLBLO_V)
		{
			// Valid
			DWORD dwAddr = tlb.pfnohi | (dwAddress & tlb.mask2);

			*pTranslated = g_pu8RamBase + (dwAddr & 0x007FFFFF);
			return MEM_RD_RAM;
		}
		else
		{
			// Throw TLB Invalid exception
			if (g_bWarnMemoryErrors)
			{
				DBGConsole_Msg(0, "Internal TLB Invalid");
			}
			return InternalWriteInvalid(dwAddress, pTranslated);
		}
	}
	else   // Even entry 
	{
		if (tlb.pfne & TLBLO_V)
		{
			// Valid
			DWORD dwAddr = tlb.pfnehi | (dwAddress & tlb.mask2);

			*pTranslated = g_pu8RamBase + (dwAddr & 0x007FFFFF);
			return MEM_RD_RAM;
		}
		else	// Throw TLB Invalid exception
		{
			if (g_bWarnMemoryErrors)
			{
				DBGConsole_Msg(0, "Internal TLB Invalid");
			}
			return InternalWriteInvalid(dwAddress, pTranslated);
		}
	}

}


static DWORD InternalWrite_4Mb_8000_803F(DWORD dwAddress, void ** pTranslated)
{
	*pTranslated = g_pu8RamBase + (dwAddress & 0x003FFFFF);
	return MEM_RD_RAM;
}

static DWORD InternalWrite_8Mb_8000_807F(DWORD dwAddress, void ** pTranslated)
{
	*pTranslated = g_pu8RamBase + (dwAddress & 0x007FFFFF);
	return MEM_RD_RAM;
}


