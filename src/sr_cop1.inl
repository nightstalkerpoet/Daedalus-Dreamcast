#define SR_COP1_OPTIMISE_FLAG		(pCode->dwOptimiseLevel < 1)
//#define SR_COP1_OPTIMISE_FLAG		1

BOOL SR_Emit_Cop1_MFC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	SR_Stat_D(dwRT);
	SR_Stat_Single_S(dwFS);
	
	if (SR_COP1_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_MFC1);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Simulate SR_FP stuff
		SR_FP_MFC1(dwFS);


		// MFC1 in the manual says this is a sign-extended result
		//g_qwGPR[dwRT] = (s64)(s32)LoadFPR_Word(dwFS);

		// Get value from fp reg
		MOV_REG_MEM(EAX_CODE, &g_qwCPR[1][dwFS]);

		// Sign extend
		CDQ;

		StoreMIPSLo(pCode, dwRT, EAX_CODE);

		// OR: (thanks zilmar)
		// SARI(EAX_CODE, 31);
		// StoreMIPSHi(pCode, dwRT, EAX_CODE);
		
		StoreMIPSHi(pCode, dwRT, EDX_CODE);
	}
	return TRUE;
}


BOOL SR_Emit_Cop1_MTC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwFS = fs(dwOp);

	SR_Stat_S(dwRT);
	SR_Stat_Single_D(dwFS);

	if (SR_COP1_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_MTC1);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Manual says top bits undefined after load
		//StoreFPR_Word(dwFS, (s32)g_qwGPR[dwRT]);

		// Simulate SR_FP stuff
		SR_FP_MTC1(dwFS);

		DWORD iReg = g_MIPSRegInfo[dwRT].dwCachedIReg;

		// Optimisation - avoid reg->reg copy by moving value directly!
		if (iReg != ~0 && g_MIPSRegInfo[dwRT].bValid)
		{
			DPF(DEBUG_DYNREC, "  ++ Using value direct from cahced reg...\\/");
			MOV_MEM_REG(&g_qwCPR[1][dwFS], (WORD)iReg);
		}
		else
		{
			LoadMIPSLo(pCode, EAX_CODE, dwRT);
			MOV_MEM_REG(&g_qwCPR[1][dwFS], EAX_CODE);
		}

	}
	return TRUE;
}


BOOL SR_Emit_Cop1_CFC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwRT = rt(dwOp);

	SR_Stat_D(dwRT);

	// Uses CCR reg
	if (SR_COP1_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_CFC1);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Only defined for reg 0 or 31
		if (dwFS == 0 || dwFS == 31)
		{
			// Just do Lo load/store - high bits not valid
			//g_qwGPR[dwRT] = (s64)(s32)g_qwCCR[1][dwFS];

			DWORD iCachedReg;

			iCachedReg = g_MIPSRegInfo[dwRT].dwCachedIReg;
			if (iCachedReg != ~0)
			{
				DPF(DEBUG_DYNREC, "  ++ Optimising for cached value");
				MOV_REG_MEM((WORD)iCachedReg, ((BYTE *)&g_qwCCR[1][dwFS]));	

				g_MIPSRegInfo[dwRT].bDirty = TRUE;
				g_MIPSRegInfo[dwRT].bValid = TRUE;
			}
			else
			{
				MOV_REG_MEM((WORD)EAX_CODE, ((BYTE *)&g_qwCCR[1][dwFS]));	
				
				StoreMIPSLo(pCode, dwRT, EAX_CODE);
			}
			
		}
		else
		{
			// Ignore!
		}

	}
	return TRUE;
}


BOOL SR_Emit_Cop1_CTC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwRT = rt(dwOp);

	SR_Stat_S(dwRT);
	// USes CCR

	if (SR_COP1_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_CTC1);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Only defined for reg 0 or 31
		if (dwFS == 0 || dwFS == 31)
		{
			// Just do Lo load/store - high bits not valid
			//g_qwCCR[1][dwFS] = g_qwGPR[dwRT];
			DWORD iCachedReg;

			iCachedReg = g_MIPSRegInfo[dwRT].dwCachedIReg;
			if (iCachedReg != ~0)
			{
				EnsureCachedValidLo(pCode, dwRT);

				DPF(DEBUG_DYNREC, "  ++ Optimising for cached value");
				MOV_MEM_REG(((BYTE *)&g_qwCCR[1][dwFS]), (WORD)iCachedReg );	
			}
			else
			{
				LoadMIPSLo(pCode, EAX_CODE, dwRT);
				MOV_MEM_REG(((BYTE *)&g_qwCCR[1][dwFS]), EAX_CODE );
			}
			
		}
		else
		{
			// Ignore!
		}

	}
	return TRUE;
}
