#define SR_MATH_OPTIMISE_FLAG		pCode->dwOptimiseLevel < 1
//#define SR_MATH_OPTIMISE_FLAG		1


BOOL SR_Emit_Special_MFHI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRD = rd(dwOp);

	SR_Stat_D(dwRD);
	
	if (SR_MATH_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_MFHI);
	}
	else
	{
		pCode->dwNumOptimised++;

		//g_qwGPR[dwRD] = g_qwMultHI;

		MOV_REG_MEM(EAX_CODE, (BYTE *)&g_qwMultHI + 0);
		MOV_REG_MEM(EDX_CODE, (BYTE *)&g_qwMultHI + 4);

		StoreMIPSLo(pCode, dwRD, EAX_CODE);
		StoreMIPSHi(pCode, dwRD, EDX_CODE);

	}
	return TRUE;
}

BOOL SR_Emit_Special_MFLO(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRD = rd(dwOp);

	SR_Stat_D(dwRD);
	
	if (SR_MATH_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_MFLO);
	}
	else
	{
		pCode->dwNumOptimised++;

		//g_qwGPR[dwRD] = g_qwMultLO;

		MOV_REG_MEM(EAX_CODE, (BYTE *)&g_qwMultLO + 0);
		MOV_REG_MEM(EDX_CODE, (BYTE *)&g_qwMultLO + 4);

		StoreMIPSLo(pCode, dwRD, EAX_CODE);
		StoreMIPSHi(pCode, dwRD, EDX_CODE);

	}

	return TRUE;
}


/*
0040B870 8B C1                mov         eax,ecx
0040B872 C1 E8 15             shr         eax,15h
0040B87F 83 E0 1F             and         eax,1Fh
0040B882 8B 04 C5 50 1C 49 00 mov         eax,dword ptr [eax*8+491C50h]
0040B889 C1 E9 10             shr         ecx,10h
0040B88C 83 E1 1F             and         ecx,1Fh
0040B88F F7 24 CD 50 1C 49 00 mul         eax,dword ptr [ecx*8+491C50h]


0040B896 8B CA                mov         ecx,edx
0040B898 99                   cdq
0040B899 A3 58 1D 49 00       mov         [g_qwMultLO],eax
0040B89E 8B C1                mov         eax,ecx
0040B8A0 89 15 5C 1D 49 00    mov         dword ptr ds:[g_qwMultLO + 4],edx
0040B8A6 99                   cdq
0040B8A7 A3 50 1D 49 00       mov         [g_qwMultHI],eax
0040B8AC 89 15 54 1D 49 00    mov         dword ptr ds:[g_qwMultHI + 4],edx*/


BOOL SR_Emit_Special_MULTU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);

	SR_Stat_S_S(dwRS, dwRT);
	
	if (SR_MATH_OPTIMISE_FLAG)
	{

		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_MULTU);
	}
	else
	{
		pCode->dwNumOptimised++;

		//u64 dwResult = ((u64)(u32)g_qwGPR[dwRS]) * ((u64)(u32)g_qwGPR[dwRT]);
		//g_qwMultLO = (s64)(s32)(dwResult);
		//g_qwMultHI = (s64)(s32)(dwResult >> 32);

		// We can do the following optimisations:
		//  
		// If neither is cached, load rs, mul rs, [rt]
		// If rs is cached, and rt is not, do mul rs, [rt]
		// If rt is cached, and rs is not, do mul rt, [rs]
		// If both are cached, need to store one and use that value

		// Optimise for in-place addition
		DWORD iCachedRegRS = g_MIPSRegInfo[dwRS].dwCachedIReg;
		DWORD iCachedRegRT = g_MIPSRegInfo[dwRT].dwCachedIReg;

		BOOL bCachedRS = iCachedRegRS != ~0;
		BOOL bCachedRT = iCachedRegRT != ~0;

		if (!bCachedRT)
		{
			LoadMIPSLo(pCode, EAX_CODE, dwRS);

			// mul eax, [dwRT]
			MUL_EAX_MEM( ((BYTE *)&g_qwGPR[0]) + (lohalf(dwRT)*4)  );

		}
		else if (!bCachedRS && bCachedRT)
		{
			LoadMIPSLo(pCode, EAX_CODE, dwRT);
			// mul eax, [dwRS]
			MUL_EAX_MEM( ((BYTE *)&g_qwGPR[0]) + (lohalf(dwRS)*4)  );

		}
		else		// Both cached
		{
			LoadMIPSLo(pCode, EAX_CODE, dwRS);
			LoadMIPSLo(pCode, (WORD)iCachedRegRT, dwRT);			// Ensure register is loaded
			// mul eax, dwRT
			MUL((WORD)iCachedRegRT);
		}

		MOV(ECX_CODE, EDX_CODE);
		CDQ;

		MOV_MEM_REG((BYTE *)&g_qwMultLO + 0, EAX_CODE);
		MOV(EAX_CODE, ECX_CODE);
		MOV_MEM_REG((BYTE *)&g_qwMultLO + 4, EDX_CODE);

		CDQ;
		MOV_MEM_REG((BYTE *)&g_qwMultHI + 0, EAX_CODE);
		MOV_MEM_REG((BYTE *)&g_qwMultHI + 4, EDX_CODE);

	}
	return TRUE;
}




BOOL SR_Emit_Special_ADD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Ignore exceptions - just use unsigned routine
	return SR_Emit_Special_ADDU(pCode,dwOp, pdwFlags);
}
BOOL SR_Emit_Special_ADDU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);

	if (SR_MATH_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_ADDU);
	}
	else
	{
		//g_qwGPR[dwRD] = (s64)((s32)g_qwGPR[dwRS] + (s32)g_qwGPR[dwRT]);

		pCode->dwNumOptimised++;

		if (dwRD == 0) return TRUE;	// Don't clear r0

		// We could optimise for the use of r0 as a source register,
		// but this never seems to happen, and it confuses the logic.
		LoadMIPSLo(pCode, EAX_CODE, dwRS);
		LoadMIPSLo(pCode, EDX_CODE, dwRT);
		ADD(EAX_CODE, EDX_CODE);
		CDQ;	// We can use this, only as we're using EAX/EDX

		StoreMIPSLo(pCode, dwRD, EAX_CODE);

		// OR: (thanks zilmar)
		// SARI(EAX_CODE, 31);
		// StoreMIPSHi(pCode, dwRT, EAX_CODE);

		StoreMIPSHi(pCode, dwRD, EDX_CODE);
	}
	return TRUE;
}

BOOL SR_Emit_Special_SUB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){
	// Ignore exceptions - just use unsigned routine
	return SR_Emit_Special_SUBU(pCode,dwOp, pdwFlags);
}

BOOL SR_Emit_Special_SUBU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);


	if (SR_MATH_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_SUBU);
	}
	else
	{
		//g_qwGPR[dwRD] = (s64)((s32)g_qwGPR[dwRS] - (s32)g_qwGPR[dwRT]);

		pCode->dwNumOptimised++;

		if (dwRD == 0) return TRUE;	// Don't clear r0

		LoadMIPSLo(pCode, EAX_CODE, dwRS);
		LoadMIPSLo(pCode, EDX_CODE, dwRT);

		SUB(EAX_CODE, EDX_CODE);
		CDQ;	// We can use this, only as we're using EAX/EDX

		StoreMIPSLo(pCode, dwRD, EAX_CODE);

		// OR: (thanks zilmar)
		// SARI(EAX_CODE, 31);
		// StoreMIPSHi(pCode, dwRT, EAX_CODE);

		StoreMIPSHi(pCode, dwRD, EDX_CODE);
	}
	return TRUE;
}




BOOL SR_Emit_Special_AND(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);


	if (SR_MATH_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_AND);
	}
	else
	{
		//g_qwGPR[dwRD] = g_qwGPR[dwRS] & g_qwGPR[dwRT];

		pCode->dwNumOptimised++;

		// Don't write to r0
		if (dwRD == 0) return TRUE;	// Don't clear r0

		if (dwRS == 0 || dwRT == 0) {
			// This doesn't seem to happen much - is it worth optimising?
			// One or both registers are zero - we can just clear the destination register
			SetMIPSLo(pCode, dwRD, 0);
			SetMIPSHi(pCode, dwRD, 0);
		} else {
			// Both registers non zero - perform operation as normal
			LoadMIPSLo(pCode, EAX_CODE, dwRS);
			LoadMIPSLo(pCode, EDX_CODE, dwRT);
			AND(EAX_CODE, EDX_CODE);
			StoreMIPSLo(pCode, dwRD, EAX_CODE);

			LoadMIPSHi(pCode, EDX_CODE, dwRT);	// Put EDX first to allow pipelining with above
			LoadMIPSHi(pCode, EAX_CODE, dwRS);
			AND(EAX_CODE, EDX_CODE);
			StoreMIPSHi(pCode, dwRD, EAX_CODE);
		}
	}
	return TRUE;

}


BOOL SR_Emit_Special_OR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);


	if (SR_MATH_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_OR);
	}
	else
	{
		//g_qwGPR[dwRD] = g_qwGPR[dwRS] | g_qwGPR[dwRT];

		pCode->dwNumOptimised++;

		// Don't write to r0
		if (dwRD == 0) return TRUE;	// Don't clear r0

		if (dwRS == 0 && dwRT == 0) {
			// Both registers are zero - we can just clear the destination register
			SetMIPSLo(pCode, dwRD, 0);
			SetMIPSHi(pCode, dwRD, 0);
		} else if (dwRS == 0) {
			// This case rarely seems to happen...
			// As RS is zero, the OR is just a copy of RT to RD.
			LoadMIPSLo(pCode, EAX_CODE, dwRT);
			LoadMIPSHi(pCode, EDX_CODE, dwRT);
			StoreMIPSLo(pCode, dwRD, EAX_CODE);
			StoreMIPSHi(pCode, dwRD, EDX_CODE);
		} else if (dwRT == 0) {
			// As RT is zero, the OR is just a copy of RS to RD.
			LoadMIPSLo(pCode, EAX_CODE, dwRS);
			LoadMIPSHi(pCode, EDX_CODE, dwRS);
			StoreMIPSLo(pCode, dwRD, EAX_CODE);
			StoreMIPSHi(pCode, dwRD, EDX_CODE);
		} else {
			// Both registers non zero - perform operation as normal
			// Can we just do OR(EAX_CODE, lohalf(dwRT)) here????
			LoadMIPSLo(pCode, EAX_CODE, dwRS);
			LoadMIPSLo(pCode, EDX_CODE, dwRT);
			OR(EAX_CODE, EDX_CODE);
			StoreMIPSLo(pCode, dwRD, EAX_CODE);

			LoadMIPSHi(pCode, EDX_CODE, dwRT);	// Put EDX first to allow pipelining with above
			LoadMIPSHi(pCode, EAX_CODE, dwRS);
			OR(EAX_CODE, EDX_CODE);
			StoreMIPSHi(pCode, dwRD, EAX_CODE);
		}
	}
	return TRUE;

}


BOOL SR_Emit_Special_XOR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);


	if (SR_MATH_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_XOR);
	}
	else
	{
		//g_qwGPR[dwRD] = g_qwGPR[dwRS] ^ g_qwGPR[dwRT];

		pCode->dwNumOptimised++;

		// Don't write to r0
		if (dwRD == 0) return TRUE;	// Don't clear r0

		// We could special case RS or RT being zero (or the same), but this never seems to happen
		// Both registers non zero - perform operation as normal
		LoadMIPSLo(pCode, EAX_CODE, dwRS);
		LoadMIPSLo(pCode, EDX_CODE, dwRT);
		XOR(EAX_CODE, EDX_CODE);
		StoreMIPSLo(pCode, dwRD, EAX_CODE);

		LoadMIPSHi(pCode, EDX_CODE, dwRT);	// Put EDX first to allow pipelining with above
		LoadMIPSHi(pCode, EAX_CODE, dwRS);
		XOR(EAX_CODE, EDX_CODE);
		StoreMIPSHi(pCode, dwRD, EAX_CODE);
	}
	return TRUE;
}



BOOL SR_Emit_Special_SLT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{

	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);


	if (SR_MATH_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_SLT);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Cast to ints to ensure sign is taken into account
		//if ((s64)g_qwGPR[dwRS] < (s64)g_qwGPR[dwRT]) {
		//	g_qwGPR[dwRD] = 1;
		//} else {
		//	g_qwGPR[dwRD] = 0;
		//}
		if (dwRD == 0) return TRUE;		// Don't set r0

		if (dwRS == REG_r0)
		{
			// TODO - CMPI with 0, not CMP with ecx
			XOR(ECX_CODE, ECX_CODE);		// RS == 0
			LoadMIPSHi(pCode, EDX_CODE, dwRT);

			XOR(EAX_CODE, EAX_CODE);		// Clear dlo so that SETcc AL works
			CMP(ECX_CODE, EDX_CODE);		// 2 bytes
			SETL(EAX_CODE);					// 3 bytes

			// RS/lo is still 0
			LoadMIPSLo(pCode, EDX_CODE, dwRT);		// 
			
			JNE(0x5);						// Next two instructions are 5 bytes

			CMP(ECX_CODE, EDX_CODE);		// 2 bytes
			SETB(EAX_CODE);					// 3 bytes
		}
		else if (dwRT == REG_r0)
		{
			// TODO - CMPI with 0?
			LoadMIPSHi(pCode, ECX_CODE, dwRS);
			XOR(EDX_CODE, EDX_CODE);		// RT == 0

			XOR(EAX_CODE, EAX_CODE);		// Clear dlo so that SETcc AL works
			CMP(ECX_CODE, EDX_CODE);		// 2 bytes
			SETL(EAX_CODE);					// 3 bytes

			LoadMIPSLo(pCode, ECX_CODE, dwRS);		// See comments on SLT for why we load before branch
			// RT is still 0

			JNE(0x5);						// Next two instructions are 5 bytes

			CMP(ECX_CODE, EDX_CODE);		// 2 bytes
			SETB(EAX_CODE);					// 3 bytes
		}
		else
		{
			LoadMIPSHi(pCode, ECX_CODE, dwRS);
			LoadMIPSHi(pCode, EDX_CODE, dwRT);

			XOR(EAX_CODE, EAX_CODE);		// Clear dlo so that SETcc AL works
			CMP(ECX_CODE, EDX_CODE);		// 2 bytes
			SETL(EAX_CODE);					// 3 bytes

			LoadMIPSLo(pCode, ECX_CODE, dwRS);		// See comments on SLT for why we load before branch
			LoadMIPSLo(pCode, EDX_CODE, dwRT);		// 
			
			JNE(0x5);						// Next two instructions are 5 bytes

			CMP(ECX_CODE, EDX_CODE);		// 2 bytes
			SETB(EAX_CODE);					// 3 bytes
		}
		// Store result in RD
		StoreMIPSLo(pCode, dwRD, EAX_CODE);
		SetMIPSHi(pCode, dwRD, 0);
		
	}
	
	return TRUE;

}
BOOL SR_Emit_Special_SLTU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S_S(dwRD, dwRS, dwRT);

	if (SR_MATH_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_SLTU);
	}
	else
	{
		pCode->dwNumOptimised++;

		if (dwRD == 0) return TRUE;		// Don't set r0

		// Code is the same as above, but uses SETB for both tests
		
		if (dwRS == REG_r0)
		{
			XOR(ECX_CODE, ECX_CODE);		// RS is zero!
			LoadMIPSHi(pCode, EDX_CODE, dwRT);

			XOR(EAX_CODE, EAX_CODE);		// Clear dlo so that SETcc AL works
			CMP(ECX_CODE, EDX_CODE);		// 2 bytes
			SETB(EAX_CODE);					// 3 bytes
			
			// ECX is still 0
			LoadMIPSLo(pCode, EDX_CODE, dwRT);				// See comments on SLT for why we load before branch
			
			JNE(0x5);						// Next two instructions are 5 bytes

			CMP(ECX_CODE, EDX_CODE);	// 2 bytes
			SETB(EAX_CODE);				// 3 bytes
		}
		else if (dwRT == REG_r0)
		{
			LoadMIPSHi(pCode, ECX_CODE, dwRS);
			XOR(EDX_CODE, EDX_CODE);		// RT is zero

			XOR(EAX_CODE, EAX_CODE);		// Clear dlo so that SETcc AL works
			CMP(ECX_CODE, EDX_CODE);		// 2 bytes
			SETB(EAX_CODE);					// 3 bytes
			
			LoadMIPSLo(pCode, ECX_CODE, dwRS);		// See comments on SLT for why we load before branch
			// EDX is still 0
			
			JNE(0x5);						// Next two instructions are 5 bytes

			CMP(ECX_CODE, EDX_CODE);	// 2 bytes
			SETB(EAX_CODE);				// 3 bytes
		}
		else
		{
			LoadMIPSHi(pCode, ECX_CODE, dwRS);
			LoadMIPSHi(pCode, EDX_CODE, dwRT);

			XOR(EAX_CODE, EAX_CODE);		// Clear dlo so that SETcc AL works
			CMP(ECX_CODE, EDX_CODE);		// 2 bytes
			SETB(EAX_CODE);					// 3 bytes
			
			LoadMIPSLo(pCode, ECX_CODE, dwRS);		// See comments on SLT for why we load before branch
			LoadMIPSLo(pCode, EDX_CODE, dwRT);		//
			
			JNE(0x5);						// Next two instructions are 5 bytes

			CMP(ECX_CODE, EDX_CODE);	// 2 bytes
			SETB(EAX_CODE);				// 3 bytes

		}
			
		// Store result in RD
		StoreMIPSLo(pCode, dwRD, EAX_CODE);
		SetMIPSHi(pCode, dwRD, 0);
	}
	return TRUE;

}

