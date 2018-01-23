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

#include "main.h"
#include "ROM.h"

#include "inifile.h"

#include "ultra_types.h"
#include "ultra_r4300.h"
#include "DBGConsole.h"
#include "Debug.h"
#include "CPU.h"
#include "RSP.h"
#include "RDP.h"
#include "Patch.h"			// Patch_ApplyPatches
#include "Controller.h"		// Controller_Initialise
#include "daedalus_crc.h"
#include "ultra_os.h"		// System type


static void ROM_GetRomNameFromHeader(TCHAR * szName, ROMHeader * pHdr);
static u32 ROM_CrcBootCode(u8 * pRomBase);
static void ROM_CheckSumMario();
static void ROM_CheckSumZelda();

RomInfo g_ROM;

static void ROM_CheckIniFile(LPROMINFO pRomInfo);
static BOOL ROM_LoadCartToMemory(LPCTSTR szFileName);
static HRESULT ROM_LoadDataFromFile(LPCTSTR szFileName, BYTE ** ppBuffer, DWORD * pdwLenToRead);
static void DumpROMInfo();
static BOOL ROM_ByteSwap(u8 * pBytes, DWORD dwLen);
static void ROM_ByteSwap_2301(void *v, DWORD numBytes);
static void ROM_ByteSwap_3210(void *v, DWORD numBytes);

inline DWORD SwapEndian(DWORD x)
{
	return ((x >> 24)&0x000000FF) |
		   ((x >> 8 )&0x0000FF00) |
		   ((x << 8 )&0x00FF0000) |
		   ((x << 24)&0xFF000000);
}


/*
-mario    = Mario-Type CIC Bootchip. (CIC-6102 or CIC-7101) 
-starf    = Starf-Type CIC Bootchip. (CIC-6101)
-lylat    = Lylat-Type CIC Bootchip. (CIC-7102)
-diddy    = Diddy-Type CIC Bootchip. (CIC-6103 or CIC-7103)
-yoshi    = Yoshi-Type CIC Bootchip. (CIC-6106 or CIC-7106)
-zelda    = Zelda-Type CIC Bootchip. (CIC-6105 or CIC-7105)
*/

#define MARIO_BOOT_CRC 0xb9c47dc8		// CIC-6102 or CIC-7101
#define STARF_BOOT_CRC 0xb4086651		// CIC-6101
#define LYLAT_BOOT_CRC 0xb3d6a525		// CIC-7102				// Lylat - Entyrpoint = 0x80000480
#define BANJO_BOOT_CRC 0xedce5ad9		// CIC-6103 or CIC-7103 // Diddy - Entrypoint + 0x00100000 
#define YOSHI_BOOT_CRC 0x06d8ed9c		// CIC-6106 or CIC-7106 // Yoshi - Entrypoint + 0x00200000 
#define ZELDA_BOOT_CRC 0xb53c588a		// CIC-6105 or CIC-7105

// Rom uses Unknown boot (0x51ae9f98)
// Rom uses Unknown boot (0x8dbba989)

#define MARIO_BOOT_CIC 0x3f
#define STARF_BOOT_CIC 0x3f // Incorrect
#define LYLAT_BOOT_CIC 0x3f // Incorrect
#define BANJO_BOOT_CIC 0x78
#define YOSHI_BOOT_CIC 0x85				// Same as FZero???
#define ZELDA_BOOT_CIC 0x91


const CountryIDInfo g_CountryCodeInfo[] = 
{
	{ 0, TEXT("0"), OS_TV_NTSC },
	{ '7', TEXT("Beta"), OS_TV_NTSC },
	{ 'A', TEXT("NTSC"), OS_TV_NTSC },
	{ 'D', TEXT("Germany"), OS_TV_PAL },
	{ 'E', TEXT("USA"), OS_TV_NTSC },
	{ 'F', TEXT("France"), OS_TV_PAL },
	{ 'I', TEXT("Italy"), OS_TV_PAL },
	{ 'J', TEXT("Japan"), OS_TV_NTSC },
	{ 'P', TEXT("Europe"), OS_TV_PAL },
	{ 'S', TEXT("Spain"), OS_TV_PAL },
	{ 'U', TEXT("Australia"), OS_TV_PAL },
	{ 'X', TEXT("PAL"), OS_TV_PAL },
	{ 'Y', TEXT("PAL"), OS_TV_PAL },
	{ -1, NULL, OS_TV_NTSC }
};

///////////////////////////////
//

void DumpROMInfo()
{	
	DBGConsole_Msg(0, "First DWORD:     0x%02x%02x%02x%02x", g_ROM.rh.x1, g_ROM.rh.x2, g_ROM.rh.x3, g_ROM.rh.x4);
	DBGConsole_Msg(0, "Clockrate:       0x%08x", g_ROM.rh.dwClockRate);
	DBGConsole_Msg(0, "Program Counter: 0x%08x", SwapEndian(g_ROM.rh.dwBootAddressOffset));
	DBGConsole_Msg(0, "Release:         0x%08x", g_ROM.rh.dwRelease);
	DBGConsole_Msg(0, "CRC1:            0x%08x", g_ROM.rh.dwCRC1);
	DBGConsole_Msg(0, "CRC2:            0x%08x", g_ROM.rh.dwCRC2);
	//DBGConsole_Msg(0, "Unknown64:       0x%08x%08x", *((DWORD *)&g_ROM.rh.qwUnknown1 + 0),  *((DWORD *)&g_ROM.rh.qwUnknown1 + 1));
	DBGConsole_Msg(0, "ImageName:       '%s'", g_ROM.rh.szName);
	DBGConsole_Msg(0, "Unknown32:       0x%08x", g_ROM.rh.dwUnknown2);
	DBGConsole_Msg(0, "Manufacturer:    0x%02x", g_ROM.rh.nManufacturer);
	DBGConsole_Msg(0, "CartID:          0x%04x", g_ROM.rh.wCartID);
	DBGConsole_Msg(0, "CountryID:       0x%02x - '%c'", g_ROM.rh.nCountryID, (char)g_ROM.rh.nCountryID);
}

static void ROM_RunPIFBoot(u32 cic)
{
	CPU_SetPC(0xBFC00000);

	// Set up CIC value in 0xbfc007e4
	*(DWORD*)((BYTE*)g_pMemoryBuffers[MEM_PI_RAM] + 0x24) = cic << 8;
}

static void ROM_SimulatePIFBoot(u32 cic, u32 nSystemType)
{
   // g_qwCPR[0][C0_SR]		= 0x34000000;	//*SR_FR |*/ SR_ERL | SR_CU2|SR_CU1|SR_CU0;
	R4300_SetSR(0x34000000);
	g_qwCPR[0][C0_CONFIG]	= 0x0006E463;	// 0x00066463;	

	// These are the effects of the PIF Boot Rom:

	g_qwGPR[REG_v0] = 0xFFFFFFFFD1731BE9LL;	// Required by Zelda, ignored by other boot codes
	g_qwGPR[REG_v1]	= 0xFFFFFFFFD1731BE9LL;
	g_qwGPR[REG_a0]	= 0x0000000000001BE9LL;
	g_qwGPR[REG_a1]	= 0xFFFFFFFFF45231E5LL;
	g_qwGPR[REG_a2]	= 0xFFFFFFFFA4001F0CLL;
	g_qwGPR[REG_a3]	= 0xFFFFFFFFA4001F08LL;
	g_qwGPR[REG_t0]	= 0x00000000000000C0LL;
	g_qwGPR[REG_t1]	= 0x0000000000000000LL;
	g_qwGPR[REG_t2]	= 0x0000000000000040LL;
	g_qwGPR[REG_t3] = 0xFFFFFFFFA4000040LL;	// Required by Zelda, ignored by other boot codes
	g_qwGPR[REG_t4]	= 0xFFFFFFFFD1330BC3LL;
	g_qwGPR[REG_t5]	= 0xFFFFFFFFD1330BC3LL;
	g_qwGPR[REG_t6]	= 0x0000000025613A26LL;
	g_qwGPR[REG_t7]	= 0x000000002EA04317LL;
	g_qwGPR[REG_t8]	= 0x0000000000000000LL;
	g_qwGPR[REG_t9]	= 0xFFFFFFFFD73F2993LL;

	g_qwGPR[REG_s1]	= 0x0000000000000000LL;
	g_qwGPR[REG_s2]	= 0x0000000000000000LL;
	g_qwGPR[REG_s3]	= 0x0000000000000000LL;
	g_qwGPR[REG_s4] = nSystemType;
	g_qwGPR[REG_s5] = 0x0000000000000000LL;
    g_qwGPR[REG_s6] = cic;
    g_qwGPR[REG_s7] = 6;
	g_qwGPR[REG_s8] = 0x0000000000000000LL;
	g_qwGPR[REG_sp] = 0xFFFFFFFFA4001FF0LL;
	//g_qwGPR[REG_ra]	= 0xFFFFFFFFA4000040;????
	g_qwGPR[REG_ra]	= 0xFFFFFFFFA4001554LL;
	// Copy low 1000 bytes to MEM_SP_MEM
	memcpy((BYTE*)g_pMemoryBuffers[MEM_SP_MEM], 
		   (BYTE*)g_pMemoryBuffers[MEM_CARTROM],
		   0x1000);

	// Also need to copy crap to SP_IMEM!

	CPU_SetPC(0xA4000040);
}

/*
<_Demo_>            lui t1,0x3400
<_Demo_>            mtc0 t1,status
<_Demo_>            lui t1,0x0006
<_Demo_>            ori t1,0xe463
<_Demo_>            dw 0x40898000
<_Demo_>             li     at,$00000000
<_Demo_>             li     v0,$D1731BE9
<_Demo_>             li     v1,$D1731BE9
<_Demo_>             li     a0,$00001BE9
<_Demo_>             li     a1,$F45231E5
<_Demo_>             li     a2,$A4001F0C
<_Demo_>             li     a3,$A4001F08
<_Demo_>             li     t0,$000000C0
<_Demo_>             li     t1,$00000000
<_Demo_>             li     t2,$00000040
<_Demo_>             li     t3,$0A4000040
<_Demo_>             li     t4,$D1330BC3
<_Demo_>             li     t5,$D1330BC3
<_Demo_>             li     t6,$25613A26
<_Demo_>             li     t7,$2EA04317
<_Demo_>             li     s0,$00000000
<_Demo_>             li     s1,$00000000
<_Demo_>             li     s2,$00000000
<_Demo_>             li     s3,$00000000
<_Demo_>             li     s4,$00000001
<_Demo_>             li     s5,$00000000
<_Demo_>             li     s6,$00000000
<_Demo_>             li     s7,$00000000
<_Demo_>             li     t8,$00000000
<_Demo_>             li     t9,$D73F2993
<_Demo_>             li     k0,$00000000
<_Demo_>             li     k1,$00000000
<_Demo_>             li     gp,$00000000
<_Demo_>             li     sp,$A4001FF0
<_Demo_>             li     s8,$00000000
<_Demo_>             li     ra,$A4001554
<_Demo_>             li     s6,$91
<_Demo_>             li     s7,$6
<_Demo_>             jr     t3
*/
/* A test running the pif rom with different values for the cic
cic: 0x00000000
r0:00000000 t0:000000f0 s0:00000000 t8:00000000
at:00000000 t1:00000000 s1:00000000 t9:d73f2993
v0:d1731be9 t2:00000040 s2:00000000 k0:00000000
v1:d1731be9 t3:a4000000 s3:00000000 k1:00000000
a0:00001be9 t4:d1330bc3 s4:00000001 gp:00000000
a1:f45231e5 t5:d1330bc3 s5:00000000 sp:a4001ff0
a2:a4001f0c t6:25613a26 s6:00000000 s8:00000000
a3:a4001f08 t7:2ea04317 s7:00000000 ra:a4001550
  
0xcdcdcdcd
r0:00000000 t0:000000f0 s0:00000000 t8:00000002
at:00000001 t1:00000000 s1:00000000 t9:688076ac
v0:57ab3f3c t2:00000040 s2:00000000 k0:00000000
v1:57ab3f3c t3:a4000000 s3:00000001 k1:00000000
a0:00003f3c t4:1c9fac27 s4:00000001 gp:00000000
a1:05825895 t5:112fe29d s5:00000000 sp:a4001ff0
a2:a4001f0c t6:191df4b2 s6:000000cd s8:00000000
a3:a4001f08 t7:eafcd1fc s7:00000001 ra:a4001550

0xffffffff
r0:00000000 t0:000000f0 s0:00000000 t8:00000000
at:00000000 t1:00000000 s1:00000000 t9:a4e54e8c
v0:4a882c3f t2:00000040 s2:00000000 k0:00000000
v1:4a882c3f t3:a4000000 s3:00000001 k1:00000000
a0:00002c3f t4:4ceb6550 s4:00000001 gp:00000000
a1:6848e133 t5:4ceb6550 s5:00000001 sp:a4001ff0
a2:a4001f0c t6:24a38463 s6:000000ff s8:00000000
a3:a4001f08 t7:3257a25f s7:00000001 ra:a4001550

<icepir8> reg values after PIF ROM has executed
<icepir8> 		gReg.r[r0] = 0;
<icepir8> 		gReg.r[at] = 0;
<icepir8> 		gReg.r[v0] = 0xffffffffd1731be9;
<icepir8> 		gReg.r[v1] = 0xffffffffd1731be9;
<icepir8> 		gReg.r[a0] = 0x01be9;
<icepir8> 		gReg.r[a1] = 0xfffffffff45231e5;
<icepir8> 		gReg.r[a2] = 0xffffffffa4001f0c;
<icepir8> 		gReg.r[a3] = 0xffffffffa4001f08;
<icepir8> 		gReg.r[t0] = 0x070;
<icepir8> 		gReg.r[t1] = 0;
<icepir8> 		gReg.r[t2] = 0x040;
<icepir8> 		gReg.r[t3] = 0xffffffffa4000040;
<icepir8> 		gReg.r[t4] = 0xffffffffd1330bc3;
<icepir8> 		gReg.r[t5] = 0xffffffffd1330bc3;
<icepir8> 		gReg.r[t6] = 0x025613a26;
<icepir8> 		gReg.r[t7] = 0x02ea04317;
<icepir8> 		gReg.r[s0] = 0;
<icepir8> 		gReg.r[s1] = 0;
<icepir8> 		gReg.r[s2] = 0;
<icepir8> 		gReg.r[s3] = 0;
<icepir8> 		gReg.r[s4] = rominfo.TV_System;
<icepir8> 		gReg.r[s5] = 0;
<icepir8> 		gReg.r[s6] = rominfo.CIC;
<icepir8> 		gReg.r[s7] = 0x06;
<icepir8> 		gReg.r[t8] = 0;
<icepir8> 		gReg.r[t9] = 0xffffffffd73f2993;
<icepir8> 		gReg.r[k0] = 0;
<icepir8> 		gReg.r[k1] = 0;
<icepir8> 		gReg.r[gp] = 0;
<icepir8> 		gReg.r[sp] = 0xffffffffa4001ff0;
<icepir8> 		gReg.r[s8] = 0;
<icepir8> 		gReg.r[ra] = 0xffffffffa4001554;
*/


void ROM_ReBoot()
{
	u32 crc;
	u32 cic;
	u32 nTvType;
	u8 * pBase;
	DWORD dwBase;
	LONG i;

	// Do this here - probably not necessary as plugin loaded/unloaded with audio thread
	/*KOS
	if (g_pAiPlugin != NULL)
		g_pAiPlugin->RomClosed();
	if (g_pGfxPlugin != NULL)
		g_pGfxPlugin->RomClosed();
    */

	// Reset here
	CPU_Reset(0xbfc00000);
	SR_Reset();
	RSP_Reset();
	RDP_Reset();
	if (g_ROM.bExpansionPak)
		Memory_Reset(MEMORY_8_MEG);		// Later set this to rom specific size
	else
		Memory_Reset(MEMORY_4_MEG);
	Patch_Reset();
	Controller_Initialise();

	// CRC from Cart + 0x40 for 0x1000 - 0x40 bytes
	dwBase = InternalReadAddress(0xb0000000, (void**)&pBase);
	if (dwBase != MEM_UNUSED)
	{
		crc = ROM_CrcBootCode(pBase);
		switch (crc)
		{
		case MARIO_BOOT_CRC: cic = MARIO_BOOT_CIC;  DBGConsole_Msg(0, "[YRom uses Mario boot]"); break;
		case STARF_BOOT_CRC: cic = LYLAT_BOOT_CIC;  DBGConsole_Msg(0, "[YRom uses Starfox boot]"); break;
		case LYLAT_BOOT_CRC: cic = LYLAT_BOOT_CIC;  DBGConsole_Msg(0, "[YRom uses Lylat boot]"); break;
		case YOSHI_BOOT_CRC: cic = YOSHI_BOOT_CIC;  DBGConsole_Msg(0, "[YRom uses Yoshi boot]"); break;
		case ZELDA_BOOT_CRC: cic = ZELDA_BOOT_CIC;  DBGConsole_Msg(0, "[YRom uses Zelda boot]"); break;
		case BANJO_BOOT_CRC: cic = BANJO_BOOT_CIC;  DBGConsole_Msg(0, "[YRom uses Banjo boot]"); break;
		default:			 cic = MARIO_BOOT_CIC;  DBGConsole_Msg(0, "[YRom uses Unknown boot (0x%08x)]", crc); break;
		}
	}
	else
	{
		crc = MARIO_BOOT_CRC;
		cic = MARIO_BOOT_CIC;
	}

	// Switch again, doing boot specific stuff
	g_bSkipFirstRSP = FALSE;
	switch (crc)
	{
	case MARIO_BOOT_CRC:
		ROM_CheckSumMario();
		break;
	case ZELDA_BOOT_CRC:
		g_bSkipFirstRSP = TRUE;
		ROM_CheckSumZelda();
		break;
	}

	//ROM_RunPIFBoot(cic);

	nTvType = OS_TV_NTSC;
	for (i = 0; g_CountryCodeInfo[i].szName != NULL; i++)
	{
		if (g_CountryCodeInfo[i].nCountryID == g_ROM.rh.nCountryID)
		{
			nTvType = g_CountryCodeInfo[i].nTvType;
			break;
		}
	}


	//ROM_RunPIFBoot(cic);
	ROM_SimulatePIFBoot(cic, nTvType);		

    /*KOS
	if (g_pGfxPlugin != NULL)
		g_pGfxPlugin->RomOpen();
	*/


	DBGConsole_UpdateDisplay();
}

void ROM_Unload()
{
	Memory_Cleanup();
	Controller_Finalise();
}



// Copy across text, null terminate, and strip spaces
void ROM_GetRomNameFromHeader(TCHAR * szName, ROMHeader * pHdr)
{
	TCHAR * p;

	memcpy(szName, pHdr->szName, 20);
	szName[20] = '\0';

	p = szName + (lstrlen(szName) -1);		// -1 to skip null
	while (p >= szName && *p == ' ')
	{
		*p = 0;
		p--;
	}
}

BOOL ROM_LoadFile(LPCTSTR szFileName)
{
	ROM_Unload();

	DBGConsole_Msg(0, "Reading rom image: [C%s]", szFileName);
	
	if (!ROM_LoadCartToMemory(szFileName))
	{
		DPF(DEBUG_INFO, "ROM_LoadCartToMemory(%s) Failed", szFileName);
		DBGConsole_Msg(0, "ROM_LoadCartToMemory(%s) Failed", szFileName);
		return FALSE;
	}

	lstrcpyn(g_ROM.szFileName, szFileName, MAX_PATH);
	
	// Get information about the rom header
	memcpy(&g_ROM.rh, g_pMemoryBuffers[MEM_CARTROM], sizeof(ROMHeader));
	ROM_ByteSwap_3210( &g_ROM.rh, sizeof(ROMHeader) );

	ROM_GetRomNameFromHeader(g_ROM.szGameName, &g_ROM.rh);
	DumpROMInfo();

	// Copy BootCode from ROM to DMEM
	g_ROM.u32BootAddres = SwapEndian(g_ROM.rh.dwBootAddressOffset);


	ROM_CheckIniFile(&g_ROM);


	if (g_ROM.bDisablePatches)
		g_bApplyPatches = FALSE;
	else
		g_bApplyPatches = TRUE;

	if (g_ROM.bDisableTextureCRC)
		g_bCRCCheck = FALSE;
	else
		g_bCRCCheck = TRUE;

	if (g_ROM.bDisableEeprom)
		g_bEepromPresent = FALSE;
	else
		g_bEepromPresent = TRUE;

	if (g_ROM.bIncTexRectEdge)
		g_bIncTexRectEdge = TRUE;
	else
		g_bIncTexRectEdge = FALSE;

	if (g_ROM.bDisableSpeedSync)
		g_bSpeedSync = FALSE;
	else
		g_bSpeedSync = TRUE;

	if (g_ROM.bDisableDynarec)
		g_bUseDynarec = FALSE;
	else
		g_bUseDynarec = TRUE;

	DBGConsole_Msg(0, "[G%s]", g_ROM.szGameName);
	DBGConsole_Msg(0,"This game has been certified as [G%s] (%s)",
		g_ROM.szComment, g_ROM.szInfo);

	DBGConsole_Msg(0, "ApplyPatches: [G%s]", g_bApplyPatches ? "on" : "off");
	DBGConsole_Msg(0, "Texture CRC Check: [G%s]", g_bCRCCheck ? "on" : "off");
	DBGConsole_Msg(0, "Eeprom: [G%s]", g_bEepromPresent ? "on" : "off");
	DBGConsole_Msg(0, "Inc Text Rect Edge: [G%s]", g_bIncTexRectEdge ? "on" : "off");
	DBGConsole_Msg(0, "SpeedSync: [G%s]", g_bSpeedSync ? "on" : "off");
	DBGConsole_Msg(0, "DynaRec: [G%s]", g_bUseDynarec ? "on" : "off");

	//Patch_ApplyPatches();

	ROM_ReBoot();

	if (g_bRunAutomatically)
	{
		CHAR szReason[300+1];
		// Ignore failure
		StartCPUThread(szReason, 300);
	}

	return TRUE;
}


// Initialise the RomInfo structure
BOOL ROM_InitRomInfo(LPROMINFO pRomInfo)
{
	ROMHeader * prh;
	BYTE * pBytes;
	HRESULT hr;
	DWORD dwRomSize;
	u32 crc;

	// Set rom size to rom headers size to indicate just this bit of file
	// should be read
	dwRomSize = 0x1000;
		
	hr = ROM_LoadDataFromFile(pRomInfo->szFileName,
								  (LPBYTE*)&pBytes,
								  &dwRomSize);
	if (FAILED(hr))
	{
		DBGConsole_Msg(0, "[CUnable to get rom info from %s]", pRomInfo->szFileName);
		return FALSE;
	}

	prh = (ROMHeader *)pBytes;

	if (!ROM_ByteSwap(pBytes, 0x1000))
	{
		DBGConsole_Msg(0, "[CUnknown ROM format for %s: 0x%02x %02x %02x %02x",
			pRomInfo->szFileName, pBytes[0], pBytes[1], pBytes[2], pBytes[3]);
		return FALSE;
	}

	ROM_ByteSwap_3210( prh, sizeof(ROMHeader) );

	crc = ROM_CrcBootCode(pBytes);
	switch (crc)
	{
	case MARIO_BOOT_CRC:	lstrcpyn(pRomInfo->szBootName, "Mario", 20); break;
	case STARF_BOOT_CRC:	lstrcpyn(pRomInfo->szBootName, "Starf", 20); break;
	case LYLAT_BOOT_CRC:	lstrcpyn(pRomInfo->szBootName, "Lylat", 20); break;
	case YOSHI_BOOT_CRC:	lstrcpyn(pRomInfo->szBootName, "Yoshi", 20); break;
	case ZELDA_BOOT_CRC:	lstrcpyn(pRomInfo->szBootName, "Zelda", 20); break;
	case BANJO_BOOT_CRC:	lstrcpyn(pRomInfo->szBootName, "Banjo", 20); break;
	default:				lstrcpyn(pRomInfo->szBootName, "Unknown", 20);
		DBGConsole_Msg(0, "%s has unknown BootRom: (0x%08x)", pRomInfo->szFileName, crc);
		break;
	}

	pRomInfo->rh = *prh;
	pRomInfo->u32BootAddres = SwapEndian(pRomInfo->rh.dwBootAddressOffset);

	lstrcpyn(pRomInfo->szComment, "No Comment", 100);

	pRomInfo->dwRomSize = dwRomSize;
	pRomInfo->nCountryID = prh->nCountryID;

	ROM_GetRomNameFromHeader(pRomInfo->szGameName, prh);

	// Initialise comments, other stuff
	ROM_CheckIniFile(pRomInfo);

	delete [] pBytes;

	return TRUE;
}


// Read a cartridge into memory. this routine byteswaps if necessary
BOOL ROM_LoadCartToMemory(LPCTSTR szFileName)
{
	HRESULT hr;
	DWORD dwROMSize;
	BYTE *pBytes;

	Memory_FreeCart();

	dwROMSize = 0;
	hr = ROM_LoadDataFromFile(szFileName,
							  (LPBYTE*)&pBytes,
							  &dwROMSize);
	if (FAILED(hr))
	{
		DBGConsole_Msg(0, "[CUnable to get rom info from %s]", szFileName);
		return FALSE;
	}

	if (!ROM_ByteSwap(pBytes, dwROMSize))
	{
		DBGConsole_Msg(0, "[CUnknown ROM format for %s: 0x%02x %02x %02x %02x",
			szFileName, pBytes[0], pBytes[1], pBytes[2], pBytes[3]);

		delete []pBytes;
		return FALSE;
	}

	Memory_SetCart(pBytes, dwROMSize);

	return TRUE;
}




// Load specified number of bytes from the rom
// Allocates memory and returns pointer
// Byteswapping is handled elsewhere
// Pass in length to read in pdwLenToRead. (0 for entire rom)
// Actual length read returned in pdwLenToRead
HRESULT ROM_LoadDataFromFile(LPCTSTR szFileName, BYTE ** ppBuffer, DWORD * pdwLenToRead)
{
	HANDLE hFile;
	BOOL bSuccess;
	DWORD dwNumRead;
	DWORD dwLenToRead;
	BYTE * pBytes;
	DWORD dwFileSize;

	dwLenToRead = * pdwLenToRead;

	*pdwLenToRead = 0;
	*ppBuffer = NULL;

    FILE *fp = fopen( szFileName, "rb" );

	if( fp == NULL )
	{ 
		return E_FAIL;
	}

	// If length to read is unspecified, use entire filesize
	fseek( fp, 0L, SEEK_END );
	dwFileSize = ftell( fp );
	fseek( fp, 0L, SEEK_SET );

	if (dwLenToRead == 0)
		dwLenToRead = dwFileSize;

	if (dwLenToRead == ~0)
	{
		fclose( fp );
		return E_FAIL;
	}

	// Now, allocate memory for rom. 
	pBytes = new BYTE[dwLenToRead];
	if (pBytes == NULL)
	{
		fclose( fp );
		return E_OUTOFMEMORY;
	}

	// Try and read in data
	dwNumRead = fread( (void *)pBytes, 1, dwLenToRead, fp );
	printf( "%d - %d\n", dwNumRead,dwLenToRead );
	if (dwNumRead != dwLenToRead)
	{
		fclose( fp );
		delete [] pBytes;
		return E_FAIL;
	}

	// We're done with it now
	fclose( fp );

	*ppBuffer = pBytes;
	*pdwLenToRead = dwFileSize;

	return S_OK;
}


BOOL ROM_ByteSwap(u8 * pBytes, DWORD dwLen)
{
	u32 nSig;

	nSig = *(u32*)pBytes;

	switch (nSig)
	{
	case 0x80371240:
		// Pre byteswapped - no need to do anything
		break;
	case 0x40123780:
		ROM_ByteSwap_3210(pBytes, dwLen);
		break;
	case 0x12408037:
		ROM_ByteSwap_2301(pBytes, dwLen);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}


// Swap bytes from 37 80 40 12
// to              40 12 37 80
void ROM_ByteSwap_2301(void *v, DWORD dwLen)
{
    BYTE *rom = (BYTE *)v;
	for( int i=0; i<(dwLen/4); i++)
	{
	   BYTE tmp=rom[i*4];
	   rom[i*4]=rom[i*4+2];
	   rom[i*4+2]=tmp;
	   tmp=rom[i*4+1];
	   rom[i*4+1]=rom[i*4+3];
	   rom[i*4+3]=tmp;
	}
	
}


// Swap bytes from 80 37 12 40
// to              40 12 37 80
void ROM_ByteSwap_3210(void *v, DWORD dwLen)
{
    BYTE *rom = (BYTE *)v;
	for( int i=0; i<(dwLen/4); i++)
	{
	   BYTE tmp=rom[i*4];
	   rom[i*4]=rom[i*4+3];
	   rom[i*4+3]=tmp;
	   tmp=rom[i*4+1];
	   rom[i*4+1]=rom[i*4+2];
	   rom[i*4+2]=tmp;
	}
}


u32 ROM_CrcBootCode(u8 * pRomBase)
{
	return daedalus_crc32(0, pRomBase + 0x40, 0x1000 - 0x40);
}

void ROM_CheckSumMario()
{

	DWORD * pdwBase;
	DWORD dwAddress;
	DWORD a1;
	DWORD t7;
	DWORD v1 = 0;
	DWORD t0 = 0;
	DWORD v0 = 0xF8CA4DDC; //(MARIO_BOOT_CIC * 0x5d588b65) + 1;
	DWORD a3 = 0xF8CA4DDC;
	DWORD t2 = 0xF8CA4DDC;
	DWORD t3 = 0xF8CA4DDC;
	DWORD s0 = 0xF8CA4DDC;
	DWORD a2 = 0xF8CA4DDC;
	DWORD t4 = 0xF8CA4DDC;
	DWORD t8, t6, a0;


	pdwBase = (DWORD *)g_pMemoryBuffers[MEM_CARTROM];


	DBGConsole_Msg(0, "");		// Blank line

	for (dwAddress = 0; dwAddress < 0x00100000; dwAddress+=4)
	{
		if ((dwAddress % 0x2000) == 0)
		{
			DBGConsole_MsgOverwrite(0, "Generating CRC [M%d / %d]",
				dwAddress, 0x00100000);
		}


		v0 = pdwBase[(dwAddress + 0x1000)>>2];
		v1 = a3 + v0;
		a1 = v1;
		if (v1 < a3) 
			t2++;
	
		v1 = v0 & 0x001f;
		t7 = 0x20 - v1;
		t8 = (v0 >> (t7&0x1f));
		t6 = (v0 << (v1&0x1f));
		a0 = t6 | t8;

		a3 = a1;
		t3 ^= v0;
		s0 += a0;
		if (a2 < v0)
			a2 ^= a3 ^ v0;
		else
			a2 ^= a0;

		t0 += 4;
		t7 = v0 ^ s0;
		t4 += t7;
	}
	DBGConsole_MsgOverwrite(0, "Generating CRC [M%d / %d]",
		0x00100000, 0x00100000);


	a3 ^= t2 ^ t3;	// CRC1
	s0 ^= a2 ^ t4;	// CRC2

	if (a3 != pdwBase[0x10>>2] || s0 != pdwBase[0x14>>2])
	{
		DBGConsole_Msg(0, "[MWarning, CRC values don't match, fixing]");

		pdwBase[0x10>>2] = a3;
		pdwBase[0x14>>2] = s0;
	}


}

// Thanks Lemmy!
void ROM_CheckSumZelda()
{
	DWORD * pdwBase;
	DWORD dwAddress;
	DWORD dwAddress2;
	DWORD t5 = 0x00000020;
	DWORD a3 = 0xDF26F436;
	DWORD t2 = 0xDF26F436;
	DWORD t3 = 0xDF26F436;
	DWORD s0 = 0xDF26F436;
	DWORD a2 = 0xDF26F436;
	DWORD t4 = 0xDF26F436;
	DWORD v0, v1, a1, a0;


	pdwBase = (DWORD *)g_pMemoryBuffers[MEM_CARTROM];

	dwAddress2 = 0;

	DBGConsole_Msg(0, "");		// Blank line

	for (dwAddress = 0; dwAddress < 0x00100000; dwAddress += 4)
	{
		if ((dwAddress % 0x2000) == 0)
			DBGConsole_MsgOverwrite(0, "Generating CRC [M%d / %d]",
				dwAddress, 0x00100000);


		v0 = pdwBase[(dwAddress + 0x1000)>>2];
		v1 = a3 + v0;
		a1 = v1;
		
		if (v1 < a3)
			t2++;

		v1 = v0 & 0x1f;
		a0 = (v0 >> (t5-v1)) | (v0 << v1);
		a3 = a1;
		t3 = t3 ^ v0;
		s0 += a0;
		if (a2 < v0)
			a2 ^= a3 ^ v0;
		else
			a2 ^= a0;

		t4 += pdwBase[(dwAddress2 + 0x750)>>2] ^ v0;
		dwAddress2 = (dwAddress2 + 4) & 0xFF;

	}

	a3 ^= t2 ^ t3;
	s0 ^= a2 ^ t4;

	if (a3 != pdwBase[0x10>>2] || s0 != pdwBase[0x14>>2])
	{
		DBGConsole_Msg(0, "[MWarning, CRC values don't match, fixing]");

		pdwBase[0x10>>2] = a3;
		pdwBase[0x14>>2] = s0;
	}
	DBGConsole_MsgOverwrite(0, "Generating CRC [M%d / %d]",
		0x00100000, 0x00100000);

}





void ROM_CheckIniFile(LPROMINFO pRomInfo)
{
	LONG i;
	pRomInfo->ucode = 0;
	pRomInfo->bDisablePatches		= TRUE;
	pRomInfo->bDisableTextureCRC	= TRUE;
	pRomInfo->bDisableEeprom		= FALSE;
	pRomInfo->bIncTexRectEdge		= FALSE;
	pRomInfo->bDisableSpeedSync		= TRUE;
	pRomInfo->bDisableDynarec		= TRUE;
	pRomInfo->bExpansionPak			= FALSE;

	pRomInfo->dwEepromSize			= 2048;
	pRomInfo->dwRescanCount			= 0;

/*	i = g_pIniFile->FindEntry(pRomInfo->rh.dwCRC1,
							  pRomInfo->rh.dwCRC2,
							  pRomInfo->rh.nCountryID,
							  pRomInfo->szGameName);

	pRomInfo->ucode = g_pIniFile->sections[i].ucode;

	lstrcpyn(pRomInfo->szGameName, g_pIniFile->sections[i].name, 50);
	lstrcpyn(pRomInfo->szComment, g_pIniFile->sections[i].comment, 100);
	lstrcpyn(pRomInfo->szInfo, g_pIniFile->sections[i].info, 100);

	pRomInfo->bDisablePatches		= g_pIniFile->sections[i].bDisablePatches;
	pRomInfo->bDisableTextureCRC	= g_pIniFile->sections[i].bDisableTextureCRC;
	pRomInfo->bDisableEeprom		= g_pIniFile->sections[i].bDisableEeprom;
	pRomInfo->bIncTexRectEdge		= g_pIniFile->sections[i].bIncTexRectEdge;
	pRomInfo->bDisableSpeedSync		= g_pIniFile->sections[i].bDisableSpeedSync;
	pRomInfo->bDisableDynarec		= g_pIniFile->sections[i].bDisableDynarec;
	pRomInfo->bExpansionPak			= g_pIniFile->sections[i].bExpansionPak;

	pRomInfo->dwEepromSize			= g_pIniFile->sections[i].dwEepromSize;
	pRomInfo->dwRescanCount			= g_pIniFile->sections[i].dwRescanCount;*/
}
