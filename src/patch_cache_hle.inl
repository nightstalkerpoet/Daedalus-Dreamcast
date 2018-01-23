#define TEST_DISABLE_CACHE_FUNCS //return PATCH_RET_NOT_PROCESSED;



DWORD Patch_osInvalICache_Mario()
{
TEST_DISABLE_CACHE_FUNCS
	DWORD dwP = (u32)g_qwGPR[REG_a0];
	DWORD dwLen = (u32)g_qwGPR[REG_a1];

	//DBGConsole_Msg(0, "osInvalICache(0x%08x, %d)", dwP, dwLen);

	return PATCH_RET_JR_RA;
}
DWORD Patch_osInvalICache_Rugrats()
{
TEST_DISABLE_CACHE_FUNCS
	DWORD dwP = (u32)g_qwGPR[REG_a0];
	DWORD dwLen = (u32)g_qwGPR[REG_a1];

	//DBGConsole_Msg(0, "osInvalICache(0x%08x, %d)", dwP, dwLen);

	return PATCH_RET_JR_RA;
}	


DWORD Patch_osInvalDCache_Mario()
{
TEST_DISABLE_CACHE_FUNCS
	DWORD dwP = (u32)g_qwGPR[REG_a0];
	DWORD dwLen = (u32)g_qwGPR[REG_a1];

	//DBGConsole_Msg(0, "osInvalDCache(0x%08x, %d)", dwP, dwLen);

	return PATCH_RET_JR_RA;
}
DWORD Patch_osInvalDCache_Rugrats()
{
TEST_DISABLE_CACHE_FUNCS
	DWORD dwP = (u32)g_qwGPR[REG_a0];
	DWORD dwLen = (u32)g_qwGPR[REG_a1];

	//DBGConsole_Msg(0, "osInvalDCache(0x%08x, %d)", dwP, dwLen);

	return PATCH_RET_JR_RA;
}	


DWORD Patch_osWritebackDCache_Mario()
{
TEST_DISABLE_CACHE_FUNCS
	DWORD dwP = (u32)g_qwGPR[REG_a0];
	DWORD dwLen = (u32)g_qwGPR[REG_a1];

	//DBGConsole_Msg(0, "osWritebackDCache(0x%08x, %d)", dwP, dwLen);

	return PATCH_RET_JR_RA;
}
DWORD Patch_osWritebackDCache_Rugrats()
{
TEST_DISABLE_CACHE_FUNCS
	DWORD dwP = (u32)g_qwGPR[REG_a0];
	DWORD dwLen = (u32)g_qwGPR[REG_a1];

	//DBGConsole_Msg(0, "osWritebackDCache(0x%08x, %d)", dwP, dwLen);

	return PATCH_RET_JR_RA;
}	


DWORD Patch_osWritebackDCacheAll()
{
TEST_DISABLE_CACHE_FUNCS
	//DBGConsole_Msg(0, "osWritebackDCacheAll()");

	return PATCH_RET_JR_RA;
}
	
	
