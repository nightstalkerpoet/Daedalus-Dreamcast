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


static void * WriteInvalid(DWORD dwAddress)
{
	DPF(DEBUG_MEMWARN, "Illegal Memory Access Tried to Write To 0x%08x PC: 0x%08x", dwAddress, g_dwPC);
	if (g_bWarnMemoryErrors)
	{
		CPU_Halt("Illegal Memory Access");
		DBGConsole_Msg(0, "Illegal Memory Access: Tried to Write To 0x%08x (PC: 0x%08x)", dwAddress, g_dwPC);
	}


	return g_pMemoryBuffers[MEM_UNUSED];

}

static void *Write_Noise(DWORD dwAddress)
{
	static BOOL bWarned = FALSE;
	//if (!bWarned)
	{
	//	DBGConsole_Msg(0, "Writing noise (0x%08x) - sizing memory?", dwAddress);
		bWarned = TRUE;
	}
	return g_pMemoryBuffers[MEM_UNUSED];

}


static void * WriteMapped(DWORD dwAddress)
{
	DWORD i;
	DWORD nCount;
	BOOL bMatched;
	DWORD iMatched;
	static DWORD iLastMatched = 0;

#ifndef DAEDALUS_RELEASE_BUILD
	g_dwTLBWHit++;
#endif 

	bMatched = FALSE;
	for (nCount = 0; nCount < 32; nCount++)
	{
		// Hack to check most recently reference entry first
		// This gives some speedup if the matched address is near 
		// the end of the tlb array (32 entries)
		i = (nCount + iLastMatched) & 0x1F;
		
		struct TLBEntry & tlb = g_TLBs[i];

		// Check that the VPN numbers match
		if ((dwAddress & tlb.vpnmask) == tlb.addrcheck)
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
			iLastMatched = i;

			// touch entry to show it has been referenced recently
			// we don't need to do this if this was refenced previously
			g_TLBs[iMatched].lastaccessed = ++s_nTLBTimeNow;
			break;
		}
	}


	if (!bMatched)
	{
		// TLBRefill

		// No valid TLB entry - throw TLB Refill Exception
		TLB_Refill_Store(dwAddress);
		return g_pMemoryBuffers[MEM_UNUSED];
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

			return (g_pu8RamBase + (dwAddr & 0x007FFFFF));
		}
		else
		{
			// Throw TLB Invalid exception
			TLB_Invalid_Store(dwAddress);
			return g_pMemoryBuffers[MEM_UNUSED];
		}
	}
	else   // Even entry 
	{
		if (tlb.pfne & TLBLO_V)
		{
			// Valid
			DWORD dwAddr = tlb.pfnehi | (dwAddress & tlb.mask2);

			return (g_pu8RamBase + (dwAddr & 0x007FFFFF));
		}
		else
		{
			// Throw TLB Invalid exception
			TLB_Invalid_Store(dwAddress);
			return g_pMemoryBuffers[MEM_UNUSED];
		}
	}

}



static void *Write_RAM_4Mb_8000_803F(DWORD dwAddress)
{
	return g_pu8RamBase_8000 + dwAddress;
}

static void *Write_RAM_8Mb_8000_807F(DWORD dwAddress)
{
	return g_pu8RamBase_8000 + dwAddress;
}

static void *Write_RAM_4Mb_A000_A03F(DWORD dwAddress)
{
	return g_pu8RamBase_A000 + dwAddress;
}

static void *Write_RAM_8Mb_A000_A07F(DWORD dwAddress)
{
	return g_pu8RamBase_A000 + dwAddress;
}


// 0x03F0 0000 to 0x03FF FFFF  RDRAM registers
static void *Write_83F0_83F0(DWORD dwAddress)
{
	
	if ((dwAddress&0x1FFFFFFF) < 0x04000000)
	{
		DPF(DEBUG_MEMORY_RDRAM_REG, "Writing to MEM_RD_REG: 0x%08x", dwAddress);
	//	DBGConsole_Msg(0, "Writing to MEM_RD_REG: 0x%08x", dwAddress);

		return ((BYTE *)g_pMemoryBuffers[MEM_RD_REG] + (dwAddress & 0xFF));
	}
	else
	{
		return WriteInvalid(dwAddress);
	} 
}

// 0x0400 0000 to 0x0400 FFFF  SP memory
static void * Write_8400_8400(DWORD dwAddress)
{

	if ((dwAddress&0x1FFFFFFF) <= SP_IMEM_END)
	{
		DPF(DEBUG_MEMORY_SP_IMEM, "Writing to SP_MEM: 0x%08x", dwAddress);

		return ((BYTE *)g_pMemoryBuffers[MEM_SP_MEM] + (dwAddress & 0x1FFF));
	}
	else
	{	
		return WriteInvalid(dwAddress);
	}
}

