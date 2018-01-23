#define TEST_DISABLE_UTIL_FUNCS //return PATCH_RET_NOT_PROCESSED;

DWORD Patch___osAtomicDec()
{
TEST_DISABLE_UTIL_FUNCS
	DBGConsole_Msg(0, "osAtomicDec");

	DWORD dwP = (u32)g_qwGPR[REG_a0];
	DWORD dwValue = Read32Bits(dwP);
	DWORD dwRetval;

	if (dwValue != 0)
	{
		Write32Bits(dwP, dwValue - 1);
		dwRetval = 1;
	}
	else
	{
		dwRetval = 0;
	}
	
	g_qwGPR[REG_v0] = (u64)dwRetval;

	return PATCH_RET_JR_RA;
}



DWORD Patch_memcpy()
{
TEST_DISABLE_UTIL_FUNCS
	DWORD dwDst = (u32)g_qwGPR[REG_a0];
	DWORD dwSrc = (u32)g_qwGPR[REG_a1];
	DWORD dwLen = (u32)g_qwGPR[REG_a2];
	DWORD i;

	//DBGConsole_Msg(0, "memcpy(0x%08x, 0x%08x, %d)", dwDst, dwSrc, dwLen);

	for (i = 0; i < dwLen; i++)
	{
		Write8Bits(dwDst + i,  Read8Bits(dwSrc + i));
	}

	// return value of dest
	g_qwGPR[REG_v0] = g_qwGPR[REG_a0];	

	return PATCH_RET_JR_RA;
}

DWORD Patch_strlen()
{
TEST_DISABLE_UTIL_FUNCS

	DWORD i;
	DWORD dwString = (u32)g_qwGPR[REG_a0];
	//static CHAR last[300] = "";
	/*CHAR buf[300];

	for (i = 0; ; i++)
	{
		u8 u = Read8Bits(dwString + i);

		buf[i] = (char)u;

		if (u == 0)
			break;
	}
	//if (lstrcmpi(last, buf) != 0)
	//{
		DBGConsole_Msg(0, "strlen(%s) ra 0x%08x", buf, (DWORD)g_qwGPR[REG_ra]);
	//	lstrcpyn(last, buf, 300);
	//}*/

	// Calculate length now:
	for (i = 0; Read8Bits(dwString+i) != 0; i++)
		;

	g_qwGPR[REG_v0] = (s64)(s32)i;

	return PATCH_RET_JR_RA;

}


DWORD Patch_strchr()
{
TEST_DISABLE_UTIL_FUNCS
	//char buf[300];
	DWORD i;
	DWORD dwString = (u32)g_qwGPR[REG_a0];
	DWORD dwMatchChar = (u32)g_qwGPR[REG_a1] & 0xFF;
	DWORD dwSrcChar;
	DWORD dwMatchAddr = 0;

	/*for (i = 0; ; i++)
	{
		dwSrcChar = Read8Bits(dwString + i);

		buf[i] = (char)dwSrcChar;

		if (dwSrcChar == 0)
			break;
	}*/
	//DBGConsole_Msg(0, "strchr('%s', %c) (ra == 0x%08x)", buf, dwMatchChar, (u32)g_qwGPR[REG_ra]);

	for (i = 0; ; i++)
	{
		dwSrcChar = Read8Bits(dwString + i);

		if (dwSrcChar == dwMatchChar)
		{
			dwMatchAddr = dwString + i;
			break;
		}

		if (dwSrcChar == 0)
			break;
	}

	g_qwGPR[REG_v0] = (s64)(s32)dwMatchAddr;

	return PATCH_RET_JR_RA;
}



/*
// int strcmp(u8 * buf1, u8 * buf2, a2 = len)
0x8000a898: <0x54c00004> BNEL      a2 != r0 --> 0x8000a8ac
0x8000a89c: <0x90820000> LBU       v0 <- 0x0000(a0)

0x8000a8a0: <0x03e00008> JR        ra
0x8000a8a4: <0x00001025> CLEAR     v0 = 0						// return 0

0x8000a8a8: <0x90820000> LBU       v0 <- 0x0000(a0)				// v0 = a
0x8000a8ac: <0x90a30000> LBU       v1 <- 0x0000(a1)				// v1 = b
0x8000a8b0: <0x24c6ffff> ADDIU     a2 = a2 + 0xffff				// len--
0x8000a8b4: <0x24840001> ADDIU     a0 = a0 + 0x0001				// ptra++
0x8000a8b8: <0x10430007> BEQ       v0 == v1 --> 0x8000a8d8		// a==b
0x8000a8bc: <0x0043082a> SLT       at = (v0<v1)
0x8000a8c0: <0x10200003> BEQ       at == r0 --> 0x8000a8d0
0x8000a8c4: <0x00000000> NOP
0x8000a8c8: <0x03e00008> JR        ra
0x8000a8cc: <0x2402ffff> ADDIU     v0 = r0 + 0xffff				// return -1

0x8000a8d0: <0x03e00008> JR        ra
0x8000a8d4: <0x24020001> ADDIU     v0 = r0 + 0x0001				// return +1

// a == b
0x8000a8d8: <0x14400003> BNE       v0 != r0 --> 0x8000a8e8		// a == 0
0x8000a8dc: <0x00000000> NOP
0x8000a8e0: <0x03e00008> JR        ra
0x8000a8e4: <0x00001025> CLEAR     v0 = 0						// len 0, return 0

// still len
0x8000a8e8: <0x1000ffeb> B         --> 0x8000a898				// recurse?
0x8000a8ec: <0x24a50001> ADDIU     a1 = a1 + 0x0001				// ptrb++
0x8000a8f0: <0x03e00008> JR        ra
0x8000a8f4: <0x00000000> NOP*/

DWORD Patch_strcmp()
{
	/*CHAR bufa[300];
	CHAR bufb[300];
	DWORD i;
	DWORD dwA = (u32)g_qwGPR[REG_a0];
	DWORD dwB = (u32)g_qwGPR[REG_a1];
	DWORD dwLen = (u32)g_qwGPR[REG_a2];

	for (i = 0; ; i++)
	{
		u8 u = Read8Bits(dwA + i);

		bufa[i] = (char)u;

		if (u == 0)
			break;
	}
	for (i = 0; ; i++)
	{
		u8 u = Read8Bits(dwB + i);

		bufb[i] = (char)u;

		if (u == 0)
			break;
	}

	DBGConsole_Msg(0, "strcmp(%s,%s,%d)", bufa, bufb, dwLen);
	*/

	return PATCH_RET_NOT_PROCESSED;

}



/*void * bcopy(const void * src, void * dst, len) */
// Note different order to src/dst than memcpy!
DWORD Patch_bcopy()
{
TEST_DISABLE_UTIL_FUNCS
	DWORD dwSrc = (u32)g_qwGPR[REG_a0];
	DWORD dwDst = (u32)g_qwGPR[REG_a1];
	DWORD dwLen = (u32)g_qwGPR[REG_a2];

	//	DBGConsole_Msg(0, "bcopy(0x%08x, 0x%08x, %d)", dwSrc, dwDst, dwLen);


	// Set return val here (return dest)
	g_qwGPR[REG_v0] = g_qwGPR[REG_a1];

	if (dwLen == 0)
		return PATCH_RET_JR_RA;

	if (dwSrc == dwDst)
		return PATCH_RET_JR_RA;

	if (dwDst < dwSrc || dwDst >= (dwSrc + dwLen))
	{
		// Copy forwards
		if (dwLen < 16 || (dwSrc & 3) != (dwDst & 3))
		{
			// Copy bytewise...
			while (dwLen > 0)
			{
				Write8Bits(dwDst,Read8Bits(dwSrc));
				dwSrc++;
				dwDst++;
				dwLen--;
			}
		}
		else
		{
			// Align src/dst to 4 bytes - bit of a hack!
			while ((dwSrc & 3) != 0)
			{
				Write8Bits(dwDst,Read8Bits(dwSrc));
				dwSrc++;
				dwDst++;
				dwLen--;
			}

			// TODO - Cache read/write pointer here
			DWORD * pSrcBase = (DWORD *)ReadAddress(dwSrc);
			DWORD * pDstBase = (DWORD *)WriteAddress(dwDst);
			DWORD dwBlockSize = (dwLen>>2) * 4;

			// We know dwLen is at the very least 16-3
			memcpy(pDstBase, pSrcBase, dwBlockSize);

			dwSrc += dwBlockSize;
			dwDst += dwBlockSize;
			dwLen -= dwBlockSize;

			// Would could update the src/dst counter outside of the loop

			while (dwLen > 0)
			{
				Write8Bits(dwDst,Read8Bits(dwSrc));
				dwSrc++;
				dwDst++;
				dwLen--;
			}
		}
	}
	else
	{
		// Copy backwards
		
		dwDst += (dwLen - 1);
		dwSrc += (dwLen - 1);
		
		if (dwLen < 16 || (dwSrc & 3) != (dwDst & 3))
		{
			// Copy bytewise...
			while (dwLen > 0)
			{
				Write8Bits(dwDst,Read8Bits(dwSrc));
				dwSrc--;
				dwDst--;
				dwLen--;
			}
		}
		else
		{
			// Align src/dst to 4 bytes
					
			// TODO - Perform 4, 16, 32 byte copy here
			while (dwLen > 0)
			{
				Write8Bits(dwDst,Read8Bits(dwSrc));
				dwSrc--;
				dwDst--;
				dwLen--;
			}
		}
		
	}

	return PATCH_RET_JR_RA;

}


// By Jun Su
DWORD Patch_bzero() 
{ 
	DWORD dwDst = (u32)g_qwGPR[REG_a0]; 
	DWORD dwLen = (u32)g_qwGPR[REG_a1]; 
	DWORD i; 
	
	for (i = 0; i < dwLen; i++) 
	{ 
		Write8Bits(dwDst + i, 0); 
	} 
	
	// return value of dest 
	g_qwGPR[REG_v0] = g_qwGPR[REG_a0]; 
	
	return PATCH_RET_JR_RA; 
} 




