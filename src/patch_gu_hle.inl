#define TEST_DISABLE_GU_FUNCS //return PATCH_RET_NOT_PROCESSED;

static const DWORD s_IdentMatrixF[16] = 
{
	0x3f800000,	0x00000000, 0x00000000,	0x00000000,
	0x00000000,	0x3f800000, 0x00000000,	0x00000000,
	0x00000000, 0x00000000, 0x3f800000,	0x00000000,
	0x00000000, 0x00000000, 0x00000000,	0x3f800000
};

static const DWORD s_IdentMatrixL[16] = 
{
	0x00010000,	0x00000000,
	0x00000001,	0x00000000,
	0x00000000,	0x00010000,
	0x00000000,	0x00000001,

	0x00000000, 0x00000000,
	0x00000000,	0x00000000,
	0x00000000, 0x00000000,
	0x00000000,	0x00000000
};

DWORD Patch_guMtxIdentF()
{
TEST_DISABLE_GU_FUNCS
	DWORD dwAddress = (DWORD)g_qwGPR[REG_a0];

//	DBGConsole_Msg(0, "guMtxIdentF(0x%08x)", dwAddress);

	BYTE * pMtxBase = (BYTE *)ReadAddress(dwAddress);


	// 0x00000000 is 0.0 in IEEE fp
	// 0x3f800000 is 1.0 in IEEE fp
	memcpy(pMtxBase, s_IdentMatrixF, sizeof(s_IdentMatrixF));

	/*
	QuickWrite32Bits(pMtxBase, 0x00, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x04, 0);
	QuickWrite32Bits(pMtxBase, 0x08, 0);
	QuickWrite32Bits(pMtxBase, 0x0c, 0);

	QuickWrite32Bits(pMtxBase, 0x10, 0);
	QuickWrite32Bits(pMtxBase, 0x14, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x18, 0);
	QuickWrite32Bits(pMtxBase, 0x1c, 0);

	QuickWrite32Bits(pMtxBase, 0x20, 0);
	QuickWrite32Bits(pMtxBase, 0x24, 0);
	QuickWrite32Bits(pMtxBase, 0x28, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x2c, 0);

	QuickWrite32Bits(pMtxBase, 0x30, 0);
	QuickWrite32Bits(pMtxBase, 0x34, 0);
	QuickWrite32Bits(pMtxBase, 0x38, 0);
	QuickWrite32Bits(pMtxBase, 0x3c, 0x3f800000);*/

/*
	g_dwNumMtxIdent++;
	if ((g_dwNumMtxIdent % 10000) == 0)
	{
		DBGConsole_Msg(0, "%d guMtxIdentF calls intercepted", g_dwNumMtxIdent);
	}*/


	return PATCH_RET_JR_RA;
}



DWORD Patch_guMtxIdent()
{
TEST_DISABLE_GU_FUNCS
	DWORD dwAddress = (DWORD)g_qwGPR[REG_a0];

	//DBGConsole_Msg(0, "guMtxIdent(0x%08x)", dwAddress);

	BYTE * pMtxBase = (BYTE *)ReadAddress(dwAddress);

	// This is a lot faster than the real method, which calls
	// glMtxIdentF followed by guMtxF2L
	memcpy(pMtxBase, s_IdentMatrixL, sizeof(s_IdentMatrixL));
/*	QuickWrite32Bits(pMtxBase, 0x00, 0x00010000);
	QuickWrite32Bits(pMtxBase, 0x04, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x08, 0x00000001);
	QuickWrite32Bits(pMtxBase, 0x0c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x10, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x14, 0x00010000);
	QuickWrite32Bits(pMtxBase, 0x18, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x1c, 0x00000001);

	QuickWrite32Bits(pMtxBase, 0x20, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x24, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x28, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x2c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x30, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x34, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x38, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x3c, 0x00000000);
*/

/*	g_dwNumMtxIdent++;
	if ((g_dwNumMtxIdent % 10000) == 0)
	{
		DBGConsole_Msg(0, "%d guMtxIdent calls intercepted", g_dwNumMtxIdent);
	}*/

	return PATCH_RET_JR_RA;
}



DWORD Patch_guTranslateF()
{
TEST_DISABLE_GU_FUNCS
	DWORD dwAddress = (DWORD)g_qwGPR[REG_a0];
	DWORD dwX = (DWORD)g_qwGPR[REG_a1];
	DWORD dwY = (DWORD)g_qwGPR[REG_a2];
	DWORD dwZ = (DWORD)g_qwGPR[REG_a3];

	//DBGConsole_Msg(0, "guTranslateF(0x%08x, %f, %f, %f)", dwAddress, dwX, dwY, dwZ);

	BYTE * pMtxBase = (BYTE *)ReadAddress(dwAddress);


	// 0x00000000 is 0.0 in IEEE fp
	// 0x3f800000 is 1.0 in IEEE fp
	QuickWrite32Bits(pMtxBase, 0x00, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x04, 0);
	QuickWrite32Bits(pMtxBase, 0x08, 0);
	QuickWrite32Bits(pMtxBase, 0x0c, 0);

	QuickWrite32Bits(pMtxBase, 0x10, 0);
	QuickWrite32Bits(pMtxBase, 0x14, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x18, 0);
	QuickWrite32Bits(pMtxBase, 0x1c, 0);

	QuickWrite32Bits(pMtxBase, 0x20, 0);
	QuickWrite32Bits(pMtxBase, 0x24, 0);
	QuickWrite32Bits(pMtxBase, 0x28, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x2c, 0);

	QuickWrite32Bits(pMtxBase, 0x30, dwX);
	QuickWrite32Bits(pMtxBase, 0x34, dwY);
	QuickWrite32Bits(pMtxBase, 0x38, dwZ);
	QuickWrite32Bits(pMtxBase, 0x3c, 0x3f800000);

/*	g_dwNumMtxTranslate++;
	if ((g_dwNumMtxTranslate % 10000) == 0)
	{
		DBGConsole_Msg(0, "%d guMtxTranslate calls intercepted", g_dwNumMtxTranslate);
	}*/
	return PATCH_RET_JR_RA;
}

DWORD Patch_guTranslate()
{
TEST_DISABLE_GU_FUNCS
	DWORD dwAddress = (DWORD)g_qwGPR[REG_a0];
	DWORD dwX = (DWORD)g_qwGPR[REG_a1];
	DWORD dwY = (DWORD)g_qwGPR[REG_a2];
	DWORD dwZ = (DWORD)g_qwGPR[REG_a3];
	const float fScale = 65536.0f;

	//DBGConsole_Msg(0, "guTranslate(0x%08x, %f, %f, %f) ra:0x%08x",
	//	dwAddress, ToFloat(dwX), ToFloat(dwY), ToFloat(dwZ), (DWORD)g_qwGPR[REG_ra]);

	BYTE * pMtxBase = (BYTE *)ReadAddress(dwAddress);

	DWORD x = (DWORD)((*(float*)&dwX) * fScale);
	DWORD y = (DWORD)((*(float*)&dwY) * fScale);
	DWORD z = (DWORD)((*(float*)&dwZ) * fScale);
	DWORD one = (DWORD)(1.0f * fScale);

	DWORD xyhibits = (x & 0xFFFF0000) | (y >> 16);
	DWORD xylobits = (x << 16) | (y & 0x0000FFFF);

	DWORD z1hibits = (z & 0xFFFF0000) | (one >> 16);
	DWORD z1lobits = (z << 16) | (one & 0x0000FFFF);


	// 0x00000000 is 0.0 in IEEE fp
	// 0x3f800000 is 1.0 in IEEE fp
	QuickWrite32Bits(pMtxBase, 0x00, 0x00010000);
	QuickWrite32Bits(pMtxBase, 0x04, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x08, 0x00000001);
	QuickWrite32Bits(pMtxBase, 0x0c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x10, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x14, 0x00010000);
	QuickWrite32Bits(pMtxBase, 0x18, xyhibits);	// xy
	QuickWrite32Bits(pMtxBase, 0x1c, z1hibits);	// z1

	QuickWrite32Bits(pMtxBase, 0x20, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x24, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x28, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x2c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x30, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x34, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x38, xylobits);	// xy
	QuickWrite32Bits(pMtxBase, 0x3c, z1lobits);	// z1

/*	g_dwNumMtxTranslate++;
	if ((g_dwNumMtxTranslate % 10000) == 0)
	{
		DBGConsole_Msg(0, "%d guMtxTranslate calls intercepted", g_dwNumMtxTranslate);
	}*/

	return PATCH_RET_JR_RA;
}

DWORD Patch_guScaleF()
{
TEST_DISABLE_GU_FUNCS
	DWORD dwAddress = (DWORD)g_qwGPR[REG_a0];
	DWORD dwX = (DWORD)g_qwGPR[REG_a1];
	DWORD dwY = (DWORD)g_qwGPR[REG_a2];
	DWORD dwZ = (DWORD)g_qwGPR[REG_a3];

	//DBGConsole_Msg(0, "guScaleF(0x%08x, %f, %f, %f)", dwAddress, dwX, dwY, dwZ);

	BYTE * pMtxBase = (BYTE *)ReadAddress(dwAddress);

	// 0x00000000 is 0.0 in IEEE fp
	// 0x3f800000 is 1.0 in IEEE fp
	QuickWrite32Bits(pMtxBase, 0x00, dwX);
	QuickWrite32Bits(pMtxBase, 0x04, 0);
	QuickWrite32Bits(pMtxBase, 0x08, 0);
	QuickWrite32Bits(pMtxBase, 0x0c, 0);

	QuickWrite32Bits(pMtxBase, 0x10, 0);
	QuickWrite32Bits(pMtxBase, 0x14, dwY);
	QuickWrite32Bits(pMtxBase, 0x18, 0);
	QuickWrite32Bits(pMtxBase, 0x1c, 0);

	QuickWrite32Bits(pMtxBase, 0x20, 0);
	QuickWrite32Bits(pMtxBase, 0x24, 0);
	QuickWrite32Bits(pMtxBase, 0x28, dwZ);
	QuickWrite32Bits(pMtxBase, 0x2c, 0);

	QuickWrite32Bits(pMtxBase, 0x30, 0);
	QuickWrite32Bits(pMtxBase, 0x34, 0);
	QuickWrite32Bits(pMtxBase, 0x38, 0);
	QuickWrite32Bits(pMtxBase, 0x3c, 0x3f800000);

/*	g_dwNumMtxScale++;
	if ((g_dwNumMtxScale % 10000) == 0)
	{
		DBGConsole_Msg(0, "%d guMtxScale calls intercepted", g_dwNumMtxScale);
	}*/

	return PATCH_RET_JR_RA;
}

DWORD Patch_guScale()
{
TEST_DISABLE_GU_FUNCS
	DWORD dwAddress = (DWORD)g_qwGPR[REG_a0];
	DWORD dwX = (DWORD)g_qwGPR[REG_a1];
	DWORD dwY = (DWORD)g_qwGPR[REG_a2];
	DWORD dwZ = (DWORD)g_qwGPR[REG_a3];
	const float fScale = 65536.0f;

	//DBGConsole_Msg(0, "guScale(0x%08x, %f, %f, %f)", dwAddress, dwX, dwY, dwZ);

	BYTE * pMtxBase = (BYTE *)ReadAddress(dwAddress);

	DWORD x = (DWORD)((*(float*)&dwX) * fScale);
	DWORD y = (DWORD)((*(float*)&dwY) * fScale);
	DWORD z = (DWORD)((*(float*)&dwZ) * fScale);
	DWORD one = (DWORD)(1.0f * fScale);
	DWORD zer = (DWORD)(0.0f);

	DWORD xzhibits = (x & 0xFFFF0000) | (zer >> 16);
	DWORD xzlobits = (x << 16) | (zer & 0x0000FFFF);

	DWORD zyhibits = (zer & 0xFFFF0000) | (y >> 16);
	DWORD zylobits = (zer << 16) | (y & 0x0000FFFF);

	DWORD zzhibits = (z & 0xFFFF0000) | (zer >> 16);
	DWORD zzlobits = (z << 16) | (zer & 0x0000FFFF);

	// 0x00000000 is 0.0 in IEEE fp
	// 0x3f800000 is 1.0 in IEEE fp
	QuickWrite32Bits(pMtxBase, 0x00, xzhibits);
	QuickWrite32Bits(pMtxBase, 0x04, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x08, zyhibits);
	QuickWrite32Bits(pMtxBase, 0x0c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x10, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x14, zzhibits);
	QuickWrite32Bits(pMtxBase, 0x18, 0x00000000);	// xy
	QuickWrite32Bits(pMtxBase, 0x1c, 0x00000001);	// z1

	QuickWrite32Bits(pMtxBase, 0x20, xzlobits);
	QuickWrite32Bits(pMtxBase, 0x24, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x28, zylobits);
	QuickWrite32Bits(pMtxBase, 0x2c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x30, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x34, zzlobits);
	QuickWrite32Bits(pMtxBase, 0x38, 0x00000000);	// xy
	QuickWrite32Bits(pMtxBase, 0x3c, 0x00000000);	// z1

/*	g_dwNumMtxScale++;
	if ((g_dwNumMtxScale % 10000) == 0)
	{
		DBGConsole_Msg(0, "%d guMtxScale calls intercepted", g_dwNumMtxScale);
	}*/



	return PATCH_RET_JR_RA;
}




DWORD Patch_guMtxF2L()
{
TEST_DISABLE_GU_FUNCS
	DWORD dwFloatMtx = (DWORD)g_qwGPR[REG_a0];
	DWORD dwFixedMtx = (DWORD)g_qwGPR[REG_a1];

	const float fScale = 65536.0f;

	BYTE * pMtxFBase = (BYTE *)ReadAddress(dwFloatMtx);
	BYTE * pMtxLBaseHiBits = (BYTE *)ReadAddress(dwFixedMtx + 0x00);
	BYTE * pMtxLBaseLoBits = (BYTE *)ReadAddress(dwFixedMtx + 0x20);
	DWORD dwFA;
	DWORD dwFB;
	DWORD a, b;
	DWORD hibits;
	DWORD lobits;
	LONG row;

	for (row = 0; row < 4; row++)
	{
		dwFA = QuickRead32Bits(pMtxFBase, row*16 + 0x0);
		dwFB = QuickRead32Bits(pMtxFBase, row*16 + 0x4);
		
		// Should be TRUNC
		a = (DWORD)((*(float*)&dwFA) * fScale);
		b = (DWORD)((*(float*)&dwFB) * fScale);

		hibits = (a & 0xFFFF0000) | (b >> 16);
		lobits = (a << 16) | (b & 0x0000FFFF);

		QuickWrite32Bits(pMtxLBaseHiBits, row*8 + 0, hibits);
		QuickWrite32Bits(pMtxLBaseLoBits, row*8 + 0, lobits);
		
		/////
		dwFA = QuickRead32Bits(pMtxFBase, row*16 + 0x8);
		dwFB = QuickRead32Bits(pMtxFBase, row*16 + 0xc);
		
		// Should be TRUNC
		a = (DWORD)((*(float*)&dwFA) * fScale);
		b = (DWORD)((*(float*)&dwFB) * fScale);

		hibits = (a & 0xFFFF0000) | (b >> 16);
		lobits = (a << 16) | (b & 0x0000FFFF);

		QuickWrite32Bits(pMtxLBaseHiBits, row*8 + 4, hibits);
		QuickWrite32Bits(pMtxLBaseLoBits, row*8 + 4, lobits);

	}


/*	g_dwNumMtxF2L++;
	if ((g_dwNumMtxF2L % 10000) == 0)
	{
		DBGConsole_Msg(0, "%d guMtxF2L calls intercepted", g_dwNumMtxF2L);
	}*/


	return PATCH_RET_JR_RA;
}


DWORD Patch_guNormalize_Mario()
{
TEST_DISABLE_GU_FUNCS
	DWORD dwX = (DWORD)g_qwGPR[REG_a0];
	DWORD dwY = (DWORD)g_qwGPR[REG_a1];
	DWORD dwZ = (DWORD)g_qwGPR[REG_a2];

	DWORD x = Read32Bits(dwX);
	DWORD y = Read32Bits(dwY);
	DWORD z = Read32Bits(dwZ);

	float fX = *(float*)&x;
	float fY = *(float*)&y;
	float fZ = *(float*)&z;

	//DBGConsole_Msg(0, "guNormalize(0x%08x %f, 0x%08x %f, 0x%08x %f)",
	//	dwX, fX, dwY, fY, dwZ, fZ);

	float fLenRecip = 1.0f / fsqrt((fX * fX) + (fY * fY) + (fZ * fZ));

	fX *= fLenRecip;
	fY *= fLenRecip;
	fZ *= fLenRecip;

/*	g_dwNumNormalize++;
	if ((g_dwNumNormalize % 1000) == 0)
	{
		DBGConsole_Msg(0, "%d guNormalize calls intercepted", g_dwNumNormalize);
	}*/

	Write32Bits(dwX, *(DWORD*)&fX);
	Write32Bits(dwY, *(DWORD*)&fY);
	Write32Bits(dwZ, *(DWORD*)&fZ);



	return PATCH_RET_JR_RA;
}

// NOT the same function as guNormalise_Mario
// This take one pointer, not 3
DWORD Patch_guNormalize_Rugrats()
{
TEST_DISABLE_GU_FUNCS
	DWORD dwX = (DWORD)g_qwGPR[REG_a0];
	DWORD dwY = dwX + 4;
	DWORD dwZ = dwX + 8;

	DWORD x = Read32Bits(dwX);
	DWORD y = Read32Bits(dwY);
	DWORD z = Read32Bits(dwZ);

	float fX = *(float*)&x;
	float fY = *(float*)&y;
	float fZ = *(float*)&z;

	//DBGConsole_Msg(0, "guNormalize(0x%08x %f, 0x%08x %f, 0x%08x %f)",
	//	dwX, fX, dwY, fY, dwZ, fZ);

	float fLenRecip = 1.0f / ((fX * fX) + (fY * fY) + (fZ * fZ));

	fX *= fLenRecip;
	fY *= fLenRecip;
	fZ *= fLenRecip;

/*	g_dwNumNormalize++;
	if ((g_dwNumNormalize % 1000) == 0)
	{
		DBGConsole_Msg(0, "%d guNormalize calls intercepted", g_dwNumNormalize);
	}*/

	Write32Bits(dwX, *(DWORD*)&fX);
	Write32Bits(dwY, *(DWORD*)&fY);
	Write32Bits(dwZ, *(DWORD*)&fZ);



	return PATCH_RET_JR_RA;
}

DWORD Patch_guOrthoF()
{
TEST_DISABLE_GU_FUNCS
	DWORD dwMtx = (DWORD)g_qwGPR[REG_a0];
	DWORD dwL = (DWORD)g_qwGPR[REG_a1];
	DWORD dwR = (DWORD)g_qwGPR[REG_a2];
	DWORD dwB = (DWORD)g_qwGPR[REG_a3];
	DWORD dwT = Read32Bits((DWORD)g_qwGPR[REG_sp] + 0x10);
	DWORD dwN = Read32Bits((DWORD)g_qwGPR[REG_sp] + 0x14);
	DWORD dwF = Read32Bits((DWORD)g_qwGPR[REG_sp] + 0x18);
	DWORD dwS = Read32Bits((DWORD)g_qwGPR[REG_sp] + 0x1c);


	//DBGConsole_Msg(0, "guOrthoF(0x%08x, %f, %f", dwMtx, ToFloat(dwL), ToFloat(dwR));
	//DBGConsole_Msg(0, "                     %f, %f", ToFloat(dwB), ToFloat(dwT));
	//DBGConsole_Msg(0, "                     %f, %f", ToFloat(dwN), ToFloat(dwF));
	//DBGConsole_Msg(0, "                     %f)", ToFloat(dwS));

	BYTE * pMtxBase = (BYTE *)ReadAddress(dwMtx);

	float fL = ToFloat(dwL);
	float fR = ToFloat(dwR);
	float fB = ToFloat(dwB);
	float fT = ToFloat(dwT);
	float fN = ToFloat(dwN);
	float fF = ToFloat(dwF);
	float scale = ToFloat(dwS);

	float fRmL = fR - fL;
	float fTmB = fT - fB;
	float fFmN = fF - fN;
	float fRpL = fR + fL;
	float fTpB = fT + fB;
	float fFpN = fF + fN;

	float sx = (2 * scale)/fRmL;
	float sy = (2 * scale)/fTmB;
	float sz = (-2 * scale)/fFmN;

	float tx = -fRpL * scale / fRmL;
	float ty = -fTpB * scale / fTmB;
	float tz = -fFpN * scale / fFmN;

	/*
	0   2/(r-l)
	1                2/(t-b)
	2                            -2/(f-n)
	3 -(l+r)/(r-l) -(t+b)/(t-b) -(f+n)/(f-n)     1*/

	// 0x3f800000 is 1.0 in IEEE fp
	QuickWrite32Bits(pMtxBase, 0x00, ToInt(sx) );
	QuickWrite32Bits(pMtxBase, 0x04, 0);
	QuickWrite32Bits(pMtxBase, 0x08, 0);
	QuickWrite32Bits(pMtxBase, 0x0c, 0);

	QuickWrite32Bits(pMtxBase, 0x10, 0);
	QuickWrite32Bits(pMtxBase, 0x14, ToInt(sy)  );
	QuickWrite32Bits(pMtxBase, 0x18, 0);
	QuickWrite32Bits(pMtxBase, 0x1c, 0);

	QuickWrite32Bits(pMtxBase, 0x20, 0);
	QuickWrite32Bits(pMtxBase, 0x24, 0);
	QuickWrite32Bits(pMtxBase, 0x28, ToInt(sz)  );
	QuickWrite32Bits(pMtxBase, 0x2c, 0);

	QuickWrite32Bits(pMtxBase, 0x30, ToInt(tx)  );
	QuickWrite32Bits(pMtxBase, 0x34, ToInt(ty)  );
	QuickWrite32Bits(pMtxBase, 0x38, ToInt(tz)  );
	QuickWrite32Bits(pMtxBase, 0x3c, ToInt(scale)  );

	return PATCH_RET_JR_RA;
}

DWORD Patch_guOrtho()
{
TEST_DISABLE_GU_FUNCS
	//DBGConsole_Msg(0, "guOrtho");
	return PATCH_RET_NOT_PROCESSED;

}

DWORD Patch_guRotateF()
{
TEST_DISABLE_GU_FUNCS
	/*g_dwNumMtxRotate++;
	if ((g_dwNumMtxRotate % 10000) == 0)
	{
		DBGConsole_Msg(0, "%d guRotate calls intercepted", g_dwNumMtxRotate);
	}*/

	//D3DXMatrixRotationAxis(mat, axis, angle)

	return PATCH_RET_NOT_PROCESSED;

}
