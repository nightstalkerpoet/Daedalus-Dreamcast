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



static DWORD InternalReadInvalid(DWORD dwAddress, void ** pTranslated)
{
	//DBGConsole_Msg(0, "Illegal Internal Read of 0x%08x at 0x%08x", dwAddress, (DWORD)g_dwPC);
	*pTranslated = g_pMemoryBuffers[MEM_UNUSED];
	return MEM_UNUSED;

}

static DWORD InternalReadMapped(DWORD dwAddress, void ** pTranslated);

static DWORD InternalReadMappedGolden(DWORD dwAddress, void ** pTranslated)
{
	DWORD dwTranslated = (dwAddress - 0x7f000000) + GOLDENEYE_HACKED_OFFSET;

	if ((dwTranslated&0x00FFFFFF) < MemoryRegionSizes[MEM_CARTROM])
	{
		*pTranslated = (BYTE *)g_pMemoryBuffers[MEM_CARTROM] + (dwTranslated & 0x00FFFFFF);
		return MEM_CARTROM;
	}
	else
	{
		return InternalReadInvalid(dwAddress, pTranslated);
	}
}

static DWORD InternalReadMapped(DWORD dwAddress, void ** pTranslated)
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
		return InternalReadInvalid(dwAddress, pTranslated);
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
			return InternalReadInvalid(dwAddress, pTranslated);
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
			return InternalReadInvalid(dwAddress, pTranslated);
		}
	}

}


static DWORD InternalRead_4Mb_8000_803F(DWORD dwAddress, void ** pTranslated)
{
	*pTranslated = g_pu8RamBase + (dwAddress & 0x003FFFFF);
	return MEM_RD_RAM;
}

static DWORD InternalRead_8Mb_8000_807F(DWORD dwAddress, void ** pTranslated)
{
	*pTranslated = g_pu8RamBase + (dwAddress & 0x007FFFFF);
	return MEM_RD_RAM;
}


static DWORD InternalReadROM(DWORD dwAddress, void ** pTranslated)
{
	// Note: NOT 0x1FFFFFFF
	if ((dwAddress&0x00FFFFFF) < MemoryRegionSizes[MEM_CARTROM])
	{
		//DPF(DEBUG_MEMORY, "Reading from ROM (MEM_CARTROM): 0x%08x", dwAddress);
		*pTranslated = (BYTE *)g_pMemoryBuffers[MEM_CARTROM] + (dwAddress & 0x00FFFFFF);
		return MEM_CARTROM;
	}
	else 
	{
		return InternalReadInvalid(dwAddress, pTranslated);
	} 

}

static DWORD InternalRead_8400_8400(DWORD dwAddress, void ** pTranslated)
{
	DWORD dwOffset;

	// 0x0400 0000 to 0x0400 FFFF  SP registers
	if ((dwAddress&0x1FFFFFFF) < 0x4002000)
	{
		DPF(DEBUG_MEMORY_SP_IMEM, "Reading from SP_MEM: 0x%08x", dwAddress);

		dwOffset = dwAddress & 0x1FFF;

		*pTranslated = (BYTE *)g_pMemoryBuffers[MEM_SP_MEM] + dwOffset;
		return MEM_SP_MEM;
	}
	else
	{
		return InternalReadInvalid(dwAddress, pTranslated);
	}
}

static DWORD InternalRead_9FC0_9FCF(DWORD dwAddress, void ** pTranslated)
{
	DWORD dwOffset;
	
	if ((dwAddress&0x1FFFFFFF) <= PIF_ROM_END)
	{
		DPF(DEBUG_MEMORY_PIF, "Reading from MEM_PI_ROM: 0x%08x", dwAddress);

		dwOffset = dwAddress & 0x0FFF;

		*pTranslated = (BYTE *)g_pMemoryBuffers[MEM_PI_ROM] + dwOffset;
		return MEM_PI_ROM;
	}
 	
	else if ((dwAddress&0x1FFFFFFF) <= PIF_RAM_END)
	{
		DPF(DEBUG_MEMORY_PIF, "Reading from MEM_PI_RAM: 0x%08x", dwAddress);
		DBGConsole_Msg(0, "[WReading directly from PI ram]: 0x%08x!", dwAddress);

		dwOffset = (dwAddress&0x1FFFFFFF) - PIF_RAM_START;

		*pTranslated = (BYTE *)g_pMemoryBuffers[MEM_PI_RAM] + dwOffset;
		return MEM_PI_RAM;
	}
	else
	{
		return InternalReadInvalid(dwAddress, pTranslated);
	} 
}



