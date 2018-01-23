#define SR_MATH_IMM_OPTIMISE_FLAG		pCode->dwOptimiseLevel < 1
//#define SR_MATH_IMM_OPTIMISE_FLAG		1



BOOL SR_Emit_ADDI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	return SR_Emit_ADDIU(pCode, dwOp, pdwFlags);
	//return TRUE;
}

BOOL SR_Emit_ADDIU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);

	SR_Stat_D_S(dwRT, dwRS);
			
	if (SR_MATH_IMM_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_ADDIU);
	}
	else
	{
		s32   nData = (s32)(s16)imm(dwOp);

		//g_qwGPR[dwRT] = (s64)g_qwGPR[dwRS] + (s64)(s32)wData;

		// We can do the following optimisations:
		//  If rs is reg0, we just copy the immediate data to regRT
		//  If wData is 0, we can copy regRS to regRT
		pCode->dwNumOptimised++;

		if (dwRT == REG_r0) return TRUE;

		if (dwRS == REG_r0)
		{
			// The source register is zero, so we only have to consider the immediate value
			SetMIPSLo(pCode, dwRT, nData);
			if (nData >= 0 )
			{
				SetMIPSHi(pCode, dwRT, 0);		// Set top bits to zero
			}
			else
			{
				SetMIPSHi(pCode, dwRT, 0xFFFFFFFF);	// Set top bits to -1
			}
		}
		else
		{
			// Try to optimise low-word addition
			if (dwRS == dwRT)
			{
				// Optimise for in-place addition
				DWORD iCachedReg = g_MIPSRegInfo[dwRT].dwCachedIReg;

				if (iCachedReg != ~0)
				{
					DPF(DEBUG_DYNREC, "  ++ ADDI1: Adding inplace");
					// Ensure reg is loaded...
					EnsureCachedValidLo(pCode, dwRT);

					// Add inplace
					ADDI((WORD)iCachedReg, nData);
					g_MIPSRegInfo[dwRT].bDirty = TRUE;
				}
				else
				{
					LoadMIPSLo(pCode, EAX_CODE, dwRS);
					ADDI(EAX_CODE, nData);
					StoreMIPSLo(pCode, dwRT, EAX_CODE);
				}

			}
			else
			{
				// Optimise for in-place addition
				DWORD iCachedReg = g_MIPSRegInfo[dwRT].dwCachedIReg;
				
				if (iCachedReg != ~0)
				{
					DPF(DEBUG_DYNREC, "  ++ ADDI2: Adding inplace");
					
					// Ignore validity:
					LoadMIPSLo(pCode, (WORD)iCachedReg, dwRS);
					ADDI((WORD)iCachedReg, nData);
					g_MIPSRegInfo[dwRT].bDirty = TRUE;
					g_MIPSRegInfo[dwRT].bValid = TRUE;
				}
				else
				{
					LoadMIPSLo(pCode, EAX_CODE, dwRS);
					ADDI(EAX_CODE, nData);
					StoreMIPSLo(pCode, dwRT, EAX_CODE);
				}
			}

			// Finally perform high-word addition
			// WARN - the ADCI will only work if the flags have not been modified by a store above!
			LoadMIPSHi(pCode, EDX_CODE, dwRS);
			if (nData >= 0 ) {
				ADCI(EDX_CODE, 0);
			} else {
				ADCI(EDX_CODE, -1);		// 0xFF
			}
			StoreMIPSHi(pCode, dwRT, EDX_CODE);
		}
	}

	return TRUE;
}

BOOL SR_Emit_ANDI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{

	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);

	SR_Stat_D_S(dwRT, dwRS);

	if (SR_MATH_IMM_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_ANDI);
	}
	else
	{
		u32 dwData = (u32)(u16)imm(dwOp);

		pCode->dwNumOptimised++;

		if (dwRT == REG_r0) return TRUE;			// Skip attempts to write to r0

		//g_qwGPR[dwRT] = g_qwGPR[dwRS] & (u64)wData;
		// Note that top half is always set to 0...

		if (dwRT == dwRS)
		{
			// Optimise for ANDI inplace
			DWORD iDstReg = g_MIPSRegInfo[dwRT].dwCachedIReg;

			if (iDstReg != ~0)
			{
				// Load up reg if it's not already valid:
				EnsureCachedValidLo(pCode, dwRT);
				
				// And onto cached value
				DPF(DEBUG_DYNREC, "  ++ ANDI1: Anding inplace");
				ANDI((WORD)iDstReg, dwData);

				g_MIPSRegInfo[dwRT].bDirty = TRUE;
			}
			else
			{
				// If the reg is cached, we could still avoid a reg->reg copy here!?
				// Dest is uncached - we can't optimise any more
				LoadMIPSLo(pCode, EAX_CODE, dwRS);		// Reg->reg copy if cached
				ANDI(EAX_CODE, dwData);
				StoreMIPSLo(pCode, dwRT, EAX_CODE);
			}
		}
		else
		{
			// Source/Dest are different. See if we can optimise for destination being cached
			DWORD iDstReg = g_MIPSRegInfo[dwRT].dwCachedIReg;
			
			if (iDstReg != ~0) // Ignore validity - we're overwriting
			{
				// If dwRS is cached, this will be a reg->reg copy
				DPF(DEBUG_DYNREC, "  ++ ANDI2: Inplace, ignoring validity");
				LoadMIPSLo(pCode, (WORD)iDstReg, dwRS);
				ANDI((WORD)iDstReg, dwData);
				g_MIPSRegInfo[dwRT].bDirty = TRUE;
				g_MIPSRegInfo[dwRT].bValid = TRUE;
			}
			else
			{
				LoadMIPSLo(pCode, EAX_CODE, dwRS);
				ANDI(EAX_CODE, dwData);
				StoreMIPSLo(pCode, dwRT, EAX_CODE);
			}
		}
		
		
		SetMIPSHi(pCode, dwRT, 0);

	}
	
	return TRUE;
}

BOOL SR_Emit_ORI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{

	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);

	SR_Stat_D_S(dwRT, dwRS);

	if (SR_MATH_IMM_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_ORI);
	}
	else
	{
		u32 dwData = (u32)(u16)imm(dwOp);

		pCode->dwNumOptimised++;

		if (dwRT == REG_r0) return TRUE;			// Skip attempts to write to r0

		//g_qwGPR[dwRT] = g_qwGPR[dwRS] | (u64)wData;


		// Note that there is also a case when dwRT == dwRS
		if (dwRS == REG_r0)					// Setting to immediate data
		{
			// g_qwGRP[dwRT] = (u64)dwData
			DPF(DEBUG_DYNREC, "  ++ ORI0: Setting value");
			SetMIPSLo(pCode, dwRT, dwData);
			SetMIPSHi(pCode, dwRT, 0);
		}
		else if (dwRT == dwRS)
		{
			// Optimise for inplace oring
			DWORD iDstReg = g_MIPSRegInfo[dwRT].dwCachedIReg;

			if (iDstReg != ~0)
			{
				// Load up reg if it's not already valid:
				EnsureCachedValidLo(pCode, dwRT);
				
				// Or onto cached value
				DPF(DEBUG_DYNREC, "  ++ ORI1: Oring inplace");
				ORI((WORD)iDstReg, dwData);

				g_MIPSRegInfo[dwRT].bDirty = TRUE;

				// High bits are the same!
			}
			else
			{
				// If the reg is cached, we could still avoid a reg->reg copy here!?
				// Dest is uncached - we can't optimise any more
				LoadMIPSLo(pCode, EAX_CODE, dwRS);		// Reg->reg copy if cached
				ORI(EAX_CODE, dwData);
				StoreMIPSLo(pCode, dwRT, EAX_CODE);

				// Hi bits are the same!
			}

		}
		else								// "Normal" operation source/dest different
		{
			// Source/Dest are different. See if we can optimise for destination being cached
			DWORD iDstReg = g_MIPSRegInfo[dwRT].dwCachedIReg;
			
			if (iDstReg != ~0) // Ignore validity - we're overwriting
			{
				// If dwRS is cached, this will be a reg->reg copy
				DPF(DEBUG_DYNREC, "  ++ ORI2: Inplace, ignoring validity");
				LoadMIPSLo(pCode, (WORD)iDstReg, dwRS);
				ORI((WORD)iDstReg, dwData);
				g_MIPSRegInfo[dwRT].bDirty = TRUE;
				g_MIPSRegInfo[dwRT].bValid = TRUE;
			}
			else
			{
				LoadMIPSLo(pCode, EAX_CODE, dwRS);
				ORI(EAX_CODE, dwData);		// Code ORI to ignore oring in 0?
				StoreMIPSLo(pCode, dwRT, EAX_CODE);
			}

			// Copy top half, as regs are different			
			LoadMIPSHi(pCode, EAX_CODE, dwRS);
			StoreMIPSHi(pCode, dwRT, EAX_CODE);
		}
	}

	return TRUE;
}

BOOL SR_Emit_XORI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);

	SR_Stat_D_S(dwRT, dwRS);

	if (SR_MATH_IMM_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_XORI);
	}
	else
	{
		u32 dwData = (u32)(u16)imm(dwOp);
		//g_qwGPR[dwRT] = g_qwGPR[dwRS] ^ (u64)wData;

		pCode->dwNumOptimised++;

		if (dwRT == 0) return TRUE;
		
		LoadMIPSLo(pCode, EAX_CODE, dwRS);
		XORI(EAX_CODE, dwData);

		// Copy high bits to dwRT (i.e. xoring with 0)
		if (dwRS != dwRT)	// Don't bother if we're doing e.g. XORI s1,s1,0xFFFF
		{
			LoadMIPSHi(pCode, EDX_CODE, dwRS);
			StoreMIPSHi(pCode, dwRT, EDX_CODE);
		}

		StoreMIPSLo(pCode, dwRT, EAX_CODE);

	}
	return TRUE;
}

BOOL SR_Emit_LUI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{

	DWORD dwRT = rt(dwOp);

	SR_Stat_D(dwRT);

	if (SR_MATH_IMM_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_LUI);
	}
	else
	{
		pCode->dwNumOptimised++;
		
		s32 nData = ((s32)(s16)imm(dwOp))<<16;

		//g_qwGPR[dwRT] = (s64)(s32)(wData<<16);

		if (dwRT == 0) return TRUE;		// Don't set reg0

		// Check for next op being ADDIU or ADDI (common way of setting entire register
		//if (op(dwNextOp) == ADDIU || op(dwNextOp) == ADDI) {

		SetMIPSLo(pCode, dwRT, nData);
		if (nData >= 0)
		{
			SetMIPSHi(pCode, dwRT, 0);			// Clear tops bits
		}
		else
		{
			SetMIPSHi(pCode, dwRT, 0xFFFFFFFF);	// Set top bits
		}
	}

	return TRUE;
}



BOOL SR_Emit_SLTI(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{

	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);

	SR_Stat_D_S(dwRT, dwRS);

	if (SR_MATH_IMM_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_SLTI);
	}
	else
	{
		s32 nData = (s32)(s16)imm(dwOp);

		// Cast to ints to ensure sign is taken into account
		//g_qwGPR[dwRT] = ((s64)g_qwGPR[dwRS] < (s64)(s32)wData ? 1: 0);

		pCode->dwNumOptimised++;

		if (dwRT == 0) return TRUE;		// Don't set r0

		// TODO optimise for when cmp reg is cached/valid (compare direct)

		if (nData >= 0)
		{
			// nData is positive
			LoadMIPSHi(pCode, EDX_CODE, dwRS);
			XOR(EAX_CODE, EAX_CODE);			// Clear dlo so that SETcc AL works
			CMPI(EDX_CODE, 0x00000000);			// 6 bytes
			SETL(EAX_CODE);						// 3 bytes

			LoadMIPSLo(pCode, EDX_CODE, dwRS);			// Doesn't affect flags, but we must load before branch to preserve our internal record of the status
			
			JNE(0x9);							// Next two instructions are 9 bytes
			CMPI(EDX_CODE, nData);				// 6 bytes
			SETB(EAX_CODE);						// 3 bytes
		}
		else 
		{
			// nData is negative
			LoadMIPSHi(pCode, EDX_CODE, dwRS);
			XOR(EAX_CODE, EAX_CODE);			// Clear EAX so that SETcc AL works
			CMPI(EDX_CODE, 0xFFFFFFFF);			// 6 bytes
			SETL(EAX_CODE);						// 3 bytes
			
			LoadMIPSLo(pCode, EDX_CODE, dwRS);			// See comments above
			
			JNE(0x9);							// Next two instructions are 9 bytes
			CMPI(EDX_CODE, nData);				// 6 bytes
			SETB(EAX_CODE);						// 3 bytes
		}

		// Store result in RT
		StoreMIPSLo(pCode, dwRT, EAX_CODE);
		SetMIPSHi(pCode, dwRT, 0);				

	}
	return TRUE;
}

BOOL SR_Emit_SLTIU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwRS = rs(dwOp);

	SR_Stat_D_S(dwRT, dwRS);

	if (SR_MATH_IMM_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_SLTIU);
	}
	else
	{
		s32 nData = (s32)(s16)imm(dwOp);

		// Cast to ints to ensure sign is taken into account
		//g_qwGPR[dwRT] = ((s64)g_qwGPR[dwRS] < (s64)(s32)wData ? 1: 0);

		pCode->dwNumOptimised++;

		if (dwRT == 0) return TRUE;		// Don't set r0

		LoadMIPSHi(pCode, EDX_CODE, dwRS);
		if (nData >= 0) {
			// nData is positive
			XOR(EAX_CODE, EAX_CODE);			// Clear dlo so that SETcc AL works
			CMPI(EDX_CODE, 0x00000000);			// 6 bytes
			SETB(EAX_CODE);						// 3 bytes
			
			LoadMIPSLo(pCode, EDX_CODE, dwRS);			// See comments on SLT for why we load before branch
			
			JNE(0x9);							// Next two instructions are 9 bytes
			CMPI(EDX_CODE, nData);				// 6 bytes
			SETB(EAX_CODE);						// 3 bytes
		} else {
			// nData is negative
			XOR(EAX_CODE, EAX_CODE);			// Clear EAX so that SETcc AL works
			CMPI(EDX_CODE, 0xFFFFFFFF);			// 6 bytes
			SETB(EAX_CODE);						// 3 bytes
			
			LoadMIPSLo(pCode, EDX_CODE, dwRS);			// See comments on SLT for why we load before branch
			
			JNE(0x9);							// Next two instructions are 9 bytes
			CMPI(EDX_CODE, nData);				// 6 bytes
			SETB(EAX_CODE);						// 3 bytes
		}

		// Store result in RT
		StoreMIPSLo(pCode, dwRT, EAX_CODE);
		SetMIPSHi(pCode, dwRT, 0);				

	}
	return TRUE;
}







BOOL SR_Emit_Special_SLL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){

	if (dwOp == 0) 
	{
		pCode->dwNumOptimised++;
		return TRUE;
	}
	
	DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S(dwRD, dwRT);
	

	// Skip NOPS

	if (SR_MATH_IMM_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_SLL);
	}
	else
	{
		//g_qwGPR[dwRD] = (s64)(s32)((((u32)g_qwGPR[dwRT]) << (dwSA)) & 0xFFFFFFFF);

		pCode->dwNumOptimised++;

		if (dwRD == 0) return TRUE;

		if (dwRD == dwRT)
		{
			// Optimise for n = n << x
			DWORD iDstReg = g_MIPSRegInfo[dwRD].dwCachedIReg;
			
			if (iDstReg != ~0 && g_MIPSRegInfo[dwRD].bValid)
			{
				DPF(DEBUG_DYNREC, "  ++ Shifting in place using cached reg...\\/");
				SHLI((WORD)iDstReg, dwSA);
				g_MIPSRegInfo[dwRD].bDirty = TRUE;
				// Valid already set

				MOV(EAX_CODE, (WORD)iDstReg);
				CDQ;
				StoreMIPSHi(pCode, dwRD, EDX_CODE);
			}
			else
			{
				// Register is not cached! Use normal version
				LoadMIPSLo(pCode, EAX_CODE, dwRT);
				SHLI(EAX_CODE, dwSA);
				CDQ;
				StoreMIPSLo(pCode, dwRD, EAX_CODE);
				StoreMIPSHi(pCode, dwRD, EDX_CODE);	
			}
		}
		else
		{
			// Optimise for CACHED = UNCACHED << n
			LoadMIPSLo(pCode, EAX_CODE, dwRT);
			SHLI(EAX_CODE, dwSA);
			CDQ;

			StoreMIPSLo(pCode, dwRD, EAX_CODE);

			// OR: (thanks zilmar)
			// SARI(EAX_CODE, 31);
			// StoreMIPSHi(pCode, dwRT, EAX_CODE);
			StoreMIPSHi(pCode, dwRD, EDX_CODE);
		}
	}
	return TRUE;
}

BOOL SR_Emit_Special_SRL(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S(dwRD, dwRT);
	
	if (SR_MATH_IMM_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_SRL);
	}
	else
	{
		//g_qwGPR[dwRD] = (s64)(s32)(((u32)g_qwGPR[dwRT]) >> dwSA);

		pCode->dwNumOptimised++;

		if (dwRD == 0) return TRUE;	// Don't set reg0

		LoadMIPSLo(pCode, EAX_CODE, dwRT);
		SHRI(EAX_CODE, dwSA);
		//D2Q(EAX_CODE, EDX_CODE);	// This isn't necessary - top bits will ALWAYS be 0. I think
		SetMIPSHi(pCode, dwRD, 0);
		StoreMIPSLo(pCode, dwRD, EAX_CODE);
	}
	return TRUE;
}

BOOL SR_Emit_Special_SRA(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags){

	DWORD dwSA = sa(dwOp);
	DWORD dwRT = rt(dwOp);
	DWORD dwRD = rd(dwOp);

	SR_Stat_D_S(dwRD, dwRT);


	if (SR_MATH_IMM_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_Special_SRA);
	}
	else
	{
		// Need to check that this correctly works with sign
		//g_qwGPR[dwRD] = (s64)(s32)(((s32)g_qwGPR[dwRT]) >> dwSA);

		pCode->dwNumOptimised++;

		if (dwRD == 0) return TRUE;	// Don't set reg0

		LoadMIPSLo(pCode, EAX_CODE, dwRT);
		SARI(EAX_CODE, dwSA);
		CDQ;		// This isn't necessary - top bits will ALWAYS be same as before. I think

		StoreMIPSLo(pCode, dwRD, EAX_CODE);

		// OR: (thanks zilmar)
		// SARI(EAX_CODE, 31);
		// StoreMIPSHi(pCode, dwRT, EAX_CODE);

		StoreMIPSHi(pCode, dwRD, EDX_CODE);
		
	}
	return TRUE;
}