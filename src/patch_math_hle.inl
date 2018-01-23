#include <math.h>
#define TEST_DISABLE_MATH_FUNCS //return PATCH_RET_NOT_PROCESSED;


DWORD Patch___ll_mul()
{
TEST_DISABLE_MATH_FUNCS
	DWORD dwHiA = (u32)g_qwGPR[REG_a0];
	DWORD dwLoA = (u32)g_qwGPR[REG_a1];
	DWORD dwHiB = (u32)g_qwGPR[REG_a2];
	DWORD dwLoB = (u32)g_qwGPR[REG_a3];

	u64 qwA = ((u64)dwHiA << 32) | (u64)dwLoA;
	u64 qwB = ((u64)dwHiB << 32) | (u64)dwLoB;

	u64 qwR = qwA * qwB;

	g_qwGPR[REG_v0] = (s64)qwR >> 32;
	g_qwGPR[REG_v1] = (s64)(s32)(qwR & 0xFFFFFFFF);

	return PATCH_RET_JR_RA;
}


// By Jun Su
DWORD Patch___ll_div() 
{ 
TEST_DISABLE_MATH_FUNCS
	s64 op1, op2, result; 
	
	// Fixed by  StrmnNrmn - regs cast to 32 bits so shift didn't work
	// This was breaking 007
	op1 = (s64)((u64)(u32)g_qwGPR[REG_a0] << 32 | (u64)(u32)g_qwGPR[REG_a1]); 
	op2 = (s64)((u64)(u32)g_qwGPR[REG_a2] << 32 | (u64)(u32)g_qwGPR[REG_a3]); 
	
	if (op2==0) 
	{ 
		return PATCH_RET_NOT_PROCESSED; 
	} 
	else 
	{ 
		result = op1 / op2; 
		// StrmnNrmn - the s32 casts were originally u32. Not sure if this is needed
		g_qwGPR[REG_v1] = (s64)(s32)(result & 0xffffffff); 
		g_qwGPR[REG_v0] = (s64)(s32)(result>>32); 
		return PATCH_RET_JR_RA; 
	} 
} 

// By Jun Su
DWORD Patch___ull_div() 
{ 
TEST_DISABLE_MATH_FUNCS
	u64 op1, op2, result; 
	
	// Fixed by  StrmnNrmn - regs cast to 32 bits so shift didn't work
	op1 = (u64)((u64)(u32)g_qwGPR[REG_a0] << 32 | (u64)(u32)g_qwGPR[REG_a1]); 
	op2 = (u64)((u64)(u32)g_qwGPR[REG_a2] << 32 | (u64)(u32)g_qwGPR[REG_a3]); 
	
	if (op2==0) 
	{ 
		return PATCH_RET_NOT_PROCESSED; 
	} 
	else 
	{ 
		result = op1 / op2; 
		// StrmnNrmn - the s32 casts were originally u32. Not sure if this is needed
		g_qwGPR[REG_v1] = (s64)(s32)(result & 0xffffffff); 
		g_qwGPR[REG_v0] = (s64)(s32)(result>>32); 
		return PATCH_RET_JR_RA; 
	} 
} 


// By Jun Su
DWORD Patch___ull_rshift() 
{ 
TEST_DISABLE_MATH_FUNCS
	u64 op1, op2, result; 
	
	// Fixed by  StrmnNrmn - regs cast to 32 bits so shift didn't work
	op1 = (u64)((u64)(u32)g_qwGPR[REG_a0] << 32 | (u64)(u32)g_qwGPR[REG_a1]); 
	op2 = (u64)((u64)(u32)g_qwGPR[REG_a2] << 32 | (u64)(u32)g_qwGPR[REG_a3]); 
	
	result = op1 >> op2; 
	// StrmnNrmn - the s32 casts were originally u32. Not sure if this is needed
	g_qwGPR[REG_v1] = (s64)(s32)(result & 0xffffffff); 
	g_qwGPR[REG_v0] = (s64)(s32)(result>>32); 
	return PATCH_RET_JR_RA; 
} 

// By Jun Su
DWORD Patch___ll_lshift() 
{ 
TEST_DISABLE_MATH_FUNCS
	s64 op1, result; 
	u64 op2; 

	// Fixed by  StrmnNrmn - regs cast to 32 bits so shift didn't work
	op1 = (s64)((u64)(u32)g_qwGPR[REG_a0] << 32 | (u64)(u32)g_qwGPR[REG_a1]); 
	op2 = (u64)((u64)(u32)g_qwGPR[REG_a2] << 32 | (u64)(u32)g_qwGPR[REG_a3]); 
	
	result = op1 << op2; 
	// StrmnNrmn - the s32 casts were originally u32. Not sure if this is needed
	g_qwGPR[REG_v1] = (s64)(s32)(result & 0xffffffff); 
	g_qwGPR[REG_v0] = (s64)(s32)(result>>32); 
	return PATCH_RET_JR_RA; 
} 


// By Jun Su
DWORD Patch___ll_rshift() 
{ 
TEST_DISABLE_MATH_FUNCS
	s64 op1, result; 
	u64 op2; 
	
	// Fixed by  StrmnNrmn - regs cast to 32 bits so shift didn't work
	op1 = (s64)((u64)(u32)g_qwGPR[REG_a0] << 32 | (u64)(u32)g_qwGPR[REG_a1]); 
	op2 = (u64)((u64)(u32)g_qwGPR[REG_a2] << 32 | (u64)(u32)g_qwGPR[REG_a3]); 
	
	result = op1 >> op2; 
	// StrmnNrmn - the s32 casts were originally u32. Not sure if this is needed
	g_qwGPR[REG_v1] = (s64)(s32)(result & 0xffffffff); 
	g_qwGPR[REG_v0] = (s64)(s32)(result>>32); 
	return PATCH_RET_JR_RA; 
} 

// By Jun Su
DWORD Patch___ll_mod() 
{ 
TEST_DISABLE_MATH_FUNCS
	s64 op1, op2, result; 
	
	// Fixed by  StrmnNrmn - regs cast to 32 bits so shift didn't work
	op1 = (s64)((u64)(u32)g_qwGPR[REG_a0] << 32 | (u64)(u32)g_qwGPR[REG_a1]); 
	op2 = (s64)((u64)(u32)g_qwGPR[REG_a2] << 32 | (u64)(u32)g_qwGPR[REG_a3]); 
	
	if (op2==0) 
	{ 
		return PATCH_RET_NOT_PROCESSED; 
	} 
	else 
	{ 
		result = op1 % op2; 
		// StrmnNrmn - the s32 casts were originally u32. Not sure if this is needed
		g_qwGPR[REG_v1] = (s64)(s32)(result & 0xffffffff); 
		g_qwGPR[REG_v0] = (s64)(s32)(result>>32); 
		return PATCH_RET_JR_RA; 
	} 
} 





DWORD Patch_sqrtf()
{
TEST_DISABLE_MATH_FUNCS
	f32 f = ToFloat(g_qwCPR[1][12]);
	ToFloat(g_qwCPR[1][00]) = fsqrt(f);
	return PATCH_RET_JR_RA;
}



DWORD Patch_sinf()
{
TEST_DISABLE_MATH_FUNCS
	// FP12 is input
	// FP00 is output
	f32 f = ToFloat(g_qwCPR[1][12]);

	f32 r = sinf(f);

	//DBGConsole_Msg(0, "sinf(%f) = %f (ra 0x%08x)", f, r, (DWORD)g_qwGPR[REG_ra]);

	ToFloat(g_qwCPR[1][00]) = r;

/*	g_dwNumCosSin++;
	if ((g_dwNumCosSin % 100000) == 0)
	{
		DBGConsole_Msg(0, "%d sin/cos calls intercepted", g_dwNumCosSin);
	}*/
	return PATCH_RET_JR_RA;

}
DWORD Patch_cosf()
{
TEST_DISABLE_MATH_FUNCS
	// FP12 is input
	// FP00 is output
	f32 f = ToFloat(g_qwCPR[1][12]);

	f32 r = cosf(f);

	//DBGConsole_Msg(0, "cosf(%f) = %f (ra 0x%08x)", f, r, (DWORD)g_qwGPR[REG_ra]);

	ToFloat(g_qwCPR[1][00]) = r;

/*	g_dwNumCosSin++;
	if ((g_dwNumCosSin % 100000) == 0)
	{
		DBGConsole_Msg(0, "%d sin/cos calls intercepted", g_dwNumCosSin);
	}*/
	return PATCH_RET_JR_RA;

}
