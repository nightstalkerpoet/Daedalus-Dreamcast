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
#include <stdlib.h>

#include "CPU.h"
#include "RDP.h"
#include "RSP.h"
#include "Memory.h"
#include "Debug.h"

#include "ultra_rcp.h"
#include "ultra_mbi.h"
#include "ultra_sptask.h"

#include "RDP_AUD.h"
#include "RDP_GFX.h"

#include "DBGConsole.h"

#include "RDP_GFX_State.h"

#define throw
#define try          if(true)
#define catch(...)   if(false)


// Used globally
DWORD g_dwViWidth = 320;
DWORD g_dwViHeight = 240;


static OSTask	* g_pTask;



///////// Stuff we need //////////////
BOOL			g_bRDPHalted = TRUE;


//// Reg type things for GFXTask

// Render mode info:





static RDPGfxState g_AppliedState;
static RDPGfxState g_CurrentState;

// Used in a few places - don't declare as static
DWORD g_dwRDPTime = 0;


DWORD g_dwNumFrames = 0;








/*

#define RDP_NOEXIST() { /*MessageBox(g_hMainWindow, "RDP: Instruction Unknown", g_szDaedalusName, MB_OK); * //*RDPHalt();* / }

*/


/////////////////////////////////////////////////////////////////////////////////////
HRESULT RDPInit()
{
	HRESULT hr;

	g_dwRDPTime = timeGetTime();

	g_dwNumFrames = 0;

	hr = RDP_GFX_Init();

	if (FAILED(hr))
		return hr;


	return S_OK;
}

void RDPCleanup()
{
	RDP_GFX_Cleanup();
}

void RDP_Reset()
{
	RDP_GFX_Reset();
}

void RDPStep()
{
	RDP_ExecuteTask(NULL);
}

void RDPStepHalted() {}

void RDPHalt()
{
	StopRSP();	// Stop the Reality Signal Processor
	StopRDP();
}



void RDP_EnableGfx()
{
	g_bRDPEnableGfx = TRUE;
}

void RDP_DisableGfx()
{
	g_bRDPEnableGfx = FALSE;
}



void RDP_DumpRSPCode(char * szName, DWORD dwCRC, DWORD * pBase, DWORD dwPCBase, DWORD dwLen)
{
	TCHAR opinfo[400];
	TCHAR szFilePath[MAX_PATH+1];
	TCHAR szFileName[MAX_PATH+1];
	FILE * fp;

    /*KOS
	Dump_GetDumpDirectory(szFilePath, TEXT("rsp_dumps"));
	*/

	wsprintf(szFileName, "task_dump_%s_crc_0x%08x.txt", szName, dwCRC);

	PathAppend(szFilePath, szFileName);
	

	fp = fopen(szFilePath, "w");
	if (fp == NULL)
		return;

	DWORD dwIndex;
	for (dwIndex = 0; dwIndex < dwLen; dwIndex+=4)
	{
		DWORD dwOpCode;
		DWORD pc = dwIndex&0x0FFF;
		dwOpCode = pBase[dwIndex/4];

        /*KOS
		SprintRSPOpCodeInfo(opinfo, pc + dwPCBase, dwOpCode);
		*/

		fprintf(fp, "0x%08x: <0x%08x> %s\n", pc + dwPCBase, dwOpCode, opinfo);
		//fprintf(fp, "<0x%08x>\n", dwOpCode);
	}
	
	fclose(fp);

}


void RDP_DumpRSPData(char * szName, DWORD dwCRC, DWORD * pBase, DWORD dwPCBase, DWORD dwLen)
{
	TCHAR szFilePath[MAX_PATH+1];
	TCHAR szFileName[MAX_PATH+1];
	FILE * fp;

    /*KOS
	Dump_GetDumpDirectory(szFilePath, TEXT("rsp_dumps"));
	*/

	wsprintf(szFileName, "task_data_dump_%s_crc_0x%08x.txt", szName, dwCRC);

	PathAppend(szFilePath, szFileName);

	fp = fopen(szFilePath, "w");
	if (fp == NULL)
		return;

	DWORD dwIndex;
	for (dwIndex = 0; dwIndex < dwLen; dwIndex+=4)
	{
		DWORD dwData;
		DWORD pc = dwIndex&0x0FFF;
		dwData = pBase[dwIndex/4];

		fprintf(fp, "0x%08x: 0x%08x\n", pc + dwPCBase, dwData);
	}
	
	fclose(fp);

}



void RDP_LoadTask(OSTask * pTask)
{

	DWORD dwTask = pTask->t.type;

	DPF(DEBUG_DP, "DP: ###########Checking for task###############");
	DPF(DEBUG_DP, "DP: RSP is Halted...Checking stuff\n");
	DPF(DEBUG_DP, "DP: Task:0x%08x Flags:0x%08x BootCode:0x%08x BootCodeSize:0x%08x",
		pTask->t.type, pTask->t.flags, pTask->t.ucode_boot, pTask->t.ucode_boot_size);
	DPF(DEBUG_DP, "DP: uCode:0x%08x uCodeSize:0x%08x uCodeData:0x%08x uCodeDataSize:0x%08x",
		pTask->t.ucode, pTask->t.ucode_size, pTask->t.ucode_data, pTask->t.ucode_data_size);
	DPF(DEBUG_DP, "DP: Stack:0x%08x StackSize:0x%08x OutputBuff:0x%08x OutputBuffSize:0x%08x",
		pTask->t.dram_stack, pTask->t.dram_stack_size, pTask->t.output_buff, pTask->t.output_buff_size);
	DPF(DEBUG_DP, "DP: Data(PC):0x%08x DataSize:0x%08x YieldData:0x%08x YieldDataSize:0x%08x",
		pTask->t.data_ptr, pTask->t.data_size, pTask->t.yield_data_ptr, pTask->t.yield_data_size);
	
	g_pTask = NULL;
	switch (dwTask)
	{
		case M_GFXTASK:
			
			DPF(DEBUG_DP, "DP: M_GFXTASK");

			//DBGConsole_Msg(0, "VOutputBuf: 0x%08x, 0x%08x Flags 0x%08x", pTask->t.output_buff, pTask->t.output_buff_size, pTask->t.flags);

			g_pTask = pTask;
			StartRDP();
			//CPU_Halt("GFXTask");

			break;
		case M_AUDTASK:
			DPF(DEBUG_DP, "DP: M_AUDTASK");
			//DBGConsole_Msg(0, "AOutputBuf: 0x%08x, 0x%08x Flags 0x%08x", pTask->t.output_buff, pTask->t.output_buff_size, pTask->t.flags);

			g_pTask = pTask;
			StartRDP();

			break;
		case M_VIDTASK:
			DPF(DEBUG_DP, "DP: M_VIDTASK. I don't know what to do! Help!!");
			//DBGConsole_Msg(0, "SkippingVITask!");
			break;
		case M_JPGTASK:

			{
/*
JPEG Task
0x00151640 00151d00 00000004
0x00151648 00000002 00151860
0x00151650 001518e0 00151960
0x00151658 00000000 00000000
0x00151660 00000000 00000000
0x00151668 00000000 00000000
0x00151670 00000000 00000000
0x00151678 00000000 00000000
0x00151680 00000000 00000000
0x00151688 00000000 00000000*/

			/*	DWORD dwPC = (u32)pTask->t.data_ptr;
				DWORD dwSize = (u32)pTask->t.data_size;
				DBGConsole_Msg(0, "JPEG Task");
				for (DWORD i = 0; i < 10 && i < dwSize; i++)
				{
					//(u32)pTask->t.data_ptr
					DBGConsole_Msg(0, "0x%08x %08x %08x",
						dwPC,
						g_pu32RamBase[(dwPC>>2) + 0],
						g_pu32RamBase[(dwPC>>2) + 1]);

					dwPC += 8;
				}*/
				//CPU_Halt("JPEG");
			}
			break;
		default:
			DBGConsole_Msg(0, "Unknown task: %d", dwTask);
			g_pTask = pTask;
			break;

	}

	DPF(DEBUG_DP, "");
	DPF(DEBUG_DP, "DP: ###########################################");
}

void RDP_ExecuteTask(OSTask * pTask)
{
	DWORD dwTask;

	// Update timer..
	g_dwRDPTime = timeGetTime();

	if (pTask == NULL)
		pTask = g_pTask;
	if (pTask == NULL)
	{
		RDPHalt();		// No task loaded!
		return;
	}
	
	dwTask = pTask->t.type;

	if (dwTask == M_GFXTASK)
	{
		try
		{
			RDP_GFX_ExecuteTask(pTask);
		}
		catch (...)
		{
			if (g_bTrapExceptions)
			{
				DBGConsole_Msg(0, "Exception in GFX processing");
				Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_DP);
				AddCPUJob(CPU_CHECK_INTERRUPTS);
			}
		}
	}
	else if (dwTask == M_AUDTASK)
	{

		/*KOS
        if (g_pAiPlugin != NULL)
		{
			//RDP_AUD_ExecuteTask(pTask);
			g_pAiPlugin->ProcessAList();
		}*/

		RDPHalt();
	}
	else
	{
		DBGConsole_Msg(0, "Boot/uCode CRC not found! Dumping");

		DBGConsole_Msg(0, " Task:0x%08x Flags:0x%08x BootCode:0x%08x BootCodeSize:0x%08x",
			pTask->t.type, pTask->t.flags, pTask->t.ucode_boot, pTask->t.ucode_boot_size);
		DBGConsole_Msg(0, " uCode:0x%08x uCodeSize:0x%08x uCodeData:0x%08x uCodeDataSize:0x%08x",
			pTask->t.ucode, pTask->t.ucode_size, pTask->t.ucode_data, pTask->t.ucode_data_size);
		DBGConsole_Msg(0, " Stack:0x%08x StackSize:0x%08x OutputBuff:0x%08x OutputBuffSize:0x%08x",
			pTask->t.dram_stack, pTask->t.dram_stack_size, pTask->t.output_buff, pTask->t.output_buff_size);
		DBGConsole_Msg(0, " Data(PC):0x%08x DataSize:0x%08x YieldData:0x%08x YieldDataSize:0x%08x",
			pTask->t.data_ptr, pTask->t.data_size, pTask->t.yield_data_ptr, pTask->t.yield_data_size);


		RDP_DumpRSPCode("boot", 0xDEAFF00D,
			(DWORD*)(g_pu8RamBase + (((u32)pTask->t.ucode_boot)&0x00FFFFFF)),
			0x04001000, pTask->t.ucode_boot_size);
		
		RDP_DumpRSPCode("unkcode", 0xDEAFF00D,
			(DWORD*)(g_pu8RamBase + (((u32)pTask->t.ucode)&0x00FFFFFF)),
			0x04001080, 0x1000 - 0x80);//pTask->t.ucode_size);

		RDPHalt();
		
	}

	// Need to point to last instr?
	//Memory_DPC_SetRegister(DPC_CURRENT_REG, (u32)pTask->t.data_ptr);
	//RSP_Special_BREAK();

	Memory_SP_SetRegisterBits(SP_STATUS_REG, SP_STATUS_SIG2|SP_STATUS_BROKE|SP_STATUS_HALT);

	if(Memory_SP_GetRegister(SP_STATUS_REG) & SP_STATUS_INTR_BREAK)
	{
		Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SP);
		AddCPUJob(CPU_CHECK_INTERRUPTS);
	}

	g_pTask = NULL;
}

