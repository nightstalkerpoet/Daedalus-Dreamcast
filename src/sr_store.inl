#define SR_STORE_OPTIMISE_FLAG		pCode->dwOptimiseLevel < 1
//#define SR_STORE_OPTIMISE_FLAG		1




BOOL SR_Emit_SW(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Save_B_S(dwBase, dwRT);
		
	if (SR_STORE_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_SW);
	}
	else
	{
		s32  nOffset = (s32)(s16)imm(dwOp);

		pCode->dwNumOptimised++;
		
		// We only optimise write using sp as the base (pushes onto the stack)
		// The reason for this is that other bases may point to hardware regs,
		// and so we may need to Update memory following the write
		if (dwBase == REG_sp && pCode->bSpCachedInESI)
		{
			// TODO - optimise for when dwRT is cached!
			LoadMIPSLo(pCode, EAX_CODE, dwRT);

			if (nOffset < 128 && nOffset > -128)
			{
				DPF(DEBUG_DYNREC, " Using cached reg in ESI - using s16 offset");
				// Use s16 form:
				// mov dword ptr [esi + b], eax
				pCode->EmitBYTE(0x89);
				pCode->EmitBYTE(0x46);
				pCode->EmitBYTE((BYTE)nOffset);
			}
			else
			{
				DPF(DEBUG_DYNREC, " Using cached reg in ESI - using long offset");
				// Use long form:
				// mov dword ptr [esi + nnnnnnnn], eax
				pCode->EmitBYTE(0x89);
				pCode->EmitBYTE(0x86);
				pCode->EmitDWORD((u32)nOffset);
			}
			
		}
		else
		{
			// This code is different from SWC1, because we want to call MemoryUpdate after
			// the call to update system registers, copy memory etc

			// push data - if data is cached, push that reg!!
			if (dwRT == 0)
			{
				// push 0
				DPF(DEBUG_DYNREC, "  ++ Pushing constant 0");
				XOR(EDX_CODE, EDX_CODE);		// fastcall
			}
			else if (g_MIPSRegInfo[dwRT].dwCachedIReg != ~0)
			{
				// If dwRT is cached, push that value instead
				DPF(DEBUG_DYNREC, "  ++ Pushing cached value");

				EnsureCachedValidLo(pCode, dwRT);

				MOV(EDX_CODE, (WORD)g_MIPSRegInfo[dwRT].dwCachedIReg);	// Fastcall
			}
			else
			{
				LoadMIPSLo(pCode, EDX_CODE, dwRT);
			}

			// Get base - we could cache this?
			LoadMIPSLo(pCode, ECX_CODE, dwBase);			// For fastcall, arg in ecx


			// Add offset
			if (nOffset != 0)
			{
				ADDI(ECX_CODE, nOffset);
			}

			// dwAddress in ECX
			// dwData in EDX

			MOV(EAX_CODE, ECX_CODE);			// For fastcall

			// Get top bits (offset into table) in ECX
			SHRI(EAX_CODE, 18);

			// call dword ptr [g_WriteAddressValueLookupTable + eax*4]
			pCode->EmitBYTE(0xFF);
			pCode->EmitBYTE(0x14);
			pCode->EmitBYTE(0x85);
			pCode->EmitDWORD((DWORD)g_WriteAddressValueLookupTable);		// Won't work if g_ReadAddressLookupTable recreated

		}
	}
	
	return TRUE;
}





BOOL SR_Emit_SH(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Save_B_S(dwBase, dwRT);
	
	if (SR_STORE_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_SH);
	}
	else
	{
		s32  nOffset = (s32)(s16)imm(dwOp);

		pCode->dwNumOptimised++;

		// TODO - Try and use cached base?
		// This is different from SW(), because for the most part we don't care about
		// Writes to system registers!
		
		// Get base (this could be cached!)
		LoadMIPSLo(pCode, EAX_CODE, dwBase);

		// Add offset
		if (nOffset != 0)
		{
			ADDI(EAX_CODE, nOffset);
		}

		// Fiddle Bits!
		XORI(EAX_CODE, 0x2);

		MOV(ECX_CODE, EAX_CODE);		// For fastcall


		// Get top bits (offset into table) in ECX
		SHRI(EAX_CODE, 18);

		// call dword ptr [g_WriteAddressLookupTable + eax*4]
		pCode->EmitBYTE(0xFF);
		pCode->EmitBYTE(0x14);
		pCode->EmitBYTE(0x85);
		pCode->EmitDWORD((DWORD)g_WriteAddressLookupTable);		// Won't work if g_WriteAddressLookupTable recreated

		// EAX hold memory address to write to
		// Get value to write
		if (dwRT == 0)
		{
			DPF(DEBUG_DYNREC, "  ++ Using constant 0");
			XOR(EDX_CODE, EDX_CODE);
		}
		else
		{
			LoadMIPSLo(pCode, EDX_CODE, dwRT);
		}

		// mov word ptr [eax], dx
		pCode->EmitBYTE(0x66);
		pCode->EmitBYTE(0x89);
		pCode->EmitBYTE(0x10);

	}
	
	
	return TRUE;
}

BOOL SR_Emit_SB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Save_B_S(dwBase, dwRT);

	if (SR_STORE_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_SB);
	}
	else
	{
		s32  nOffset = (s32)(s16)imm(dwOp);

		pCode->dwNumOptimised++;

		// TODO - Try and use cached base?
		// This is different from SW(), because for the most part we don't care about
		// Writes to system registers!
		
		// Get base (this could be cached!)
		LoadMIPSLo(pCode, EAX_CODE, dwBase);

		// Add offset
		if (nOffset != 0)
		{
			ADDI(EAX_CODE, nOffset);
		}

		// Fiddle Bits!
		XORI(EAX_CODE, 0x3);

		MOV(ECX_CODE, EAX_CODE);
		
		// Get top bits (offset into table) in ECX
		SHRI(EAX_CODE, 18);

		// call dword ptr [g_WriteAddressLookupTable + eax*4]
		pCode->EmitBYTE(0xFF);
		pCode->EmitBYTE(0x14);
		pCode->EmitBYTE(0x85);
		pCode->EmitDWORD((DWORD)g_WriteAddressLookupTable);		// Won't work if g_WriteAddressLookupTable recreated

		// EAX hold memory address to write to
		// Get value to write
		if (dwRT == 0)
		{
			DPF(DEBUG_DYNREC, "  ++ Using constant 0");
			XOR(EDX_CODE, EDX_CODE);
		}
		else
		{
			LoadMIPSLo(pCode, EDX_CODE, dwRT);
		}

		// mov byte ptr [eax], dl
		pCode->EmitBYTE(0x88);
		pCode->EmitBYTE(0x10);

	}
		
	return TRUE;
}




BOOL SR_Emit_SWC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwBase = rs(dwOp);
	DWORD dwFT    = ft(dwOp);

	SR_Stat_Save_B(dwBase);
	SR_Stat_Single_S(dwFT);
	
	if (SR_STORE_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_SWC1);
	}
	else
	{
		pCode->dwNumOptimised++;

		s32  nOffset = (s32)(s16)imm(dwOp);

		/*
		DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
		Write32Bits(dwAddress, LoadFPR_Word(dwFT));
		*/

		// Simulate SR_FP stuff
		SR_FP_SWC1(dwFT);


		// If the base is SP, and we have SP cached, use ESI instead:
		if (dwBase == REG_sp && pCode->bSpCachedInESI)
		{
			// Get value to write
			MOV_REG_MEM(EAX_CODE, &g_qwCPR[1][dwFT]);

			if (nOffset < 128 && nOffset > -128)
			{
				DPF(DEBUG_DYNREC, " Using cached reg in ESI - using s16 offset");
				// Use s16 form:
				// mov dword ptr [esi + b], eax
				pCode->EmitBYTE(0x89);
				pCode->EmitBYTE(0x46);
				pCode->EmitBYTE((BYTE)nOffset);
			}
			else
			{
				DPF(DEBUG_DYNREC, " Using cached reg in ESI - using long offset");
				// Use long form:
				// mov dword ptr [esi + nnnnnnnn], eax
				pCode->EmitBYTE(0x89);
				pCode->EmitBYTE(0x86);
				pCode->EmitDWORD((DWORD)nOffset);
			}				
		}
		else
		{
			// This is different from SW(), because for the most part we don't care about
			// Writes to system registers!
			
			// Get base (this could be cached!)
			LoadMIPSLo(pCode, EAX_CODE, dwBase);

			// Add offset
			if (nOffset != 0)
			{
				ADDI(EAX_CODE, nOffset);
			}

			MOV(ECX_CODE, EAX_CODE);			// For fastcall

			// Get top bits (offset into table) in ECX
			SHRI(EAX_CODE, 18);

			// call dword ptr [g_WriteAddressLookupTable + eax*4]
			pCode->EmitBYTE(0xFF);
			pCode->EmitBYTE(0x14);
			pCode->EmitBYTE(0x85);
			pCode->EmitDWORD((DWORD)g_WriteAddressLookupTable);		// Won't work if g_WriteAddressLookupTable recreated

			// EAX hold memory address to write to
			// Get value to write
			MOV_REG_MEM(EDX_CODE, &g_qwCPR[1][dwFT]);

			// mov dword ptr [eax], edx
			pCode->EmitBYTE(0x89);
			pCode->EmitBYTE(0x10);
		}

	}

	return TRUE;
}
