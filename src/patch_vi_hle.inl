#define TEST_DISABLE_VI_FUNCS //return PATCH_RET_NOT_PROCESSED;




DWORD Patch_osViSetMode()
{
TEST_DISABLE_VI_FUNCS
	DWORD dwViMode = (u32)g_qwGPR[REG_a0];

	/*
    u8				type;		// Mode type
    OSViCommonRegs	comRegs;	// Common registers for both fields
    OSViFieldRegs	fldRegs[2];	// Registers for Field 1  & 2
	*/

	//DBGConsole_Msg(0, "[WosViSetMode({%d, ...})]",
	//	Read8Bits(dwViMode + offsetof(OSViMode, type)));


	//Force pause
	return PATCH_RET_NOT_PROCESSED;
}


DWORD Patch_osViBlack()
{
TEST_DISABLE_VI_FUNCS
	DWORD dwVal = (u32)g_qwGPR[REG_a0];

	//DBGConsole_Msg(0, "[WosViBlack(%d)]", dwVal);

	//Force pause
	return PATCH_RET_NOT_PROCESSED;
}


DWORD Patch_osViSwapBuffer()
{
TEST_DISABLE_VI_FUNCS
	// Ignore stack change
	// Ignore interrupts disable
	// Ignore save parameter

	//DBGConsole_Msg(0, "osViSwapBuffer(0x%08x)", (u32)g_qwGPR[REG_a0]);

	DWORD dwPointer = Read32Bits(VAR_ADDRESS(osViSetModeGubbins));
	WORD wControl;

	Write32Bits(dwPointer + 0x4, (u32)g_qwGPR[REG_a0]);

	wControl = Read16Bits(dwPointer + 0x0);
	wControl |= 0x0010;
	Write16Bits(dwPointer + 0x0, wControl);

	return PATCH_RET_JR_RA;
}

