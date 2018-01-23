#define TEST_DISABLE_TLB_FUNCS //return PATCH_RET_NOT_PROCESSED;

DWORD Patch_osMapTLB()
{
TEST_DISABLE_TLB_FUNCS
	//osMapTLB(s32, OSPageMask, void *, u32, u32, s32)
	u32 dwW = (u32)g_qwGPR[REG_a0];
	u32 dwX = (u32)g_qwGPR[REG_a1];
	u32 dwY = (u32)g_qwGPR[REG_a2];
	u32 dwZ = (u32)g_qwGPR[REG_a3];
	u32 dwA = (u32)Read32Bits((u32)g_qwGPR[REG_sp] + 0x10);
	u32 dwB = (u32)Read32Bits((u32)g_qwGPR[REG_sp] + 0x14);

	DBGConsole_Msg(0, "[WosMapTLB(0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x)]",
		dwW,dwX,dwY,dwZ,dwA,dwB);

	return PATCH_RET_NOT_PROCESSED;
}




// ENTRYHI left untouched after call
DWORD Patch___osProbeTLB()
{   
TEST_DISABLE_TLB_FUNCS	
	DWORD dwVAddr = (u32)g_qwGPR[REG_a0];

	g_qwGPR[REG_v0] = OS_HLE___osProbeTLB(dwVAddr);

	//DBGConsole_Msg(0, "Probe: 0x%08x -> 0x%08x", dwVAddr, dwPAddr);

	return PATCH_RET_JR_RA;
}

DWORD Patch_osVirtualToPhysical_Mario()
{
TEST_DISABLE_TLB_FUNCS
	DWORD dwVAddr = (u32)g_qwGPR[REG_a0];

	//DBGConsole_Msg(0, "osVirtualToPhysical(0x%08x)", (u32)g_qwGPR[REG_a0]);

	if (IS_KSEG0(dwVAddr))
	{
		g_qwGPR[REG_v0] = (s64)(s32)K0_TO_PHYS(dwVAddr);
	}
	else if (IS_KSEG1(dwVAddr))
	{
		g_qwGPR[REG_v0] = (s64)(s32)K1_TO_PHYS(dwVAddr);
	}
	else
	{
		g_qwGPR[REG_v0] = OS_HLE___osProbeTLB(dwVAddr);
	}

	return PATCH_RET_JR_RA;

}

// IDentical - just optimised
DWORD Patch_osVirtualToPhysical_Rugrats()
{
TEST_DISABLE_TLB_FUNCS
	//DBGConsole_Msg(0, "osVirtualToPhysical(0x%08x)", (u32)g_qwGPR[REG_a0]);
	return Patch_osVirtualToPhysical_Mario();
}
