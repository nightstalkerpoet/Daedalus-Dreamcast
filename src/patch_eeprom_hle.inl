#define TEST_DISABLE_EEPROM_FUNCS //return PATCH_RET_NOT_PROCESSED;

DWORD Patch___osEepStatus()
{
TEST_DISABLE_EEPROM_FUNCS
	// Return status of Eeeprom in OSContStatus struct passed in a1.
	// a0 is the message queue to block on, and is ignored

	DWORD dwMQ         = (u32)g_qwGPR[REG_a0];
	DWORD dwContStatus = (u32)g_qwGPR[REG_a1];

	//DBGConsole_Msg(0, "osEepStatus(0x%08x, 0x%08x), ra = 0x%08x", dwMQ, dwContStatus, (u32)g_qwGPR[REG_ra]);

	// Set up dwContStatus values
	if (g_bEepromPresent)
	{
		Write16Bits(dwContStatus + 0, CONT_EEPROM);		// type
		Write8Bits(dwContStatus + 2, 0);				// status
		Write8Bits(dwContStatus + 3, 0);				// errno

		g_qwGPR[REG_v0] = 0;
	}
	else
	{
		Write16Bits(dwContStatus + 0, CONT_EEPROM);				// type
		Write8Bits(dwContStatus + 2, 0);						// status
		Write8Bits(dwContStatus + 3, CONT_NO_RESPONSE_ERROR);	// errno
		g_qwGPR[REG_v0] = CONT_NO_RESPONSE_ERROR;

	}
	return PATCH_RET_JR_RA;
}


DWORD Patch_osEepromProbe()
{
TEST_DISABLE_EEPROM_FUNCS
	// Returns 1 on EEPROM detected, 0 on error/no eeprom
	//DBGConsole_Msg(0, "osEepromProbe(), ra = 0x%08x", (u32)g_qwGPR[REG_ra]);
	
	// No EEPROM
	if (g_bEepromPresent)
		g_qwGPR[REG_v0] = 1;
	else
		g_qwGPR[REG_v0] = 0;

	// Side effect From osEepStatus
	//Write32Bits(VAR_ADDRESS(osEepPifThingamy2), 5);

	return PATCH_RET_JR_RA;
}

DWORD Patch_osEepromRead()
{
TEST_DISABLE_EEPROM_FUNCS
	// s32 osEepromRead(OSMesgQueue * mq, u8 page, u8 * buf);
	DWORD dwMQ   = (u32)g_qwGPR[REG_a0];
	DWORD dwPage = (u32)g_qwGPR[REG_a1];
	DWORD dwBuf  = (u32)g_qwGPR[REG_a2];


	//DBGConsole_Msg(0, "osEepromRead(0x%08x, 0x%08x, 0x%08x), ra = 0x%08x",
	//	dwMQ, dwPage, dwBuf, (u32)g_qwGPR[REG_ra]);
	
	return PATCH_RET_NOT_PROCESSED;
}


DWORD Patch_osEepromLongRead()
{
TEST_DISABLE_EEPROM_FUNCS
	DWORD dwMQ   = (u32)g_qwGPR[REG_a0];
	DWORD dwPage = (u32)g_qwGPR[REG_a1];
	DWORD dwBuf  = (u32)g_qwGPR[REG_a2];
	DWORD dwLen  = (u32)g_qwGPR[REG_a3];


	//DBGConsole_Msg(0, "[WosEepromLongRead(0x%08x, %d, 0x%08x, 0x%08x), ra = 0x%08x]",
	//	dwMQ, dwPage, dwBuf, dwLen, (u32)g_qwGPR[REG_ra]);
	
	return PATCH_RET_NOT_PROCESSED;

}

DWORD Patch_osEepromWrite()
{
TEST_DISABLE_EEPROM_FUNCS
	// s32 osEepromWrite(OSMesgQueue * mq, u8 page, u8 * buf);
	DWORD dwMQ   = (u32)g_qwGPR[REG_a0];
	DWORD dwPage = (u32)g_qwGPR[REG_a1];
	DWORD dwBuf  = (u32)g_qwGPR[REG_a2];

	DWORD dwA = Read32Bits(dwBuf + 0);
	DWORD dwB = Read32Bits(dwBuf + 4);


	//DBGConsole_Msg(0, "osEepromWrite(0x%08x, %d, [0x%08x] = 0x%08x%08x), ra = 0x%08x",
	//	dwMQ, dwPage, dwBuf, dwA, dwB, (u32)g_qwGPR[REG_ra]);
	
	return PATCH_RET_NOT_PROCESSED;
}

DWORD Patch_osEepromLongWrite()
{
TEST_DISABLE_EEPROM_FUNCS
	DWORD dwMQ   = (u32)g_qwGPR[REG_a0];
	DWORD dwPage = (u32)g_qwGPR[REG_a1];
	DWORD dwBuf  = (u32)g_qwGPR[REG_a2];
	DWORD dwLen  = (u32)g_qwGPR[REG_a3];


	//DBGConsole_Msg(0, "[WosEepromLongWrite(0x%08x, %d, 0x%08x, 0x%08x), ra = 0x%08x]",
	//	dwMQ, dwPage, dwBuf, dwLen, (u32)g_qwGPR[REG_ra]);


	return PATCH_RET_NOT_PROCESSED;

}
