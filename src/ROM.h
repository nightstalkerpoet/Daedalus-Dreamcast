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


#ifndef __DAEDALUS_ROM_H__
#define __DAEDALUS_ROM_H__

#include "ultra_types.h"
#include "IniFile.h"

#pragma pack(1)
struct ROMHeader
{
	BYTE  x1, x2, x3, x4;
	DWORD dwClockRate;
	DWORD dwBootAddressOffset;
	DWORD dwRelease;
	DWORD dwCRC1;
	DWORD dwCRC2;
	u64   qwUnknown1;
	char  szName[20];
	DWORD dwUnknown2;
	WORD  wUnknown3;
	BYTE  nUnknown4;
	BYTE  nManufacturer;
	WORD  wCartID;
	s8    nCountryID;
	BYTE  nUnknown5;
};
#pragma pack()

typedef struct 
{
	TCHAR	szFileName[MAX_PATH+1];

	// Other info from the rom. This is for convenience
	TCHAR	szGameName[50+1];
	TCHAR	szBootName[20+1];
	DWORD	dwRomSize;

	s8	nCountryID;

	// Copy of the ROM header
	ROMHeader	rh;
	u32			u32BootAddres;

	// Config stuff
	DWORD	ucode;
	TCHAR	szComment[100+1];
	TCHAR	szInfo[100+1];
	BOOL	bDisablePatches;
	BOOL	bDisableTextureCRC;
	BOOL	bDisableEeprom;
	BOOL	bIncTexRectEdge;
	BOOL	bDisableSpeedSync;
	BOOL	bDisableDynarec;
	BOOL	bExpansionPak;
	DWORD	dwEepromSize;
	DWORD	dwRescanCount;

	
} RomInfo, *LPROMINFO;

typedef struct
{
	s8	nCountryID;
	LPCTSTR szName;
	u32 nTvType;
} CountryIDInfo;


extern const CountryIDInfo g_CountryCodeInfo[];

extern RomInfo g_ROM;

void ROM_ReBoot();
void ROM_Unload();
BOOL ROM_LoadFile(LPCTSTR szFileName);

BOOL ROM_InitRomInfo(LPROMINFO pRomInfo);

#endif
