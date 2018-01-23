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


static void WriteValueInvalid(DWORD dwAddress, DWORD dwValue)
{
	DPF(DEBUG_MEMWARN, "Illegal Memory Access Tried to Write To 0x%08x PC: 0x%08x", dwAddress, g_dwPC);
	if (g_bWarnMemoryErrors)
	{
		CPU_Halt("Illegal Memory Access");
		DBGConsole_Msg(DEBUG_MEMWARN, "Illegal Memory Access: Tried to Write To 0x%08x (PC: 0x%08x)", dwAddress, g_dwPC);
	}
}

static void WriteValueNoise(DWORD dwAddress, DWORD dwValue)
{

	//CPUHalt();
	static BOOL bWarned = FALSE;
	if (!bWarned)
	{
		DBGConsole_Msg(0, "Writing noise (0x%08x) - sizing memory?", dwAddress);
		bWarned = TRUE;
	}

	//return g_pMemoryBuffers[MEM_UNUSED];
	// Do nothing

}


void TLB_Refill_Store(DWORD dwVAddr)
{	
	DWORD dwVPN2;

	DBGConsole_Msg(DEBUG_TLB, "TLB: Refill - 0x%08x at 0x%08x", dwVAddr, g_dwPC);
	DPF(DEBUG_TLB, "TLB: Refill - 0x%08x at 0x%08x", dwVAddr, g_dwPC);

	g_qwCPR[0][C0_BADVADDR] = dwVAddr;

	dwVPN2 = (dwVAddr>>TLBHI_VPN2SHIFT);	// Shift off odd/even indicator

	g_qwCPR[0][C0_CONTEXT] &= ~0x007FFFFF;	// Mask off bottom 23 bits
	g_qwCPR[0][C0_CONTEXT] |= (dwVPN2<<4);

	g_qwCPR[0][C0_ENTRYHI] = (s64)(s32)(dwVPN2 << TLBHI_VPN2SHIFT);

	g_nTLBExceptionReason = EXCEPTION_TLB_REFILL_STORE;
	AddCPUJob(CPU_TLB_EXCEPTION);
}

void TLB_Invalid_Store(DWORD dwVAddr)
{
	DWORD dwVPN2;

	DBGConsole_Msg(DEBUG_TLB, "TLB: Invalid - 0x%08x at 0x%08x", dwVAddr, g_dwPC);
	DPF(DEBUG_TLB, "TLB: Invalid - 0x%08x at 0x%08x", dwVAddr, g_dwPC);

	g_qwCPR[0][C0_BADVADDR] = dwVAddr;

	dwVPN2 = (dwVAddr>>TLBHI_VPN2SHIFT);	// Shift off odd/even indicator

	g_qwCPR[0][C0_CONTEXT] &= ~0x007FFFFF;	// Mask off bottom 23 bits
	g_qwCPR[0][C0_CONTEXT] |= (dwVPN2<<4);

	g_qwCPR[0][C0_ENTRYHI] = (s64)(s32)(dwVPN2 << TLBHI_VPN2SHIFT);
	

	// Jump to common exception vector, use TLBL or TLBS in ExcCode field
	g_nTLBExceptionReason = EXCEPTION_TLB_INVALID_STORE;
	AddCPUJob(CPU_TLB_EXCEPTION);
}


static void WriteValueMapped(DWORD dwAddress, DWORD dwValue)
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
		return;
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

			*(DWORD*)(g_pu8RamBase + (dwAddr & 0x007FFFFF)) = dwValue;
			return;
		}
		else
		{
			// Throw TLB Invalid exception
			TLB_Invalid_Store(dwAddress);
			return;
		}
	}
	else   // Even entry 
	{
		if (tlb.pfne & TLBLO_V)
		{
			// Valid
			DWORD dwAddr = tlb.pfnehi | (dwAddress & tlb.mask2);

			*(DWORD*)(g_pu8RamBase + (dwAddr & 0x007FFFFF)) = dwValue;
			return;
		}
		else
		{
			// Throw TLB Invalid exception
			TLB_Invalid_Store(dwAddress);
			return;
		}
	}

}



static void WriteValue_RAM_4Mb_8000_803F(DWORD dwAddress, DWORD dwValue)
{
	*(DWORD *)(g_pu8RamBase_8000 + dwAddress) = dwValue;
}

static void WriteValue_RAM_8Mb_8000_807F(DWORD dwAddress, DWORD dwValue)
{
	*(DWORD *)(g_pu8RamBase_8000 + dwAddress) = dwValue;
}

static void WriteValue_RAM_4Mb_A000_A03F(DWORD dwAddress, DWORD dwValue)
{
	*(DWORD *)(g_pu8RamBase_A000 + dwAddress) = dwValue;
}

static void WriteValue_RAM_8Mb_A000_A07F(DWORD dwAddress, DWORD dwValue)
{
	*(DWORD *)(g_pu8RamBase_A000 + dwAddress) = dwValue;
}




// 0x03F0 0000 to 0x03FF FFFF  RDRAM registers
static void WriteValue_83F0_83F0(DWORD dwAddress, DWORD dwValue)
{
	
	if ((dwAddress&0x1FFFFFFF) < 0x04000000)
	{
		DPF(DEBUG_MEMORY_RDRAM_REG, "Writing to MEM_RD_REG: 0x%08x", dwAddress);
	//DBGConsole_Msg(0, "Writing to MEM_RD_REG: 0x%08x, 0x%08x", dwAddress,dwValue);


		*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_RD_REG] + (dwAddress & 0xFF)) = dwValue;
	}
	else
	{
		WriteValueInvalid(dwAddress, dwValue);
	} 
}

// 0x0400 0000 to 0x0400 FFFF  SP registers
static void WriteValue_8400_8400(DWORD dwAddress, DWORD dwValue)
{

	if ((dwAddress&0x1FFFFFFF) <= SP_IMEM_END)
	{
		DPF(DEBUG_MEMORY_SP_IMEM, "Writing to SP_MEM: 0x%08x", dwAddress);

		//DBGConsole_Msg(0, "Writing to SP IMEM/DMEM: 0x%08x, 0x%08x", dwAddress,dwValue);

		*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_SP_MEM] + (dwAddress & 0x1FFF)) = dwValue;
	}
	else
	{	
		WriteValueInvalid(dwAddress, dwValue);
	}
}

static void WriteValue_8404_8404(DWORD dwAddress, DWORD dwValue)
{
	DWORD dwOffset;

	if ((dwAddress&0x1FFFFFFF) <= SP_LAST_REG)
	{
		DPF(DEBUG_MEMORY_SP_REG, "Writing to SP_REG: 0x%08x/0x%08x", dwAddress, dwValue);

		dwOffset = dwAddress & 0xFF;

		switch (SP_BASE_REG + dwOffset)
		{
		case SP_MEM_ADDR_REG:
		case SP_DRAM_ADDR_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_SP_REG] + dwOffset) = dwValue;
			break;

		case SP_RD_LEN_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_SP_REG] + dwOffset) = dwValue;
			MemoryDoSP_RDRAM_IDMEM_Copy();
			break;

		case SP_WR_LEN_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_SP_REG] + dwOffset) = dwValue;
			MemoryDoSP_IDMEM_RDRAM_Copy();
			break;

		case SP_STATUS_REG:
			MemoryUpdateSP(dwAddress, dwValue);
			break;


		case SP_DMA_FULL_REG:
		case SP_DMA_BUSY_REG:
			// Prevent writing to read-only mem
			break;

		case SP_SEMAPHORE_REG:
			//TODO - Make this do something?
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_SP_REG] + dwOffset) = dwValue;
			break;
		}
		
	}
	else
	{
		WriteValueInvalid(dwAddress, dwValue);
	}
}

// 0x04080000 to 0x04080003  SP_PC_REG
// 0x04080004 to 0x04080007  SP_IBIST_REG
static void WriteValue_8408_8408(DWORD dwAddress, DWORD dwValue)
{
	if ((dwAddress&0x1FFFFFFF) <= SP_PC_REG)
	{
		DPF(DEBUG_MEMORY_SP_REG, "Writing to SP_PC_REG: 0x%08x", dwValue);

		*(DWORD *)g_pMemoryBuffers[MEM_SP_PC_REG] = dwValue;
		
		//TODO: Does this affect anything?

	}
	else if ((dwAddress&0x1FFFFFFF) <= SP_IBIST_REG)
	{
		DPF(DEBUG_MEMORY_SP_REG, "Writing to SP_IBIST_REG: 0x%08x", dwValue);
		DBGConsole_Msg(0, "Writing to SP_IBIST_REG: 0x%08x", dwValue);

		*(DWORD *)g_pMemoryBuffers[MEM_SP_IBIST_REG] = dwValue;
	}
	else
	{
		WriteValueInvalid(dwAddress, dwValue);
	}
}

// 0x0410 0000 to 0x041F FFFF DP Command Registers
static void WriteValue_8410_841F(DWORD dwAddress, DWORD dwValue)
{
	DWORD dwOffset;
	
	if ((dwAddress&0x1FFFFFFF) <= DPC_LAST_REG)
	{
		DPF(DEBUG_MEMORY_DP, "Writing to DP_COMMAND_REG: 0x%08x", dwAddress);

		dwOffset = dwAddress & 0xFF;

		switch (DPC_BASE_REG + dwOffset)
		{
		case DPC_START_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_DP_COMMAND_REG] + dwOffset) = dwValue;
			break;


		case DPC_END_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_DP_COMMAND_REG] + dwOffset) = dwValue;
			MemoryDoDP();
			break;

		case DPC_CURRENT_REG:// - Read Only
			break;

		case DPC_STATUS_REG:
			// Set flags etc
			MemoryUpdateDP(dwAddress, dwValue);
			break;

		case DPC_CLOCK_REG: //- Read Only
		case DPC_BUFBUSY_REG: //- Read Only
		case DPC_PIPEBUSY_REG: //- Read Only
		case DPC_TMEM_REG: //- Read Only
			break;

		}
	}
	else
	{
		WriteValueInvalid(dwAddress, dwValue);
	}
}

// 0x0420 0000 to 0x042F FFFF DP Span Registers
static void WriteValue_8420_842F(DWORD dwAddress, DWORD dwValue)
{
	
	DBGConsole_Msg(0, "Write to DP Span Registers is unhandled (0x%08x, PC: 0x%08x)",
		dwAddress, g_dwPC);

	WriteValueInvalid(dwAddress, dwValue);
}


// 0x0430 0000 to 0x043F FFFF MIPS Interface (MI) Registers
static void WriteValue_8430_843F(DWORD dwAddress, DWORD dwValue)
{
	DWORD dwOffset;
	
	if ((dwAddress&0x1FFFFFFF) <= MI_LAST_REG)
	{
		DPF(DEBUG_MEMORY_MI, "Writing to MI Registers: 0x%08x", dwAddress);

		dwOffset = dwAddress & 0xFF;

		switch (MI_BASE_REG + dwOffset)
		{
		case MI_INIT_MODE_REG:
			MemoryUpdateMI(dwAddress, dwValue);
			break;

		case MI_VERSION_REG:
			// Read Only
			break;


		case MI_INTR_REG:
			// Read Only
			break;

		case MI_INTR_MASK_REG:
			MemoryUpdateMI(dwAddress, dwValue);
			break;
		}
	}
	else
	{
		WriteValueInvalid(dwAddress, dwValue);
	}
}



// 0x0440 0000 to 0x044F FFFF Video Interface (VI) Registers
static void WriteValue_8440_844F(DWORD dwAddress, DWORD dwValue)
{
	if ((dwAddress&0x1FFFFFFF) <= VI_LAST_REG)
	{
		MemoryUpdateVI(dwAddress, dwValue);
	}
	else
	{
		WriteValueInvalid(dwAddress, dwValue);
	}
}


// 0x0450 0000 to 0x045F FFFF Audio Interface (AI) Registers
static void WriteValue_8450_845F(DWORD dwAddress, DWORD dwValue)
{
	DWORD dwOffset;
 	
	if ((dwAddress&0x1FFFFFFF) <= AI_LAST_REG)
	{
		DPF(DEBUG_MEMORY_AI, "Writing to AI Registers: 0x%08x", dwAddress);

		dwOffset = dwAddress & 0xFF;

		switch (AI_BASE_REG + dwOffset)
		{
		case AI_DRAM_ADDR_REG:
			// 64bit aligned
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_AI_REG] + dwOffset) = dwValue;
			break;

		case AI_LEN_REG:
			// LS 3 bits ignored
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_AI_REG] + dwOffset) = dwValue;
            /*KOS
			if (g_pAiPlugin != NULL)
				g_pAiPlugin->LenChanged();
            */
			//MemoryDoAI();
			break;

		case AI_CONTROL_REG:
			// DMA enable/Disable
			//if (g_dwAIBufferFullness == 2)
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_AI_REG] + dwOffset) = dwValue;
			//MemoryDoAI();
			break;

		case AI_STATUS_REG:
			Memory_MI_ClrRegisterBits(MI_INTR_REG, MI_INTR_AI);
			break;

		case AI_DACRATE_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_AI_REG] + dwOffset) = dwValue;

			// TODO: Fix type
			/*KOS
			if (g_pAiPlugin != NULL)
				g_pAiPlugin->DacrateChanged(SYSTEM_NTSC);
            */
			break;

		case AI_BITRATE_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_AI_REG] + dwOffset) = dwValue;
			break;
		}
	}
	else
	{
		WriteValueInvalid(dwAddress, dwValue);
	}
}


// 0x0460 0000 to 0x046F FFFF Peripheral Interface (PI) Registers
static void WriteValue_8460_846F(DWORD dwAddress, DWORD dwValue)
{
	DWORD dwOffset;

	if ((dwAddress&0x1FFFFFFF) <= PI_LAST_REG)
	{
		dwOffset = dwAddress & 0xFF;

		switch (PI_BASE_REG + dwOffset)
		{

		case PI_DRAM_ADDR_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_PI_REG] + dwOffset) = dwValue;
			break;

		case PI_CART_ADDR_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_PI_REG] + dwOffset) = dwValue;
			break;

		case PI_RD_LEN_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_PI_REG] + dwOffset) = dwValue;

			/*DBGConsole_Msg(0, "[YWriting %d to PI_RD_LEN_REG (0x%08x -> 0x%08x)!!!]",
				dwValue, Memory_PI_GetRegister(PI_DRAM_ADDR_REG),
				Memory_PI_GetRegister(PI_CART_ADDR_REG));*/

			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_PI_REG] + dwOffset) = dwValue;
			Memory_PI_CopyFromRDRAM();

			break;

		case PI_WR_LEN_REG:
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_PI_REG] + dwOffset) = dwValue;
			// Do memory transfer
			Memory_PI_CopyToRDRAM();
			break;

		default:
			// Do status reg stuff.
			MemoryUpdatePI(dwOffset, dwValue);
			break;

		}
	}
	else
	{
		WriteValueInvalid(dwAddress, dwValue);
	}

}



// 0x0470 0000 to 0x047F FFFF RDRAM Interface (RI) Registers
static void WriteValue_8470_847F(DWORD dwAddress, DWORD dwValue)
{
	DWORD dwOffset;

	if ((dwAddress&0x1FFFFFFF) <= RI_LAST_REG)
	{
		DPF(DEBUG_MEMORY_RI, "Writing to MEM_RI_REG: 0x%08x", dwAddress);

		dwOffset = dwAddress & 0xFF;

		*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_RI_REG] + dwOffset) = dwValue;
	}
	else 
	{		
		WriteValueInvalid(dwAddress, dwValue);
	}

}


// 0x0480 0000 to 0x048F FFFF Serial Interface (SI) Registers
static void WriteValue_8480_848F(DWORD dwAddress, DWORD dwValue)
{
	DWORD dwOffset;

	if ((dwAddress&0x1FFFFFFF) <= SI_LAST_REG)
	{
		DPF(DEBUG_MEMORY_SI, "Writing to MEM_SI_REG: 0x%08x", dwAddress);

		dwOffset = dwAddress & 0xFF;

		switch (SI_BASE_REG + dwOffset)
		{
		case SI_DRAM_ADDR_REG:

			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_SI_REG] + dwOffset) = dwValue;
			break;
			

		case SI_PIF_ADDR_RD64B_REG:

			// Trigger DRAM -> PIF DMA
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_SI_REG] + dwOffset) = dwValue;
			Memory_SI_CopyToDRAM();
			break;

		// Reserved Registers here!


		case SI_PIF_ADDR_WR64B_REG:

			// Trigger DRAM -> PIF DMA
			*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_SI_REG] + dwOffset) = dwValue;
			Memory_SI_CopyFromDRAM();
			break;

		case SI_STATUS_REG:	
			// Any write to this reg clears interrupts
			Memory_MI_ClrRegisterBits(MI_INTR_REG, MI_INTR_SI);
			break;
		}		
	}
	else
	{
		WriteValueInvalid(dwAddress, dwValue);
	}
}

// 0x1FC0 0000 to 0x1FC0 07BF PIF Boot ROM
// 0x1FC0 07C0 to 0x1FC0 07FF PIF RAM
static void WriteValue_9FC0_9FCF(DWORD dwAddress, DWORD dwValue)
{
	DWORD dwOffset;
	
	if ((dwAddress&0x1FFFFFFF) <= PIF_ROM_END)
	{
		DPF(DEBUG_MEMORY_PIF, "Writing to MEM_PI_ROM: 0x%08x", dwAddress);

		dwOffset = dwAddress & 0x0FFF;

		DBGConsole_Msg(0, "[WWarning]: trying to write to PIF ROM");

		*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_PI_ROM] + dwOffset) = dwValue;

	}
	else if ((dwAddress&0x1FFFFFFF) <= PIF_RAM_END)
	{
		DBGConsole_Msg(0, "[WWriting directly to PI ram]: 0x%08x:0x%08x!", dwAddress, dwValue);
		DPF(DEBUG_MEMORY_PIF, "Writing to MEM_PI_RAM: 0x%08x", dwAddress);

		dwOffset = (dwAddress&0x1FFFFFFF) - PIF_RAM_START;

		switch (dwOffset)
		{
		case 0x3c:
			DBGConsole_Msg(0, "[YWriting to Control Byte]");
			// Seems to want high bt set after 0x20 is written
			if (dwValue & 0x20)
				dwValue |= 0x80; break;
			
			dwValue = 0; break;
		}

		*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_PI_RAM] + dwOffset) = dwValue;
	}
	else
	{
		WriteValueInvalid(dwAddress, dwValue);
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
*/
static void WriteValue_Cartridge(DWORD dwAddress, DWORD dwValue)
{
	//0x10000000 | 0xA0000000 = 0xB0000000

	DWORD dwPhys;
	DWORD dwOffset;

	dwPhys = K0_TO_PHYS(dwAddress);		// & 0x1FFFFFFF;

	if (dwPhys >= PI_DOM2_ADDR1 && dwPhys < PI_DOM1_ADDR1)
	{
		if (!g_bSRAMUsed)
		{
			// Load SRAM
			Memory_LoadSRAM();
			g_bSRAMUsed = TRUE;
		}

//		DBGConsole_Msg(0, "[GWrite to  SRAM (addr1)] 0x%08x", dwAddress);
		dwOffset = dwPhys - PI_DOM2_ADDR1;
		if (dwOffset < MemoryRegionSizes[MEM_SRAM])
		{
			*(DWORD*)((BYTE *)g_pMemoryBuffers[MEM_SRAM] + dwOffset) = dwValue;	
			return;
		}
	}
	else if (dwPhys >= PI_DOM1_ADDR1 && dwPhys < PI_DOM2_ADDR2)
	{
//		DBGConsole_Msg(0, "[GWrite to  Cart (addr1)] 0x%08x", dwAddress);
		dwOffset = dwPhys - PI_DOM1_ADDR1;
		//if (dwOffset < MemoryRegionSizes[MEM_CARTROM])
			//return (BYTE *)g_pMemoryBuffers[MEM_CARTROM] + dwOffset;	
	}
	else if (dwPhys >= PI_DOM2_ADDR2 && dwPhys < PI_DOM1_ADDR2)
	{
		if (!g_bSRAMUsed)
		{
			// Load SRAM
			Memory_LoadSRAM();
			g_bSRAMUsed = TRUE;
		}

/*
Write to  FLASHRAM (addr2) 0xa8010000 0xd2000000
Read from FLASHRAM (addr2) 0xa8000000
Write to  FLASHRAM (addr2) 0xa8010000 0xd2000000
Read from FLASHRAM (addr2) 0xa8000000
Write to  FLASHRAM (addr2) 0xa8010000 0xe1000000
Write to  FLASHRAM (addr2) 0xa8010000 0xd2000000
Read from FLASHRAM (addr2) 0xa8000000
Write to  FLASHRAM (addr2) 0xa8010000 0xd2000000
Read from FLASHRAM (addr2) 0xa8000000
Write to  FLASHRAM (addr2) 0xa8010000 0xe1000000*/

DBGConsole_Msg(0, "[GWrite to  FLASHRAM (addr2)] 0x%08x 0x%08x", dwAddress, dwValue);
		dwOffset = dwPhys - PI_DOM2_ADDR2;
		if (dwOffset < MemoryRegionSizes[MEM_SRAM])
		{
			*(DWORD*)((BYTE *)g_pMemoryBuffers[MEM_SRAM] + dwOffset) = dwValue;	
			return;
		}
	}
	else if (dwPhys >= PI_DOM1_ADDR2 && dwPhys < 0x1FBFFFFF)
	{
//		DBGConsole_Msg(0, "[GWrite to  Cart (addr2)] 0x%08x", dwAddress);
		dwOffset = dwPhys - PI_DOM1_ADDR2;
		//if (dwOffset < MemoryRegionSizes[MEM_CARTROM])
		//	return (BYTE *)g_pMemoryBuffers[MEM_CARTROM] + dwOffset;	
	}

	else if (dwPhys >= PI_DOM1_ADDR3 && dwPhys < 0x7FFFFFFF)
	{
//		DBGConsole_Msg(0, "[GWrite to  Cart (addr3)] 0x%08x", dwAddress);
		dwOffset = dwPhys - PI_DOM1_ADDR3;
		//if (dwOffset < MemoryRegionSizes[MEM_CARTROM])
		//	return (BYTE *)g_pMemoryBuffers[MEM_CARTROM] + dwOffset;	
	}


	DBGConsole_Msg(0, "[WWarning, attempting to write to invalid Cart address (0x%08x)]", dwAddress);
	WriteValueInvalid(dwAddress, dwValue);

}


