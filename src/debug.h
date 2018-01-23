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

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "main.h"
#include "windef.h"

#ifdef _DEBUG
#ifndef DEBUG
#define DEBUG
#endif
#endif


#ifdef DAEDALUS_LOG
// Pre and Post debug string info
#define START_STR	TEXT ("")
#define END_STR		TEXT ("\r\n")
#endif // DAEDALUS_LOG

// Debug Levels
#define DEBUG_ALWAYS	5L
#define DEBUG_CRITICAL	4L
#define DEBUG_ERROR		3L
#define DEBUG_MINOR		2L
#define DEBUG_WARN		1L
#define DEBUG_DETAILS	0L

#define DEBUG_INFO		1L

#define DEBUG_MEMWARN	10L



#define DEBUG_TEST_STUFF	20L

#define DEBUG_PI		21L
#define DEBUG_SP		22L
#define DEBUG_DP		23L
#define	DEBUG_MI		24L
#define DEBUG_SI		25L
#define DEBUG_VI		26L
#define DEBUG_AI		27L

#define DEBUG_MEMORY	30L
#define DEBUG_MEMORYREAD 31L
#define DEBUG_MEMORYWRITE 32L
#define DEBUG_TLB		33L

#define DEBUG_ERET		41L
#define DEBUG_BREAK		42L

#define DEBUG_REGS		50L
#define DEBUG_CAUSE		51L
#define DEBUG_COMPARE	52L
#define DEBUG_STATUS	53L
#define DEBUG_COUNT		54L
#define DEBUG_OPS		55L


#define DEBUG_INTRS		60L
#define DEBUG_INTR_VI	61L
#define DEBUG_INTR_AI	62L
#define DEBUG_INTR_PI	63L
#define DEBUG_INTR_SP	64L
#define DEBUG_INTR_DP	65L
#define DEBUG_INTR_SI	66L
#define DEBUG_INTR_COMPARE 67L

#define DEBUG_DSOUND	80L

#define DEBUG_MEMORY_RDRAM_REG 1000
#define DEBUG_MEMORY_SP_DMEM	1001
#define DEBUG_MEMORY_SP_IMEM	1002
#define DEBUG_MEMORY_SP_REG		1003
#define DEBUG_MEMORY_MI			1004
#define DEBUG_MEMORY_VI			1005
#define DEBUG_MEMORY_AI			1006
#define DEBUG_MEMORY_RI			1007
#define DEBUG_MEMORY_SI			1008
#define DEBUG_MEMORY_PI			1009
#define DEBUG_MEMORY_PIF		1010
#define DEBUG_MEMORY_DP			1011

#define DEBUG_DYNREC_STOPPAGE	2000
#define DEBUG_DYNREC			2001
#define DEBUG_THANDLER			2002
#define DEBUG_OS				2003
#define DEBUG_UCODE				2004


#define DEBUG_DP_AUD			2005
#define DEBUG_DP_GFX			2006
/*
**-----------------------------------------------------------------------------
**  Macros
**-----------------------------------------------------------------------------
*/

#ifdef DAEDALUS_LOG
    #define DPF dprintf
    #define ASSERT(x) \
        if (! (x)) \
        { \
            DPF (DEBUG_ALWAYS, TEXT("Assertion violated: %s, File = %s, Line = #%ld\n"), \
                 TEXT(#x), TEXT(__FILE__), (DWORD)__LINE__ ); \
            abort (); \
        }        

   #define REPORTERR(x) \
        DPF (DEBUG_ALWAYS, TEXT("File = %s, Line = #%ld\n"), \
             TEXT(#x), TEXT(__FILE__), (DWORD)__LINE__ ); \

   #define FATALERR(x) \
        DPF (DEBUG_ALWAYS, TEXT("File = %s, Line = #%ld\n"), \
             TEXT(#x), TEXT(__FILE__), (DWORD)__LINE__ ); \
       DestroyWindow (g_hMainWindow);

#else
   #define REPORTERR(x)
   #define DPF 1 ? (void)0 : (void)
   #define ASSERT(x)
   #define FATALERR(x) \
       DestroyWindow (g_hMainWindow);
#endif // DEBUG


/*
**-----------------------------------------------------------------------------
**  Global Variables
**-----------------------------------------------------------------------------
*/

// Debug Variables
#ifdef DAEDALUS_LOG
//	extern DWORD g_dwDebugLevel;
#endif

extern BOOL  g_bLog;



/*
**-----------------------------------------------------------------------------
**  Function Prototypes
**-----------------------------------------------------------------------------
*/

// Debug Routines
#ifdef DAEDALUS_LOG
	void __cdecl dprintf (DWORD dwDebugLevel, LPCTSTR szFormat, ...);
#endif //DEBUG


HANDLE Debug_CreateDumpFile(LPCTSTR szFileName);
void Debug_CloseDumpFile(HANDLE hFile);
void Debug_DumpLine(HANDLE hFile, LPCTSTR szFormat, va_list va);

#endif // End DEBUG_H


