#define TEST_DISABLE_SP_FUNCS //return PATCH_RET_NOT_PROCESSED;


DWORD Patch___osSpRawStartDma()
{
TEST_DISABLE_SP_FUNCS
	DWORD dwRWFlag = (u32)g_qwGPR[REG_a0];
	DWORD dwSPAddr = (u32)g_qwGPR[REG_a1];
	DWORD dwVAddr = (u32)g_qwGPR[REG_a2];
	DWORD dwLen = (u32)g_qwGPR[REG_a3];

	DWORD dwPAddr = VirtToPhys(dwVAddr);

	/*
	DBGConsole_Msg(0, "osSpRawStartDma(%d, 0x%08x, 0x%08x (0x%08x), %d)", 
		dwRWFlag,
		dwSPAddr,
		dwVAddr, dwPAddr,
		dwLen);*/

	if (IsSpDeviceBusy())
	{
		g_qwGPR[REG_v0] = (s64)(s32)~0;
	}
	else
	{
		if (dwPAddr == 0)
		{
			//FIXME
			DBGConsole_Msg(0, "Address Translation necessary!");
		}
	
		Write32Bits(PHYS_TO_K1(SP_MEM_ADDR_REG), dwSPAddr);
		Write32Bits(PHYS_TO_K1(SP_DRAM_ADDR_REG), dwPAddr);
		
		if (dwRWFlag == OS_READ)  
		{
			// This is correct - SP_WR_LEN_REG is a read (from RDRAM to device!)
			Write32Bits(PHYS_TO_K1(SP_WR_LEN_REG), dwLen - 1);
		}
		else
		{
			Write32Bits(PHYS_TO_K1(SP_RD_LEN_REG), dwLen - 1);
		}

		g_qwGPR[REG_v0] = 0;
	}

	return PATCH_RET_JR_RA;
}


DWORD Patch___osSpDeviceBusy_Mario()
{
TEST_DISABLE_SP_FUNCS
	if (IsSpDeviceBusy())
		g_qwGPR[REG_v0] = 1;
	else
		g_qwGPR[REG_v1] = 0;

	return PATCH_RET_JR_RA;
}
// Identical, but optimised
DWORD Patch___osSpDeviceBusy_Rugrats()
{
TEST_DISABLE_SP_FUNCS
	if (IsSpDeviceBusy())
		g_qwGPR[REG_v0] = 1;
	else
		g_qwGPR[REG_v1] = 0;

	return PATCH_RET_JR_RA;
}




// Very similar to osSpDeviceBusy, 
DWORD Patch___osSpGetStatus_Mario()
{
TEST_DISABLE_SP_FUNCS
	DWORD dwStatus = SpGetStatus();

	g_qwGPR[REG_v0] = (s64)(s32)dwStatus;
	return PATCH_RET_JR_RA;
}
DWORD Patch___osSpGetStatus_Rugrats()
{
TEST_DISABLE_SP_FUNCS
	DWORD dwStatus = SpGetStatus();

	g_qwGPR[REG_v0] = (s64)(s32)dwStatus;
	return PATCH_RET_JR_RA;
}



DWORD Patch___osSpSetStatus_Mario()
{
TEST_DISABLE_SP_FUNCS
	DWORD dwStatus = (u32)g_qwGPR[REG_a0];

	Write32Bits(PHYS_TO_K1(SP_STATUS_REG), dwStatus);
	return PATCH_RET_JR_RA;
}
DWORD Patch___osSpSetStatus_Rugrats()
{
TEST_DISABLE_SP_FUNCS
	DWORD dwStatus = (u32)g_qwGPR[REG_a0];

	Write32Bits(PHYS_TO_K1(SP_STATUS_REG), dwStatus);
	return PATCH_RET_JR_RA;
}



DWORD Patch___osSpSetPc()
{
TEST_DISABLE_SP_FUNCS
	DWORD dwPC = (u32)g_qwGPR[REG_a0];

	//DBGConsole_Msg(0, "__osSpSetPc(0x%08x)", dwPC);
	
	DWORD dwStatus = SpGetStatus();

	if (dwStatus & SP_STATUS_HALT)
	{
		// Halted, we can safely set the pc:
		Write32Bits(PHYS_TO_K1(SP_PC_REG), dwPC);

		g_qwGPR[REG_v0] = (s64)(s32)0;
	}
	else
	{
		g_qwGPR[REG_v0] = (s64)(s32)~0;
	}

	return PATCH_RET_JR_RA;
}


// Translate task...

DWORD Patch_osSpTaskLoad()
{
TEST_DISABLE_SP_FUNCS
	DWORD dwTask = (u32)g_qwGPR[REG_a0];


	DWORD dwStatus = SpGetStatus();

	if ((dwStatus & SP_STATUS_HALT) == 0 ||
		IsSpDeviceBusy())
	{
		DBGConsole_Msg(0, "Sp Device is not HALTED, or is busy");
		// We'd have to loop, and we can't do this...
		return PATCH_RET_NOT_PROCESSED;
	}
	
	OSTask * pSrcTask = (OSTask *)ReadAddress(dwTask);
	OSTask * pDstTask = (OSTask *)ReadAddress(VAR_ADDRESS(osSpTaskLoadTempTask));

	// Translate virtual addresses to physical...
	memcpy(pDstTask, pSrcTask, sizeof(OSTask));

	if (pDstTask->t.ucode != 0)
		pDstTask->t.ucode = (u64 *)VirtToPhys((u32)pDstTask->t.ucode);

	if (pDstTask->t.ucode_data != 0)
		pDstTask->t.ucode_data = (u64 *)VirtToPhys((u32)pDstTask->t.ucode_data);

	if (pDstTask->t.dram_stack != 0)
		pDstTask->t.dram_stack = (u64 *)VirtToPhys((u32)pDstTask->t.dram_stack);
	
	if (pDstTask->t.output_buff != 0)
		pDstTask->t.output_buff = (u64 *)VirtToPhys((u32)pDstTask->t.output_buff);
	
	if (pDstTask->t.output_buff_size != 0)
		pDstTask->t.output_buff_size = (u64 *)VirtToPhys((u32)pDstTask->t.output_buff_size);
	
	if (pDstTask->t.data_ptr != 0)
		pDstTask->t.data_ptr = (u64 *)VirtToPhys((u32)pDstTask->t.data_ptr);
	
	if (pDstTask->t.yield_data_ptr != 0)
		pDstTask->t.yield_data_ptr = (u64 *)VirtToPhys((u32)pDstTask->t.yield_data_ptr);

	// If yielded, use the yield data info
	if (pSrcTask->t.flags & OS_TASK_YIELDED)
	{
		pDstTask->t.ucode_data = pDstTask->t.yield_data_ptr;
		pDstTask->t.ucode_data_size = pDstTask->t.yield_data_size;

		pSrcTask->t.flags &= ~(OS_TASK_YIELDED);
	}

	// Writeback the DCache for pDstTask

	Write32Bits(PHYS_TO_K1(SP_STATUS_REG),
		SP_CLR_SIG2|SP_CLR_SIG1|SP_CLR_SIG0|SP_SET_INTR_BREAK);	


	// Set the PC
	Write32Bits(PHYS_TO_K1(SP_PC_REG), 0x04001000);

	// Copy the task info to dmem
	Write32Bits(PHYS_TO_K1(SP_MEM_ADDR_REG), 0x04000fc0);
	Write32Bits(PHYS_TO_K1(SP_DRAM_ADDR_REG), VAR_ADDRESS(osSpTaskLoadTempTask));
	Write32Bits(PHYS_TO_K1(SP_RD_LEN_REG), 64 - 1);

	//	Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SP);
	//AddCPUJob(CPU_CHECK_INTERRUPTS);


	//Memory_SP_SetRegisterBits(SP_STATUS_REG, SP_STATUS_HALT);	// Or just set entire?

	// We can load the task directly here, but this causes a few bugs
	// when using audio/gfx plugins that expect the 0x04000fc0 to be set up
	// correctly
//	RDP_LoadTask(pDstTask);

	// We know that we're not busy!
	Write32Bits(PHYS_TO_K1(SP_MEM_ADDR_REG), 0x04001000);
	Write32Bits(PHYS_TO_K1(SP_DRAM_ADDR_REG), (u32)pDstTask->t.ucode_boot);//	-> Translate boot ucode to physical address!
	Write32Bits(PHYS_TO_K1(SP_RD_LEN_REG), pDstTask->t.ucode_boot_size - 1);
	
	return PATCH_RET_JR_RA;
	
}




DWORD Patch_osSpTaskStartGo()
{
TEST_DISABLE_SP_FUNCS
	// Device busy? 
	if (IsSpDeviceBusy())
	{
		// LOOP Until device not busy -
		// we can't do this, so we just exit. What we could to is
		// a speed hack and jump to the next interrupt

		return PATCH_RET_NOT_PROCESSED;
	}

	//DBGConsole_Msg(0, "__osSpTaskStartGo()");

	Write32Bits(PHYS_TO_K1(SP_STATUS_REG), (SP_SET_INTR_BREAK|SP_CLR_SSTEP|SP_CLR_BROKE|SP_CLR_HALT));

	//RDP_ExecuteTask(NULL);

	return PATCH_RET_JR_RA;

}




