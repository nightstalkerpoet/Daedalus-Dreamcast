#define SR_LOAD_OPTIMISE_FLAG		pCode->dwOptimiseLevel < 1
//#define SR_LOAD_OPTIMISE_FLAG		1


BOOL SR_Emit_LB(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);

	if (SR_LOAD_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_LB);
	}
	else
	{
		s32  nOffset = (s32)(s16)imm(dwOp);

		pCode->dwNumOptimised++;

		// TODO: If the base is SP, and we have SP cached, use ESI instead:
		{
			// Use the alternative code

			//DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
			//g_qwGPR[dwRT] = (s64)(s16)Read16Bits(dwAddress);

			// Get base - we could cache this?
			LoadMIPSLo(pCode, EAX_CODE, dwBase);

			// Add offset
			if (nOffset != 0)
			{
				ADDI(EAX_CODE, nOffset);
			}

			// Fiddle bits!
			XORI(EAX_CODE, 0x3);		

			MOV(ECX_CODE, EAX_CODE);			// For fastcall

			// Get top bits (offset into table) in EAX
			SHRI(EAX_CODE, 18);

			// call dword ptr [g_ReadAddressLookupTable + eax*4]
			pCode->EmitBYTE(0xFF);
			pCode->EmitBYTE(0x14);
			pCode->EmitBYTE(0x85);
			pCode->EmitDWORD((DWORD)g_ReadAddressLookupTable);		// Won't work if g_ReadAddressLookupTable recreated
			
			// mov al, byte ptr [eax]
			pCode->EmitBYTE(0x8A);
			pCode->EmitBYTE(0x00);

			// EAX_CODE now holds read 16 bits!
			// movsx	eax, al
			pCode->EmitBYTE(0x0F);
			pCode->EmitBYTE(0xBE);		// 0xBF for word sign extension
			pCode->EmitBYTE(0xC0);

			CDQ;

			StoreMIPSLo(pCode, dwRT, EAX_CODE);

			// OR: (thanks zilmar)
			// SARI(EAX_CODE, 31);
			// StoreMIPSHi(pCode, dwRT, EAX_CODE);

			StoreMIPSHi(pCode, dwRT, EDX_CODE);
		}

	}
		
	return TRUE;
}


BOOL SR_Emit_LBU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);

	if (SR_LOAD_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_LBU);
	}
	else
	{
		s32  nOffset = (s32)(s16)imm(dwOp);

		pCode->dwNumOptimised++;

		// TODO: If the base is SP, and we have SP cached, use ESI instead:
		{
			// Use the alternative code

			//DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
			//g_qwGPR[dwRT] = (u64)(u16)(u8)Read8Bits(dwAddress);

			// Get base - we could cache this?
			LoadMIPSLo(pCode, EAX_CODE, dwBase);

			// Add offset
			if (nOffset != 0)
			{
				ADDI(EAX_CODE, nOffset);
			}

			// Fiddle bits!
			XORI(EAX_CODE, 0x3);		

			MOV(ECX_CODE, EAX_CODE);			// For fastcall

			// Get top bits (offset into table) in ECX
			SHRI(EAX_CODE, 18);

			// call dword ptr [g_ReadAddressLookupTable + eax*4]
			pCode->EmitBYTE(0xFF);
			pCode->EmitBYTE(0x14);
			pCode->EmitBYTE(0x85);
			pCode->EmitDWORD((DWORD)g_ReadAddressLookupTable);		// Won't work if g_ReadAddressLookupTable recreated
			
			// mov al, byte ptr [eax]
			pCode->EmitBYTE(0x8A);
			pCode->EmitBYTE(0x00);

			// EAX_CODE now holds read 16 bits!
			// and		eax, 0x00FF	- Mask off top bits!
			//ANDI(EAX_CODE, 0x00FF);		- One byte longer than ax form
			pCode->EmitBYTE(0x25);
			pCode->EmitDWORD(0x000000FF);

			StoreMIPSLo(pCode, dwRT, EAX_CODE);

			// Top bits always 0
			SetMIPSHi(pCode, dwRT, 0);
		}

	}
		
	return TRUE;
}


BOOL SR_Emit_LH(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);
	
	if (SR_LOAD_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_LH);
	}
	else
	{
		s32  nOffset = (s32)(s16)imm(dwOp);

		pCode->dwNumOptimised++;

		// TODO: If the base is SP, and we have SP cached, use ESI instead:
		{
			// Use the alternative code

			//DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
			//g_qwGPR[dwRT] = (s64)(s16)Read16Bits(dwAddress);

			// Get base - we could cache this?
			LoadMIPSLo(pCode, EAX_CODE, dwBase);

			// Add offset
			if (nOffset != 0)
			{
				ADDI(EAX_CODE, nOffset);
			}

			// Fiddle bits!
			XORI(EAX_CODE, 0x2);		

			MOV(ECX_CODE, EAX_CODE);			// For fastcall

			// Get top bits (offset into table) in ECX
			SHRI(EAX_CODE, 18);

			// call dword ptr [g_ReadAddressLookupTable + eax*4]
			pCode->EmitBYTE(0xFF);
			pCode->EmitBYTE(0x14);
			pCode->EmitBYTE(0x85);
			pCode->EmitDWORD((DWORD)g_ReadAddressLookupTable);		// Won't work if g_ReadAddressLookupTable recreated
			
			// mov ax, word ptr [eax]
			pCode->EmitBYTE(0x66);
			pCode->EmitBYTE(0x8B);
			pCode->EmitBYTE(0x00);

			// EAX_CODE now holds read 16 bits!
			// movsx	eax, ax
			pCode->EmitBYTE(0x0F);
			pCode->EmitBYTE(0xBF);
			pCode->EmitBYTE(0xC0);

			CDQ;

			StoreMIPSLo(pCode, dwRT, EAX_CODE);

			// OR: (thanks zilmar)
			// SARI(EAX_CODE, 31);
			// StoreMIPSHi(pCode, dwRT, EAX_CODE);
			
			StoreMIPSHi(pCode, dwRT, EDX_CODE);
		}

	}

	return TRUE;
}

BOOL SR_Emit_LHU(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);
	
	if (SR_LOAD_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_LHU);
	}
	else
	{
		s32  nOffset = (s32)(s16)imm(dwOp);

		pCode->dwNumOptimised++;

		// TODO: If the base is SP, and we have SP cached, use ESI instead:
		{
			// Use the alternative code

			//DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
			//g_qwGPR[dwRT] = (u64)(u16)Read16Bits(dwAddress);

			// Get base - we could cache this?
			LoadMIPSLo(pCode, EAX_CODE, dwBase);

			// Add offset
			if (nOffset != 0)
			{
				ADDI(EAX_CODE, nOffset);
			}

			// Fiddle bits!
			XORI(EAX_CODE, 0x2);		

			MOV(ECX_CODE, EAX_CODE);			// For fastcall

			// Get top bits (offset into table) in ECX
			SHRI(EAX_CODE, 18);

			// call dword ptr [g_ReadAddressLookupTable + eax*4]
			pCode->EmitBYTE(0xFF);
			pCode->EmitBYTE(0x14);
			pCode->EmitBYTE(0x85);
			pCode->EmitDWORD((DWORD)g_ReadAddressLookupTable);		// Won't work if g_ReadAddressLookupTable recreated

			// mov ax, word ptr [eax]
			pCode->EmitBYTE(0x66);
			pCode->EmitBYTE(0x8B);
			pCode->EmitBYTE(0x00);

			// EAX_CODE now holds read 16 bits!
			// and		eax, 0xFFFF	- Mask off top bits!
			//ANDI(EAX_CODE, 0xFFFF);		- One byte longer than ax form
			pCode->EmitBYTE(0x25);
			pCode->EmitDWORD(0x0000FFFF);

			StoreMIPSLo(pCode, dwRT, EAX_CODE);

			// Top bits always 0
			SetMIPSHi(pCode, dwRT, 0);
		}

	}
	return TRUE;
}


BOOL SR_Emit_LW(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{
	DWORD dwRT = rt(dwOp);
	DWORD dwBase = rs(dwOp);

	SR_Stat_Load_B_D(dwBase, dwRT);

	if (SR_LOAD_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_LW);
	}
	else
	{
		s32  nOffset = (s32)(s16)imm(dwOp);

		pCode->dwNumOptimised++;

		// If the base is SP, and we have SP cached, use ESI instead:
		if (dwBase == REG_sp && pCode->bSpCachedInESI)
		{
			if (nOffset < 128 && nOffset > -128)
			{
				DPF(DEBUG_DYNREC, " Using cached reg in ESI - using s16 offset");
				// Use s16 form:
				// mov eax, dword ptr [esi + b]
				pCode->EmitBYTE(0x8B);
				pCode->EmitBYTE(0x46);
				pCode->EmitBYTE((BYTE)nOffset);
			}
			else
			{
				DPF(DEBUG_DYNREC, " Using cached reg in ESI - using long offset");
				// Use long form:
				// mov eax, dword ptr [esi + nnnnnnnn]
				pCode->EmitBYTE(0x8B);
				pCode->EmitBYTE(0x86);
				pCode->EmitDWORD((DWORD)nOffset);
			}
			

			// EAX_CODE now holds read 32 bits!
			CDQ;

			StoreMIPSLo(pCode, dwRT, EAX_CODE);

			// OR: (thanks zilmar)
			// SARI(EAX_CODE, 31);
			// StoreMIPSHi(pCode, dwRT, EAX_CODE);
			
			StoreMIPSHi(pCode, dwRT, EDX_CODE);
			

		}
		else
		{
			// Use the alternative code

			//DWORD	dwAddress = (u32)((s32)g_qwGPR[dwBase] + nOffset);
			//g_qwGPR[dwRT] = (s64)(s32)Read32Bits(dwAddress);

			// Get base - we could cache this?
			LoadMIPSLo(pCode, EAX_CODE, dwBase);

			// Add offset
			if (nOffset != 0)
			{
				ADDI(EAX_CODE, nOffset);
			}

			MOV(ECX_CODE, EAX_CODE);			// For fastcall

			// Get top bits (offset into table) in ECX
			SHRI(EAX_CODE, 18);

			// call dword ptr [g_ReadAddressLookupTable + eax*4]
			pCode->EmitBYTE(0xFF);
			pCode->EmitBYTE(0x14);
			pCode->EmitBYTE(0x85);
			pCode->EmitDWORD((DWORD)g_ReadAddressLookupTable);		// Won't work if g_ReadAddressLookupTable recreated

			// mov eax, dword ptr [eax]
			pCode->EmitBYTE(0x8B);
			pCode->EmitBYTE(0x00);

			// EAX_CODE now holds read 32 bits!
			CDQ;

			StoreMIPSLo(pCode, dwRT, EAX_CODE);

			// OR: (thanks zilmar)
			// SARI(EAX_CODE, 31);
			// StoreMIPSHi(pCode, dwRT, EAX_CODE);
			
			StoreMIPSHi(pCode, dwRT, EDX_CODE);
		}

	}

	return TRUE;
}


BOOL SR_Emit_LWC1(CDynarecCode *pCode, DWORD dwOp, DWORD * pdwFlags)
{

	DWORD dwBase = rs(dwOp);
	DWORD dwFT   = ft(dwOp);

	SR_Stat_Load_B(dwBase);
	SR_Stat_Single_D(dwFT);

	if (SR_LOAD_OPTIMISE_FLAG)
	{
		SR_Emit_Generic_R4300(pCode, dwOp, R4300_LWC1);
	}
	else
	{
		pCode->dwNumOptimised++;

		s32  nOffset = (s32)(s16)imm(dwOp);

		// Simulate SR_FP stuff
		SR_FP_LWC1(dwFT);

		/*
		DWORD dwAddress = (u32)((s32)g_qwGPR[dwBase] + (s32)wOffset);
		StoreFPR_Word(dwFT, Read32Bits(dwAddress));
		*/

		// If the base is SP, and we have SP cached, use ESI instead:
		if (dwBase == REG_sp && pCode->bSpCachedInESI)
		{
			if (nOffset < 128 && nOffset > -128)
			{
				DPF(DEBUG_DYNREC, " Using cached reg in ESI - using s16 offset");
				// Use s16 form:
				// mov eax, dword ptr [esi + b]
				pCode->EmitBYTE(0x8B);
				pCode->EmitBYTE(0x46);
				pCode->EmitBYTE((BYTE)nOffset);
			}
			else
			{
				DPF(DEBUG_DYNREC, " Using cached reg in ESI - using long offset");
				// Use long form:
				// mov eax, dword ptr [esi + nnnnnnnn]
				pCode->EmitBYTE(0x8B);
				pCode->EmitBYTE(0x86);
				pCode->EmitDWORD((DWORD)nOffset);
			}
			

			// EAX_CODE now holds read 32 bits!
			// StoreFPR_Word(dwFT, Read32Bits(dwAddress));
			MOV_MEM_REG(((BYTE *)&g_qwCPR[1][dwFT]), EAX_CODE );
			
		}
		else
		{
			// Get base (this could be cached!)
			LoadMIPSLo(pCode, EAX_CODE, dwBase);

			// Add offset
			if (nOffset != 0)
			{
				ADDI(EAX_CODE, nOffset);
			}

			MOV(ECX_CODE, EAX_CODE);
			
			// Get top bits (offset into table) in ECX
			SHRI(EAX_CODE, 18);

			// call dword ptr [g_ReadAddressLookupTable + eax*4]
			pCode->EmitBYTE(0xFF);
			pCode->EmitBYTE(0x14);
			pCode->EmitBYTE(0x85);
			pCode->EmitDWORD((DWORD)g_ReadAddressLookupTable);		// Won't work if g_ReadAddressLookupTable recreated

			// mov eax, dword ptr [eax]
			pCode->EmitBYTE(0x8B);
			pCode->EmitBYTE(0x00);

			// StoreFPR_Word(dwFT, Read32Bits(dwAddress));
			MOV_MEM_REG(((BYTE *)&g_qwCPR[1][dwFT]), EAX_CODE );
		}

	}
	return TRUE;
}