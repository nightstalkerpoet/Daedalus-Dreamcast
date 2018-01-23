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

// Lots of additional work by Lkb - thanks!

#include "main.h"


#include "ultra_r4300.h"

#include "DBGConsole.h"

#include "Registers.h"	// For RegNames
#include "CPU.h"
#include "RSP.h"
#include "RDP.h"
#include "RDP_GFX.h"
#include "OpCode.h"
#include "Patch.h"		// To get patch names
#include "Controller.h"

static BOOL DBGConsole_Init();
static void DBGConsole_Fini();

static BOOL DBGConsole_ResizeConBufAndWindow(HANDLE hConsole, SHORT xSize, SHORT ySize);
static DWORD DBGConsole_ConsoleFunc(LPVOID pArg);

static void DBGConsole_ParseStringHighlights(LPSTR szString, WORD * arrwAttributes, WORD wAttr);
static BOOL DBGConsole_WriteString(LPCTSTR szString, BOOL bParse, SHORT x, SHORT y, WORD wAttr, SHORT width);
static void DBGConsole_DisplayOutput(LONG nOffset);

enum DBGCON_MIDPANETYPE
{
	DBGCON_CPU_CODE_VIEW = 0,
	DBGCON_RSP_CODE_VIEW,
};

CHAR * s_szPaneNameLabels[] = { "Disassembly", "RSP Disassembly", "Memory" };

enum DBGCON_PANETYPE
{
	DBGCON_REG_PANE = 0,		// Register view
	DBGCON_MEM_PANE,
	DBGCON_MID_PANE,			// Disasm/Mem area
	DBGCON_BOT_PANE				// Output area
};

static DWORD g_dwMemAddress = 0xb0000000;
static DWORD g_dwDisplayPC = ~0;
static DWORD g_dwDisplayRSP_PC = ~0;
static LONG  g_nOutputBufOffset = 0;

static DBGCON_MIDPANETYPE s_nMidPaneType = DBGCON_CPU_CODE_VIEW;

static DBGCON_PANETYPE s_nActivePane = DBGCON_MID_PANE;

static void DBGConsole_SetActivePane(DBGCON_PANETYPE pane);
static void DBGConsole_RedrawPaneLabels();

static void DBGConsole_ProcessInput();
static void DBGConsole_HelpCommand(LPCTSTR szCommand);
static void DBGConsole_GoCommand();
static void DBGConsole_StopCommand();

//////////////////////////////////////////////////////////////////////////
static void DBGConsole_DumpContext(DWORD dwPC);
static void DBGConsole_DumpContext_NextLine();
static void DBGConsole_DumpContext_NextPage();
static void DBGConsole_DumpContext_PrevLine();
static void DBGConsole_DumpContext_PrevPage();

static void DBGConsole_DumpMemory(DWORD dwAddress);
static void DBGConsole_DumpCPUContext(DWORD dwPC);
static void DBGConsole_DumpRSPContext(DWORD dwPC);


//////////////////////////////////////////////////////////////////////////
static void DBGConsole_DumpRegisters();
static void DBGConsole_DumpCPURegisters();
static void DBGConsole_DumpRSPRegisters();



static TCHAR g_szInput[1024+1];

static const int sc_nTopPaneTextOffset = 0;  static const int sc_nTopPaneLines = 11;
static const int sc_nMemPaneTextOffset = 12; static const int sc_nMemPaneLines = 7;
static const int sc_nMidPaneTextOffset = 20; static const int sc_nMidPaneLines = 13;
static const int sc_nBotPaneTextOffset = 34; static const int sc_nBotPaneLines = 24;
static const int sc_nCommandTextOffset = 59 ;



typedef struct tagDebugCommandInfo
{
	LPCTSTR szCommand;
	void (* pArglessFunc)();
	void (* pArgFunc)(LPCTSTR);
	LPCTSTR szHelpText;
} DebugCommandInfo;

typedef struct tagHelpTopicInfo
{
	LPCTSTR szHelpTopic;
	LPCTSTR szHelpText;
} HelpTopicInfo;

static void DBGConsole_IntPi() { DBGConsole_Msg(0, "Pi Interrupt"); Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_PI); AddCPUJob(CPU_CHECK_INTERRUPTS); }
static void DBGConsole_IntAi() { DBGConsole_Msg(0, "Ai Interrupt"); Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_AI); AddCPUJob(CPU_CHECK_INTERRUPTS); }
static void DBGConsole_IntDp() { DBGConsole_Msg(0, "Dp Interrupt"); Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_DP); AddCPUJob(CPU_CHECK_INTERRUPTS); }
static void DBGConsole_IntSp() { DBGConsole_Msg(0, "Sp Interrupt"); Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SP); AddCPUJob(CPU_CHECK_INTERRUPTS); }
static void DBGConsole_IntSi() { DBGConsole_Msg(0, "Si Interrupt"); Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SI); AddCPUJob(CPU_CHECK_INTERRUPTS); }
static void DBGCommand_FP(LPCTSTR szCommand);
static void DBGCommand_Vec(LPCTSTR szCommand);
static void DBGCommand_Mem(LPCTSTR szCommand);
static void DBGCommand_List(LPCTSTR szCommand);
static void DBGCommand_ListOS(LPCTSTR szCommand);
static void DBGCommand_BPX(LPCTSTR szCommand);
static void DBGCommand_BPD(LPCTSTR szCommand);
static void DBGCommand_BPE(LPCTSTR szCommand);
static void DBGCommand_ShowCPU();
static void DBGCommand_ShowRSP();

static void DBGCommand_Write8(LPCTSTR szCommand);
static void DBGCommand_Write16(LPCTSTR szCommand);
static void DBGCommand_Write32(LPCTSTR szCommand);
static void DBGCommand_Write64(LPCTSTR szCommand);
static void DBGCommand_WriteReg(LPCTSTR szCommand);

static void DBGCommand_Dis(LPCTSTR szCommand);
static void DBGCommand_RDis(LPCTSTR szCommand);

static void DBGCommand_Close();
static void DBGCommand_Quit();

#define BEGIN_DBGCOMMAND_MAP(name) \
static const DebugCommandInfo name[] =	\
{
#define DBGCOMMAND_ARGLESS(cmd, func) \
{ cmd, func, NULL, NULL },
#define DBGCOMMAND_ARG(cmd, func) \
{ cmd, NULL, func, NULL },

#define DBGCOMMAND_ARGLESS_HELP(cmd, func, help) \
{ cmd, func, NULL, help },
#define DBGCOMMAND_ARG_HELP(cmd, func, help) \
{ cmd, NULL, func, help },

#define BEGIN_DBGCOMMAND_ARGLESS_HELP(cmd, func) \
{ cmd, func, NULL, 
#define BEGIN_DBGCOMMAND_ARG_HELP(cmd, func) \
{ cmd, NULL, func,

#define BEGIN_DBGCOMMAND() },

#define END_DBGCOMMAND_MAP()		\
	{ NULL, NULL }					\
};

#define BEGIN_HELPTOPIC_MAP(name) \
static const HelpTopicInfo name[] =	\
{

#define HELPTOPIC(topic, text) \
{ topic, text },

#define BEGIN_HELPTOPIC(topic) { topic, 

#define END_HELPTOPIC() },

#define END_HELPTOPIC_MAP()		\
	{ NULL, NULL }				\
};

BEGIN_DBGCOMMAND_MAP(g_DBGCommands)
	DBGCOMMAND_ARG_HELP( "help", DBGConsole_HelpCommand, "Displays commands and help topics or shows help text for one of them" ) // Implemented by Lkb
	DBGCOMMAND_ARGLESS_HELP( "go", DBGConsole_GoCommand, "Starts the CPU" )
	DBGCOMMAND_ARGLESS_HELP( "stop", DBGConsole_StopCommand, "Stops the CPU" )
	DBGCOMMAND_ARGLESS_HELP( "skip", CPUSkip, "When the CPU is stopped, skips the current instruction" )
	DBGCOMMAND_ARGLESS_HELP( "dump dlist", RDP_GFX_DumpNextDisplayList, "Dumps the next display list to disk" )
	DBGCOMMAND_ARGLESS_HELP( "dump textures", RDP_GFX_DumpTextures, "Dumps existing/new textures to disk as .png files" )
	DBGCOMMAND_ARGLESS_HELP( "nodump textures", RDP_GFX_NoDumpTextures, "Stops dumping textures" )
	DBGCOMMAND_ARGLESS_HELP( "drop textures", RDP_GFX_DropTextures, "Forces Daedalus to recreate the textures" )
	DBGCOMMAND_ARGLESS_HELP( "make textures blue", RDP_GFX_MakeTexturesBlue, "Makes all the textures in the scene blue" )
	DBGCOMMAND_ARGLESS_HELP( "make textures normal", RDP_GFX_MakeTexturesNormal, "Restores textures after being made blue" )
	DBGCOMMAND_ARGLESS_HELP( "enablegfx", RDP_EnableGfx, "Reenables display list processing" )
	DBGCOMMAND_ARGLESS_HELP( "disablegfx", RDP_DisableGfx, "Turns off Display List processing" )
	
	DBGCOMMAND_ARGLESS_HELP( "stat dr", SR_Stats, "Displays some statistics on the dynamic recompiler" )

	DBGCOMMAND_ARGLESS( "disable si", Memory_SiDisable )
	DBGCOMMAND_ARGLESS( "enable si", Memory_SiEnable )

	DBGCOMMAND_ARGLESS_HELP( "int pi", DBGConsole_IntPi, "Generates a PI interrupt" )
	DBGCOMMAND_ARGLESS_HELP( "int ai", DBGConsole_IntAi, "Generates a AI interrupt" )
	DBGCOMMAND_ARGLESS_HELP( "int dp", DBGConsole_IntDp, "Generates a DP interrupt" )
	DBGCOMMAND_ARGLESS_HELP( "int sp", DBGConsole_IntSp, "Generates a SP interrupt" )
	DBGCOMMAND_ARGLESS_HELP( "int si", DBGConsole_IntSi, "Generates a SI interrupt" )

	DBGCOMMAND_ARG_HELP( "dis", DBGCommand_Dis, "Dumps disassembly of specified region to disk" )
	DBGCOMMAND_ARG_HELP( "rdis", DBGCommand_RDis, "Dumps disassembly of the rsp imem/dmem to disk" )
		
	DBGCOMMAND_ARG_HELP( "listos", DBGCommand_ListOS, "Shows the os symbol in the code view (e.g. __osDisableInt)" )
	DBGCOMMAND_ARG_HELP( "list", DBGCommand_List, "Shows the specified address in the code view" )
	DBGCOMMAND_ARGLESS_HELP( "cpu", DBGCommand_ShowCPU, "Show the CPU code view" )
	DBGCOMMAND_ARGLESS_HELP( "rsp", DBGCommand_ShowRSP, "Show the RSP code view" )
	DBGCOMMAND_ARG_HELP( "fp", DBGCommand_FP, "Displays the specified floating point register" )
	DBGCOMMAND_ARG_HELP( "vec", DBGCommand_Vec, "Dumps the specified rsp vector" )

	DBGCOMMAND_ARG_HELP( "mem", DBGCommand_Mem, "Shows the memory at the specified address" )
	DBGCOMMAND_ARG_HELP( "bpx", DBGCommand_BPX, "Sets a breakpoint at the specified address" )
	DBGCOMMAND_ARG_HELP( "bpd", DBGCommand_BPD, "Disables a breakpoint at the specified address" )
	DBGCOMMAND_ARG_HELP( "bpe", DBGCommand_BPE, "Enables a breakpoint at the specified address" )

	DBGCOMMAND_ARG_HELP( "w8", DBGCommand_Write8, "Writes the specified 8-bit value at the specified address. Type \"help write\" for more info.") // Added and implemented by Lkb
	DBGCOMMAND_ARG_HELP( "w16", DBGCommand_Write16, "Writes the specified 16-bit value at the specified address. Type \"help write\" for more info." ) // Added and implemented by Lkb
	DBGCOMMAND_ARG_HELP( "w32", DBGCommand_Write32, "Writes the specified 32-bit value at the specified address. Type \"help write\" for more info." ) // Added and implemented by Lkb
	DBGCOMMAND_ARG_HELP( "w64", DBGCommand_Write64, "Writes the specified 64-bit value at the specified address. Type \"help write\" for more info." ) // Added and implemented by Lkb
	DBGCOMMAND_ARG_HELP( "wr", DBGCommand_WriteReg, "Writes the specified value in the specified register. Type \"help write\" for more info." ) // Added and implemented by Lkb

	DBGCOMMAND_ARGLESS_HELP( "osinfo threads", Patch_DumpOsThreadInfo, "Displays a list of active threads" )
	DBGCOMMAND_ARGLESS_HELP( "osinfo queues", Patch_DumpOsQueueInfo, "Display a list of queues (some are usually invalid)" )
	DBGCOMMAND_ARGLESS_HELP( "osinfo events", Patch_DumpOsEventInfo, "Display the system event details" )

	DBGCOMMAND_ARGLESS_HELP( "close", DBGCommand_Close, "Closes the debug console") // Added and implemented by Lkb
	DBGCOMMAND_ARGLESS_HELP( "quit", DBGCommand_Quit, "Quits Daedalus") // Added and implemented by Lkb
	DBGCOMMAND_ARGLESS_HELP( "exit", DBGCommand_Quit, "Quits Daedalus") // Added and implemented by Lkb
	
END_DBGCOMMAND_MAP()


BEGIN_HELPTOPIC_MAP(g_DBGHelpTopics)

	BEGIN_HELPTOPIC("numbers")
	"In Daedalus you can specify numbers with the following notations:\n"
	"\tDecimal: 0t12345678\n"
	"\tHexadecimal: 0xabcdef12 or abcdef12\n"
	"\tRegister: %sp\n"
	"\tRegister+-value: %sp+2c\n"
	"\tDon't add spaces when using the reg+-value notation: \"%sp + 4\" is incorrect\n"
	"\tIn this release, \"mem [[sp+4[]\" is replaced by \"mem %sp+4\""
	END_HELPTOPIC()

	BEGIN_HELPTOPIC("write")
	"Syntax:\n"
	"w{8|16|32|64} <address> [[<operators>[] <value>\n"
	"wr <regname> [[<operators>[] <value>\n"
	"\n"
	"<address> - Address to modify\n"
	"<regname> - Register to modify\n"
	"<value> - Value to use\n"
	"<operators> - One or more of the following symbols:\n"
	"\t$ - Bit mode: uses (1 << <value>) instead of <value>\n"
	"\t~ - Not mode: negates <value> before using it\n"
	"\t| - Or operator: combines [[<address>[] with <value> using Or\n"
	"\t^ - Xor operator: combines [[<address>[] with <value> using Xor\n"
	"\t& - And operator: combines [[<address>[] with <value> using And\n"
	"For information on the format of <address> and <value>, type \"help numbers\""
	END_HELPTOPIC()

	BEGIN_HELPTOPIC("rspmem")
	"To read or write form RSP code memory you must replace the first zero with 'a'\n"
	"Example: to write NOP to RSP 0x04001090 use: w32 a4001090 0"
	END_HELPTOPIC()

	BEGIN_HELPTOPIC("dis")
	"Disassemble a region of memory to file\n"
	"Syntax:\n"
	"dis <address1> <address2> [[<filename>[]\n"
	"\n"
	"<address1> - Starting address\n"
	"<address2> - Ending address\n"
	"<filename> - Optional filename (default is dis.txt)\n"
	"Example: Bootrom   : dis 0xB0000000 0xB0001000\n"
	"         ExcVectors: dis 0x80000000 0x80000200"
	END_HELPTOPIC()

	BEGIN_HELPTOPIC("rdis")
	"Disassemble the rsp dmem/imem to file\n"
	"Syntax:\n"
	"dis [[<filename>[]\n"
	"\n"
	"<filename> - Optional filename (default is rdis.txt)"
	END_HELPTOPIC()


END_HELPTOPIC_MAP()

static const DWORD sc_Cop0RegsToShow[] =
{
	C0_INX,
	//C0_RAND,
	C0_ENTRYLO0,
	C0_ENTRYLO1,
	C0_CONTEXT,
	C0_PAGEMASK,
	C0_WIRED,
	C0_BADVADDR,
	C0_COUNT,
	C0_ENTRYHI,
	C0_SR,
	C0_CAUSE,
	C0_EPC,
	//C0_PRID,
	C0_COMPARE,
	C0_CONFIG,
	C0_LLADDR,
	C0_WATCHLO,
	C0_WATCHHI,
	C0_ECC,			// PERR
	C0_CACHE_ERR,
	C0_TAGLO,
	C0_TAGHI,
	C0_ERROR_EPC,
	~0
};

static BOOL s_bConsoleAllocated = FALSE;

void DBGConsole_Enable(BOOL bEnable)
{
	// Shut down
	DBGConsole_Fini();

	if (bEnable)
	{
		g_bShowDebug = DBGConsole_Init();
	}
	else
	{
		g_bShowDebug = FALSE;
	}

}


BOOL DBGConsole_Init()
{
	BOOL bRetVal;

	// Don't re-init!
	if (s_bConsoleAllocated)
		return TRUE;
	
	DBGConsole_DumpRegisters();
	DBGConsole_DumpMemory(~0);
	DBGConsole_DumpContext(~0);

	return TRUE;
}

void DBGConsole_Fini()
{
	if (s_bConsoleAllocated)
	{
		s_bConsoleAllocated = FALSE;
	}
}

void DBGConsole_UpdateDisplay()
{
	// Return if we don't have a console!
	if (!g_bShowDebug || !s_bConsoleAllocated)
		return;

	DBGConsole_DumpRegisters();
	DBGConsole_DumpMemory(~0);
	DBGConsole_DumpContext(~0);
}

void DBGConsole_DumpRegisters()
{
	if (s_nMidPaneType == DBGCON_RSP_CODE_VIEW)
		DBGConsole_DumpRSPRegisters();
	else
		DBGConsole_DumpCPURegisters();

}

void DBGConsole_ClearRegisters()
{
}

void DBGConsole_DumpCPURegisters()
{
}


void DBGConsole_DumpRSPRegisters()
{
}

void DBGConsole_DumpContext(DWORD dwPC)
{
}

void DBGConsole_DumpContext_PrevLine()
{
}

void DBGConsole_DumpContext_PrevPage()
{
}

void DBGConsole_DumpContext_NextLine()
{
}

void DBGConsole_DumpContext_NextPage()
{
}

// Pass in ~0 to display current mem pointer
void DBGConsole_DumpMemory(DWORD dwAddress)
{
}



// Pass in ~0 to display current PC
void DBGConsole_DumpCPUContext(DWORD dwPC)
{
}

void DBGConsole_DumpRSPContext(DWORD dwPC)
{
}



// Write a string of characters to a screen buffer. 
BOOL DBGConsole_WriteString(LPCTSTR szString, BOOL bParse, SHORT x, SHORT y, WORD wAttr, SHORT width)
{
    //printf( szString );
	return TRUE;
}


void DBGConsole_Msg(DWORD dwType, LPCTSTR szFormat, ...)
{
    char buffer[1024];

    va_list va;
    va_start( va, szFormat );

	vsprintf( buffer, szFormat, va );
	strcat( buffer, "\r\n");
    //printf( buffer );

    va_end( va );
}

// Used to overwrite previous lines - like for doing % complete indicators
void DBGConsole_MsgOverwrite(DWORD dwType, LPCTSTR szFormat, ...)
{
	char buffer[1024];

    va_list va;
    va_start( va, szFormat );

	vsprintf( buffer, szFormat, va );
	strcat( buffer, "\r\n");
    //printf( buffer );

    va_end( va );
}


