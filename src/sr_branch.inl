#define SR_BRANCH_OPTIMISE_FLAG		(pCode->dwOptimiseLevel < 1)
//#define SR_BRANCH_OPTIMISE_FLAG		1


BOOL SR_Emit_J(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// No registers modified!

	*pdwFlags |= SR_FLAGS_BRANCHES;

	// Set PC = StartPC + NumOps * 4
	SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_J);
	return TRUE;
}

BOOL SR_Emit_JAL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	SR_Stat_D(REG_ra);
	
	*pdwFlags |= SR_FLAGS_BRANCHES;

	// Set PC = StartPC + NumOps * 4
	SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_JAL);
	return TRUE;
}



BOOL SR_Emit_Special_JR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){

	DWORD dwRS = rs(dwOp);

	SR_Stat_S(dwRS);

	*pdwFlags |= SR_FLAGS_BRANCHES;

	// Set PC = StartPC + NumOps * 4
	SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

	// We use this, rather than R4300_Special_JR because this op is hacked
	// to allow us to detect when the rom jumps to the game boot 
	// address (so that we can apply the patches).
	// The special version of this op patches the "normal" version of
	// the op back intot he jump talbe after the patches
	// have been applied
	SR_Emit_Generic_R4300(pCode, dwOp, R4300SpecialInstruction[OP_JR]);
	return TRUE;

}

BOOL SR_Emit_Special_JALR(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){

	DWORD dwRS = rs(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S(dwRD, dwRS);

	*pdwFlags |= SR_FLAGS_BRANCHES;

	// Set PC = StartPC + NumOps * 4
	SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_JALR);
	return TRUE;
}




BOOL SR_Emit_BEQ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Only emit this operation if SpeedHack will not be executed
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	s32  nOffset = (s32)(s16)imm(dwOp);

	if (nOffset == -1)
		return FALSE;

	*pdwFlags |= SR_FLAGS_BRANCHES;
	
	SR_Stat_S_S(dwRS, dwRT);

	DWORD dwCurrPC = pCode->dwStartPC + (pCode->dwNumOps*4);
	DWORD dwJumpPC = dwCurrPC + (nOffset<<2) + 4;

	pCode->dwBranchTarget = dwJumpPC;

	
	if (SR_BRANCH_OPTIMISE_FLAG)
	{
		// Set PC = StartPC + NumOps * 4
		SetVar32(g_dwPC, dwCurrPC);

		SR_Emit_Generic_R4300(pCode, dwOp, R4300_BEQ);

	}
	else
	{
		pCode->dwNumOptimised++;
		
		if (dwJumpPC == pCode->dwStartPC)
		{
			// This is very common for BEQs
			DPF(DEBUG_DYNREC, "!!Detected block-referential BEQ jump!");

			// Get the next op:
			*pdwFlags |= SR_FLAGS_BRANCHES_TO_START;
		}

		*pdwFlags |= SR_FLAGS_HANDLE_BRANCH;

		// Perform the comparison - use cached regs if possible
		if (dwRS == dwRT)
		{
			DPF(DEBUG_DYNREC, "  ++ Optimizing for branch always taken...\\/");		
			// Branch is always taken
			SetVar32(g_dwNewPC, dwJumpPC);			// 10 bytes
			//CPU_Delay();		// Warning- this macro might change in future!
			SetVar32(g_nDelay, DO_DELAY);			// 10 bytes
	
		}
		else if (dwRT == REG_r0)	// dwRS rarely seems to be 0
		{
			DPF(DEBUG_DYNREC, "  ++ Comparing against constant 0...\\/");		

			DWORD iReg = g_MIPSRegInfo[dwRS].dwCachedIReg;
			if (iReg != ~0)
			{
				DPF(DEBUG_DYNREC, "  ++ Comparing inplace using cached reg...\\/");			
				// Make sure it's loaded
				EnsureCachedValidLo(pCode, dwRS);
				CMPI((WORD)iReg, 0);
			}
			else
			{
				LoadMIPSLo(pCode, EAX_CODE, dwRS);
				CMPI(EAX_CODE, 0);
			}
			JNE(10 + 10);
			SetVar32(g_dwNewPC, dwJumpPC);			// 10 bytes
			//CPU_Delay();		// Warning- this macro might change in future!
			SetVar32(g_nDelay, DO_DELAY);			// 10 bytes
			
		}
		else
		{
			DWORD iRegS = g_MIPSRegInfo[dwRS].dwCachedIReg;
			DWORD iRegT = g_MIPSRegInfo[dwRT].dwCachedIReg;

			if (iRegS != ~0)
			{
				DPF(DEBUG_DYNREC, "  ++ Comparing inplace using cached reg for RS...\\/");			
				EnsureCachedValidLo(pCode, dwRS);
			}
			else
			{
				LoadMIPSLo(pCode, EAX_CODE, dwRS);	
				iRegS = EAX_CODE;
			}

			if (iRegT != ~0)
			{
				DPF(DEBUG_DYNREC, "  ++ Comparing inplace using cached reg for RT...\\/");			
				EnsureCachedValidLo(pCode, dwRT);
			}
			else
			{
				LoadMIPSLo(pCode, EDX_CODE, dwRT);
				iRegT = EDX_CODE;
			}

			CMP((WORD)iRegS, (WORD)iRegT);
			JNE(10 + 10);
			SetVar32(g_dwNewPC, dwJumpPC);			// 10 bytes
			//CPU_Delay();		// Warning- this macro might change in future!
			SetVar32(g_nDelay, DO_DELAY);			// 10 bytes
		}
		


	}

	return TRUE;
}

BOOL SR_Emit_BNE(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Only emit this operation if SpeedHack will not be executed
	DWORD dwRS = rs(dwOp);
	DWORD dwRT = rt(dwOp);
	s32  nOffset = (s32)(s16)imm(dwOp);

	
	if (nOffset == -1)
		return FALSE;

	*pdwFlags |= SR_FLAGS_BRANCHES;
	
	SR_Stat_S_S(dwRS, dwRT);
	
	DWORD dwCurrPC = pCode->dwStartPC + (pCode->dwNumOps*4);
	DWORD dwJumpPC = dwCurrPC + (nOffset<<2) + 4;

	pCode->dwBranchTarget = dwJumpPC;
	
	if (SR_BRANCH_OPTIMISE_FLAG)
	{
		// Set PC = StartPC + NumOps * 4
		SetVar32(g_dwPC, dwCurrPC);

		SR_Emit_Generic_R4300(pCode, dwOp, R4300_BNE);

	}
	else
	{
		pCode->dwNumOptimised++;
		
		if (dwJumpPC == pCode->dwStartPC)
		{
			// This is very common
			DPF(DEBUG_DYNREC, "!!Detected block-referential BNE jump!");

			// Get the next op:
			*pdwFlags |= SR_FLAGS_BRANCHES_TO_START;
		}


		*pdwFlags |= SR_FLAGS_HANDLE_BRANCH;

		// Perform the comparison - use cached regs if possible
		if (dwRT == REG_r0)	// dwRS rarely seems to be 0
		{
			DPF(DEBUG_DYNREC, "  ++ Comparing against constant 0...\\/");		

			DWORD iReg = g_MIPSRegInfo[dwRS].dwCachedIReg;
			if (iReg != ~0)
			{
				DPF(DEBUG_DYNREC, "  ++ Comparing inplace using cached reg...\\/");			
				// Make sure it's loaded
				EnsureCachedValidLo(pCode, dwRS);
				CMPI((WORD)iReg, 0);
			}
			else
			{
				LoadMIPSLo(pCode, EAX_CODE, dwRS);
				CMPI(EAX_CODE, 0);
			}
		}
		else
		{
			DWORD iRegS = g_MIPSRegInfo[dwRS].dwCachedIReg;
			DWORD iRegT = g_MIPSRegInfo[dwRT].dwCachedIReg;

			if (iRegS != ~0)
			{
				DPF(DEBUG_DYNREC, "  ++ Comparing inplace using cached reg for RS...\\/");			
				EnsureCachedValidLo(pCode, dwRS);
			}
			else
			{
				LoadMIPSLo(pCode, EAX_CODE, dwRS);	
				iRegS = EAX_CODE;
			}

			if (iRegT != ~0)
			{
				DPF(DEBUG_DYNREC, "  ++ Comparing inplace using cached reg for RT...\\/");			
				EnsureCachedValidLo(pCode, dwRT);
			}
			else
			{
				LoadMIPSLo(pCode, EDX_CODE, dwRT);
				iRegT = EDX_CODE;
			}

			CMP((WORD)iRegS, (WORD)iRegT);
		}
		
		JE(10 + 10);
		SetVar32(g_dwNewPC, dwJumpPC);			// 10 bytes
		//CPU_Delay();		// Warning- this macro might change in future!
		SetVar32(g_nDelay, DO_DELAY);			// 10 bytes

		// Jump here if branch not taken

	}


	return TRUE;
}






BOOL SR_Emit_BLEZ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Only emit this operation if SpeedHack will not be executed
	DWORD dwRS = rs(dwOp);
	s32  nOffset = (s32)(s16)imm(dwOp);
	
	if (nOffset == -1)
		return FALSE;

	*pdwFlags |= SR_FLAGS_BRANCHES;

	SR_Stat_S(dwRS);
	
	DWORD dwCurrPC = pCode->dwStartPC + (pCode->dwNumOps*4);
	DWORD dwJumpPC = dwCurrPC + (nOffset<<2) + 4;

	pCode->dwBranchTarget = dwJumpPC;

	if (SR_BRANCH_OPTIMISE_FLAG)
	{
		// Set PC = StartPC + NumOps * 4
		SetVar32(g_dwPC, dwCurrPC);

		SR_Emit_Generic_R4300(pCode, dwOp, R4300_BLEZ);

	}
	else
	{
		pCode->dwNumOptimised++;
		
		if (dwJumpPC == pCode->dwStartPC)
		{
			// This is very common
			DPF(DEBUG_DYNREC, "!!Detected block-referential BLEZ jump!");

			// Get the next op:
			*pdwFlags |= SR_FLAGS_BRANCHES_TO_START;
		}

		*pdwFlags |= SR_FLAGS_HANDLE_BRANCH;

		// Perform the comparison - use cached regs if possible
		DWORD iReg = g_MIPSRegInfo[dwRS].dwCachedIReg;

		if (iReg != ~0)
		{
			DPF(DEBUG_DYNREC, "  ++ Comparing inplace using cached reg for RS...\\/");			
			EnsureCachedValidLo(pCode, dwRS);
		}
		else
		{
			LoadMIPSLo(pCode, EAX_CODE, dwRS);	
			iReg = EAX_CODE;
		}


		CMPI((WORD)iReg, 0);
		
		JG(10 + 10);	// Jump if NOT LE
		SetVar32(g_dwNewPC, dwJumpPC);			// 10 bytes
		//CPU_Delay();		// Warning- this macro might change in future!
		SetVar32(g_nDelay, DO_DELAY);			// 10 bytes

		// Jump here if branch not taken

	}


	return TRUE;
}

BOOL SR_Emit_BGTZ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Only emit this operation if SpeedHack will not be executed
	DWORD dwRS = rs(dwOp);
	s32  nOffset = (s32)(s16)imm(dwOp);
	
	if (nOffset == -1)
		return FALSE;
	
	*pdwFlags |= SR_FLAGS_BRANCHES;
	
	SR_Stat_S(dwRS);
	
	
	DWORD dwCurrPC = pCode->dwStartPC + (pCode->dwNumOps*4);
	DWORD dwJumpPC = dwCurrPC + (nOffset<<2) + 4;

	pCode->dwBranchTarget = dwJumpPC;

	if (SR_BRANCH_OPTIMISE_FLAG)
	{
		// Set PC = StartPC + NumOps * 4
		SetVar32(g_dwPC, dwCurrPC);
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_BGTZ);
	}
	else
	{
		pCode->dwNumOptimised++;
		
		if (dwJumpPC == pCode->dwStartPC)
		{
			// This is very common
			DPF(DEBUG_DYNREC, "!!Detected block-referential BGTZ jump!");

			// Get the next op:
			*pdwFlags |= SR_FLAGS_BRANCHES_TO_START;
		}

		*pdwFlags |= SR_FLAGS_HANDLE_BRANCH;

		// Perform the comparison - use cached regs if possible
		DWORD iReg = g_MIPSRegInfo[dwRS].dwCachedIReg;

		if (iReg != ~0)
		{
			DPF(DEBUG_DYNREC, "  ++ Comparing inplace using cached reg for RS...\\/");			
			EnsureCachedValidLo(pCode, dwRS);
		}
		else
		{
			LoadMIPSLo(pCode, EAX_CODE, dwRS);	
			iReg = EAX_CODE;
		}


		CMPI((WORD)iReg, 0);
		
		JLE(10 + 10);	// Jump if NOT G
		SetVar32(g_dwNewPC, dwJumpPC);			// 10 bytes
		//CPU_Delay();		// Warning- this macro might change in future!
		SetVar32(g_nDelay, DO_DELAY);			// 10 bytes

		// Jump here if branch not taken

	}

	return TRUE;
}



BOOL SR_Emit_RegImm_BLTZ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Only emit this operation if SpeedHack will not be executed
	DWORD dwRS = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);

	if (wOffset == -1)
		return FALSE;

	SR_Stat_S(dwRS);

	*pdwFlags |= SR_FLAGS_BRANCHES;

	// Set PC = StartPC + NumOps * 4
	SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_RegImm_BLTZ);
	return TRUE;
}

BOOL SR_Emit_RegImm_BGEZ(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	// Only emit this operation if SpeedHack will not be executed
	DWORD dwRS = rs(dwOp);
	s16 wOffset = (s16)imm(dwOp);

	if (wOffset == -1)
		return FALSE;

	SR_Stat_S(dwRS);
	
	*pdwFlags |= SR_FLAGS_BRANCHES;

	// Set PC = StartPC + NumOps * 4
	SetVar32(g_dwPC, pCode->dwStartPC + (pCode->dwNumOps*4));

	SR_Emit_Generic_R4300(pCode, dwOp, R4300_RegImm_BGEZ);
	return TRUE;
}
