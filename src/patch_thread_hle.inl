#define TEST_DISABLE_THREAD_FUNCS //return PATCH_RET_NOT_PROCESSED;

DWORD Patch_osCreateThread_Mario()
{
TEST_DISABLE_THREAD_FUNCS
	DWORD dwThread = (u32)g_qwGPR[REG_a0];
	DWORD dwID = (u32)g_qwGPR[REG_a1];
	DWORD dwpFunc = (u32)g_qwGPR[REG_a2];
	DWORD dwArg = (u32)g_qwGPR[REG_a3];


	// Other variables are on the stack - dig them out!
	// Stack is arg 4
	DWORD dwStack = Read32Bits((u32)g_qwGPR[REG_sp] + 4*4);

	// Pri is arg 5
	DWORD dwPri = Read32Bits((u32)g_qwGPR[REG_sp] + 4*5);

	DBGConsole_Msg(0, "[WosCreateThread](0x%08x, %d, 0x%08x(), 0x%08x, 0x%08x, %d)",
		dwThread, dwID, dwpFunc, dwArg, dwStack, dwPri );

	// fp used - we now HLE the Cop1 Unusable exception and set this
	// when the thread first accesses the FP unit
	Write32Bits(dwThread + offsetof(OSThread, fp), 0);						// pThread->fp

	
	Write16Bits(dwThread + offsetof(OSThread, state), OS_STATE_STOPPED);	// pThread->state
	Write16Bits(dwThread + offsetof(OSThread, flags), 0);					// pThread->flags

	Write32Bits(dwThread + offsetof(OSThread, id), dwID);
	Write32Bits(dwThread + offsetof(OSThread, priority), dwPri);
	

	Write32Bits(dwThread + offsetof(OSThread, next), NULL);					// pThread->next
	Write32Bits(dwThread + offsetof(OSThread, queue), NULL);				// Queue
	Write32Bits(dwThread + offsetof(OSThread, context.pc), dwpFunc);		// state.pc

	s64 qwArg = (s64)(s32)dwArg;
	Write64Bits(dwThread + offsetof(OSThread, context.a0), qwArg);			// a0	

	s64 qwStack = (s64)(s32)dwStack;
	Write64Bits(dwThread + offsetof(OSThread, context.sp), qwStack - 16);	// sp (sub 16 for a0 arg etc)

	s64 qwRA = (s64)(s32)VAR_ADDRESS(osThreadDieRA);
	Write64Bits(dwThread + offsetof(OSThread, context.ra), qwRA);			// ra

	Write32Bits(dwThread + offsetof(OSThread, context.sr), (SR_IMASK|SR_EXL|SR_IE));					// state.sr
	Write32Bits(dwThread + offsetof(OSThread, context.rcp), (OS_IM_ALL & RCP_IMASK)>>RCP_IMASKSHIFT);	// state.rcp
	Write32Bits(dwThread + offsetof(OSThread, context.fpcsr), (FPCSR_FS|FPCSR_EV));	// state.fpcsr

	// Set us as head of global list
	DWORD dwNextThread = Read32Bits(VAR_ADDRESS(osGlobalThreadList));
	Write32Bits(dwThread + offsetof(OSThread, tlnext), dwNextThread);				// pThread->next
	Write32Bits(VAR_ADDRESS(osGlobalThreadList), dwThread);

	return PATCH_RET_JR_RA;
}
// Identical to Mario code - just more optimised
DWORD Patch_osCreateThread_Rugrats()
{
TEST_DISABLE_THREAD_FUNCS
	return Patch_osCreateThread_Mario();
}




DWORD Patch_osSetThreadPri()
{
TEST_DISABLE_THREAD_FUNCS
	DWORD dwThread = (u32)g_qwGPR[REG_a0];
	DWORD dwPri    = (u32)g_qwGPR[REG_a1];

	DWORD dwActiveThread = Read32Bits(VAR_ADDRESS(osActiveThread));

	if (dwThread == 0x00000000)
	{
		dwThread = dwActiveThread;
	}
	
	//DBGConsole_Msg(0, "[WosSetThreadPri](0x%08x, %d) 0x%08x", dwThread, dwPri, dwActiveThread);

	return PATCH_RET_NOT_PROCESSED;

}

DWORD Patch_osGetThreadPri()
{
TEST_DISABLE_THREAD_FUNCS
	DWORD dwThread = (u32)g_qwGPR[REG_a0];
	DWORD dwPri;

	if (dwThread == NULL)
	{
		dwThread = Read32Bits(VAR_ADDRESS(osActiveThread));
	}

	dwPri = Read32Bits(dwThread + offsetof(OSThread, priority));

	g_qwGPR[REG_v0] = (s64)(s32)dwPri;
	return PATCH_RET_JR_RA;
}



// NOT TESTED!
DWORD Patch___osDequeueThread()
{
TEST_DISABLE_THREAD_FUNCS
	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	DWORD dwThread = (u32)g_qwGPR[REG_a1];

	//DBGConsole_Msg(0, "Dequeuing Thread");

	DWORD dwCurThread = Read32Bits(dwQueue + 0x0);
	while (dwCurThread != 0)
	{
		if (dwCurThread == dwThread)
		{
			// Set the next pointer of the previous thread
			// to the next pointer of this thread
			Write32Bits(dwQueue, Read32Bits(dwThread + offsetof(OSThread, next)));
			break;
		}
		else
		{
			// Set queue pointer to next in list
			dwQueue = dwCurThread;
			dwCurThread = Read32Bits(dwQueue + 0x0);
		}
	}

	return PATCH_RET_JR_RA;
}



DWORD Patch___osDispatchThread_Mario()
{
TEST_DISABLE_THREAD_FUNCS
	// First pop the first thread off the stack (copy of osPopThread code):
	DWORD dwThread = Read32Bits(VAR_ADDRESS(osThreadQueue));

	BYTE * pThreadBase = (BYTE *)ReadAddress(dwThread);

	// Update queue to point to next thread:
	Write32Bits(VAR_ADDRESS(osThreadQueue), QuickRead32Bits(pThreadBase, offsetof(OSThread, next)));

	// Set the current active thread:
	Write32Bits(VAR_ADDRESS(osActiveThread), dwThread);


	// Update this thread's most recently run time
	/*BOOL bFound = FALSE;
	for (LONG i = 0; i < g_ThreadTimes.size(); i++)
	{
		if (g_ThreadTimes[i].dwThread == dwThread)
		{
			g_ThreadTimes[i].dwTime = (DWORD)g_qwCPR[0][C0_COUNT];
			bFound = TRUE;
			break;
		}
	}
	if (!bFound)
	{
		ThreadTimes tt;
		tt.dwThread = dwThread;
		tt.dwTime = (DWORD)g_qwCPR[0][C0_COUNT];
		g_ThreadTimes.push_back(tt);
	}*/


	// Set the current thread's status to OS_STATE_RUNNING:
	Write16Bits(dwThread + offsetof(OSThread, state), OS_STATE_RUNNING);

	// Restore all registers:
	// For speed, we cache the base pointer!!!
	g_qwGPR[REG_at] = QuickRead64Bits(pThreadBase, 0x0020);
	g_qwGPR[REG_v0] = QuickRead64Bits(pThreadBase, 0x0028);
	g_qwGPR[REG_v1] = QuickRead64Bits(pThreadBase, 0x0030);
	g_qwGPR[REG_a0] = QuickRead64Bits(pThreadBase, 0x0038);
	g_qwGPR[REG_a1] = QuickRead64Bits(pThreadBase, 0x0040);
	g_qwGPR[REG_a2] = QuickRead64Bits(pThreadBase, 0x0048);
	g_qwGPR[REG_a3] = QuickRead64Bits(pThreadBase, 0x0050);
	g_qwGPR[REG_t0] = QuickRead64Bits(pThreadBase, 0x0058);
	g_qwGPR[REG_t1] = QuickRead64Bits(pThreadBase, 0x0060);
	g_qwGPR[REG_t2] = QuickRead64Bits(pThreadBase, 0x0068);
	g_qwGPR[REG_t3] = QuickRead64Bits(pThreadBase, 0x0070);
	g_qwGPR[REG_t4] = QuickRead64Bits(pThreadBase, 0x0078);
	g_qwGPR[REG_t5] = QuickRead64Bits(pThreadBase, 0x0080);
	g_qwGPR[REG_t6] = QuickRead64Bits(pThreadBase, 0x0088);
	g_qwGPR[REG_t7] = QuickRead64Bits(pThreadBase, 0x0090);
	g_qwGPR[REG_s0] = QuickRead64Bits(pThreadBase, 0x0098);
	g_qwGPR[REG_s1] = QuickRead64Bits(pThreadBase, 0x00a0);
	g_qwGPR[REG_s2] = QuickRead64Bits(pThreadBase, 0x00a8);
	g_qwGPR[REG_s3] = QuickRead64Bits(pThreadBase, 0x00b0);
	g_qwGPR[REG_s4] = QuickRead64Bits(pThreadBase, 0x00b8);
	g_qwGPR[REG_s5] = QuickRead64Bits(pThreadBase, 0x00c0);
	g_qwGPR[REG_s6] = QuickRead64Bits(pThreadBase, 0x00c8);
	g_qwGPR[REG_s7] = QuickRead64Bits(pThreadBase, 0x00d0);
	g_qwGPR[REG_t8] = QuickRead64Bits(pThreadBase, 0x00d8);
	g_qwGPR[REG_t9] = QuickRead64Bits(pThreadBase, 0x00e0);
	g_qwGPR[REG_gp] = QuickRead64Bits(pThreadBase, 0x00e8);
	g_qwGPR[REG_sp] = QuickRead64Bits(pThreadBase, 0x00f0);
	g_qwGPR[REG_s8] = QuickRead64Bits(pThreadBase, 0x00f8);
	g_qwGPR[REG_ra] = QuickRead64Bits(pThreadBase, 0x0100);

	g_qwMultLO = QuickRead64Bits(pThreadBase, offsetof(OSThread, context.lo));
	g_qwMultHI = QuickRead64Bits(pThreadBase, offsetof(OSThread, context.hi));

	// Set the EPC
	g_qwCPR[0][C0_EPC] = (s64)(s32)QuickRead32Bits(pThreadBase, offsetof(OSThread, context.pc));

	// Set the STATUS register. Normally this would trigger a
	// Check for pending interrupts, but we're running in kernel mode
	// So SR_ERL or SR_EXL is probably set. Don't think that a check is
	// necessary

	DWORD dwNewSR = QuickRead32Bits(pThreadBase, offsetof(OSThread, context.sr));

	R4300_SetSR(dwNewSR);

	// Don't restore CAUSE

	// Check if the FP unit was used
	DWORD dwRestoreFP = QuickRead32Bits(pThreadBase, offsetof(OSThread, fp));
	if (dwRestoreFP != 0)
	{
		// Restore control reg
		g_qwCCR[1][31] = QuickRead32Bits(pThreadBase, offsetof(OSThread, context.fpcsr));

		if ((u32)g_qwCPR[0][C0_SR] & SR_FR)
		{
			// Doubles
			for (DWORD dwFPReg = 0; dwFPReg < 16; dwFPReg++)
			{
				g_qwCPR[1][(dwFPReg*2) + 0] = QuickRead64Bits(pThreadBase, 0x0130 + (dwFPReg * 8));
			}
		}
		else
		{	
			// Floats - can probably optimise this to eliminate 64 bits reads...
			for (DWORD dwFPReg = 0; dwFPReg < 16; dwFPReg++)
			{
				u64 qwData;

				qwData = QuickRead64Bits(pThreadBase, 0x0130 + (dwFPReg * 8));

				g_qwCPR[1][(dwFPReg*2) + 0] = (s64)(s32)(qwData & 0xFFFFFFFF);
				g_qwCPR[1][(dwFPReg*2) + 1] = (s64)(s32)(qwData>>32);
			}
		
		}
	}

	// Set interrupt mask...does this do anything???
	DWORD dwRCP = QuickRead32Bits(pThreadBase, 0x0128);

	WORD wTempVal = Read16Bits(VAR_ADDRESS(osDispatchThreadRCPThingamy) + (dwRCP*2));
	Write32Bits(PHYS_TO_K1(MI_INTR_MASK_REG), (u32)wTempVal);		// MI_INTR_MASK_REG

	// Done - when we exit we should ERET
	return PATCH_RET_ERET;
}

// Neither of these are correct- they ignore the interrupt mask thing
DWORD Patch___osDispatchThread_MarioKart()
{
TEST_DISABLE_THREAD_FUNCS
	return Patch___osDispatchThread_Mario();
}
DWORD Patch___osDispatchThread_Rugrats()
{
TEST_DISABLE_THREAD_FUNCS

	DWORD dwThread = Read32Bits(VAR_ADDRESS(osThreadQueue));

	BYTE * pThreadBase = (BYTE *)ReadAddress(dwThread);

	// Update queue to point to next thread:
	Write32Bits(VAR_ADDRESS(osThreadQueue), QuickRead32Bits(pThreadBase, offsetof(OSThread, next)));

	// Set the current active thread:
	Write32Bits(VAR_ADDRESS(osActiveThread), dwThread);

	// Set the current thread's status to OS_STATE_RUNNING:
	Write16Bits(dwThread + offsetof(OSThread, state), OS_STATE_RUNNING);
/*
0x80051ad0: <0x0040d021> ADDU      k0 = v0 + r0
0x80051ad4: <0x8f5b0118> LW        k1 <- 0x0118(k0)*/
	DWORD dwK1 = QuickRead32Bits(pThreadBase, offsetof(OSThread, context.sr));

/*
0x80051ad8: <0x3c088006> LUI       t0 = 0x80060000
0x80051adc: <0x25081880> ADDIU     t0 = t0 + 0x1880
0x80051ae0: <0x8d080000> LW        t0 <- 0x0000(t0)*/
	DWORD dwT0 = Read32Bits(VAR_ADDRESS(osInterruptMaskThingy));

/*
0x80051ae4: <0x3108ff00> ANDI      t0 = t0 & 0xff00
0x80051ae8: <0x3369ff00> ANDI      t1 = k1 & 0xff00
0x80051aec: <0x01284824> AND       t1 = t1 & t0
0x80051af0: <0x3c01ffff> LUI       at = 0xffff0000
0x80051af4: <0x342100ff> ORI       at = at | 0x00ff
0x80051af8: <0x0361d824> AND       k1 = k1 & at
0x80051afc: <0x0369d825> OR        k1 = k1 | t1
0x80051b00: <0x409b6000> MTC0      k1 -> Status*/

	dwT0 &= 0xFF00;
	DWORD dwT1 = dwK1 & dwT0 & 0xFF00;

	dwK1 &= 0xFFFF00FF;
	dwK1 = dwK1 | dwT1;

	R4300_SetSR(dwK1);

	// Restore all registers:
	// For speed, we cache the base pointer!!!
	g_qwGPR[REG_at] = QuickRead64Bits(pThreadBase, 0x0020);
	g_qwGPR[REG_v0] = QuickRead64Bits(pThreadBase, 0x0028);
	g_qwGPR[REG_v1] = QuickRead64Bits(pThreadBase, 0x0030);
	g_qwGPR[REG_a0] = QuickRead64Bits(pThreadBase, 0x0038);
	g_qwGPR[REG_a1] = QuickRead64Bits(pThreadBase, 0x0040);
	g_qwGPR[REG_a2] = QuickRead64Bits(pThreadBase, 0x0048);
	g_qwGPR[REG_a3] = QuickRead64Bits(pThreadBase, 0x0050);
	g_qwGPR[REG_t0] = QuickRead64Bits(pThreadBase, 0x0058);
	g_qwGPR[REG_t1] = QuickRead64Bits(pThreadBase, 0x0060);
	g_qwGPR[REG_t2] = QuickRead64Bits(pThreadBase, 0x0068);
	g_qwGPR[REG_t3] = QuickRead64Bits(pThreadBase, 0x0070);
	g_qwGPR[REG_t4] = QuickRead64Bits(pThreadBase, 0x0078);
	g_qwGPR[REG_t5] = QuickRead64Bits(pThreadBase, 0x0080);
	g_qwGPR[REG_t6] = QuickRead64Bits(pThreadBase, 0x0088);
	g_qwGPR[REG_t7] = QuickRead64Bits(pThreadBase, 0x0090);
	g_qwGPR[REG_s0] = QuickRead64Bits(pThreadBase, 0x0098);
	g_qwGPR[REG_s1] = QuickRead64Bits(pThreadBase, 0x00a0);
	g_qwGPR[REG_s2] = QuickRead64Bits(pThreadBase, 0x00a8);
	g_qwGPR[REG_s3] = QuickRead64Bits(pThreadBase, 0x00b0);
	g_qwGPR[REG_s4] = QuickRead64Bits(pThreadBase, 0x00b8);
	g_qwGPR[REG_s5] = QuickRead64Bits(pThreadBase, 0x00c0);
	g_qwGPR[REG_s6] = QuickRead64Bits(pThreadBase, 0x00c8);
	g_qwGPR[REG_s7] = QuickRead64Bits(pThreadBase, 0x00d0);
	g_qwGPR[REG_t8] = QuickRead64Bits(pThreadBase, 0x00d8);
	g_qwGPR[REG_t9] = QuickRead64Bits(pThreadBase, 0x00e0);
	g_qwGPR[REG_gp] = QuickRead64Bits(pThreadBase, 0x00e8);
	g_qwGPR[REG_sp] = QuickRead64Bits(pThreadBase, 0x00f0);
	g_qwGPR[REG_s8] = QuickRead64Bits(pThreadBase, 0x00f8);
	g_qwGPR[REG_ra] = QuickRead64Bits(pThreadBase, 0x0100);


	g_qwMultLO = QuickRead64Bits(pThreadBase, offsetof(OSThread, context.lo));
	g_qwMultHI = QuickRead64Bits(pThreadBase, offsetof(OSThread, context.hi));

	// Set the EPC
	g_qwCPR[0][C0_EPC] = (s64)(s32)QuickRead32Bits(pThreadBase, offsetof(OSThread, context.pc));


	// Check if the FP unit was used
	DWORD dwRestoreFP = QuickRead32Bits(pThreadBase, offsetof(OSThread, fp));
	if (dwRestoreFP != 0)
	{
		// Restore control reg
		g_qwCCR[1][31] = QuickRead32Bits(pThreadBase, offsetof(OSThread, context.fpcsr));

		if ((u32)g_qwCPR[0][C0_SR] & SR_FR)
		{
			// Doubles
			for (DWORD dwFPReg = 0; dwFPReg < 16; dwFPReg++)
			{
				g_qwCPR[1][(dwFPReg*2) + 0] = QuickRead64Bits(pThreadBase, 0x0130 + (dwFPReg * 8));
			}
		}
		else

		{	
			// Floats - can probably optimise this to eliminate 64 bits reads...
			for (DWORD dwFPReg = 0; dwFPReg < 16; dwFPReg++)
			{
				u64 qwData;

				qwData = QuickRead64Bits(pThreadBase, 0x0130 + (dwFPReg * 8));

				g_qwCPR[1][(dwFPReg*2) + 0] = (s64)(s32)(qwData & 0xFFFFFFFF);
				g_qwCPR[1][(dwFPReg*2) + 1] = (s64)(s32)(qwData>>32);
			}
		
		}


	}
/*
0x80051be4: <0x8f5b0128> LW        k1 <- 0x0128(k0)
0x80051be8: <0x3c1a8006> LUI       k0 = 0x80060000
0x80051bec: <0x275a1880> ADDIU     k0 = k0 + 0x1880
0x80051bf0: <0x8f5a0000> LW        k0 <- 0x0000(k0)
*/
	// Set interrupt mask...does this do anything???
	DWORD dwRCP = QuickRead32Bits(pThreadBase, 0x0128);

	DWORD dwIntMask = Read32Bits(VAR_ADDRESS(osInterruptMaskThingy));


/*
0x80051bf4: <0x001ad402> SRL       k0 = k0 >> 0x0010
0x80051bf8: <0x037ad824> AND       k1 = k1 & k0
0x80051bfc: <0x001bd840> SLL       k1 = k1 << 0x0001
0x80051c00: <0x3c1a8006> LUI       k0 = 0x80060000
0x80051c04: <0x275a6ab0> ADDIU     k0 = k0 + 0x6ab0
0x80051c08: <0x037ad821> ADDU      k1 = k1 + k0
0x80051c0c: <0x977b0000> LHU       k1 <- 0x0000(k1)*/
	dwIntMask >>= 0x10;
	dwRCP = dwRCP & dwIntMask;

	WORD wTempVal = Read16Bits(VAR_ADDRESS(osDispatchThreadRCPThingamy) + (dwRCP*2));


/*
0x80051c10: <0x3c1aa430> LUI       k0 = 0xa4300000
0x80051c14: <0x375a000c> ORI       k0 = k0 | 0x000c
0x80051c18: <0xaf5b0000> SW        k1 -> 0x0000(k0)
0x80051c1c: <0x00000000> NOP
0x80051c20: <0x00000000> NOP
0x80051c24: <0x00000000> NOP
0x80051c28: <0x00000000> NOP
0x80051c2c: <0x42000018> ERET*/

	Write32Bits(PHYS_TO_K1(MI_INTR_MASK_REG), (u32)wTempVal);		// MI_INTR_MASK_REG

	// Done - when we exit we should ERET
	return PATCH_RET_ERET;

}

DWORD Patch_osDestroyThread_Mario()
{
TEST_DISABLE_THREAD_FUNCS

	DWORD dwThread = (u32)g_qwGPR[REG_a0];
	DWORD dwCurrThread;
	DWORD dwNextThread;
	DWORD dwActiveThread;
	WORD wState;


	dwActiveThread = Read32Bits(VAR_ADDRESS(osActiveThread));

	if (dwThread == NULL)
	{
		dwThread = dwActiveThread;
	}
	DBGConsole_Msg(0, "osDestroyThread(0x%08x)", dwThread);


	wState = Read16Bits(dwThread + offsetof(OSThread, state));
	if (wState != OS_STATE_STOPPED)
	{
		DWORD dwQueue = Read32Bits(dwThread + offsetof(OSThread, queue));

		g_qwGPR[REG_a0] = (s64)(s32)dwQueue;
		g_qwGPR[REG_a1] = (s64)(s32)dwThread;

		g___osDequeueThread_s.pFunction();
	}
	
	dwCurrThread = Read32Bits(VAR_ADDRESS(osGlobalThreadList));
	dwNextThread = Read32Bits(dwCurrThread + offsetof(OSThread, tlnext));

	if (dwThread == dwCurrThread)
	{
		Write32Bits(VAR_ADDRESS(osGlobalThreadList), dwNextThread);
	}
	else
	{
		while (dwNextThread != NULL)
		{
			if (dwThread == dwNextThread)
			{
				Write32Bits(dwCurrThread + offsetof(OSThread, tlnext),
					Read32Bits(dwThread + offsetof(OSThread, tlnext)));
				break;
			}

			dwCurrThread = dwNextThread;
			dwNextThread = Read32Bits(dwCurrThread + offsetof(OSThread, tlnext));

		}
	}

	// If we're destorying the active thread, dispatch the next thread
	// Otherwise, just return control to the caller
	if (dwThread == dwActiveThread)
	{
		return g___osDispatchThread_s.pFunction();
	}
	else
	{
		return PATCH_RET_JR_RA;
	}

}

DWORD Patch_osDestroyThread_Zelda()
{
TEST_DISABLE_THREAD_FUNCS

	DWORD dwThread = (u32)g_qwGPR[REG_a0];

	DBGConsole_Msg(0, "osDestroyThread(0x%08x)", dwThread);

	return PATCH_RET_NOT_PROCESSED;
}


DWORD Patch___osEnqueueThread_Mario()
{
TEST_DISABLE_THREAD_FUNCS
	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	DWORD dwThread = (u32)g_qwGPR[REG_a1];
	DWORD dwThreadPri = Read32Bits(dwThread + 0x4);

	//DBGConsole_Msg(0, "osEnqueueThread(queue = 0x%08x, thread = 0x%08x)", dwQueue, dwThread);
	//DBGConsole_Msg(0, "  thread->priority = 0x%08x", dwThreadPri);

	DWORD dw_t9 = dwQueue;

	DWORD dwCurThread = Read32Bits(dw_t9);
	DWORD dwCurThreadPri = Read32Bits(dwCurThread + 0x4);

	//DBGConsole_Msg(0, curthread = 0x%08x, curthread->priority = 0x%08x", dwCurThread, dwCurThreadPri);

	while ((s32)dwCurThreadPri >= (s32)dwThreadPri)
	{
		dw_t9 = dwCurThread;
		dwCurThread = Read32Bits(dwCurThread + 0x0);		// Get next thread
		// Check if dwCurThread is null there?
		dwCurThreadPri = Read32Bits(dwCurThread + 0x4);
		//DBGConsole_Msg(0, "  curthread = 0x%08x, curthread->priority = 0x%08x", dwCurThread, dwCurThreadPri);
	}

	dwCurThread = Read32Bits(dw_t9);
	Write32Bits(dwThread + 0x0, dwCurThread);	// Set thread->next
	Write32Bits(dwThread + 0x8, dwQueue);		// Set thread->queue
	Write32Bits(dw_t9, dwThread);				// Set prevthread->next

	return PATCH_RET_JR_RA;
}

// Identical - just different compilation
DWORD Patch___osEnqueueThread_Rugrats()
{
TEST_DISABLE_THREAD_FUNCS
	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	DWORD dwThread = (u32)g_qwGPR[REG_a1];
	DWORD dwThreadPri = Read32Bits(dwThread + 0x4);

	//DBGConsole_Msg(0, "osEnqueueThread(queue = 0x%08x, thread = 0x%08x)", dwQueue, dwThread);
	//DBGConsole_Msg(0, "  thread->priority = 0x%08x", dwThreadPri);

	DWORD dw_t9 = dwQueue;

	DWORD dwCurThread = Read32Bits(dw_t9);
	DWORD dwCurThreadPri = Read32Bits(dwCurThread + 0x4);

	//DBGConsole_Msg(0, curthread = 0x%08x, curthread->priority = 0x%08x", dwCurThread, dwCurThreadPri);

	while ((s32)dwCurThreadPri >= (s32)dwThreadPri)
	{
		dw_t9 = dwCurThread;
		dwCurThread = Read32Bits(dwCurThread + 0x0);		// Get next thread
		// Check if dwCurThread is null there?
		dwCurThreadPri = Read32Bits(dwCurThread + 0x4);
		//DBGConsole_Msg(0, "  curthread = 0x%08x, curthread->priority = 0x%08x", dwCurThread, dwCurThreadPri);
	}

	dwCurThread = Read32Bits(dw_t9);
	Write32Bits(dwThread + 0x0, dwCurThread);	// Set thread->next
	Write32Bits(dwThread + 0x8, dwQueue);		// Set thread->queue
	Write32Bits(dw_t9, dwThread);				// Set prevthread->next

	return PATCH_RET_JR_RA;
}



// Gets active thread in a1. Adds to queue in a0 (if specified), dispatches
DWORD Patch___osEnqueueAndYield_Mario()
{
TEST_DISABLE_THREAD_FUNCS
	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	// Get the active thread
	DWORD dwThread = Read32Bits(VAR_ADDRESS(osActiveThread));
	BYTE * pThreadBase = (BYTE *)WriteAddress(dwThread);

	//DBGConsole_Msg(0, "EnqueueAndYield()");


	// Set a1 (necessary if we call osEnqueueThread
	g_qwGPR[REG_a1] = (s64)(s32)dwThread;


	// Store various registers:
	// For speed, we cache the base pointer!!!

	DWORD dwStatus = (u32)g_qwCPR[0][C0_SR];

	dwStatus |= SR_EXL;
	
	QuickWrite32Bits(pThreadBase, 0x118, dwStatus);
	
	QuickWrite64Bits(pThreadBase, 0x0098, g_qwGPR[REG_s0]);
	QuickWrite64Bits(pThreadBase, 0x00a0, g_qwGPR[REG_s1]);
	QuickWrite64Bits(pThreadBase, 0x00a8, g_qwGPR[REG_s2]);
	QuickWrite64Bits(pThreadBase, 0x00b0, g_qwGPR[REG_s3]);
	QuickWrite64Bits(pThreadBase, 0x00b8, g_qwGPR[REG_s4]);
	QuickWrite64Bits(pThreadBase, 0x00c0, g_qwGPR[REG_s5]);
	QuickWrite64Bits(pThreadBase, 0x00c8, g_qwGPR[REG_s6]);
	QuickWrite64Bits(pThreadBase, 0x00d0, g_qwGPR[REG_s7]);

	QuickWrite64Bits(pThreadBase, 0x00e8, g_qwGPR[REG_gp]);
	QuickWrite64Bits(pThreadBase, 0x00f0, g_qwGPR[REG_sp]);
	QuickWrite64Bits(pThreadBase, 0x00f8, g_qwGPR[REG_s8]);
	QuickWrite64Bits(pThreadBase, 0x0100, g_qwGPR[REG_ra]);

	QuickWrite32Bits(pThreadBase, 0x011c, (u32)g_qwGPR[REG_ra]);

	// Check if the FP unit was used
	DWORD dwRestoreFP = QuickRead32Bits(pThreadBase, 0x0018);
	if (dwRestoreFP != 0)
	{
		// Save control reg
		QuickWrite32Bits(pThreadBase, 0x012c, (u32)g_qwCCR[1][31]);

		if ((u32)g_qwCPR[0][C0_SR] & SR_FR)
		{
			// Doubles
			for (DWORD dwFPReg = 0; dwFPReg < 16; dwFPReg++)
			{
				QuickWrite64Bits(pThreadBase, 0x0130 + (dwFPReg * 8), g_qwCPR[1][(dwFPReg*2) + 0]);
			}
		}
		else
		{	
			// Floats - can probably optimise this to eliminate 64 bits writes...
			for (DWORD dwFPReg = 0; dwFPReg < 16; dwFPReg++)
			{

				// Check this
				u64 qwTemp; 
				qwTemp = ( (g_qwCPR[1][(dwFPReg*2)+1]<<32) |
					       (g_qwCPR[1][(dwFPReg*2)+0]&0xFFFFFFFF) );

				QuickWrite64Bits(pThreadBase, 0x0130 + (dwFPReg * 8), qwTemp);
			}
		}
	}
	
	// Set interrupt mask...does this do anything???
	DWORD dwRCP = Read32Bits(PHYS_TO_K1(MI_INTR_MASK_REG));
	QuickWrite32Bits(pThreadBase, 0x128,  dwRCP);

	// Call EnqueueThread if queue is set
	if (dwQueue != 0)
	{
		//a0/a1 set already
		g___osEnqueueThread_s.pFunction();
	}

	return g___osDispatchThread_s.pFunction();


}



DWORD Patch___osEnqueueAndYield_MarioKart()
{
TEST_DISABLE_THREAD_FUNCS
	return Patch___osEnqueueAndYield_Mario();

}


DWORD Patch_osStartThread()
{
TEST_DISABLE_THREAD_FUNCS
	DWORD dwThread = (u32)g_qwGPR[REG_a0];

	// Disable interrupts

	//DBGConsole_Msg(0, "osStartThread(0x%08x)", dwThread)

	DWORD dwThreadState = Read16Bits(dwThread + 0x10);

	if (dwThreadState == OS_STATE_WAITING)
	{
		//DBGConsole_Msg(0, "  Thread is WAITING");

		Write16Bits(dwThread + 0x10, OS_STATE_RUNNABLE);

		g_qwGPR[REG_a0] = (s64)(s32)VAR_ADDRESS(osThreadQueue);
		g_qwGPR[REG_a1] = (s64)(s32)dwThread;

		g___osEnqueueThread_s.pFunction();
	}
	else if (dwThreadState == OS_STATE_STOPPED)
	{
		//DBGConsole_Msg(0, "  Thread is STOPPED");
		
		DWORD dwQueue = Read32Bits(dwThread + 0x08);

		if (dwQueue == NULL || dwQueue == VAR_ADDRESS(osThreadQueue))
		{
			//if (dwQueue == NULL)
				//DBGConsole_Msg(0, "  Thread has NULL queue");
			//else
				//DBGConsole_Msg(0, "  Thread's queue is VAR_ADDRESS(osThreadQueue)");
			
			Write16Bits(dwThread + 0x10, OS_STATE_RUNNABLE);
			
			g_qwGPR[REG_a0] = (s64)(s32)VAR_ADDRESS(osThreadQueue);
			g_qwGPR[REG_a1] = (s64)(s32)dwThread;

			g___osEnqueueThread_s.pFunction();
		}
		else
		{
			//DBGConsole_Msg(0, "  Thread has it's own queue");

			Write16Bits(dwThread + 0x10, OS_STATE_WAITING);
			
			g_qwGPR[REG_a0] = (s64)(s32)dwQueue;
			g_qwGPR[REG_a1] = (s64)(s32)dwThread;

			g___osEnqueueThread_s.pFunction();

			// Pop the highest priority thread from the queue
			DWORD dwNewThread = Read32Bits(dwQueue + 0x0);
			Write32Bits(dwQueue, Read32Bits(dwNewThread + 0x0));

			// Enqueue the next thread to run
			g_qwGPR[REG_a0] = (s64)(s32)VAR_ADDRESS(osThreadQueue);
			g_qwGPR[REG_a1] = (s64)(s32)dwNewThread;

			g___osEnqueueThread_s.pFunction();
		}
	} 
	else
	{
		DBGConsole_Msg(0, "  Thread is neither WAITING nor STOPPED");
	}

	// At this point, we check the priority of the current
	// thread and the highest priority thread on the thread queue. If
	// the current thread has a higher priority, nothing happens, else
	// the new thread is started

	DWORD dwActiveThread = Read32Bits(VAR_ADDRESS(osActiveThread));

	if (dwActiveThread == NULL)
	{
		// There is no currently active thread
		//DBGConsole_Msg(0, "  No active thread, dispatching");

		return g___osDispatchThread_s.pFunction();

	}
	else
	{
		// A thread is currently active
		DWORD dwQueueThread = Read32Bits(VAR_ADDRESS(osThreadQueue));

		DWORD dwQueueThreadPri = Read32Bits(dwQueueThread + 0x4);
		DWORD dwActiveThreadPri = Read32Bits(dwActiveThread + 0x4);

		if (dwActiveThreadPri < dwQueueThreadPri)
		{
			//DBGConsole_Msg(0, "  New thread has higher priority, enqueue/yield");			
			
			// Set the active thread's state to RUNNABLE
			Write16Bits(dwActiveThread + 0x10, OS_STATE_RUNNABLE);

			g_qwGPR[REG_a0] = (s64)(s32)VAR_ADDRESS(osThreadQueue);

			// Doing this is ok, because when the active thread is resumed, it will resume
			// after the call to osStartThread(). We don't do any processing after this
			// event (we don't bother with interrupts)
			return g___osEnqueueAndYield_s.pFunction();
		}
		else
		{
			//DBGConsole_Msg(0, "  Thread has lower priority, continuing with active thread");
		}

	}

	// Restore interrupts?

	return PATCH_RET_JR_RA;	
}






DWORD Patch___osPopThread()
{
TEST_DISABLE_THREAD_FUNCS
	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	DWORD dwThread = Read32Bits(dwQueue + 0x0);

	g_qwGPR[REG_v0] = (s64)(s32)dwThread;

	Write32Bits(dwQueue, Read32Bits(dwThread + 0x0));

	//DBGConsole_Msg(0, "0x%08x = __osPopThread(0x%08x)", dwThread, dwQueue);

	return PATCH_RET_JR_RA;
}



