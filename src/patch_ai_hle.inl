
#define TEST_DISABLE_AI_FUNCS //return PATCH_RET_NOT_PROCESSED;



DWORD Patch_osAiGetLength()
{
TEST_DISABLE_AI_FUNCS
	return PATCH_RET_NOT_PROCESSED;
	DWORD dwLen = Read32Bits(PHYS_TO_K1(AI_LEN_REG));

	//DBGConsole_Msg(0, "osAiGetLength()");

	g_qwGPR[REG_v0] = (s64)(s32)dwLen;
	return PATCH_RET_JR_RA;
}


DWORD Patch_osAiSetNextBuffer()
{
TEST_DISABLE_AI_FUNCS
	return PATCH_RET_NOT_PROCESSED;
	DWORD dwBuffer = (u32)g_qwGPR[REG_a0];
	DWORD dwLen = (u32)g_qwGPR[REG_a1];

	//DBGConsole_Msg(0, "osAiSetNextBuffer(0x%08x, %d (0x%04x))", dwBuffer, dwLen, dwLen);

	g_qwGPR[REG_v1] = 0;

	return PATCH_RET_JR_RA;
}

////// FIXME: Not implemented fully
DWORD Patch_osAiSetFrequency()
{
TEST_DISABLE_AI_FUNCS
	return PATCH_RET_NOT_PROCESSED;
	DWORD dwFreq = (u32)g_qwGPR[REG_a0];

	DBGConsole_Msg(0, "osAiSetFrequency(%d)", dwFreq);

	// What does this need setting to?
	g_qwGPR[REG_v1] = 0;

	return PATCH_RET_JR_RA;
}
