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

#include <cstdlib>


static void * ReadInvalid(DWORD dwAddress)
{
	DPF(DEBUG_MEMWARN, "Illegal Memory Access - Tried to Read From 0x%08x (PC: 0x%08x)", dwAddress, g_dwPC);
	if (g_bWarnMemoryErrors)
	{
		CPU_Halt("Illegal Memory Access");
		DBGConsole_Msg(0, "Illegal Memory Access - Tried to Read From 0x%08x (PC: 0x%08x)", dwAddress, g_dwPC);
	}

	return g_pMemoryBuffers[MEM_UNUSED];

}

static void * Read_Noise(DWORD dwAddress)
{
	//CPUHalt();
	//DBGConsole_Msg(0, "Reading noise (0x%08x)", dwAddress);

	static BOOL bWarned = FALSE;
	if (!bWarned)
	{
		DBGConsole_Msg(0, "Reading noise (0x%08x) - sizing memory?", dwAddress);
		bWarned = TRUE;
	}
	*(DWORD*)((BYTE *)g_pMemoryBuffers[MEM_UNUSED] + 0) = rand();
	*(DWORD*)((BYTE *)g_pMemoryBuffers[MEM_UNUSED] + 4) = rand();

	return g_pMemoryBuffers[MEM_UNUSED];



}

static void * ReadMapped(DWORD dwAddress);

static void * ReadMappedGolden(DWORD dwAddress)
{
	DWORD dwTranslated = (dwAddress- 0x7f000000) + GOLDENEYE_HACKED_OFFSET;
#ifndef DAEDALUS_RELEASE_BUILD
	g_dwTLBRHit++;
#endif

	return (BYTE *)g_pMemoryBuffers[MEM_CARTROM] + (dwTranslated & 0x00FFFFFF);
}

void TLB_Refill_Load(DWORD dwVAddr)
{	
	DWORD dwVPN2;

	//DBGConsole_Msg(DEBUG_TLB, "[WTLB: Refill] 0x%08x at 0x%08x", dwVAddr, g_dwPC);
	DPF(DEBUG_TLB, "TLB: Refill - 0x%08x at 0x%08x", dwVAddr, g_dwPC);

	g_qwCPR[0][C0_BADVADDR] = dwVAddr;

	dwVPN2 = (dwVAddr>>TLBHI_VPN2SHIFT);	// Shift off odd/even indicator

	g_qwCPR[0][C0_CONTEXT] &= ~0x007FFFFF;	// Mask off bottom 23 bits
	g_qwCPR[0][C0_CONTEXT] |= (dwVPN2<<4);

	g_qwCPR[0][C0_ENTRYHI] = (s64)(s32)(dwVPN2 << TLBHI_VPN2SHIFT);

	g_nTLBExceptionReason = EXCEPTION_TLB_REFILL_LOAD;
	AddCPUJob(CPU_TLB_EXCEPTION);
}

void TLB_Invalid_Load(DWORD dwVAddr)
{
	DWORD dwVPN2;

	//DBGConsole_Msg(DEBUG_TLB, "[WTLB: Invalid] 0x%08x at 0x%08x", dwVAddr, g_dwPC);
	DPF(DEBUG_TLB, "TLB: Invalid - 0x%08x at 0x%08x", dwVAddr, g_dwPC);

	g_qwCPR[0][C0_BADVADDR] = dwVAddr;

	dwVPN2 = (dwVAddr>>TLBHI_VPN2SHIFT);	// Shift off odd/even indicator

	g_qwCPR[0][C0_CONTEXT] &= ~0x007FFFFF;	// Mask off bottom 23 bits
	g_qwCPR[0][C0_CONTEXT] |= (dwVPN2<<4);

	g_qwCPR[0][C0_ENTRYHI] = (s64)(s32)(dwVPN2 << TLBHI_VPN2SHIFT);
	

	// Jump to common exception vector, use TLBL or TLBS in ExcCode field
	g_nTLBExceptionReason = EXCEPTION_TLB_INVALID_LOAD;
	AddCPUJob(CPU_TLB_EXCEPTION);
}


static void * ReadMapped(DWORD dwAddress)
{
	DWORD i;
	DWORD nCount;
	BOOL bMatched;
	DWORD iMatched;

#ifndef DAEDALUS_RELEASE_BUILD
	g_dwTLBRHit++;
#endif 

	static DWORD iLastMatched = 0;

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
		TLB_Refill_Load(dwAddress);

		return g_pMemoryBuffers[MEM_UNUSED];
	}


	struct TLBEntry & tlb = g_TLBs[iMatched];

	// Global
	if (dwAddress & tlb.checkbit) // Odd entry
	{
		if (tlb.pfno & TLBLO_V)
		{
			// Valid
			DWORD dwAddr = tlb.pfnohi | (dwAddress & tlb.mask2);

			return g_pu8RamBase + (dwAddr & 0x007FFFFF);
			//return ReadAddress(dwAddr);  ??
		}
		else
		{
			// Throw TLB Invalid exception
			TLB_Invalid_Load(dwAddress);
			return g_pMemoryBuffers[MEM_UNUSED];
		}
	}
	else   // Even entry 
	{
		if (tlb.pfne & TLBLO_V)
		{
			// Valid
			DWORD dwAddr = tlb.pfnehi | (dwAddress & tlb.mask2);

			return g_pu8RamBase + (dwAddr & 0x007FFFFF);
			//return ReadAddress(dwAddr);	??		
		}
		else	// Throw TLB Invalid exception
		{
			TLB_Invalid_Load(dwAddress);
			return g_pMemoryBuffers[MEM_UNUSED];
		}
	}
}


// Main memory
/*static void *Read_RAM_4Mb_8000_803F(DWORD dwAddress)
{
	return g_pu8RamBase + (dwAddress & 0x003FFFFF);
}

static void *Read_RAM_8Mb_8000_807F(DWORD dwAddress)
{
	return g_pu8RamBase + (dwAddress & 0x007FFFFF);
}*/

static void *Read_RAM_4Mb_8000_803F(DWORD dwAddress)
{
	return g_pu8RamBase_8000 + dwAddress;
}

static void *Read_RAM_8Mb_8000_807F(DWORD dwAddress)
{
	return g_pu8RamBase_8000 + dwAddress;
}

static void *Read_RAM_4Mb_A000_A03F(DWORD dwAddress)
{
	return g_pu8RamBase_A000 + dwAddress;
}

static void *Read_RAM_8Mb_A000_A07F(DWORD dwAddress)
{
	return g_pu8RamBase_A000 + dwAddress;
}


static void *Read_83F0_83F0(DWORD dwAddress) {

	// 0x83F0 0000 to 0x83FF FFFF  RDRAM registers
	if ((dwAddress&0x1FFFFFFF) < 0x04000000)
	{
		DPF(DEBUG_MEMORY_RDRAM_REG, "Reading from MEM_RD_REG: 0x%08x", dwAddress);

		//DBGConsole_Msg(0, "Reading from MEM_RD_REG: 0x%08x", dwAddress);

		return (BYTE *)g_pMemoryBuffers[MEM_RD_REG] + (dwAddress & 0xFF);
	} else {
		return ReadInvalid(dwAddress);
	}
}
static void *Read_8400_8400(DWORD dwAddress)
{
	// 0x0400 0000 to 0x0400 FFFF  SP registers
	if ((dwAddress&0x1FFFFFFF) < 0x4002000)
	{
		DPF(DEBUG_MEMORY_SP_IMEM, "Reading from SP_MEM: 0x%08x", dwAddress);
		return (BYTE *)g_pMemoryBuffers[MEM_SP_MEM] + (dwAddress & 0x1FFF);
	}
	else
	{
		return ReadInvalid(dwAddress);
	}
}


static void *Read_8404_8404(DWORD dwAddress)
{
	if ((dwAddress&0x1FFFFFFF) < 0x04040020) {
		DPF(DEBUG_MEMORY_SP_REG, "Reading from SP_REG: 0x%08x", dwAddress);
		return (BYTE *)g_pMemoryBuffers[MEM_SP_REG] + (dwAddress & 0xFF);
	} else {
		return ReadInvalid(dwAddress);
	}

}

static void *Read_8408_8408(DWORD dwAddress)
{

	// 0x04080000 to 0x04080003  SP_PC_REG
	if ((dwAddress&0x1FFFFFFF) < 0x04080004)
	{
		DPF(DEBUG_MEMORY_SP_REG, "Reading from SP_PC_REG: 0x%08x", dwAddress);
		//DBGConsole_Msg(0, "Reading from SP_PC_REG: 0x%08x", dwAddress);
		return (BYTE *)g_pMemoryBuffers[MEM_SP_PC_REG] + (dwAddress &0x03);

	}
	// 0x04080004 to 0x04080007  SP_IBIST_REG
	else if ((dwAddress&0x1FFFFFFF) < 0x04080008)
	{
		DPF(DEBUG_MEMORY_SP_REG, "Reading from SP_IBIST_REG: 0x%08x", dwAddress);
		//DBGConsole_Msg(0, "Reading from SP_IBIST_REG: 0x%08x", dwAddress);
		return (BYTE *)g_pMemoryBuffers[MEM_SP_IBIST_REG] + (dwAddress - 0x04080004);
	}
	else
	{
		return ReadInvalid(dwAddress);
	}
}

static void *Read_8410_841F(DWORD dwAddress)
{
	// 0x0410 0000 to 0x041F FFFF DP Command Registers
	if ((dwAddress&0x1FFFFFFF) < 0x04100020)
	{
		DPF(DEBUG_MEMORY_DP, "Reading from DP_COMMAND_REG: 0x%08x", dwAddress);
		//DBGConsole_Msg(0, "Reading from DP_COMMAND_REG: 0x%08x", dwAddress);
		return (BYTE *)g_pMemoryBuffers[MEM_DP_COMMAND_REG] + (dwAddress & 0xFF);
	}
	else
	{
		DBGConsole_Msg(0, "Read from DP Command Registers is unhandled (0x%08x, PC: 0x%08x)",
			dwAddress, g_dwPC);
		
		return ReadInvalid(dwAddress);
	}
}

static void *Read_8420_842F(DWORD dwAddress)
{
	// 0x0420 0000 to 0x042F FFFF DP Span Registers
	DBGConsole_Msg(0, "Read from DP Span Registers is unhandled (0x%08x, PC: 0x%08x)",
		dwAddress, g_dwPC);
	return ReadInvalid(dwAddress);
}

// 0x0430 0000 to 0x043F FFFF MIPS Interface (MI) Registers
static void *Read_8430_843F(DWORD dwAddress)
{	
	if ((dwAddress&0x1FFFFFFF) <= MI_LAST_REG)
	{
		DPF(DEBUG_MEMORY_MI, "Reading from MI Registers: 0x%08x", dwAddress);
		//if ((dwAddress & 0xFF) == 0x08)
		//	DBGConsole_Msg(0, "Reading from MI REG: 0x%08x", dwAddress);
		return (BYTE *)g_pMemoryBuffers[MEM_MI_REG] + (dwAddress & 0xFF);
	}
	else
	{
		return ReadInvalid(dwAddress);
	}
}


// 0x0440 0000 to 0x044F FFFF Video Interface (VI) Registers
static void *Read_8440_844F(DWORD dwAddress)
{
	
	if ((dwAddress&0x1FFFFFFF) <= VI_LAST_REG)
	{
		DPF(DEBUG_MEMORY_VI, "Reading from MEM_VI_REG: 0x%08x", dwAddress);
		if ((dwAddress & 0xFF) == 0x10)
		{
			// We should return how far down the screen we are
			static LONG dwVIPos = 0;
			//u64 dwCountToVBL = (VID_CLOCK-1) - (g_qwNextVBL - g_qwCPR[0][C0_COUNT]);
			//dwVIPos = (u32)((dwCountToVBL*512)/VID_CLOCK);
			dwVIPos += 2;

			if (dwVIPos >= 512)
				dwVIPos = 0;

			//DBGConsole_Msg(0, "Reading vi pos: %d", dwVIPos);
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_VI_REG] + 0x10) = dwVIPos;
		}
		return (BYTE *)g_pMemoryBuffers[MEM_VI_REG] + (dwAddress & 0xFF);
	}
	else
	{
		return ReadInvalid(dwAddress);
	}
}

//static DWORD g_dwAIBufferFullness = 0;

// 0x0450 0000 to 0x045F FFFF Audio Interface (AI) Registers
static void *Read_8450_845F(DWORD dwAddress)
{
	DWORD dwOffset;
 	
	if ((dwAddress&0x1FFFFFFF) <= AI_LAST_REG)
	{
		dwOffset = dwAddress & 0xFF;

		switch (AI_BASE_REG + dwOffset)
		{
		case AI_DRAM_ADDR_REG:
			break;

		case AI_LEN_REG:
			{
				DWORD dwLen;
				/*KOS
				if (g_pAiPlugin != NULL)
					dwLen = g_pAiPlugin->ReadLength();
				else
					dwLen = 0;
                */
				Memory_AI_SetRegister(AI_LEN_REG, dwLen);
			}
			break;

		case AI_CONTROL_REG:
			break;

		case AI_STATUS_REG:
			Memory_AI_SetRegister(AI_STATUS_REG, 0);
			break;

		case AI_DACRATE_REG:
			break;
		case AI_BITRATE_REG:
			break;

		}
		DPF(DEBUG_MEMORY_AI, "Reading from AI Registers: 0x%08x", dwAddress);
		return (BYTE *)g_pMemoryBuffers[MEM_AI_REG] + dwOffset;
	}
	else
	{
		return ReadInvalid(dwAddress);
	}
}


// 0x0460 0000 to 0x046F FFFF Peripheral Interface (PI) Registers
static void *Read_8460_846F(DWORD dwAddress)
{
 	
	if ((dwAddress&0x1FFFFFFF) <= PI_LAST_REG)
	{
		DPF(DEBUG_MEMORY_PI, "Reading from MEM_PI_REG: 0x%08x", dwAddress);
		return (BYTE *)g_pMemoryBuffers[MEM_PI_REG] + (dwAddress & 0xFF);
	}
	else
	{
		return ReadInvalid(dwAddress);
	} 
}


// 0x0470 0000 to 0x047F FFFF RDRAM Interface (RI) Registers
static void *Read_8470_847F(DWORD dwAddress)
{
	if ((dwAddress&0x1FFFFFFF) <= RI_LAST_REG)
	{
		DPF(DEBUG_MEMORY_RI, "Reading from MEM_RI_REG: 0x%08x", dwAddress);
		return (BYTE *)g_pMemoryBuffers[MEM_RI_REG] + (dwAddress & 0xFF);
	}
	else
	{
		return ReadInvalid(dwAddress);
	} 
}

// 0x0480 0000 to 0x048F FFFF Serial Interface (SI) Registers
static void *Read_8480_848F(DWORD dwAddress)
{
	DWORD dwOffset;
 	
	if ((dwAddress&0x1FFFFFFF) <= SI_LAST_REG)
	{
		dwOffset = dwAddress & 0xFF;

		if (SI_BASE_REG + dwOffset == SI_STATUS_REG)
		{
			// Init SI_STATUS_INTERRUPT bit!
			BOOL bSIInterruptSet = Memory_MI_GetRegister(MI_INTR_REG) & MI_INTR_SI;

			if (bSIInterruptSet)
			{
				Memory_SI_SetRegisterBits(SI_STATUS_REG, SI_STATUS_INTERRUPT);
				DBGConsole_Msg(0, "Checking SI_STATUS in inconsistant state!");
			}
		}


		DPF(DEBUG_MEMORY_SI, "Reading from MEM_SI_REG: 0x%08x", dwAddress);
		return (BYTE *)g_pMemoryBuffers[MEM_SI_REG] + (dwAddress & 0xFF);
	}
	else
	{

		DBGConsole_Msg(0, "Read from SI Registers is unhandled (0x%08x, PC: 0x%08x)",
			dwAddress, g_dwPC);

		return ReadInvalid(dwAddress);
	} 
}

/*

#define	K0_TO_K1(x)	((u32)(x)|0xA0000000)	// kseg0 to kseg1 
#define	K1_TO_K0(x)	((u32)(x)&0x9FFFFFFF)	// kseg1 to kseg0 
#define	K0_TO_PHYS(x)	((u32)(x)&0x1FFFFFFF)	// kseg0 to physical 
#define	K1_TO_PHYS(x)	((u32)(x)&0x1FFFFFFF)	// kseg1 to physical 
#define	KDM_TO_PHYS(x)	((u32)(x)&0x1FFFFFFF)	// direct mapped to physical 
#define	PHYS_TO_K0(x)	((u32)(x)|0x80000000)	// physical to kseg0 
#define	PHYS_TO_K1(x)	((u32)(x)|0xA0000000)	// physical to kseg1 

#define PI_DOM2_ADDR1		0x05000000	// to 0x05FFFFFF 
#define PI_DOM1_ADDR1		0x06000000	// to 0x07FFFFFF 
#define PI_DOM2_ADDR2		0x08000000	// to 0x0FFFFFFF 
#define PI_DOM1_ADDR2		0x10000000	// to 0x1FBFFFFF 
#define PI_DOM1_ADDR3		0x1FD00000	// to 0x7FFFFFFF

0x0500_0000 .. 0x05ff_ffff	cartridge domain 2
0x0600_0000 .. 0x07ff_ffff	cartridge domain 1
0x0800_0000 .. 0x0fff_ffff	cartridge domain 2
0x1000_0000 .. 0x1fbf_ffff	cartridge domain 1

//0xa8010000 FlashROM
*/


static void * ReadROM(DWORD dwAddress)
{
	//0x10000000 | 0xA0000000 = 0xB0000000

	// Few things read from (0xbff00000)
	// Brunswick bowling

	// 0xb0ffb000

	DWORD dwPhys;
	DWORD dwOffset;

	dwPhys = K0_TO_PHYS(dwAddress);		// & 0x1FFFFFFF;

	if (dwPhys >= PI_DOM2_ADDR1 && dwPhys < PI_DOM1_ADDR1)
	{
		//DBGConsole_Msg(0, "[GRead from SRAM (addr1)] 0x%08x", dwAddress);
		if (!g_bSRAMUsed)
		{
			// Load SRAM
			Memory_LoadSRAM();
			g_bSRAMUsed = TRUE;
		}

		dwOffset = dwPhys - PI_DOM2_ADDR1;
		if (dwOffset < MemoryRegionSizes[MEM_SRAM])
			return (BYTE *)g_pMemoryBuffers[MEM_SRAM] + dwOffset;	
	}
	else if (dwPhys >= PI_DOM1_ADDR1 && dwPhys < PI_DOM2_ADDR2)
	{
		//DBGConsole_Msg(0, "[GRead from Cart (addr1)] 0x%08x", dwAddress);
		dwOffset = dwPhys - PI_DOM1_ADDR1;
		if (dwOffset < MemoryRegionSizes[MEM_CARTROM])
			return (BYTE *)g_pMemoryBuffers[MEM_CARTROM] + dwOffset;	
	}
	else if (dwPhys >= PI_DOM2_ADDR2 && dwPhys < PI_DOM1_ADDR2)
	{
		if (!g_bSRAMUsed)
		{
			// Load SRAM
			Memory_LoadSRAM();
			g_bSRAMUsed = TRUE;
		}

DBGConsole_Msg(0, "[GRead from FLASHRAM (addr2)] 0x%08x", dwAddress);
		dwOffset = dwPhys - PI_DOM2_ADDR2;
		if (dwOffset < MemoryRegionSizes[MEM_SRAM])
			return (BYTE *)g_pMemoryBuffers[MEM_SRAM] + dwOffset;	

	}
	else if (dwPhys >= PI_DOM1_ADDR2 && dwPhys < 0x1FBFFFFF)
	{
		//DBGConsole_Msg(0, "[GRead from Cart (addr2)] 0x%08x", dwAddress);
		dwOffset = dwPhys - PI_DOM1_ADDR2;
		if (dwOffset < MemoryRegionSizes[MEM_CARTROM])
			return (BYTE *)g_pMemoryBuffers[MEM_CARTROM] + dwOffset;	
		else
			DBGConsole_Msg(0, "[GRead from Cart (addr2) out of range! (0x%08x)] 0x%08x",
			dwAddress, MemoryRegionSizes[MEM_CARTROM]);
	}
	else if (dwPhys >= PI_DOM1_ADDR3 && dwPhys < 0x7FFFFFFF)
	{
		//DBGConsole_Msg(0, "[GRead from Cart (addr3)] 0x%08x", dwAddress);
		dwOffset = dwPhys - PI_DOM1_ADDR3;
		if (dwOffset < MemoryRegionSizes[MEM_CARTROM])
			return (BYTE *)g_pMemoryBuffers[MEM_CARTROM] + dwOffset;	
	}
	DBGConsole_Msg(0, "[WWarning, attempting to read from invalid Cart address (0x%08x)]", dwAddress);
	return Read_Noise(dwAddress);

}


// 0x1FC0 0000 to 0x1FC0 07BF PIF Boot ROM
// 0x1FC0 07C0 to 0x1FC0 07FF PIF RAM
static void * Read_9FC0_9FCF(DWORD dwAddress)
{
	DWORD dwOffset;
	//DWORD dwCIC = 0x91;
	
	if ((dwAddress&0x1FFFFFFF) <= PIF_ROM_END)
	{
		DPF(DEBUG_MEMORY_PIF, "Reading from MEM_PI_ROM: 0x%08x", dwAddress);

		dwOffset = dwAddress & 0x0FFF;

		return (BYTE *)g_pMemoryBuffers[MEM_PI_ROM] + dwOffset;
	}
 	
	else if ((dwAddress&0x1FFFFFFF) <= PIF_RAM_END)
	{
		DPF(DEBUG_MEMORY_PIF, "Reading from MEM_PI_RAM: 0x%08x", dwAddress);
		DBGConsole_Msg(0, "[WReading directly from PI ram]: 0x%08x!", dwAddress);

		dwOffset = (dwAddress&0x1FFFFFFF) - PIF_RAM_START;


		switch (dwOffset)
		{
		case 0x24:
			DBGConsole_Msg(0, "[YReading CIC Values]"); 
			//*(DWORD*)((BYTE*)g_pMemoryBuffers[MEM_PI_RAM] + dwOffset) = dwCIC << 8;
			break;
		case 0x3c:
			DBGConsole_Msg(0, "[YReading Control Byte] 0x%08x", 
				*(DWORD*)((BYTE *)g_pMemoryBuffers[MEM_PI_RAM] + dwOffset));
			break;
			
		}

		return (BYTE *)g_pMemoryBuffers[MEM_PI_RAM] + dwOffset;
	}
	else
	{
		return ReadInvalid(dwAddress);
	} 
}


