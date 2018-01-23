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

// Various stuff to map an address onto the correct memory region

#include "main.h"

#include "ultra_rcp.h"

#include "cpu.h"
#include "rsp.h"
#include "RDP.h"
#include "memory.h"
#include "debug.h"
#include "rom.h"

#include "Interrupt.h"		// g_nTLBExceptionReason

#include "Controller.h"

#include "ultra_r4300.h"

#include "DBGConsole.h"
#include "patch.h"

DWORD g_dwTLBRHit = 0;
DWORD g_dwTLBWHit = 0;

// Arbritary counter for least recently used algorithm
static LONG s_nTLBTimeNow = 0;
static LONG s_nNumDmaTransfers = 0;			// Incremented on every Cart->RDRAM Xfer
static LONG s_nTotalDmaTransferSize = 0;	// Total size of every Cart->RDRAM Xfer
static LONG s_nNumSPTransfers = 0;			// Incremented on every RDRAM->SPMem Xfer
static LONG s_nTotalSPTransferSize = 0;		// Total size of every RDRAM->SPMem Xfer

// US version
//mapmem=7f000000,10034b30,1000000
// PAL version (CrJack)
//mapmem=7f000000,100329f0,1000000

// dwCartAddr = (dwAddress - 0x7f000000) + GOLDENEYE_HACKED_OFFSET;
#define GOLDENEYE_HACKED_OFFSET 0x10034b30;

static void Memory_PI_CopyToRDRAM();
static void Memory_PI_CopyFromRDRAM();
static void MemoryDoSP_RDRAM_IDMEM_Copy();
static void MemoryDoSP_IDMEM_RDRAM_Copy();
static void MemoryDoDP();
static void MemoryDoCheckForDPTask();
static void MemoryDoAI();

static void MemoryUpdateSP(DWORD dwAddress, DWORD dwValue);
static void MemoryUpdateDP(DWORD dwAddress, DWORD dwValue);
static void MemoryUpdateMI(DWORD dwAddress, DWORD dwValue);
static void MemoryUpdateVI(DWORD dwAddress, DWORD dwValue);
#ifdef DAEDALUS_LOG
static void DisplayVIControlInfo(DWORD dwControlReg);
#endif
static void MemoryUpdatePI(DWORD dwAddress, DWORD dwValue);

static void Memory_SI_CopyFromDRAM();
static void Memory_SI_CopyToDRAM();

static void Memory_InitTables();

static void Memory_LoadSRAM();
static void Memory_SaveSRAM();

static BOOL g_bSRAMUsed = FALSE;


DWORD MemoryRegionSizes[NUM_MEM_BUFFERS] =
{
	8*1024,				// Allocate 8k bytes - a bit excessive but some of the internal functions assume it's there!
	MEMORY_4_MEG,		// RD_RAM
	0x2000,				// SP_MEM

	0x04,				// SP_PC_REG
	0x04,				// SP_IBITS_REG

	0x7C0,				// PI_ROM
	0x40,				// PI_RAM

	1*1024*1024,		// RD_REG Don't need this much really?
	0x20,				// SP_REG
	0x20,				// DP_COMMAND_REG
	0x10,				// MI_REG
	0x38,				// VI_REG
	0x18,				// AI_REG
	0x34,				// PI_REG
	0x20,				// RI_REG
	0x1C,				// SI_REG

	0x40000,			// SRAM
	
	0					// CARTROM This isn't known until we read the cart

};

DWORD g_dwRamSize;

// Ram base, offset by 0x80000000 and 0xa0000000
static u8 * g_pu8RamBase_8000 = NULL;
static u8 * g_pu8RamBase_A000 = NULL;


#include "Memory_Read.inl"
#include "Memory_WriteValue.inl"
#include "Memory_Write.inl"
#include "Memory_ReadInternal.inl"
#include "Memory_WriteInternal.inl"


struct MemMapEntry {
	DWORD dwStart, dwEnd;
	MemFastFunction ReadFastFunction;
	MemFastFunction WriteFastFunction;
	MemWriteValueFunction WriteValueFunction;
};

struct InternalMemMapEntry {
	DWORD dwStart, dwEnd;
	InternalMemFastFunction InternalReadFastFunction;		// Same as read instruction, but 
	InternalMemFastFunction InternalWriteFastFunction;		// Same as read instruction, but 
};

// Physical ram: 0x80000000 upwards is set up when tables are initialised
InternalMemMapEntry InternalMemMapEntries[] = {
	{ 0x0000, 0x7FFF, InternalReadMapped,		InternalWriteMapped },			// Mapped Memory
	{ 0x8000, 0x807F, InternalReadInvalid,		InternalWriteInvalid },	// RDRAM - Initialised later
	{ 0x8080, 0x83FF, InternalReadInvalid,		InternalWriteInvalid },			// Cartridge Domain 2 Address 1
	{ 0x8400, 0x8400, InternalRead_8400_8400,	InternalWriteInvalid },			// Cartridge Domain 2 Address 1
	{ 0x8404, 0x85FF, InternalReadInvalid,		InternalWriteInvalid },			// Cartridge Domain 2 Address 1
	{ 0x8600, 0x87FF, InternalReadROM,			InternalWriteInvalid },			// Cartridge Domain 1 Address 1
	{ 0x8800, 0x8FFF, InternalReadROM,			InternalWriteInvalid },			// Cartridge Domain 2 Address 2
	{ 0x9000, 0x9FBF, InternalReadROM,			InternalWriteInvalid },			// Cartridge Domain 1 Address 2
	{ 0x9FC0, 0x9FCF, InternalRead_9FC0_9FCF,	InternalWriteInvalid },			// pif RAM/ROM
	{ 0x9FD0, 0x9FFF, InternalReadROM,			InternalWriteInvalid },			// Cartridge Domain 1 Address 3

	{ 0xA000, 0xA07F, InternalReadInvalid,		InternalWriteInvalid },		// Physical RAM - Copy of above
	{ 0xA080, 0xA3FF, InternalReadInvalid,		InternalWriteInvalid },			// Unused
	{ 0xA400, 0xA400, InternalRead_8400_8400,	InternalWriteInvalid },			// Unused
	{ 0xA404, 0xA4FF, InternalReadInvalid,		InternalWriteInvalid },			// Unused
	{ 0xA500, 0xA5FF, InternalReadROM,			InternalWriteInvalid },			// Cartridge Domain 2 Address 1
	{ 0xA600, 0xA7FF, InternalReadROM,			InternalWriteInvalid },			// Cartridge Domain 1 Address 1
	{ 0xA800, 0xAFFF, InternalReadROM,			InternalWriteInvalid },			// Cartridge Domain 2 Address 2
	{ 0xB000, 0xBFBF, InternalReadROM,			InternalWriteInvalid },			// Cartridge Domain 1 Address 2
	{ 0xBFC0, 0xBFCF, InternalRead_9FC0_9FCF,	InternalWriteInvalid },			// pif RAM/ROM
	{ 0xBFD0, 0xBFFF, InternalReadROM,			InternalWriteInvalid },			// Cartridge Domain 1 Address 3

	{ 0xC000, 0xDFFF, InternalReadMapped,		InternalWriteMapped },			// Mapped Memory
	{ 0xE000, 0xFFFF, InternalReadMapped,		InternalWriteMapped },			// Mapped Memory

	{ ~0,  ~0, NULL, NULL }
};



MemMapEntry MemMapEntries[] =
{
	{ 0x0000, 0x7FFF, ReadMapped,		WriteMapped,	WriteValueMapped },			// Mapped Memory
	{ 0x8000, 0x807F, Read_Noise,		Write_Noise,	WriteValueNoise },			// RDRAM - filled in with correct function later
	{ 0x8080, 0x83EF, Read_Noise,		Write_Noise,	WriteValueNoise },			// Unused - electrical noise
	{ 0x83F0, 0x83F0, Read_83F0_83F0,	Write_83F0_83F0,WriteValue_83F0_83F0 },		// RDRAM reg
	{ 0x83F4, 0x83FF, ReadInvalid,		WriteInvalid,	WriteValueInvalid },			// Unused
	{ 0x8400, 0x8400, Read_8400_8400,	WriteInvalid,	WriteValue_8400_8400 },		// DMEM/IMEM
	{ 0x8404, 0x8404, Read_8404_8404,	Write_8400_8400,	WriteValue_8404_8404 },		// SP Reg
	{ 0x8408, 0x8408, Read_8408_8408,	WriteInvalid,	WriteValue_8408_8408 },		// SP PC/IBIST
	{ 0x840C, 0x840F, ReadInvalid,		WriteInvalid,	WriteValueInvalid },			// Unused
	{ 0x8410, 0x841F, Read_8410_841F,	WriteInvalid,	WriteValue_8410_841F },		// DPC Reg
	{ 0x8420, 0x842F, Read_8420_842F,	WriteInvalid,	WriteValue_8420_842F },		// DPS Reg
	{ 0x8430, 0x843F, Read_8430_843F,	WriteInvalid,	WriteValue_8430_843F},		// MI Reg
	{ 0x8440, 0x844F, Read_8440_844F,	WriteInvalid,	WriteValue_8440_844F },		// VI Reg
	{ 0x8450, 0x845F, Read_8450_845F,	WriteInvalid,	WriteValue_8450_845F },		// AI Reg
	{ 0x8460, 0x846F, Read_8460_846F,	WriteInvalid,	WriteValue_8460_846F },		// PI Reg
	{ 0x8470, 0x847F, Read_8470_847F,	WriteInvalid,	WriteValue_8470_847F },		// RI Reg
	{ 0x8480, 0x848F, Read_8480_848F,	WriteInvalid,	WriteValue_8480_848F },		// SI Reg
	{ 0x8490, 0x84FF, ReadInvalid,		WriteInvalid,	WriteValueInvalid },			// Unused
	{ 0x8500, 0x85FF, ReadInvalid/*ReadROM*/,	WriteInvalid, WriteValue_Cartridge },	// Cartridge Domain 2 Address 1
	{ 0x8600, 0x87FF, ReadROM,			WriteInvalid,	WriteValue_Cartridge },			// Cartridge Domain 1 Address 1
	{ 0x8800, 0x8FFF, ReadROM,			WriteInvalid,	WriteValue_Cartridge },			// Cartridge Domain 2 Address 2
	{ 0x9000, 0x9FBF, ReadROM,			WriteInvalid,	WriteValue_Cartridge },			// Cartridge Domain 1 Address 2
	{ 0x9FC0, 0x9FCF, Read_9FC0_9FCF,	WriteInvalid,	WriteValue_9FC0_9FCF },		// pif RAM/ROM
	{ 0x9FD0, 0x9FFF, ReadROM,			WriteInvalid,	WriteValue_Cartridge },			// Cartridge Domain 1 Address 3

	{ 0xA000, 0xA07F, Read_Noise,		Write_Noise,	WriteValueNoise },			// Physical RAM - Copy of above
	{ 0xA080, 0xA3EF, Read_Noise,		Write_Noise,	WriteValueNoise },			// Unused
	{ 0xA3F0, 0xA3FF, Read_83F0_83F0,	Write_83F0_83F0,WriteValue_83F0_83F0 },		// RDRAM Reg
	//{ 0xA3F4, 0xA3FF, ReadInvalid,		WriteInvalid },		// Unused
	{ 0xA400, 0xA400, Read_8400_8400,	Write_8400_8400,	WriteValue_8400_8400 },		// DMEM/IMEM
	{ 0xA404, 0xA404, Read_8404_8404,	WriteInvalid,	WriteValue_8404_8404 },		// SP Reg
	{ 0xA408, 0xA408, Read_8408_8408,	WriteInvalid,	WriteValue_8408_8408 },		// SP PC/OBOST
	{ 0xA40C, 0xA40F, ReadInvalid,		WriteInvalid,	WriteValueInvalid },			// Unused
	{ 0xA410, 0xA41F, Read_8410_841F,	WriteInvalid,	WriteValue_8410_841F },		// DPC Reg
	{ 0xA420, 0xA42F, Read_8420_842F,	WriteInvalid,	WriteValue_8420_842F },		// DPS Reg

	{ 0xA430, 0xA43F, Read_8430_843F,	WriteInvalid,	WriteValue_8430_843F },		// MI Reg
	{ 0xA440, 0xA44F, Read_8440_844F,	WriteInvalid,	WriteValue_8440_844F },		// VI Reg
	{ 0xA450, 0xA45F, Read_8450_845F,	WriteInvalid,	WriteValue_8450_845F },		// AI Reg
	{ 0xA460, 0xA46F, Read_8460_846F,	WriteInvalid,	WriteValue_8460_846F },		// PI Reg
	{ 0xA470, 0xA47F, Read_8470_847F,	WriteInvalid,	WriteValue_8470_847F },		// RI Reg
	{ 0xA480, 0xA48F, Read_8480_848F,	WriteInvalid,	WriteValue_8480_848F },		// SI Reg
	{ 0xA490, 0xA4FF, ReadInvalid,		WriteInvalid,	WriteValueInvalid },		// Unused
	{ 0xA500, 0xA5FF, ReadROM,			WriteInvalid,	WriteValue_Cartridge },		// Cartridge Domain 2 Address 1
	{ 0xA600, 0xA7FF, ReadROM,			WriteInvalid,	WriteValue_Cartridge },		// Cartridge Domain 1 Address 1
	{ 0xA800, 0xAFFF, ReadROM,			WriteInvalid,	WriteValue_Cartridge },		// Cartridge Domain 2 Address 2
	{ 0xB000, 0xBFBF, ReadROM,			WriteInvalid,	WriteValue_Cartridge },		// Cartridge Domain 1 Address 2
	{ 0xBFC0, 0xBFCF, Read_9FC0_9FCF,	WriteInvalid,	WriteValue_9FC0_9FCF },		// pif RAM/ROM
	{ 0xBFD0, 0xBFFF, ReadROM,			WriteInvalid,	WriteValue_Cartridge },		// Cartridge Domain 1 Address 3

	{ 0xC000, 0xDFFF, ReadMapped,		WriteMapped,	WriteValueMapped },			// Mapped Memory
	{ 0xE000, 0xFFFF, ReadMapped,		WriteMapped,	WriteValueMapped },			// Mapped Memory

	{ ~0,  ~0, NULL, NULL }
};


MemFastFunction *g_ReadAddressLookupTable = NULL;
MemFastFunction *g_WriteAddressLookupTable = NULL;
MemWriteValueFunction * g_WriteAddressValueLookupTable = NULL;

InternalMemFastFunction * InternalReadFastTable = NULL;
InternalMemFastFunction * InternalWriteFastTable = NULL;


void * g_pMemoryBuffers[NUM_MEM_BUFFERS] =
{
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL
};



BOOL Memory_Init()
{
	g_dwRamSize = 4*1024*1024;
	
	for (DWORD m = 0; m < NUM_MEM_BUFFERS; m++)
	{
		// Skip zero sized areas. An example of this is the cart rom
		if (MemoryRegionSizes[m] > 0)
		{
			g_pMemoryBuffers[m] = new BYTE[MemoryRegionSizes[m]];
			if (g_pMemoryBuffers[m] == NULL)
				return FALSE;

			ZeroMemory(g_pMemoryBuffers[m], MemoryRegionSizes[m]);
		}
	}


	g_ReadAddressLookupTable = new MemFastFunction[0x4000];
	g_WriteAddressLookupTable = new MemFastFunction[0x4000];
	g_WriteAddressValueLookupTable = new MemWriteValueFunction[0x4000];

	InternalReadFastTable = new InternalMemFastFunction[0x4000];
	InternalWriteFastTable = new InternalMemFastFunction[0x4000];


	g_pu8RamBase_8000 = ((u8*)g_pMemoryBuffers[MEM_RD_RAM]) - 0x80000000;
	g_pu8RamBase_A000 = ((u8*)g_pMemoryBuffers[MEM_RD_RAM]) - 0xa0000000;


	Memory_InitTables();

	return TRUE;
}


void Memory_Fini(void)
{
	DPF(DEBUG_INFO, "Freeing Memory");

	delete [] g_ReadAddressLookupTable;
	delete [] g_WriteAddressLookupTable;
	delete [] g_WriteAddressValueLookupTable;

	for (DWORD m = 0; m < NUM_MEM_BUFFERS; m++)
	{
		if (g_pMemoryBuffers[m] != NULL)
		{
			delete []g_pMemoryBuffers[m];
			g_pMemoryBuffers[m] = NULL;
		}
	}
}


inline DWORD SwapEndian(DWORD x)
{
	return ((x >> 24)&0x000000FF) |
		   ((x >> 8 )&0x0000FF00) |
		   ((x << 8 )&0x00FF0000) |
		   ((x << 24)&0xFF000000);
}


void Memory_Reset(DWORD dwMainMem)
{
	DWORD i;
	
	DBGConsole_Msg(DEBUG_INFO, "Reseting Memory - %d MB", dwMainMem/(1024*1024));

	s_nNumDmaTransfers = 0;
	s_nTotalDmaTransferSize = 0;
	s_nNumSPTransfers = 0;
	s_nTotalSPTransferSize = 0;

	// Set memory size to specified value
	// Note that we do not reallocate the memory - we always have 8Mb!
	g_dwRamSize = dwMainMem;

	// Reinit the tables - this will update the RAM pointers, 
	// and also the Goldeneye mapped memory (if it's mapped)
	Memory_InitTables();

	// Clear memory
	for (i = 0; i < NUM_MEM_BUFFERS; i++)
	{
		// Don't clear cart memory!
		if (i == MEM_CARTROM)
			continue;
		if (g_pMemoryBuffers[i] != NULL) 
		{
			memset(g_pMemoryBuffers[i], 0, MemoryRegionSizes[i]);
		}
	}

	//Memory_MI_SetRegister(MI_VERSION_REG, 0x01010101);
	Memory_MI_SetRegister(MI_VERSION_REG, 0x02020202);
	// SP is halted
	Memory_SP_SetRegister(SP_STATUS_REG, SP_STATUS_HALT);
	((DWORD *)g_pMemoryBuffers[MEM_RI_REG])[3] = 1;					// RI_CONFIG_REG Skips most of init

	g_bSRAMUsed = FALSE;
}

void Memory_Cleanup()
{
	if (g_bSRAMUsed)
	{
		Memory_SaveSRAM();
	}
}


void Memory_LoadSRAM()
{
/*	TCHAR szSRAMFileName[MAX_PATH+1];
	FILE * fp;
	LONG d, i;
	u8 b[2048];
	u8 * pDst = (u8*)g_pMemoryBuffers[MEM_SRAM];

    /*KOS
	Dump_GetSaveDirectory(szSRAMFileName, g_ROM.szFileName, TEXT(".sra"));
	*/

/*	DBGConsole_Msg(0, "Loading sram from [C%s]", szSRAMFileName);

	fp = fopen(szSRAMFileName, "rb");
	if (fp != NULL)
	{
		for (d = 0; d < MemoryRegionSizes[MEM_SRAM]; d += sizeof(b))
		{
			fread(b, 1, sizeof(b), fp);

			for (i = 0; i < sizeof(b); i++)
			{
				pDst[d+i] = b[i^0x3];
			}
		}
		fclose(fp);
	}*/
}

void Memory_SaveSRAM()
{
	TCHAR szSRAMFileName[MAX_PATH+1];
	FILE * fp;
	LONG d, i;
	u8 b[2048];
	u8 * pSrc = (u8*)g_pMemoryBuffers[MEM_SRAM];
	
    /*KOS
	Dump_GetSaveDirectory(szSRAMFileName, g_ROM.szFileName, TEXT(".sra"));
	*/

	DBGConsole_Msg(0, "Saving sram to [C%s]", szSRAMFileName);

	fp = fopen(szSRAMFileName, "wb");
	if (fp != NULL)
	{
		for (d = 0; d < MemoryRegionSizes[MEM_SRAM]; d += sizeof(b))
		{
			for (i = 0; i < sizeof(b); i++)
			{
				b[i^0x3] = pSrc[d+i];
			}		
			fwrite(b, 1, sizeof(b), fp);
		}
		fclose(fp);
	}
}

void Memory_InitTables()
{
	DWORD dwStart;
	DWORD dwEnd;
	DWORD i;
	BOOL bMapGolden;

	// 0x00000000 - 0x7FFFFFFF Mapped Memory
	DWORD dwEntry = 0;

	ZeroMemory(g_ReadAddressLookupTable, sizeof(MemFastFunction) * 0x4000);
	ZeroMemory(g_WriteAddressLookupTable, sizeof(MemFastFunction) * 0x4000);
	ZeroMemory(g_WriteAddressValueLookupTable, sizeof(MemWriteValueFunction) * 0x4000);
	ZeroMemory(InternalReadFastTable, sizeof(InternalMemFastFunction) * 0x4000);
	ZeroMemory(InternalWriteFastTable, sizeof(InternalMemFastFunction) * 0x4000);
	
	while (MemMapEntries[dwEntry].dwStart != ~0)
	{
		dwStart = MemMapEntries[dwEntry].dwStart;
		dwEnd = MemMapEntries[dwEntry].dwEnd;

		for (i = (dwStart>>2); i <= (dwEnd>>2); i++)
		{
			g_ReadAddressLookupTable[i]  = MemMapEntries[dwEntry].ReadFastFunction;
			g_WriteAddressLookupTable[i] = MemMapEntries[dwEntry].WriteFastFunction;
			g_WriteAddressValueLookupTable[i] = MemMapEntries[dwEntry].WriteValueFunction;
		}

		dwEntry++;
	}

	dwEntry = 0;
	while (InternalMemMapEntries[dwEntry].dwStart != ~0)
	{
		dwStart = InternalMemMapEntries[dwEntry].dwStart;
		dwEnd = InternalMemMapEntries[dwEntry].dwEnd;

		for (i = (dwStart>>2); i <= (dwEnd>>2); i++) {
			InternalReadFastTable[i]  = InternalMemMapEntries[dwEntry].InternalReadFastFunction;
			InternalWriteFastTable[i] = InternalMemMapEntries[dwEntry].InternalWriteFastFunction;
		}

		dwEntry++;
	}

	// Check the tables
	/*for (i = 0; i < 0x4000; i++)
	{
		if (g_ReadAddressLookupTable[i] == NULL ||
			g_WriteAddressLookupTable[i] == NULL ||
			g_WriteAddressValueLookupTable[i] == NULL ||
			InternalReadFastTable[i] == NULL ||
			InternalWriteFastTable[i] == NULL)
		{
			CHAR str[300];
			wsprintf(str, "Warning: 0x%08x is null", i<<14);
			MessageBox(NULL, str, g_szDaedalusName, MB_OK);
		}
	}*/


	// Set up RDRAM areas:
	DWORD dwRAMSize = g_dwRamSize;
	MemFastFunction pReadRam;
	MemFastFunction pWriteRam;
	MemWriteValueFunction pWriteValueRam;

	InternalMemFastFunction pInternalReadRam;
	InternalMemFastFunction pInternalWriteRam;
	
	if (dwRAMSize == MEMORY_4_MEG)
	{
		DBGConsole_Msg(0, "Initialising 4Mb main memory");
		pReadRam = Read_RAM_4Mb_8000_803F;
		pWriteRam = Write_RAM_4Mb_8000_803F;
		pWriteValueRam = WriteValue_RAM_4Mb_8000_803F;

		pInternalReadRam = InternalRead_4Mb_8000_803F;
		pInternalWriteRam = InternalWrite_4Mb_8000_803F;
	}
	else
	{
		DBGConsole_Msg(0, "Initialising 8Mb main memory");
		pReadRam = Read_RAM_8Mb_8000_807F;
		pWriteRam = Write_RAM_8Mb_8000_807F;
		pWriteValueRam = WriteValue_RAM_8Mb_8000_807F;
		
		pInternalReadRam = InternalRead_8Mb_8000_807F;
		pInternalWriteRam = InternalWrite_8Mb_8000_807F;
	}

	// "Real"
	dwStart = 0x8000;
	dwEnd = 0x8000 + ((dwRAMSize>>16)-1);

	for (i = (dwStart>>2); i <= (dwEnd>>2); i++)
	{
		g_ReadAddressLookupTable[i]  = pReadRam;
		g_WriteAddressLookupTable[i] = pWriteRam;
		g_WriteAddressValueLookupTable[i] = pWriteValueRam;

		InternalReadFastTable[i]  = pInternalReadRam;
		InternalWriteFastTable[i] = pInternalWriteRam;
	}


	// Shadow
	if (dwRAMSize == MEMORY_4_MEG)
	{
		pReadRam = Read_RAM_4Mb_A000_A03F;
		pWriteRam = Write_RAM_4Mb_A000_A03F;
		pWriteValueRam = WriteValue_RAM_4Mb_A000_A03F;

		pInternalReadRam = InternalRead_4Mb_8000_803F;
		pInternalWriteRam = InternalWrite_4Mb_8000_803F;
	}
	else
	{
		pReadRam = Read_RAM_8Mb_A000_A07F;
		pWriteRam = Write_RAM_8Mb_A000_A07F;
		pWriteValueRam = WriteValue_RAM_8Mb_A000_A07F;
		
		pInternalReadRam = InternalRead_8Mb_8000_807F;
		pInternalWriteRam = InternalWrite_8Mb_8000_807F;
	}


	dwStart = 0xA000;
	dwEnd = 0xA000 + ((dwRAMSize>>16)-1);

	for (i = (dwStart>>2); i <= (dwEnd>>2); i++)
	{
		g_ReadAddressLookupTable[i]  = pReadRam;
		g_WriteAddressLookupTable[i] = pWriteRam;
		g_WriteAddressValueLookupTable[i] = pWriteValueRam;

		InternalReadFastTable[i]  = pInternalReadRam;
		InternalWriteFastTable[i] = pInternalWriteRam;
	}

	// The range 0x7f000000 -> 0x7fffffff is initialised to ReadMapped by default
	// We map the Goldeneye range over this region
	if (g_ROM.rh.dwCRC1 == 0xd150bcdc &&
		g_ROM.rh.dwCRC2 == 0xa31afd09 && 
		g_ROM.rh.nCountryID == 0x45)
	{
		bMapGolden = TRUE;
	}
	else
	{
		bMapGolden = FALSE;
	}


	if (bMapGolden)
	{
		//{ 0x7f00, 0x7FFF, InternalReadMappedGolden,	InternalWriteMappedGolden },	// Mapped Memory
		//{ 0x7f00, 0x7FFF, ReadMappedGolden,	WriteMappedGolden,	WriteValueMappedGolden },	// Mapped Memory
		DBGConsole_Msg(0, "[MMapping Virtual Memory (HACK)");

		for (i = (0x7f00>>2); i <= (0x7FFF>>2); i++)
		{
			g_ReadAddressLookupTable[i]  = ReadMappedGolden;
			g_WriteAddressLookupTable[i] = WriteInvalid;
			g_WriteAddressValueLookupTable[i] = WriteValueInvalid;

			InternalReadFastTable[i]  = InternalReadMappedGolden;
			InternalWriteFastTable[i] = InternalWriteInvalid;
		}
	}
}

// Free any cart loaded into memory.
void Memory_FreeCart()
{
	if (g_pMemoryBuffers[MEM_CARTROM] != NULL)
	{
		delete []g_pMemoryBuffers[MEM_CARTROM];
		g_pMemoryBuffers[MEM_CARTROM] = NULL;
		MemoryRegionSizes[MEM_CARTROM] = 0;
	}
}

void Memory_SetCart(u8 * pBytes, DWORD dwLen)
{
	Memory_FreeCart();

	g_pMemoryBuffers[MEM_CARTROM] = pBytes;
	MemoryRegionSizes[MEM_CARTROM] = dwLen;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Like Write16Bits, but not inline (for Dynamic Recomp)
void Memory_Write16Bits(DWORD dwAddress, u16 wData)
{
	*(u16 *)WriteAddress(dwAddress ^ 0x2) = wData;	
//	MemoryUpdate();
}


//////////////////////////////////////////////////////////////////////////////////////////
void MemoryDoSP_RDRAM_IDMEM_Copy()
{
	DWORD i;
	DWORD dwIDMEMAddrReg= Memory_SP_GetRegister(SP_MEM_ADDR_REG);
	DWORD dwMemAddrReg  = Memory_SP_GetRegister(SP_DRAM_ADDR_REG);
	DWORD dwLenReg      = Memory_SP_GetRegister(SP_RD_LEN_REG);

	DWORD dwMemAddr = (dwMemAddrReg&0x00FFFFFF);	// Limit to 0x00400000?
	DWORD dwIDMEMAddr = (dwIDMEMAddrReg&0x1FFF);
	DWORD dwLength = ((dwLenReg    )&0x0FFF) + 1;	// +1 !!!!
	DWORD dwCount  = ((dwLenReg>>12)&0x00FF);
	DWORD dwSkip   = ((dwLenReg>>20)&0x0FFF);
	
	DPF(DEBUG_SP, "SP: Copying from RDRAM (0x%08x) to (0x%08x)", dwMemAddrReg, dwIDMEMAddrReg );
	DPF(DEBUG_SP, "    Length: 0x%08x Count: 0x%08x Skip: 0x%08x", dwLength, dwCount, dwSkip);

	/*if (!g_bRSPHalted)
	{
		DBGConsole_Msg(0, "SP: Copying from RDRAM (0x%08x) to (0x%08x)", dwMemAddrReg, dwIDMEMAddrReg );
		DBGConsole_Msg(0, "    Length: 0x%08x Count: 0x%08x Skip: 0x%08x", dwLength, dwCount, dwSkip);
	}*/


	if (dwSkip != 0 || dwCount != 0)
	{
		DBGConsole_Msg(0, "Warning: RDRAM -> I/DMEM Transfer with non-zero count (%d)/skip(%d)",
			dwCount, dwSkip);
	}

	if ((dwMemAddr & 0x3) == 0 &&
		(dwIDMEMAddr & 0x3) == 0)
	{
		// Optimise for u32 alignment
		DWORD dwLen;

		// Do multiple of four using memcpy
		dwLen = (dwLength & ~0x3);
		
		if (dwLen)		// Might be 0 if xref is less than 4 bytes in total
			memcpy(&g_pu8SpMemBase[dwIDMEMAddr], &g_pu8RamBase[dwMemAddr], dwLen);			

		// Do remainder - this is only 0->3 bytes
		for (i = dwLen; i < dwLength; i++)
		{
			g_pu8SpMemBase[(i + dwIDMEMAddr)^3] = g_pu8RamBase[(i + dwMemAddr)^3];
		}
	}
	else
	{
		for (i = 0; i < dwLength; i++)
		{
			g_pu8SpMemBase[(i + dwIDMEMAddr)^3] = g_pu8RamBase[(i + dwMemAddr)^3];
		}
		DBGConsole_Msg(0, "Couldn't optimise: 0x%08x 0x%08x 0x%08x", 
			dwIDMEMAddr, dwMemAddr, dwLength);
	}


	s_nTotalSPTransferSize += dwLength;
	s_nNumSPTransfers++;

	//DBGConsole_Stats(11, "SP: S<-R %d %dMB", s_nNumSPTransfers, s_nTotalSPTransferSize/(1024*1024));



	Memory_SP_SetRegister(SP_STATUS_REG, SP_STATUS_HALT);
	Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SP);
	AddCPUJob(CPU_CHECK_INTERRUPTS);

	if (dwIDMEMAddr == 0x0fc0 &&
		g_bRSPHalted)
	{
		// We've written to the IMEM - check for a new task starting
		DPF(DEBUG_DP, "DP: #########Task load?");
		//DBGConsole_Msg(DEBUG_DP, "DP: #########Task load?");
		OSTask * pTask = (OSTask *)(g_pu8SpMemBase + 0x0FC0);
		RDP_LoadTask(pTask);
	}
	
	

}


void MemoryDoSP_IDMEM_RDRAM_Copy()
{
	DWORD dwIDMEMAddrReg= Memory_SP_GetRegister(SP_MEM_ADDR_REG);
	DWORD dwMemAddrReg  = Memory_SP_GetRegister(SP_DRAM_ADDR_REG);
	DWORD dwLenReg      = Memory_SP_GetRegister(SP_WR_LEN_REG);

	DWORD dwMemAddr = (dwMemAddrReg&0x00FFFFFF);	// Limit to 0x00400000?
	DWORD dwIDMEMAddr = (dwIDMEMAddrReg&0x1FFF);
	DWORD dwLength = ((dwLenReg)    &0x0FFF) + 1;		// +1
	DWORD dwCount  = ((dwLenReg>>12)&0x00FF);
	DWORD dwSkip   = ((dwLenReg>>20)&0x0FFF);
		
	/*if (!g_bRSPHalted)
	{
		DBGConsole_Msg(0, "SP: Copying from (0x%08x) to RDRAM (0x%08x) LenReg: 0x%08x", dwIDMEMAddrReg, dwMemAddrReg, dwLenReg );
		DBGConsole_Msg(0, "    Length: 0x%08x Count: 0x%08x Skip: 0x%08x", dwLength, dwCount, dwSkip);
	}*/

	DPF(DEBUG_SP, "SP: Copying from (0x%08x) to RDRAM (0x%08x)", dwIDMEMAddrReg, dwMemAddrReg );
	DPF(DEBUG_SP, "    Length: 0x%08x Count: 0x%08x Skip: 0x%08x", dwLength, dwCount, dwSkip);

	//DBGConsole_Msg(0, "0x%08x 0x%08x", dwMemAddr, dwIDMEMAddr);
	LONG i, c;

	// Not sure if count/skip is implemented properly
	for (c = 0; c < dwCount; c++)
	{
		for (i = 0; i < dwLength; i++)
		{
			//DBGConsole_Msg(0,"SP: Wrote 0x%02x", pSrc[(dwI+dwIDMEMAddr)^3]);
			g_pu8RamBase[(i+dwMemAddr)^3] = g_pu8SpMemBase[(i+dwIDMEMAddr)^3];
		}
		dwMemAddr += dwSkip;
		dwIDMEMAddr += dwLength;
	}

	Memory_SP_SetRegister(SP_STATUS_REG, SP_STATUS_HALT);
	Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SP);
	AddCPUJob(CPU_CHECK_INTERRUPTS);
}


/*
#define PI_DOM2_ADDR1		0x05000000	// to 0x05FFFFFF 
#define PI_DOM1_ADDR1		0x06000000	// to 0x07FFFFFF 
#define PI_DOM2_ADDR2		0x08000000	// to 0x0FFFFFFF 
#define PI_DOM1_ADDR2		0x10000000	// to 0x1FBFFFFF 
#define PI_DOM1_ADDR3		0x1FD00000	// to 0x7FFFFFFF 
*/
void Memory_PI_CopyToRDRAM()
{
	DWORD i;
	u8 * pSrc;
	DWORD dwCartSize;
	DWORD dwMemAddr  = Memory_PI_GetRegister(PI_DRAM_ADDR_REG) & 0x00FFFFFF;
	DWORD dwCartAddr = Memory_PI_GetRegister(PI_CART_ADDR_REG)  & 0xFFFFFFFF;
	DWORD dwPILenReg = (Memory_PI_GetRegister(PI_WR_LEN_REG)  & 0xFFFFFFFF) + 1;

	DPF(DEBUG_MEMORY_PI, "PI: Copying %d bytes of data from 0x%08x to 0x%08x", 
		dwPILenReg, dwCartAddr, dwMemAddr);

	if (dwCartAddr >= PI_DOM2_ADDR1 && dwCartAddr < PI_DOM1_ADDR1) //  0x05000000
	{
//		DBGConsole_Msg(0, "[YReading from Cart domain 2/addr1]");
		pSrc = (u8 *)g_pMemoryBuffers[MEM_SRAM];
		dwCartSize = MemoryRegionSizes[MEM_SRAM];
		dwCartAddr -= PI_DOM2_ADDR1;

		if (!g_bSRAMUsed)
		{
			// Load SRAM
			Memory_LoadSRAM();
			g_bSRAMUsed = TRUE;
		}
	}
	else if (dwCartAddr >= PI_DOM1_ADDR1 && dwCartAddr < PI_DOM2_ADDR2)
	{
//		DBGConsole_Msg(0, "[YReading from Cart domain 1/addr1]");
		pSrc = (u8 *)g_pMemoryBuffers[MEM_CARTROM];
		dwCartSize = MemoryRegionSizes[MEM_CARTROM];
		dwCartAddr -= PI_DOM1_ADDR1;

	}
	else if (dwCartAddr >= PI_DOM2_ADDR2 && dwCartAddr < PI_DOM1_ADDR2)
	{
		DBGConsole_Msg(0, "[YReading from Cart domain 2/addr2]");
		DBGConsole_Msg(0, "PI: Copying %d bytes of data from 0x%08x to 0x%08x", 
			dwPILenReg, dwCartAddr, dwMemAddr);

		pSrc = (u8 *)g_pMemoryBuffers[MEM_SRAM];
		dwCartSize = MemoryRegionSizes[MEM_SRAM];
		dwCartAddr -= PI_DOM2_ADDR2;

		if (!g_bSRAMUsed)
		{
			// Load SRAM
			Memory_LoadSRAM();
			g_bSRAMUsed = TRUE;
		}

	}
	else if (dwCartAddr >= PI_DOM1_ADDR2 && dwCartAddr < 0x1FBFFFFF)
	{
	//	DBGConsole_Msg(0, "[YReading from Cart domain 1/addr2]");
		pSrc = (u8 *)g_pMemoryBuffers[MEM_CARTROM];
		dwCartSize = MemoryRegionSizes[MEM_CARTROM];
		dwCartAddr -= PI_DOM1_ADDR2;
	}
	else if (dwCartAddr >= PI_DOM1_ADDR3 && dwCartAddr < 0x7FFFFFFF)
	{
//		DBGConsole_Msg(0, "[YReading from Cart domain 1/addr3]");
		pSrc = (u8 *)g_pMemoryBuffers[MEM_CARTROM];
		dwCartSize = MemoryRegionSizes[MEM_CARTROM];
		dwCartAddr -= PI_DOM1_ADDR3;
	}
	else
	{
		DBGConsole_Msg(0, "[YUnknown PI Address 0x%08x]", dwCartAddr);
	}


	if ((dwCartAddr + dwPILenReg) > dwCartSize ||
		(dwMemAddr + dwPILenReg) > g_dwRamSize)
	{
		DBGConsole_Msg(0, "PI: Copying 0x%08x bytes of data from 0x%08x to 0x%08x", 
			Memory_PI_GetRegister(PI_WR_LEN_REG),
			Memory_PI_GetRegister(PI_CART_ADDR_REG),
			Memory_PI_GetRegister(PI_DRAM_ADDR_REG));
		DBGConsole_Msg(0, "PIXFer: Copy overlaps RAM/ROM boundary");
		DBGConsole_Msg(0, "PIXFer: Not copying, but issuing interrupt");

		Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_PI);
		AddCPUJob(CPU_CHECK_INTERRUPTS);	
		return;
	}

	// We only have to fiddle the bytes when
	// a) the src is not word aligned
	// b) the dst is not word aligned
	// c) the length is not a multiple of 4 (although we can copy most directly)
	// If the source/dest are word aligned, we can simply copy most of the
	// words using memcpy. Any remaining bytes are then copied individually
	if ((dwMemAddr & 0x3) == 0 &&
		(dwCartAddr & 0x3) == 0)
	{
		// Optimise for u32 alignment
		DWORD dwLen;

		// Do multiple of four using memcpy
		dwLen = (dwPILenReg & ~0x3);
		
		if (dwLen)		// Might be 0 if xref is less than 4 bytes in total
			memcpy(&g_pu8RamBase[dwMemAddr],  &pSrc[dwCartAddr], dwLen);			

		// Do remainder - this is only 0->3 bytes
		for (i = dwLen; i < dwPILenReg; i++)
		{
			g_pu8RamBase[(i + dwMemAddr)^3] = pSrc[(i + dwCartAddr)^3];
		}
	}
	else
	{
		for (i = 0; i < dwPILenReg; i++)
		{
			g_pu8RamBase[(i + dwMemAddr)^3] = pSrc[(i + dwCartAddr)^3];
		}
		//DBGConsole_Msg(0, "Couldn't optimise: 0x%08x 0x%08x 0x%08x", 
		//	dwMemAddr, dwCartAddr, dwPILenReg);
	}

	s_nTotalDmaTransferSize += dwPILenReg;

	// Note if dwRescanCount is 0, the rom is only scanned when the
	// ROM jumps to the game boot address
	if (++s_nNumDmaTransfers == g_ROM.dwRescanCount)
	{
		// Try to reapply patches - certain roms load in more of the OS after
		// a number of transfers
		Patch_ApplyPatches();
	}

	//DBGConsole_Stats(10, "PI: C->R %d %dMB", s_nNumDmaTransfers, s_nTotalDmaTransferSize/(1024*1024));


	Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_PI);
	AddCPUJob(CPU_CHECK_INTERRUPTS);	

}
void Memory_PI_CopyFromRDRAM()
{
	DWORD i;
	u8 * pDst;
	DWORD dwCartSize;
	DWORD dwMemAddr  = Memory_PI_GetRegister(PI_DRAM_ADDR_REG) & 0xFFFFFFFF;
	DWORD dwCartAddr = Memory_PI_GetRegister(PI_CART_ADDR_REG)  & 0xFFFFFFFF;;
	DWORD dwPILenReg = (Memory_PI_GetRegister(PI_RD_LEN_REG)  & 0xFFFFFFFF) + 1;

	DPF(DEBUG_MEMORY_PI, "PI: Copying %d bytes of data from 0x%08x to 0x%08x", 
		dwPILenReg, dwMemAddr, dwCartAddr);

	if (dwCartAddr >= PI_DOM2_ADDR1 && dwCartAddr < PI_DOM1_ADDR1) //  0x05000000
	{
	//	DBGConsole_Msg(0, "[YWriting to Cart domain 2/addr1]");
		pDst = (u8 *)g_pMemoryBuffers[MEM_SRAM];
		dwCartSize = MemoryRegionSizes[MEM_SRAM];
		dwCartAddr -= PI_DOM2_ADDR1;

		if (!g_bSRAMUsed)
		{
			// Load SRAM
			Memory_LoadSRAM();
			g_bSRAMUsed = TRUE;
		}

	}
	else if (dwCartAddr >= PI_DOM1_ADDR1 && dwCartAddr < PI_DOM2_ADDR2)
	{
	//	DBGConsole_Msg(0, "[YWriting to Cart domain 1/addr1]");
		pDst = (u8 *)g_pMemoryBuffers[MEM_CARTROM];
		dwCartSize = MemoryRegionSizes[MEM_CARTROM];
		dwCartAddr -= PI_DOM1_ADDR1;

	}
	else if (dwCartAddr >= PI_DOM2_ADDR2 && dwCartAddr < PI_DOM1_ADDR2)
	{
		DBGConsole_Msg(0, "[YWriting to Cart domain 2/addr2]");
		DBGConsole_Msg(0, "PI: Copying %d bytes of data from 0x%08x to 0x%08x", 
			dwPILenReg, dwMemAddr, dwCartAddr);
		pDst = (u8 *)g_pMemoryBuffers[MEM_SRAM];
		dwCartSize = MemoryRegionSizes[MEM_SRAM];
		dwCartAddr -= PI_DOM2_ADDR2;

		if (!g_bSRAMUsed)
		{
			// Load SRAM
			Memory_LoadSRAM();
			g_bSRAMUsed = TRUE;
		}
		
	}
	else if (dwCartAddr >= PI_DOM1_ADDR2 && dwCartAddr < PI_DOM1_ADDR3)
	{
	//	DBGConsole_Msg(0, "[YWriting to Cart domain 1/addr2]");
		pDst = (u8 *)g_pMemoryBuffers[MEM_CARTROM];
		dwCartSize = MemoryRegionSizes[MEM_CARTROM];
		dwCartAddr -= PI_DOM1_ADDR2;
	}
	else if (dwCartAddr >= PI_DOM1_ADDR3 && dwCartAddr < 0x7FFFFFFF)
	{
	//	DBGConsole_Msg(0, "[YWriting to Cart domain 1/addr3]");
		pDst = (u8 *)g_pMemoryBuffers[MEM_CARTROM];
		dwCartSize = MemoryRegionSizes[MEM_CARTROM];
		dwCartAddr -= PI_DOM1_ADDR3;
	}
	else
	{
		DBGConsole_Msg(0, "[YUnknown PI Address 0x%08x]", dwCartAddr);
	}


	if ((dwMemAddr + dwPILenReg) > g_dwRamSize ||
		(dwCartAddr + dwPILenReg) > dwCartSize)
	{
		DBGConsole_Msg(0, "PI: Copying %d bytes of data from 0x%08x to 0x%08x", 
			dwPILenReg, dwMemAddr, dwCartAddr);
		DBGConsole_Msg(0, "PIXFer: Copy overlaps RAM/ROM boundary");
		DBGConsole_Msg(0, "PIXFer: Not copying, but issuing interrupt");

		Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_PI);
		AddCPUJob(CPU_CHECK_INTERRUPTS);	
		return;
	}

	// TODO: This isn't very fast. We only need to fiddle bytes when
	// a) the src is not word aligned
	// b) the dst is not word aligned
	// c) the length is not a multiple of 4 (although we can copy most directly
	for (i = 0; i < dwPILenReg; i++)
	{
		pDst[(i + dwCartAddr)^3] = g_pu8RamBase[(i + dwMemAddr)^3];
	}

	Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_PI);
	AddCPUJob(CPU_CHECK_INTERRUPTS);	

}


void MemoryDoDP()
{
	DBGConsole_Msg(0, "DP: Registers written to");
}

void MemoryDoAI()
{
	DWORD dwAISrcReg  = Memory_AI_GetRegister(AI_DRAM_ADDR_REG) & 0xfffff8;
	DWORD dwAILenReg  = Memory_AI_GetRegister(AI_LEN_REG) & 0x3fff8;
	DWORD dwAIDACrate = Memory_AI_GetRegister(AI_DACRATE_REG) + 1;
	DWORD dwAIBitrate = Memory_AI_GetRegister(AI_BITRATE_REG) + 1;
	DWORD dwAIDMAEnabled = Memory_AI_GetRegister(AI_CONTROL_REG);

	DWORD dwFreq = (VI_NTSC_CLOCK/dwAIDACrate);	// Might be divided by bytes/sample

	if (dwAIDMAEnabled == FALSE)
	{
		return;
	}

	WORD *pSrc = (WORD *)(g_pu8RamBase + dwAISrcReg);

	DPF(DEBUG_MEMORY_AI, "AI: Playing %d bytes of data from 0x%08x", dwAILenReg, dwAISrcReg);
	DPF(DEBUG_MEMORY_AI, "    Bitrate: %d. DACRate: 0x%08x, Freq: %d", dwAIBitrate, dwAIDACrate, dwFreq);
	DPF(DEBUG_MEMORY_AI, "    DMA: 0x%08x", dwAIDMAEnabled);

	//TODO - Ensure ranges are OK.

	// Set length to 0
	//Memory_AI_SetRegister(AI_LEN_REG, 0);
	//g_dwNextAIInterrupt = (u32)g_qwCPR[0][C0_COUNT] + ((VID_CLOCK*30)*dwAILenReg)/(22050*4);

	//Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_AI);
	//AddCPUJob(CPU_CHECK_INTERRUPTS);
	//{
	//	DSound_AddSound(pSrc, dwAILenReg);
	//}
}

static BOOL g_bMemStopOnSi = FALSE;

void Memory_SiDisable()
{
	DBGConsole_Msg(DEBUG_MEMORY_PIF, "Si Transfers disabled");
	g_bMemStopOnSi = TRUE;
}
void Memory_SiEnable()
{
	DBGConsole_Msg(DEBUG_MEMORY_PIF, "Si Transfers enabled");
	g_bMemStopOnSi = FALSE;
	
}



void MemoryUpdateSP(DWORD dwAddress, DWORD dwValue)
{

	DWORD dwStatus = Memory_SP_GetRegister(SP_STATUS_REG);
		
	if (dwValue & SP_CLR_HALT)
	{
		DPF(DEBUG_SP, "SP: Clearing Halt");
		if (g_bSkipFirstRSP)
		{
			DBGConsole_Msg(0, "[YSkipping first RSP task]");
			g_bSkipFirstRSP = FALSE;
		}
		else
		{
			dwStatus &= ~SP_STATUS_HALT;
			StartRSP();
		}
	}
	if (dwValue & SP_SET_HALT)
	{
		DPF(DEBUG_SP, "SP: Setting Halt");
		dwStatus |= SP_STATUS_HALT;
		StopRSP();
	}

	if (dwValue & SP_CLR_BROKE) {		DPF(DEBUG_SP, "SP: Clearing Broke");
		dwStatus &= ~SP_STATUS_BROKE;		}

	if (dwValue & SP_CLR_INTR) {			DPF(DEBUG_SP, "SP: Clearing Interrupt");
		Memory_MI_ClrRegisterBits(MI_INTR_REG, MI_INTR_SP);				}

	// Should set flag to make sure we listen for interrupts on this interface...?
	if (dwValue & SP_SET_INTR) {			DPF(DEBUG_SP, "SP: Setting Interrupt");
		Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SP);				}

	if (dwValue & SP_CLR_SSTEP) {		DPF(DEBUG_SP, "SP: Clearing Single Step");
		dwStatus &= ~SP_STATUS_SSTEP;		}
	if (dwValue & SP_SET_SSTEP) {		DPF(DEBUG_SP, "SP: Setting Single Step");
		dwStatus |= SP_STATUS_SSTEP;		}

	if (dwValue & SP_CLR_INTR_BREAK) {	DPF(DEBUG_SP, "SP: Clearing Interrupt on break");
		dwStatus &= ~SP_STATUS_INTR_BREAK;	}
	if (dwValue & SP_SET_INTR_BREAK) {	DPF(DEBUG_SP, "SP: Setting Interrupt on break");
		dwStatus |= SP_STATUS_INTR_BREAK;	}

	if (dwValue & SP_CLR_SIG0) {			DPF(DEBUG_SP, "SP: Clearing Sig0");
		dwStatus &= ~SP_STATUS_SIG0;		}
	if (dwValue & SP_SET_SIG0) {			DPF(DEBUG_SP, "SP: Setting Sig0");
		dwStatus |= SP_STATUS_SIG0;		}

	if (dwValue & SP_CLR_SIG1) {			DPF(DEBUG_SP, "SP: Clearing Sig1");
		dwStatus &= ~SP_STATUS_SIG1;		}
	if (dwValue & SP_SET_SIG1) {			DPF(DEBUG_SP, "SP: Setting Sig1");
		dwStatus |= SP_STATUS_SIG1;		}

	if (dwValue & SP_CLR_SIG2) {			DPF(DEBUG_SP, "SP: Clearing Sig2");
		dwStatus &= ~SP_STATUS_SIG2;		}
	if (dwValue & SP_SET_SIG2) {			DPF(DEBUG_SP, "SP: Setting Sig2");
		dwStatus |= SP_STATUS_SIG2;		}

	if (dwValue & SP_CLR_SIG3) {			DPF(DEBUG_SP, "SP: Clearing Sig3");
		dwStatus &= ~SP_STATUS_SIG3;		}
	if (dwValue & SP_SET_SIG3) {			DPF(DEBUG_SP, "SP: Setting Sig3");
		dwStatus |= SP_STATUS_SIG3;		}

	if (dwValue & SP_CLR_SIG4) {			DPF(DEBUG_SP, "SP: Clearing Sig4");
		dwStatus &= ~SP_STATUS_SIG4;		}
	if (dwValue & SP_SET_SIG4) {			DPF(DEBUG_SP, "SP: Setting Sig4");
		dwStatus |= SP_STATUS_SIG4;		}

	if (dwValue & SP_CLR_SIG5) {			DPF(DEBUG_SP, "SP: Clearing Sig5");
		dwStatus &= ~SP_STATUS_SIG5;		}
	if (dwValue & SP_SET_SIG5) {			DPF(DEBUG_SP, "SP: Setting Sig5");
		dwStatus |= SP_STATUS_SIG5;		}

	if (dwValue & SP_CLR_SIG6) {			DPF(DEBUG_SP, "SP: Clearing Sig6");
		dwStatus &= ~SP_STATUS_SIG6;		}
	if (dwValue & SP_SET_SIG6) {			DPF(DEBUG_SP, "SP: Setting Sig6");
		dwStatus |= SP_STATUS_SIG6;		}

	if (dwValue & SP_CLR_SIG7) {			DPF(DEBUG_SP, "SP: Clearing Sig7");
		dwStatus &= ~SP_STATUS_SIG7;		}
	if (dwValue & SP_SET_SIG7) {			DPF(DEBUG_SP, "SP: Setting Sig7");
		dwStatus |= SP_STATUS_SIG7;		}


	// Not sure why this is always set??!
	Memory_SP_SetRegister(SP_STATUS_REG, dwStatus | SP_STATUS_HALT);

}

void MemoryUpdateDP(DWORD dwAddress, DWORD dwValue)
{

	DPF(DEBUG_DP, "DP: Updating Status....");

	// Ignore address, as this is only called with DPC_STATUS_REG write
	// DBGConsole_Msg(0, "DP Status: 0x%08x", dwValue);

	DWORD dwDPStatusReg  =  Memory_DPC_GetRegister(DPC_STATUS_REG);

	if (dwValue & DPC_CLR_XBUS_DMEM_DMA) {				DPF(DEBUG_DP, "DP: Clearing XBus DMEM DMA. PC: 0x%08x", g_dwPC);
		dwDPStatusReg &= ~DPC_STATUS_XBUS_DMEM_DMA;		}
	if (dwValue & DPC_SET_XBUS_DMEM_DMA) {				DPF(DEBUG_DP, "DP: Setting XBus DMEM DMA. PC: 0x%08x", g_dwPC);
		dwDPStatusReg |= DPC_STATUS_XBUS_DMEM_DMA;		}
	
	if (dwValue & DPC_CLR_FREEZE) {						DPF(DEBUG_DP, "DP: Clearing Freeze. PC: 0x%08x", g_dwPC);
		dwDPStatusReg &= ~DPC_STATUS_FREEZE;			}
	// Thanks Lemmy!
	//if (dwValue & DPC_SET_FREEZE) {						DPF(DEBUG_DP, "DP: Setting Freeze. PC: 0x%08x", g_dwPC);
	//	dwDPStatusReg |= DPC_STATUS_FREEZE;				}

	if (dwValue & DPC_CLR_FLUSH) {						DPF(DEBUG_DP, "DP: Clearing Flush. PC: 0x%08x", g_dwPC);
		dwDPStatusReg &= ~DPC_STATUS_FLUSH;				}
	if (dwValue & DPC_SET_FLUSH) {						DPF(DEBUG_DP, "DP: Setting Flush. PC: 0x%08x", g_dwPC);
		dwDPStatusReg |= DPC_STATUS_FLUSH;				}

	if (dwValue & DPC_CLR_TMEM_CTR)						{	DPF(DEBUG_DP, "DP: Clearing TMEM Reg. PC: 0x%08x", g_dwPC);
		Memory_DPC_SetRegister(DPC_TMEM_REG, 0);		}

	if (dwValue & DPC_CLR_PIPE_CTR)						{	DPF(DEBUG_DP, "DP: Clearing PIPEBUSY Reg. PC: 0x%08x", g_dwPC);
		Memory_DPC_SetRegister(DPC_PIPEBUSY_REG, 0);	}

	if (dwValue & DPC_CLR_CMD_CTR)						{	DPF(DEBUG_DP, "DP: Clearing BUSBUSY Reg. PC: 0x%08x", g_dwPC);
		Memory_DPC_SetRegister(DPC_BUFBUSY_REG, 0);		}

	if (dwValue & DPC_CLR_CLOCK_CTR)					{	DPF(DEBUG_DP, "DP: Clearing CLOCK Reg. PC: 0x%08x", g_dwPC);
		Memory_DPC_SetRegister(DPC_CLOCK_REG, 0);		}

	// Write back the value
	Memory_DPC_SetRegister(DPC_STATUS_REG, dwDPStatusReg);

}

void MemoryUpdateMI(DWORD dwAddress, DWORD dwValue)
{
	
	DWORD dwOffset;
	DWORD dwMIModeReg     = Memory_MI_GetRegister(MI_MODE_REG);
	DWORD dwMIIntrMaskReg = Memory_MI_GetRegister(MI_INTR_MASK_REG);

	dwOffset = dwAddress & 0xFF;

	switch (MI_BASE_REG + dwOffset)
	{

	case MI_INIT_MODE_REG:

		if (dwValue & MI_CLR_INIT) {		DPF(DEBUG_MI, "MI: Clearing Mode Init. PC: 0x%08x", g_dwPC);
			dwMIModeReg &= ~MI_MODE_INIT;			}
		if (dwValue & MI_SET_INIT) {		DPF(DEBUG_MI, "MI: Setting Mode Init. PC: 0x%08x", g_dwPC);
			dwMIModeReg |= MI_MODE_INIT;			}

		if (dwValue & MI_CLR_EBUS) {		DPF(DEBUG_MI, "MI: Clearing EBUS test. PC: 0x%08x", g_dwPC);
			dwMIModeReg &= ~MI_MODE_EBUS;			}
		if (dwValue & MI_SET_EBUS) {		DPF(DEBUG_MI, "MI: Setting EBUS test. PC: 0x%08x", g_dwPC);
			dwMIModeReg |= MI_MODE_EBUS;			}

		if (dwValue & MI_CLR_DP_INTR) {		DPF(DEBUG_MI, "MI: Clearing DP Interrupt. PC: 0x%08x", g_dwPC);
			Memory_MI_ClrRegisterBits(MI_INTR_REG, MI_INTR_DP);				}

		if (dwValue & MI_CLR_RDRAM) {		DPF(DEBUG_MI, "MI: Clearing RDRAM reg. PC: 0x%08x", g_dwPC);
			dwMIModeReg &= ~MI_MODE_RDRAM;			}
		if (dwValue & MI_SET_RDRAM) {		DPF(DEBUG_MI, "MI: Setting RDRAM reg mode. PC: 0x%08x", g_dwPC);
			dwMIModeReg |= MI_MODE_RDRAM;			}

		break;
	case MI_INTR_MASK_REG:

		// Do IntrMaskReg writes
		if (dwValue & MI_INTR_MASK_CLR_SP) {		DPF(DEBUG_MI, "MI: Clearing SP Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg &= ~MI_INTR_MASK_SP;					}
		if (dwValue & MI_INTR_MASK_SET_SP) {		DPF(DEBUG_MI, "MI: Setting SP Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg |= MI_INTR_MASK_SP;					}

		if (dwValue & MI_INTR_MASK_CLR_SI) {		DPF(DEBUG_MI, "MI: Clearing SI Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg &= ~MI_INTR_MASK_SI;					}	
		if (dwValue & MI_INTR_MASK_SET_SI) {		DPF(DEBUG_MI, "MI: Setting SI Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg |= MI_INTR_MASK_SI;					}

		if (dwValue & MI_INTR_MASK_CLR_AI) {		DPF(DEBUG_MI, "MI: Clearing AI Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg &= ~MI_INTR_MASK_AI;					}
		if (dwValue & MI_INTR_MASK_SET_AI) {		DPF(DEBUG_MI, "MI: Setting AI Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg |= MI_INTR_MASK_AI;					}

		if (dwValue & MI_INTR_MASK_CLR_VI) {		DPF(DEBUG_MI, "MI: Clearing VI Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg &= ~MI_INTR_MASK_VI;					}
		if (dwValue & MI_INTR_MASK_SET_VI) {		DPF(DEBUG_MI, "MI: Setting VI Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg |= MI_INTR_MASK_VI;					}

		if (dwValue & MI_INTR_MASK_CLR_PI) {		DPF(DEBUG_MI, "MI: Clearing PI Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg &= ~MI_INTR_MASK_PI;					}
		if (dwValue & MI_INTR_MASK_SET_PI) {		DPF(DEBUG_MI, "MI: Setting PI Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg |= MI_INTR_MASK_PI;					}

		if (dwValue & MI_INTR_MASK_CLR_DP) {		DPF(DEBUG_MI, "MI: Clearing DP Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg &= ~MI_INTR_MASK_DP;					}
		if (dwValue & MI_INTR_MASK_SET_DP) {		DPF(DEBUG_MI, "MI: Setting DP Interrupt. PC: 0x%08x", g_dwPC);
			dwMIIntrMaskReg |= MI_INTR_MASK_DP;					}

		break;
	}

	// Write back
	Memory_MI_SetRegister(MI_MODE_REG, dwMIModeReg);
	Memory_MI_SetRegister(MI_INTR_MASK_REG, dwMIIntrMaskReg);


}

#ifdef DAEDALUS_LOG

void DisplayVIControlInfo(DWORD dwControlReg)
{
	DPF(DEBUG_VI, "VI Control:", (dwControlReg & VI_CTRL_GAMMA_DITHER_ON) ? "On" : "Off");

	CHAR *szMode = "Disabled/Unknown";
	     if ((dwControlReg & VI_CTRL_TYPE_16) == VI_CTRL_TYPE_16) szMode = "16-bit";
	else if ((dwControlReg & VI_CTRL_TYPE_32) == VI_CTRL_TYPE_32) szMode = "32-bit";

	DPF(DEBUG_VI, "         ColorDepth: %s", szMode);
	DPF(DEBUG_VI, "         Gamma Dither: %s", (dwControlReg & VI_CTRL_GAMMA_DITHER_ON) ? "On" : "Off");
	DPF(DEBUG_VI, "         Gamma: %s", (dwControlReg & VI_CTRL_GAMMA_ON) ? "On" : "Off");
	DPF(DEBUG_VI, "         Divot: %s", (dwControlReg & VI_CTRL_DIVOT_ON) ? "On" : "Off");
	DPF(DEBUG_VI, "         Interlace: %s", (dwControlReg & VI_CTRL_SERRATE_ON) ? "On" : "Off");
	DPF(DEBUG_VI, "         AAMask: 0x%02x", (dwControlReg&VI_CTRL_ANTIALIAS_MASK)>>8);
	DPF(DEBUG_VI, "         DitherFilter: %s", (dwControlReg & VI_CTRL_DITHER_FILTER_ON) ? "On" : "Off");

}

#endif


void MemoryUpdateVI(DWORD dwAddress, DWORD dwValue)
{
	DWORD dwOffset;
	
	dwOffset = dwAddress & 0xFF;

	switch (VI_BASE_REG + dwOffset)
	{
	case VI_CONTROL_REG:
		DPF(DEBUG_VI, "VI_CONTROL_REG set to 0x%08x", dwValue);
#ifdef DAEDALUS_LOG
		DisplayVIControlInfo(dwValue);
#endif
        /*KOS
		if (g_pGfxPlugin != NULL)
		{
			g_pGfxPlugin->ViStatusChanged();
		}
		*/
		

		break;
	case VI_ORIGIN_REG:
		DPF(DEBUG_VI, "VI_ORIGIN_REG set to 0x%08x (was 0x%08x)", dwValue);
			//MainWindow_UpdateScreen();
		if (Memory_VI_GetRegister(VI_ORIGIN_REG) != dwValue)
		{
			//DBGConsole_Msg(0, "Origin: 0x%08x (%d x %d)", dwValue, g_dwViWidth, g_dwViHeight);
			g_dwNumFrames++;	
		}
		break;

	case VI_WIDTH_REG:
		DPF(DEBUG_VI, "VI_WIDTH_REG set to %d pixels", dwValue);
	//	DBGConsole_Msg(DEBUG_VI, "VI_WIDTH_REG set to %d pixels", dwValue);
		//if (g_dwViWidth != dwValue)
		//{
		//	DBGConsole_Msg(DEBUG_VI, "VI_WIDTH_REG set to %d pixels (was %d)", dwValue, g_dwViWidth);
		//	g_dwViWidth = dwValue;
		//	g_dwViHeight = (dwValue * 3) / 4;
		//}
        /*KOS
		if (g_pGfxPlugin != NULL)
		{
			g_pGfxPlugin->ViWidthChanged();
		}
		*/


		break;

	case VI_INTR_REG:
		DPF(DEBUG_VI, "VI_INTR_REG set to 0x%08x", dwValue);
		//DBGConsole_Msg(DEBUG_VI, "VI_INTR_REG set to %d", dwValue);
		break;

	case VI_CURRENT_REG:
		DPF(DEBUG_VI, "VI_CURRENT_REG set to 0x%08x", dwValue);

		// Any write clears interrupt line...
		DPF(DEBUG_VI, "VI: Clearing interrupt flag. PC: 0x%08x", g_dwPC);

		Memory_MI_ClrRegisterBits(MI_INTR_REG, MI_INTR_VI);
		break;

	case VI_BURST_REG:
		DPF(DEBUG_VI, "VI_BURST_REG set to 0x%08x", dwValue);
	//	DBGConsole_Msg(DEBUG_VI, "VI_BURST_REG set to 0x%08x", dwValue);
		break;

	case VI_V_SYNC_REG:
		DPF(DEBUG_VI, "VI_V_SYNC_REG set to 0x%08x", dwValue);
	//	DBGConsole_Msg(DEBUG_VI, "VI_V_SYNC_REG set to 0x%08x", dwValue);
		break;

	case VI_H_SYNC_REG:
		DPF(DEBUG_VI, "VI_H_SYNC_REG set to 0x%08x", dwValue);
	//	DBGConsole_Msg(DEBUG_VI, "VI_H_SYNC_REG set to 0x%08x", dwValue);
		break;

	case VI_LEAP_REG:
		DPF(DEBUG_VI, "VI_LEAP_REG set to 0x%08x", dwValue);
	//	DBGConsole_Msg(DEBUG_VI, "VI_LEAP_REG set to 0x%08x", dwValue);
		break;

	case VI_H_START_REG:
		DPF(DEBUG_VI, "VI_H_START_REG set to 0x%08x", dwValue);
		//DBGConsole_Msg(DEBUG_VI, "VI_H_START_REG set to 0x%08x", dwValue);
		break;

	case VI_V_START_REG:
		DPF(DEBUG_VI, "VI_V_START_REG set to 0x%08x", dwValue);
		//DBGConsole_Msg(DEBUG_VI, "VI_V_START_REG set to 0x%08x", dwValue);
		break;

	case VI_V_BURST_REG:
		DPF(DEBUG_VI, "VI_V_BURST_REG set to 0x%08x", dwValue);
		//DBGConsole_Msg(DEBUG_VI, "VI_V_BURST_REG set to 0x%08x", dwValue);
		break;

	case VI_X_SCALE_REG:
		DPF(DEBUG_VI, "VI_X_SCALE_REG set to 0x%08x", dwValue);
		{
			DWORD dwScale = dwValue & 0xFFF;
			float fScale = (float)dwScale / (1<<10);
			//DBGConsole_Msg(DEBUG_VI, "VI_X_SCALE_REG set to 0x%08x (%f)", dwValue, 1/fScale);
			g_dwViWidth = 640 * fScale;
		}
		break;

	case VI_Y_SCALE_REG:
		DPF(DEBUG_VI, "VI_Y_SCALE_REG set to 0x%08x", dwValue);
		{
			DWORD dwScale = dwValue & 0xFFF;
			float fScale = (float)dwScale / (1<<10);
		//	DBGConsole_Msg(DEBUG_VI, "VI_Y_SCALE_REG set to 0x%08x (%f)", dwValue, 1/fScale);
			g_dwViHeight = 240 * fScale;
		}
		break;
	}

	*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_VI_REG] + dwOffset) = dwValue;
}



void MemoryUpdatePI(DWORD dwAddress, DWORD dwValue)
{
	DWORD dwOffset = dwAddress & 0xFF;
	DWORD dwPIAddress = (PI_BASE_REG) + dwOffset;


	if (dwPIAddress == PI_STATUS_REG)
	{
		if (dwValue & PI_STATUS_RESET)
		{
			DPF(DEBUG_PI, "PI: Resetting Status. PC: 0x%08x", g_dwPC);
			// Reset PIC here
			Memory_PI_SetRegister(PI_STATUS_REG, 0);
		}
		if (dwValue & PI_STATUS_CLR_INTR)
		{
			DPF(DEBUG_PI, "PI: Clearing interrupt flag. PC: 0x%08x", g_dwPC);
			Memory_MI_ClrRegisterBits(MI_INTR_REG, MI_INTR_PI);
		}
		
	}
	else
	{
		switch (dwPIAddress)
		{
		case PI_BSD_DOM1_LAT_REG:
			DPF(DEBUG_MEMORY_PI, "PI_BSD_DOM1_LAT_REG written to (dom1 device latency) - 0x%08x", dwValue);
			break;
		case PI_BSD_DOM1_PWD_REG:
			DPF(DEBUG_MEMORY_PI, "PI_BSD_DOM1_PWD_REG written to (dom1 device R/W strobe pulse width) - 0x%08x", dwValue);
			break;
		case PI_BSD_DOM1_PGS_REG:
			DPF(DEBUG_MEMORY_PI, "PI_BSD_DOM1_PGS_REG written to (dom1 device page size) - 0x%08x",	dwValue);
			break;
		case PI_BSD_DOM1_RLS_REG:
			DPF(DEBUG_MEMORY_PI, "PI_BSD_DOM1_RLS_REG written to (dom1 device R/W release duration) - 0x%08x",dwValue);
			break;
		case PI_BSD_DOM2_LAT_REG:
			DPF(DEBUG_MEMORY_PI, "PI_BSD_DOM2_LAT_REG written to (dom2 device latency) - 0x%08x", dwValue);
			break;
		case PI_BSD_DOM2_PWD_REG:
			DPF(DEBUG_MEMORY_PI, "PI_BSD_DOM2_PWD_REG written to (dom2 device R/W strobe pulse width) - 0x%08x", dwValue);
			break;
		case PI_BSD_DOM2_PGS_REG:
			DPF(DEBUG_MEMORY_PI, "PI_BSD_DOM2_PGS_REG written to (dom2 device page size) - 0x%08x",	dwValue);
			break;
		case PI_BSD_DOM2_RLS_REG:
			DPF(DEBUG_MEMORY_PI, "PI_BSD_DOM2_RLS_REG written to (dome device R/W release duration) - 0x%08x", dwValue);
			break;
		}

		*(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_PI_REG] + dwOffset) = dwValue;
	}

}


// Copy 64bytes from DRAM to PIF_RAM (and PIF_RAM_W?)
void Memory_SI_CopyFromDRAM( )
{
	DWORD dwDRAMOffset = *(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_SI_REG] + 0x00);
	BYTE * pDst = (BYTE *)g_pMemoryBuffers[MEM_PI_RAM];
	BYTE * pSrc = g_pu8RamBase + dwDRAMOffset;
	
	DPF(DEBUG_MEMORY_PIF, "DRAM (0x%08x) -> PIF Transfer ", dwDRAMOffset);
	
	memcpy(pDst, pSrc, 64);
	
	if (g_bMemStopOnSi)
	{
		CPU_Halt("SI: CopyFromDRAM");	
	}

	Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SI);
	AddCPUJob(CPU_CHECK_INTERRUPTS);
}

// Copy 64bytes to DRAM from PIF_RAM
void Memory_SI_CopyToDRAM( )
{
	DWORD dwDRAMOffset = *(DWORD *)((BYTE *)g_pMemoryBuffers[MEM_SI_REG] + 0x0);
	BYTE * pSrc = (BYTE *)g_pMemoryBuffers[MEM_PI_RAM];
	BYTE * pDst = g_pu8RamBase + dwDRAMOffset;

	DPF(DEBUG_MEMORY_PIF, "PIF -> DRAM (0x%08x) Transfer ", dwDRAMOffset);

	// Check controller status!
	Controller_Check();

	memcpy(pDst, pSrc, 64);

	if (g_bMemStopOnSi)
	{
		CPU_Halt("SI: CopyToDRAM");	
	}

	Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SI);
	AddCPUJob(CPU_CHECK_INTERRUPTS);
}
