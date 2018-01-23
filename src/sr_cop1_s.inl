#define SR_COP1_S_OPTIMISE_FLAG		pCode->dwOptimiseLevel < 1
//#define SR_COP1_S_OPTIMISE_FLAG		1

BOOL SR_Emit_Cop1_S_ADD(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_D_S_S(dwFD, dwFS, dwFT);


	if (SR_COP1_S_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_ADD);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Simulate SR_FP stuff
		SR_FP_S_ADD(dwFD, dwFS, dwFT);

		// fd = fs+ft
	/**	FLD_MEM(&g_qwCPR[1][dwFS]);
		FADD_MEM(&g_qwCPR[1][dwFT]);
		FSTP_MEM(&g_qwCPR[1][dwFD]);**/

	}
	return TRUE;
}



BOOL SR_Emit_Cop1_S_SUB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_D_S_S(dwFD, dwFS, dwFT);

	if (SR_COP1_S_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_SUB);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Simulate SR_FP stuff
		SR_FP_S_SUB(dwFD, dwFS, dwFT);
		
		// fd = fs+ft

	/**	FLD_MEM(&g_qwCPR[1][dwFS]);
		FSUB_MEM(&g_qwCPR[1][dwFT]);
		FSTP_MEM(&g_qwCPR[1][dwFD]);**/

	}
	return TRUE;
}

BOOL SR_Emit_Cop1_S_MUL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_D_S_S(dwFD, dwFS, dwFT);
	
	if (SR_COP1_S_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_MUL);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Simulate SR_FP stuff
		SR_FP_S_MUL(dwFD, dwFS, dwFT);
		
		// fd = fs+ft
	/*/	FLD_MEM(&g_qwCPR[1][dwFS]);
		FMUL_MEM(&g_qwCPR[1][dwFT]);
		FSTP_MEM(&g_qwCPR[1][dwFD]);**/
	}
	return TRUE;
}


BOOL SR_Emit_Cop1_S_DIV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFT = ft(dwOp);
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_D_S_S(dwFD, dwFS, dwFT);
	
	if (SR_COP1_S_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_DIV);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Simulate SR_FP stuff
		SR_FP_S_DIV(dwFD, dwFS, dwFT);
		
		// fd = fs/ft
	/**	FLD_MEM(&g_qwCPR[1][dwFS]);
		FDIV_MEM(&g_qwCPR[1][dwFT]);
		FSTP_MEM(&g_qwCPR[1][dwFD]);**/

	}
		
	return TRUE;
}

BOOL SR_Emit_Cop1_S_SQRT(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_D_S(dwFD, dwFS);
	
	if (SR_COP1_S_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_SQRT);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Simulate SR_FP stuff
		SR_FP_S_SQRT(dwFD, dwFS);
		
		// fd = sqrtf(fs)
	/**	FLD_MEM(&g_qwCPR[1][dwFS]);
		FSQRT;
		FSTP_MEM(&g_qwCPR[1][dwFD]);**/

	}
	return TRUE;
}

BOOL SR_Emit_Cop1_S_MOV(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_D_S(dwFD, dwFS);
	
	if (SR_COP1_S_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_MOV);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Simulate SR_FP stuff
		SR_FP_S_MOV(dwFD, dwFS);

		// fd = fs
		// Just do MOV_MEM_MEM?
	/**	FLD_MEM(&g_qwCPR[1][dwFS]);
		FSTP_MEM(&g_qwCPR[1][dwFD]);**/
	}
		
	return TRUE;
}

BOOL SR_Emit_Cop1_S_NEG(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwFS = fs(dwOp);
	DWORD dwFD = fd(dwOp);

	SR_Stat_Single_D_S(dwFD, dwFS);
	
	if (SR_COP1_S_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Cop1_S_NEG);
	}
	else
	{
		pCode->dwNumOptimised++;

		// Simulate SR_FP stuff
		SR_FP_S_NEG(dwFD, dwFS);
		
		// fd = -fs
	/**	FLD_MEM(&g_qwCPR[1][dwFS]);
		FCHS;
		FSTP_MEM(&g_qwCPR[1][dwFD]);**/
	}
	
	return TRUE;
}

