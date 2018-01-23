#define TEST_DISABLE_PI_FUNCS //return PATCH_RET_NOT_PROCESSED;



DWORD Patch___osPiCreateAccessQueue()
{
TEST_DISABLE_PI_FUNCS
	Write32Bits(VAR_ADDRESS(osPiAccessQueueCreated), 1);

	DBGConsole_Msg(0, "Creating Pi Access Queue");

	OS_HLE_osCreateMesgQueue(VAR_ADDRESS(osPiAccessQueue), VAR_ADDRESS(osPiAccessQueueBuffer), 1);

	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	DWORD dwMsg = (u32)g_qwGPR[REG_a1];
	DWORD dwBlockFlag = (u32)g_qwGPR[REG_a2];

	g_qwGPR[REG_a0] = (s64)(s32)VAR_ADDRESS(osPiAccessQueue);
	g_qwGPR[REG_a1] = 0;		// Msg value is unimportant
	g_qwGPR[REG_a2] = (s64)(s32)OS_MESG_NOBLOCK;
	

	return Patch_osSendMesg();

	//return PATCH_RET_JR_RA;
}



DWORD Patch___osPiGetAccess()
{
TEST_DISABLE_PI_FUNCS
	DWORD dwCreated = Read32Bits(VAR_ADDRESS(osPiAccessQueueCreated));

	if (dwCreated == 0)
	{
		Patch___osPiCreateAccessQueue();	// Ignore return
	}
	
	g_qwGPR[REG_a0] = (s64)(s32)VAR_ADDRESS(osPiAccessQueue);
	g_qwGPR[REG_a1] = (s64)g_qwGPR[REG_sp] - 4;		// Place on stack and ignore
	g_qwGPR[REG_a2] = (s64)(s32)OS_MESG_BLOCK;
	

	return Patch_osRecvMesg();
}

DWORD Patch___osPiRelAccess()
{
TEST_DISABLE_PI_FUNCS
	g_qwGPR[REG_a0] = (s64)(s32)VAR_ADDRESS(osPiAccessQueue);
	g_qwGPR[REG_a1] = (s64)0;		// Place on stack and ignore
	g_qwGPR[REG_a2] = (s64)(s32)OS_MESG_NOBLOCK;
	
	return Patch_osSendMesg();
}


