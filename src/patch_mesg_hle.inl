#define TEST_DISABLE_MESG_FUNCS //return PATCH_RET_NOT_PROCESSED;




DWORD Patch_osSetEventMesg_Mario()
{
TEST_DISABLE_MESG_FUNCS
	DWORD dwEvent = (u32)g_qwGPR[REG_a0];
	DWORD dwQueue = (u32)g_qwGPR[REG_a1];
	DWORD dwMsg = (u32)g_qwGPR[REG_a2];

	/*if (dwEvent < 23)
	{
		DBGConsole_Msg(0, "osSetEventMesg(%s, 0x%08x, 0x%08x)", 
			g_szEventStrings[dwEvent], dwQueue, dwMsg);
	}
	else
	{
		DBGConsole_Msg(0, "osSetEventMesg(%d, 0x%08x, 0x%08x)", 
			dwEvent, dwQueue, dwMsg);
	}*/
		

	DWORD dwP = VAR_ADDRESS(osEventMesgArray) + (dwEvent * 8);

	Write32Bits(dwP + 0x0, dwQueue);
	Write32Bits(dwP + 0x4, dwMsg);

	return PATCH_RET_JR_RA;
}

DWORD Patch_osSetEventMesg_Zelda()
{
TEST_DISABLE_MESG_FUNCS
	return PATCH_RET_NOT_PROCESSED;

	DWORD dwEvent = (u32)g_qwGPR[REG_a0];
	DWORD dwQueue = (u32)g_qwGPR[REG_a1];
	DWORD dwMsg = (u32)g_qwGPR[REG_a2];

	// TODO: Odd osSendMesg stuff

	/*if (dwEvent < 23)
	{
		DBGConsole_Msg(0, "osSetEventMesg(%s, 0x%08x, 0x%08x)", 
			g_szEventStrings[dwEvent], dwQueue, dwMsg);
	}
	else
	{
		DBGConsole_Msg(0, "osSetEventMesg(%d, 0x%08x, 0x%08x)", 
			dwEvent, dwQueue, dwMsg);
	}*/
		

	DWORD dwP = VAR_ADDRESS(osEventMesgArray) + (dwEvent * 8);

	Write32Bits(dwP + 0x0, dwQueue);
	Write32Bits(dwP + 0x4, dwMsg);

	return PATCH_RET_JR_RA;
}


DWORD Patch_osCreateMesgQueue_Mario()
{
TEST_DISABLE_MESG_FUNCS
	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	DWORD dwMsgBuf = (u32)g_qwGPR[REG_a1];
	DWORD dwMsgCount = (u32)g_qwGPR[REG_a2];

	OS_HLE_osCreateMesgQueue(dwQueue, dwMsgBuf, dwMsgCount);

	return PATCH_RET_JR_RA;
}
// Exactly the same - just optimised slightly
DWORD Patch_osCreateMesgQueue_Rugrats()
{
TEST_DISABLE_MESG_FUNCS
	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	DWORD dwMsgBuf = (u32)g_qwGPR[REG_a1];
	DWORD dwMsgCount = (u32)g_qwGPR[REG_a2];

	OS_HLE_osCreateMesgQueue(dwQueue, dwMsgBuf, dwMsgCount);

	return PATCH_RET_JR_RA;
}


DWORD Patch_osRecvMesg()
{
TEST_DISABLE_MESG_FUNCS
	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	DWORD dwMsg = (u32)g_qwGPR[REG_a1];
	DWORD dwBlockFlag = (u32)g_qwGPR[REG_a2];

	DWORD dwValidCount = Read32Bits(dwQueue + 0x8);
	DWORD dwMsgCount = Read32Bits(dwQueue + 0x10);

	/*if (dwQueue == 0x80007d40)
	{
	DBGConsole_Msg(0, "Thread: 0x%08x", Read32Bits(VAR_ADDRESS(osActiveThread)));
	DBGConsole_Msg(0, "osRecvMsg(0x%08x, 0x%08x, %s) (%d/%d pending)", 
		dwQueue, dwMsg, dwBlockFlag == OS_MESG_BLOCK ? "Block" : "Don't Block",
		dwValidCount, dwMsgCount);
	}*/

	// If there are no valid messages, then we either block until 
	// one becomes available, or return immediately
	if (dwValidCount == 0)
	{
		if (dwBlockFlag == OS_MESG_NOBLOCK)
		{
			// Don't block
			g_qwGPR[REG_v0] = (s64)(s32)~0;
			return PATCH_RET_JR_RA;
		}
		else
		{
			// We can't handle, as this would result in a yield (tricky)
			return PATCH_RET_NOT_PROCESSED;
		}
	}

	//DBGConsole_Msg(0, "  Processing Pending");

	DWORD dwFirst = Read32Bits(dwQueue + 0x0c);
	
	//Store message in pointer
	if (dwMsg != NULL)
	{
		//DBGConsole_Msg(0, "  Retrieving message");
		
		DWORD dwMsgBase = Read32Bits(dwQueue + 0x14);

		// Offset to first valid message
		dwMsgBase += dwFirst * 4;

		Write32Bits(dwMsg, Read32Bits(dwMsgBase));

	}


	// Point first to the next valid message
	if (dwMsgCount == 0)
	{
		DBGConsole_Msg(0, "Invalid message count");
		// We would break here!
	}
	else if (dwMsgCount == ~0 && dwFirst+1 == 0x80000000)
	{
		DBGConsole_Msg(0, "Invalid message count/first");
		// We would break here!
	}
	else
	{
		//DBGConsole_Msg(0, "  Generating next valid message number");
		dwFirst = (dwFirst + 1) % dwMsgCount;

		Write32Bits(dwQueue + 0x0c, dwFirst);
	}

	// Decrease the number of valid messages
	dwValidCount--;

	Write32Bits(dwQueue + 0x8, dwValidCount);

	// Start thread pending on the fullqueue
	DWORD dwFullQueueThread = Read32Bits(dwQueue + 0x04);
	DWORD dwNextThread = Read32Bits(dwFullQueueThread + 0x00);



	// If the first thread is not the idle thread, start it
	if (dwNextThread != 0)
	{
		//DBGConsole_Msg(0, "  Activating sleeping thread");

		// From Patch___osPopThread():
		Write32Bits(dwQueue + 0x04, dwNextThread);

		g_qwGPR[REG_a0] = (s64)(s32)dwFullQueueThread;

		// FIXME - How to we set the status flag here?
		return Patch_osStartThread();
	}

	// Set success status
	g_qwGPR[REG_v0] = 0;

	return PATCH_RET_JR_RA;
}



DWORD Patch_osSendMesg()
{
TEST_DISABLE_MESG_FUNCS
	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	DWORD dwMsg = (u32)g_qwGPR[REG_a1];
	DWORD dwBlockFlag = (u32)g_qwGPR[REG_a2];

	DWORD dwValidCount = Read32Bits(dwQueue + 0x8);
	DWORD dwMsgCount = Read32Bits(dwQueue + 0x10);
	
	/*if (dwQueue == 0x80007d40)
	{
		DBGConsole_Msg(0, "Thread: 0x%08x", Read32Bits(VAR_ADDRESS(osActiveThread)));
	DBGConsole_Msg(0, "osSendMsg(0x%08x, 0x%08x, %s) (%d/%d pending)", 
		dwQueue, dwMsg, dwBlockFlag == OS_MESG_BLOCK ? "Block" : "Don't Block",
		dwValidCount, dwMsgCount);
	}*/

	// If the message queue is full, then we either block until 
	// space becomes available, or return immediately
	if (dwValidCount >= dwMsgCount)
	{
		if (dwBlockFlag == OS_MESG_NOBLOCK)
		{
			// Don't block
			g_qwGPR[REG_v0] = (s64)(s32)~0;
			return PATCH_RET_JR_RA;
		}
		else
		{
			// We can't handle, as this would result in a yield (tricky)
			return PATCH_RET_NOT_PROCESSED;
		}
	}

	//DBGConsole_Msg(0, "  Processing Pending");

	DWORD dwFirst = Read32Bits(dwQueue + 0x0c);
	
	// Point first to the next valid message
	if (dwMsgCount == 0)
	{
		DBGConsole_Msg(0, "Invalid message count");
		// We would break here!
	}
	else if (dwMsgCount == ~0 && dwFirst+dwValidCount == 0x80000000)
	{
		DBGConsole_Msg(0, "Invalid message count/first");
		// We would break here!
	}
	else
	{
		DWORD dwSlot = (dwFirst + dwValidCount) % dwMsgCount;
		
		DWORD dwMsgBase = Read32Bits(dwQueue + 0x14);

		// Offset to first valid message
		dwMsgBase += dwSlot * 4;

		Write32Bits(dwMsgBase, dwMsg);

	}
	
	// Increase the number of valid messages
	dwValidCount++;

	Write32Bits(dwQueue + 0x8, dwValidCount);

	// Start thread pending on the fullqueue
	DWORD dwEmptyQueueThread = Read32Bits(dwQueue + 0x00);
	DWORD dwNextThread = Read32Bits(dwEmptyQueueThread + 0x00);


	// If the first thread is not the idle thread, start it
	if (dwNextThread != 0)
	{
		//DBGConsole_Msg(0, "  Activating sleeping thread");

		// From Patch___osPopThread():
		Write32Bits(dwQueue + 0x00, dwNextThread);

		g_qwGPR[REG_a0] = (s64)(s32)dwEmptyQueueThread;

		// FIXME - How to we set the status flag here?
		return Patch_osStartThread();
	}

	// Set success status
	g_qwGPR[REG_v0] = 0;

	return PATCH_RET_JR_RA;
}
