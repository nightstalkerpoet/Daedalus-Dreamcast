#define TEST_DISABLE_SI_FUNCS //return PATCH_RET_NOT_PROCESSED;


DWORD Patch___osSiCreateAccessQueue()
{
TEST_DISABLE_SI_FUNCS
	Write32Bits(VAR_ADDRESS(osSiAccessQueueCreated), 1);

	DBGConsole_Msg(0, "Creating Si Access Queue");

	OS_HLE_osCreateMesgQueue(VAR_ADDRESS(osSiAccessQueue), VAR_ADDRESS(osSiAccessQueueBuffer), 1);

	DWORD dwQueue = (u32)g_qwGPR[REG_a0];
	DWORD dwMsg = (u32)g_qwGPR[REG_a1];
	DWORD dwBlockFlag = (u32)g_qwGPR[REG_a2];

	g_qwGPR[REG_a0] = (s64)(s32)VAR_ADDRESS(osSiAccessQueue);
	g_qwGPR[REG_a1] = 0;		// Msg value is unimportant
	g_qwGPR[REG_a2] = (s64)(s32)OS_MESG_NOBLOCK;
	

	return Patch_osSendMesg();

	//return PATCH_RET_JR_RA;
}


DWORD Patch___osSiGetAccess()
{
TEST_DISABLE_SI_FUNCS
	DWORD dwCreated = Read32Bits(VAR_ADDRESS(osSiAccessQueueCreated));

	if (dwCreated == 0)
	{
		Patch___osSiCreateAccessQueue();	// Ignore return
	}
	
	g_qwGPR[REG_a0] = (s64)(s32)VAR_ADDRESS(osSiAccessQueue);
	g_qwGPR[REG_a1] = (s64)g_qwGPR[REG_sp] - 4;		// Place on stack and ignore
	g_qwGPR[REG_a2] = (s64)(s32)OS_MESG_BLOCK;
	

	return Patch_osRecvMesg();
}

DWORD Patch___osSiRelAccess()
{
TEST_DISABLE_SI_FUNCS
	g_qwGPR[REG_a0] = (s64)(s32)VAR_ADDRESS(osSiAccessQueue);
	g_qwGPR[REG_a1] = (s64)0;		// Place on stack and ignore
	g_qwGPR[REG_a2] = (s64)(s32)OS_MESG_NOBLOCK;
	
	return Patch_osSendMesg();
}

