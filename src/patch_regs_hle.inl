#define TEST_DISABLE_REG_FUNCS //return PATCH_RET_NOT_PROCESSED;



DWORD Patch___osDisableInt_Mario()
{
TEST_DISABLE_REG_FUNCS
	DWORD dwCurrSR = (u32)g_qwCPR[0][C0_SR];

	g_qwCPR[0][C0_SR] = dwCurrSR & ~SR_IE;
	g_qwGPR[REG_v0] = dwCurrSR & SR_IE;

	return PATCH_RET_JR_RA;
}

DWORD Patch___osDisableInt_Zelda()
{
TEST_DISABLE_REG_FUNCS
	// Same as the above
	DWORD dwCurrSR = (u32)g_qwCPR[0][C0_SR];

	g_qwCPR[0][C0_SR] = dwCurrSR & ~SR_IE;
	g_qwGPR[REG_v0] = dwCurrSR & SR_IE;

	return PATCH_RET_JR_RA;
}

DWORD Patch___osRestoreInt()
{
TEST_DISABLE_REG_FUNCS
	g_qwCPR[0][C0_SR] |= g_qwGPR[REG_a0];

	// Do this check?
	/*if (g_qwGPR[REG_a0] & SR_IE) {
		if (g_qwGPR[REG_a0] & CAUSE_IPMASK) {
			AddCPUJob(CPU_CHECK_POSTPONED_INTERRUPTS);
		} 
	}*/

	return PATCH_RET_JR_RA;
}


DWORD Patch_osGetCount()
{
TEST_DISABLE_REG_FUNCS
	// Why is this 32bit? See R4300.cpp
	g_qwGPR[REG_v0] = (s64)(s32)g_qwCPR[0][C0_COUNT];	

	return PATCH_RET_JR_RA;
}


DWORD Patch___osGetCause()
{
TEST_DISABLE_REG_FUNCS
	// Why is this 32bit? See R4300.cpp
	g_qwGPR[REG_v0] = (s64)(s32)g_qwCPR[0][C0_CAUSE];	

	return PATCH_RET_JR_RA;
}


DWORD Patch___osSetCompare()
{
TEST_DISABLE_REG_FUNCS

	CPU_SetCompare(g_qwGPR[REG_a0]);

	return PATCH_RET_JR_RA;
}


DWORD Patch___osSetSR()
{
TEST_DISABLE_REG_FUNCS

	R4300_SetSR((u32)g_qwGPR[REG_a0]);

	//DBGConsole_Msg(0, "__osSetSR()");
	
	return PATCH_RET_JR_RA;
}

DWORD Patch___osGetSR()
{
TEST_DISABLE_REG_FUNCS
	// Why is this 32bit?
	g_qwGPR[REG_v0] = (s64)(s32)g_qwCPR[0][C0_SR];

	//DBGConsole_Msg(0, "__osGetSR()");
	
	return PATCH_RET_JR_RA;
}

DWORD Patch___osSetFpcCsr()
{
TEST_DISABLE_REG_FUNCS
	// Why is the CFC1 32bit?
	g_qwGPR[REG_v0] = (s64)(s32)g_qwCCR[1][31];
	g_qwCCR[1][31] = g_qwGPR[REG_a0];
	
	DBGConsole_Msg(0, "__osSetFpcCsr()");

	return PATCH_RET_JR_RA;
}

