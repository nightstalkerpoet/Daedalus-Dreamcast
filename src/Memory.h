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

#ifndef __MEMORY_H__
#define __MEMORY_H__

#include "windef.h"
#include "DBGConsole.h"

#include "ultra_rcp.h"
#include "ultra_types.h"



enum MEMBANKTYPE
{
	MEM_UNUSED = 0,			// 8 - simplifies code so that we don't have to check for illegal memory accesses

	MEM_RD_RAM,				// 8 or 4 Mb (4/8*1024*1024)
	
	MEM_SP_MEM ,			// 0x2000

	MEM_SP_PC_REG,			// 0x04
	MEM_SP_IBIST_REG,		// 0x04

	MEM_PI_ROM,				// 0x7C0
	MEM_PI_RAM,				// 0x40

	MEM_RD_REG,				// 1 Mb
	MEM_SP_REG,				// 0x20
	MEM_DP_COMMAND_REG,		// 0x20
	MEM_MI_REG,				// 0x10
	MEM_VI_REG,				// 0x38
	MEM_AI_REG,				// 0x18
	MEM_PI_REG,				// 0x34
	MEM_RI_REG,				// 0x20
	MEM_SI_REG,				// 0x1C

	MEM_SRAM,				// 0x40000

	MEM_CARTROM,			// Variable

	NUM_MEM_BUFFERS
};

#define MEMORY_4_MEG (4*1024*1024)
#define MEMORY_8_MEG (8*1024*1024)


extern DWORD g_dwRamSize;
extern void *g_pMemoryBuffers[NUM_MEM_BUFFERS];


BOOL Memory_Init();
void Memory_Fini(void);
void Memory_Reset(DWORD dwMainMem);
void Memory_Cleanup();

void Memory_FreeCart();
void Memory_SetCart(u8 * pBytes, DWORD dwLen);


void Memory_SiDisable();
void Memory_SiEnable();

typedef void * (*MemFastFunction )(DWORD dwAddress);
typedef void (*MemWriteValueFunction )(DWORD dwAddress, DWORD dwValue);
typedef DWORD (*InternalMemFastFunction)(DWORD dwAddress, void ** pTranslated);


extern MemFastFunction * g_ReadAddressLookupTable;
extern MemFastFunction *g_WriteAddressLookupTable;
extern MemWriteValueFunction *g_WriteAddressValueLookupTable;
extern InternalMemFastFunction *InternalReadFastTable;
extern InternalMemFastFunction *InternalWriteFastTable;


#define ReadAddress(dwAddr)  (void *)(g_ReadAddressLookupTable)[(dwAddr)>>18](dwAddr)
#define WriteAddress(dwAddr)  (void *)(g_WriteAddressLookupTable)[(dwAddr)>>18](dwAddr)
#define WriteValueAddress(dwAddr, dwValue)  (g_WriteAddressValueLookupTable)[(dwAddr)>>18](dwAddr, dwValue)

#define InternalReadAddress(dwAddr, pTrans)  (InternalReadFastTable)[(dwAddr)>>18](dwAddr, pTrans)
#define InternalWriteAddress(dwAddr, pTrans)  (InternalWriteFastTable)[(dwAddr)>>18](dwAddr, pTrans)

inline u64 Read64Bits(DWORD dwAddress)
{
	u64 qwData = *(u64 *)ReadAddress(dwAddress);
	qwData = (qwData>>32) + (qwData<<32);
	return qwData;
}

inline u32 Read32Bits(const DWORD dwAddress)
{
	return *(u32 *)ReadAddress(dwAddress);
}

inline u16 Read16Bits(DWORD dwAddress)
{
	return *(u16 *)ReadAddress(dwAddress ^ 0x02);
}

inline BYTE Read8Bits(DWORD dwAddress)
{
	return *(u8 *)ReadAddress(dwAddress ^ 0x03);
}

// Useful defines for making code look nicer:
#define g_pu8RamBase ((u8*)g_pMemoryBuffers[MEM_RD_RAM])
#define g_ps8RamBase ((s8*)g_pMemoryBuffers[MEM_RD_RAM])
#define g_pu16RamBase ((u16*)g_pMemoryBuffers[MEM_RD_RAM])
#define g_pu32RamBase ((u32*)g_pMemoryBuffers[MEM_RD_RAM])

#define g_pu8SpMemBase ((u8*)g_pMemoryBuffers[MEM_SP_MEM])
#define g_ps8SpMemBase ((s8*)g_pMemoryBuffers[MEM_SP_MEM])
#define g_pu16SpMemBase ((u16*)g_pMemoryBuffers[MEM_SP_MEM])
#define g_pu32SpMemBase ((u32*)g_pMemoryBuffers[MEM_SP_MEM])


inline void Write64Bits(DWORD dwAddress, u64 qwData)
{
	*(u64 *)WriteAddress(dwAddress) = (qwData>>32) + (qwData<<32);
	//MemoryUpdate();
}

inline void Write32Bits(const DWORD dwAddress, u32 dwData)
{
	//*(u32 *)WriteAddress(dwAddress) = dwData;
	WriteValueAddress(dwAddress, dwData);
//	MemoryUpdate();
}

inline void Write16Bits(DWORD dwAddress, u16 wData)
{
	*(u16 *)WriteAddress(dwAddress ^ 0x2) = wData;	
	//MemoryUpdate();

}
inline void Write8Bits(DWORD dwAddress, u8 nData)
{
	*(u8 *)WriteAddress(dwAddress ^ 0x3) = nData;	
	//MemoryUpdate();
}

// Like Write16Bits, but not inline (for Dynamic Recomp)
void Memory_Write16Bits(const DWORD dwAddress, const u16 wData);





//////////////////////////////////////////////////////////////
// Quick Read/Write methods that require a base returned by
// ReadAddress or InternalReadAddress etc

inline u64 QuickRead64Bits(BYTE *pBase, DWORD dwOffset)
{
	u64 qwData = *(u64 *)(pBase + dwOffset);
	return (qwData>>32) + (qwData<<32);
}

inline u32 QuickRead32Bits(BYTE *pBase, DWORD dwOffset)
{
	return *(u32 *)(pBase + dwOffset);
}

inline void QuickWrite64Bits(BYTE *pBase, DWORD dwOffset, u64 qwValue)
{
	u64 qwData = (qwValue>>32) + (qwValue<<32);
	*(u64 *)(pBase + dwOffset) = qwData;
}

inline void QuickWrite32Bits(BYTE *pBase, DWORD dwOffset, u32 dwValue)
{
	*(u32 *)(pBase + dwOffset) = dwValue;
}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//               MI Register Macros                //
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////


inline void Memory_MI_SetRegister(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - MI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_MI_REG])[dwOffset] = dwValue;
}

inline void Memory_MI_SetRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - MI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_MI_REG])[dwOffset] |= dwValue;
}

inline void Memory_MI_ClrRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - MI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_MI_REG])[dwOffset] &= ~dwValue;
}

inline DWORD Memory_MI_GetRegister(DWORD dwReg)
{
	DWORD dwOffset = (dwReg - MI_BASE_REG) / 4;
	return ((DWORD *)g_pMemoryBuffers[MEM_MI_REG])[dwOffset];
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//               SP Register Macros                //
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////


inline void Memory_SP_SetRegister(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - SP_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_SP_REG])[dwOffset] = dwValue;
}

inline void Memory_SP_SetRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - SP_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_SP_REG])[dwOffset] |= dwValue;
}

inline void Memory_SP_ClrRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - SP_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_SP_REG])[dwOffset] &= ~dwValue;
}

inline DWORD Memory_SP_GetRegister(DWORD dwReg)
{
	DWORD dwOffset = (dwReg - SP_BASE_REG) / 4;
	return ((DWORD *)g_pMemoryBuffers[MEM_SP_REG])[dwOffset];
}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//               AI Register Macros                //
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////


inline void Memory_AI_SetRegister(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - AI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_AI_REG])[dwOffset] = dwValue;
}

inline void Memory_AI_SetRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - AI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_AI_REG])[dwOffset] |= dwValue;
}

inline void Memory_AI_ClrRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - AI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_AI_REG])[dwOffset] &= ~dwValue;
}

inline DWORD Memory_AI_GetRegister(DWORD dwReg)
{
	DWORD dwOffset = (dwReg - AI_BASE_REG) / 4;
	return ((DWORD *)g_pMemoryBuffers[MEM_AI_REG])[dwOffset];
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//               VI Register Macros                //
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////


inline void Memory_VI_SetRegister(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - VI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_VI_REG])[dwOffset] = dwValue;
}

inline void Memory_VI_SetRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - VI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_VI_REG])[dwOffset] |= dwValue;
}

inline void Memory_VI_ClrRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - VI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_VI_REG])[dwOffset] &= ~dwValue;
}

inline DWORD Memory_VI_GetRegister(DWORD dwReg)
{
	DWORD dwOffset = (dwReg - VI_BASE_REG) / 4;
	return ((DWORD *)g_pMemoryBuffers[MEM_VI_REG])[dwOffset];
}
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//               SI Register Macros                //
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////


inline void Memory_SI_SetRegister(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - SI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_SI_REG])[dwOffset] = dwValue;
}

inline void Memory_SI_SetRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - SI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_SI_REG])[dwOffset] |= dwValue;
}

inline void Memory_SI_ClrRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - SI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_SI_REG])[dwOffset] &= ~dwValue;
}

inline DWORD Memory_SI_GetRegister(DWORD dwReg)
{
	DWORD dwOffset = (dwReg - SI_BASE_REG) / 4;
	return ((DWORD *)g_pMemoryBuffers[MEM_SI_REG])[dwOffset];
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//               PI Register Macros                //
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////


inline void Memory_PI_SetRegister(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - PI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_PI_REG])[dwOffset] = dwValue;
}

inline void Memory_PI_SetRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - PI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_PI_REG])[dwOffset] |= dwValue;
}

inline void Memory_PI_ClrRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - PI_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_PI_REG])[dwOffset] &= ~dwValue;
}

inline DWORD Memory_PI_GetRegister(DWORD dwReg)
{
	DWORD dwOffset = (dwReg - PI_BASE_REG) / 4;
	return ((DWORD *)g_pMemoryBuffers[MEM_PI_REG])[dwOffset];
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//               DPC Register Macros               //
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////


inline void Memory_DPC_SetRegister(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - DPC_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_DP_COMMAND_REG])[dwOffset] = dwValue;
}

inline void Memory_DPC_SetRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - DPC_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_DP_COMMAND_REG])[dwOffset] |= dwValue;
}

inline void Memory_DPC_ClrRegisterBits(DWORD dwReg, DWORD dwValue)
{
	DWORD dwOffset = (dwReg - DPC_BASE_REG) / 4;
	((DWORD *)g_pMemoryBuffers[MEM_DP_COMMAND_REG])[dwOffset] &= ~dwValue;
}

inline DWORD Memory_DPC_GetRegister(DWORD dwReg)
{
	DWORD dwOffset = (dwReg - DPC_BASE_REG) / 4;
	return ((DWORD *)g_pMemoryBuffers[MEM_DP_COMMAND_REG])[dwOffset];
}

#endif
