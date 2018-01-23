/*
Copyright (C) 2001 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "main.h"
#include "ConvertImage.h"

// Still to be swapped:
// IA16


void ConvertRGBA16_16(SurfaceHandler *pSurf,
					  WORD *pSrc,
					  DWORD dwPitch,
					  DWORD dwSrcX, DWORD dwSrcY,
					  DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;
	DWORD x, y;
	LONG nFiddle;

	// Copy of the base pointer
	BYTE * pByteSrc = (BYTE *)pSrc;

	if (!pSurf->StartUpdate(&dst))
		return;

	if (bSwapped)
	{

		for (y = 0; y < dwHeight; y++)
		{
			if ((y%2) == 0)
				nFiddle = 0x2;
			else
				nFiddle = 0x2 | 0x4;

			// dwDst points to start of destination row
			WORD * wDst = (WORD *)((BYTE *)dst.lpSurface + y*dst.lPitch);

			// DWordOffset points to the current dword we're looking at
			// (process 2 pixels at a time). May be a problem if we don't start on even pixel
			DWORD dwWordOffset = ((y+dwSrcY) * dwPitch) + (dwSrcX * 2);

			for (x = 0; x < dwWidth; x++)
			{
				WORD w = *(WORD *)&pByteSrc[dwWordOffset ^ nFiddle];

				wDst[x] = Convert555ToR4G4B4A4(w);

				// Increment word offset to point to the next two pixels
				dwWordOffset += 2;
			}
		}
	}
	else
	{
		for (y = 0; y < dwHeight; y++)
		{
			// dwDst points to start of destination row
			WORD * wDst = (WORD *)((BYTE *)dst.lpSurface + y*dst.lPitch);

			// DWordOffset points to the current dword we're looking at
			// (process 2 pixels at a time). May be a problem if we don't start on even pixel
			DWORD dwWordOffset = ((y+dwSrcY) * dwPitch) + (dwSrcX * 2);

			for (x = 0; x < dwWidth; x++)
			{
				WORD w = *(WORD *)&pByteSrc[dwWordOffset ^ 0x2];

				wDst[x] = Convert555ToR4G4B4A4(w);

				// Increment word offset to point to the next two pixels
				dwWordOffset += 2;
			}
		}
	}

	pSurf->EndUpdate(&dst);
}

void ConvertRGBA32_16(SurfaceHandler *pSurf,
					  DWORD *pSrc,
					  DWORD dwPitch,
					  DWORD dwSrcX, DWORD dwSrcY,
					  DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;
	if (!pSurf->StartUpdate(&dst))
		return;

	if (bSwapped)
	{

		for (DWORD y = 0; y < dwHeight; y++)
		{
			if ((y%2) == 0)
			{

				WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);
				BYTE *pS = (BYTE *)pSrc + (y+dwSrcY) * dwPitch + (dwSrcX*4);

				for (DWORD x = 0; x < dwWidth; x++)
				{

					*pD++ = R4G4B4A4_MAKE((pS[3]>>4),		// Red
										  (pS[2]>>4),
										  (pS[1]>>4),
										  (pS[0]>>4));		// Alpha
					pS+=4;
				}
			}
			else
			{

				WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);
				BYTE *pS = (BYTE *)pSrc + (y+dwSrcY) * dwPitch + (dwSrcX*4);
				LONG n;

				n = 0;
				for (DWORD x = 0; x < dwWidth; x++)
				{
					*pD++ = R4G4B4A4_MAKE((pS[(n^0x8) + 3]>>4),		// Red
										  (pS[(n^0x8) + 2]>>4),
										  (pS[(n^0x8) + 1]>>4),
										  (pS[(n^0x8) + 0]>>4));	// Alpha

					n += 4;
				}
			}
		}
	}
	else
	{
		for (DWORD y = 0; y < dwHeight; y++)
		{
			WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);
			BYTE *pS = (BYTE *)pSrc + (y+dwSrcY) * dwPitch + (dwSrcX*4);

			for (DWORD x = 0; x < dwWidth; x++)
			{
				*pD++ = R4G4B4A4_MAKE((pS[3]>>4),		// Red
									  (pS[2]>>4),
									  (pS[1]>>4),
									  (pS[0]>>4));		// Alpha
				pS+=4;
			}
		}

	}
	pSurf->EndUpdate(&dst);

}

// E.g. Dear Mario text
// Copy, Score etc
void ConvertIA4_16(SurfaceHandler *pSurf,
					   BYTE *pSrc,
					   DWORD dwPitch,
					   DWORD dwSrcX, DWORD dwSrcY,
					   DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;
	LONG nFiddle;

	if (!pSurf->StartUpdate(&dst))
		return;

	if (bSwapped)
	{
		for (DWORD y = 0; y < dwHeight; y++)
		{
			WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);

			// For odd lines, swap words too
			if ((y%2) == 0)
				nFiddle = 0x3;
			else
				nFiddle = 0x7;


			// This may not work if X is not even?
			DWORD dwByteOffset = (y+dwSrcY) * dwPitch + (dwSrcX/2);

			// Do two pixels at a time
			for (DWORD x = 0; x < dwWidth; x+=2)
			{
				BYTE b = pSrc[dwByteOffset ^ nFiddle];

				// Even
				*pD++ = R4G4B4A4_MAKE(TwoToFour[(b & 0xc0) >> 6],
									  TwoToFour[(b & 0xc0) >> 6],
									  TwoToFour[(b & 0xc0) >> 6],
									  TwoToFour[(b & 0x30) >> 4]);

				// Odd
				*pD++ = R4G4B4A4_MAKE(TwoToFour[(b & 0x0c) >> 2],
									  TwoToFour[(b & 0x0c) >> 2],
									  TwoToFour[(b & 0x0c) >> 2],
									  TwoToFour[(b & 0x03)     ]);

				dwByteOffset++;

			}

		}
	}
	else
	{
		for (DWORD y = 0; y < dwHeight; y++)
		{
			WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);

			// This may not work if X is not even?
			DWORD dwByteOffset = (y+dwSrcY) * dwPitch + (dwSrcX/2);

			// Do two pixels at a time
			for (DWORD x = 0; x < dwWidth; x+=2)
			{
				BYTE b = pSrc[dwByteOffset ^ 0x3];

				// Even
				*pD++ = R4G4B4A4_MAKE(TwoToFour[(b & 0xc0) >> 6],
									  TwoToFour[(b & 0xc0) >> 6],
									  TwoToFour[(b & 0xc0) >> 6],
									  TwoToFour[(b & 0x30) >> 4]);
				// Odd
				*pD++ = R4G4B4A4_MAKE(TwoToFour[(b & 0x0c) >> 2],
									  TwoToFour[(b & 0x0c) >> 2],
									  TwoToFour[(b & 0x0c) >> 2],
									  TwoToFour[(b & 0x03)     ]);


				dwByteOffset++;

			}
		}
	}

	pSurf->EndUpdate(&dst);

}

// E.g Mario's head textures
void ConvertIA8_16(SurfaceHandler *pSurf,
					   BYTE *pSrc,
					   DWORD dwPitch,
					   DWORD dwSrcX, DWORD dwSrcY,
					   DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;
	LONG nFiddle;

	if (!pSurf->StartUpdate(&dst))
		return;

	if (bSwapped)
	{
		for (DWORD y = 0; y < dwHeight; y++)
		{
			// For odd lines, swap words too
			if ((y%2) == 0)
				nFiddle = 0x3;
			else
				nFiddle = 0x7;


			WORD *pD = (WORD *)((BYTE*)dst.lpSurface + y * dst.lPitch);
			// Points to current byte
			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + dwSrcX;

			for (DWORD x = 0; x < dwWidth; x++)
			{
				BYTE b = pSrc[dwByteOffset ^ nFiddle];

				*pD++ = R4G4B4A4_MAKE( ((b&0xf0)>>4),((b&0xf0)>>4),((b&0xf0)>>4),(b&0x0f));

				dwByteOffset++;
			}

		}
	}
	else
	{

		for (DWORD y = 0; y < dwHeight; y++)
		{
			WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);


			// Points to current byte
			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + dwSrcX;

			for (DWORD x = 0; x < dwWidth; x++)
			{
				BYTE b = pSrc[dwByteOffset ^ 0x3];

				*pD++ = R4G4B4A4_MAKE(((b&0xf0)>>4),((b&0xf0)>>4),((b&0xf0)>>4),(b&0x0f));

				dwByteOffset++;
			}
		}
	}

	pSurf->EndUpdate(&dst);

}

// E.g. camera's clouds, shadows
void ConvertIA16_16(SurfaceHandler *pSurf,
						WORD *pSrc,
						DWORD dwPitch,
					    DWORD dwSrcX, DWORD dwSrcY,
						DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;

	BYTE * pByteSrc = (BYTE *)pSrc;

	if (!pSurf->StartUpdate(&dst))
		return;

	for (DWORD y = 0; y < dwHeight; y++)
	{
		WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);

		// Points to current word
		DWORD dwWordOffset = ((y+dwSrcY) * dwPitch) + (dwSrcX * 2);

		for (DWORD x = 0; x < dwWidth; x++)
		{
			WORD w = *(WORD *)&pByteSrc[dwWordOffset^0x2];

			BYTE i = (BYTE)(w >> 12);
			BYTE a = (BYTE)(w & 0xFF);

			*pD++ = R4G4B4A4_MAKE(i, i, i, (a>>4));

			dwWordOffset += 2;
		}
	}
	pSurf->EndUpdate(&dst);
}



// Used by MarioKart
void ConvertI4_16(SurfaceHandler *pSurf,
					  BYTE *pSrc,
					  DWORD dwPitch,
					  DWORD dwSrcX, DWORD dwSrcY,
					  DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;
	LONG nFiddle;

	if (!pSurf->StartUpdate(&dst))
		return;

	if (bSwapped)
	{

		for (DWORD y = 0; y < dwHeight; y++)
		{
			WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);

			// Might not work with non-even starting X
			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + (dwSrcX / 2);

			// For odd lines, swap words too
			if ((y%2) == 0)
				nFiddle = 0x3;
			else
				nFiddle = 0x7;

			for (DWORD x = 0; x < dwWidth; x+=2)
			{
				BYTE b = pSrc[dwByteOffset ^ nFiddle]>>4;

				// Even
				*pD++ = R4G4B4A4_MAKE(b>>4,
									  b>>4,
									  b>>4,
									  b>>4);
				// Odd
				*pD++ = R4G4B4A4_MAKE(b & 0x0f,
									  b & 0x0f,
									  b & 0x0f,
									  b & 0x0f);

				dwByteOffset++;
			}

		}

	}
	else
	{

		for (DWORD y = 0; y < dwHeight; y++)
		{
			WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);

			// Might not work with non-even starting X
			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + (dwSrcX / 2);

			for (DWORD x = 0; x < dwWidth; x+=2)
			{
				BYTE b = pSrc[dwByteOffset ^ 0x3];

				// Even
				*pD++ = R4G4B4A4_MAKE(b>>4,
									  b>>4,
									  b>>4,
									  b>>4);

				// Odd
				*pD++ = R4G4B4A4_MAKE(b & 0x0f,
									  b & 0x0f,
									  b & 0x0f,
									  b & 0x0f);

				dwByteOffset++;
			}
		}
	}
	pSurf->EndUpdate(&dst);
}

// Used by MarioKart
void ConvertI8_16(SurfaceHandler *pSurf,
					  BYTE *pSrc,
					  DWORD dwPitch,
					  DWORD dwSrcX, DWORD dwSrcY,
					  DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;
	LONG nFiddle;

	if (!pSurf->StartUpdate(&dst))
		return;

	if (bSwapped)
	{
		for (DWORD y = 0; y < dwHeight; y++)
		{
			if ((y%2) == 0)
				nFiddle = 0x3;
			else
				nFiddle = 0x7;

			WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);

			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + dwSrcX;

			for (DWORD x = 0; x < dwWidth; x++)
			{
				BYTE b = pSrc[dwByteOffset ^ nFiddle];

				*pD++ = R4G4B4A4_MAKE(b>>4,
									  b>>4,
									  b>>4,
									  b>>4);

				dwByteOffset++;
			}
		}
	}
	else
	{
		for (DWORD y = 0; y < dwHeight; y++)
		{
			WORD *pD = (WORD*)((BYTE *)dst.lpSurface + y * dst.lPitch);

			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + dwSrcX;

			for (DWORD x = 0; x < dwWidth; x++)
			{
				BYTE b = pSrc[dwByteOffset ^ 0x3];

				*pD++ = R4G4B4A4_MAKE(b>>4,
									  b>>4,
									  b>>4,
									  b>>4);

				dwByteOffset++;
			}
		}

	}
	pSurf->EndUpdate(&dst);

}


// Used by Starfox intro
void ConvertCI4_RGBA16_16(SurfaceHandler *pSurf,
					   BYTE *pSrc,
					   DWORD dwPitch,
					   WORD * pPal,
					   DWORD dwSrcX, DWORD dwSrcY,
					   DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;
	LONG nFiddle;

	if (!pSurf->StartUpdate(&dst))
		return;

	if (bSwapped)
	{

		for (DWORD y = 0; y <  dwHeight; y++)
		{
			if ((y%2) == 0)
				nFiddle = 0x3;
			else
				nFiddle = 0x7;


			WORD * pD = (WORD *)((BYTE *)dst.lpSurface + y * dst.lPitch);

			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + (dwSrcX / 2);

			for (DWORD x = 0; x < dwWidth; x+=2)
			{
				BYTE b = pSrc[dwByteOffset ^ nFiddle];

				BYTE bhi = (b&0xf0)>>4;
				BYTE blo = (b&0x0f);

				pD[0] = Convert555ToR4G4B4A4(pPal[bhi^0x1]);	// Remember palette is in different endian order!
				pD[1] = Convert555ToR4G4B4A4(pPal[blo^0x1]);	// Remember palette is in different endian order!
				pD+=2;

				dwByteOffset++;
			}
		}

	}
	else
	{

		for (DWORD y = 0; y <  dwHeight; y++)
		{
			WORD * pD = (WORD *)((BYTE *)dst.lpSurface + y * dst.lPitch);

			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + (dwSrcX / 2);

			for (DWORD x = 0; x < dwWidth; x+=2)
			{
				BYTE b = pSrc[dwByteOffset ^ 0x3];

				BYTE bhi = (b&0xf0)>>4;
				BYTE blo = (b&0x0f);

				pD[0] = Convert555ToR4G4B4A4(pPal[bhi^0x1]);	// Remember palette is in different endian order!
				pD[1] = Convert555ToR4G4B4A4(pPal[blo^0x1]);	// Remember palette is in different endian order!
				pD+=2;

				dwByteOffset++;
			}
		}

	}
	pSurf->EndUpdate(&dst);
}

// Used by Starfox intro
void ConvertCI4_IA16_16(SurfaceHandler *pSurf,
					   BYTE *pSrc,
					   DWORD dwPitch,
					   WORD * pPal,
					   DWORD dwSrcX, DWORD dwSrcY,
					   DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;
	LONG nFiddle;

	if (!pSurf->StartUpdate(&dst))
		return;

	if (bSwapped)
	{

		for (DWORD y = 0; y <  dwHeight; y++)
		{
			if ((y%2) == 0)
				nFiddle = 0x3;
			else
				nFiddle = 0x7;


			WORD * pD = (WORD *)((BYTE *)dst.lpSurface + y * dst.lPitch);

			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + (dwSrcX / 2);

			for (DWORD x = 0; x < dwWidth; x+=2)
			{
				BYTE b = pSrc[dwByteOffset ^ nFiddle];

				BYTE bhi = (b&0xf0)>>4;
				BYTE blo = (b&0x0f);

				pD[0] = ConvertIA16ToR4G4B4A4(pPal[bhi^0x1]);	// Remember palette is in different endian order!
				pD[1] = ConvertIA16ToR4G4B4A4(pPal[blo^0x1]);	// Remember palette is in different endian order!
				pD += 2;
				dwByteOffset++;
			}
		}

	}
	else
	{

		for (DWORD y = 0; y <  dwHeight; y++)
		{
			WORD * pD = (WORD *)((BYTE *)dst.lpSurface + y * dst.lPitch);

			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + (dwSrcX / 2);

			for (DWORD x = 0; x < dwWidth; x+=2)
			{
				BYTE b = pSrc[dwByteOffset ^ 0x3];

				BYTE bhi = (b&0xf0)>>4;
				BYTE blo = (b&0x0f);

				pD[0] = ConvertIA16ToR4G4B4A4(pPal[bhi^0x1]);	// Remember palette is in different endian order!
				pD[1] = ConvertIA16ToR4G4B4A4(pPal[blo^0x1]);	// Remember palette is in different endian order!
				pD+=2;

				dwByteOffset++;
			}
		}

	}
	pSurf->EndUpdate(&dst);
}




// Used by MarioKart for Cars etc
void ConvertCI8_RGBA16_16(SurfaceHandler *pSurf,
					   BYTE *pSrc,
					   DWORD dwPitch,
					   WORD * pPal,
					   DWORD dwSrcX, DWORD dwSrcY,
					   DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;
	LONG nFiddle;

	if (!pSurf->StartUpdate(&dst))
		return;

	if (bSwapped)
	{


		for (DWORD y = 0; y < dwHeight; y++)
		{
			if ((y%2) == 0)
				nFiddle = 0x3;
			else
				nFiddle = 0x7;

			WORD *pD = (WORD *)((BYTE *)dst.lpSurface + y * dst.lPitch);

			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + dwSrcX;

			for (DWORD x = 0; x < dwWidth; x++)
			{
				BYTE b = pSrc[dwByteOffset ^ nFiddle];

				*pD++ = Convert555ToR4G4B4A4(pPal[b^0x1]);	// Remember palette is in different endian order!

				dwByteOffset++;
			}
		}


	}
	else
	{

		for (DWORD y = 0; y < dwHeight; y++)
		{
			WORD *pD = (WORD *)((BYTE *)dst.lpSurface + y * dst.lPitch);

			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + dwSrcX;

			for (DWORD x = 0; x < dwWidth; x++)
			{
				BYTE b = pSrc[dwByteOffset ^ 0x3];

				*pD++ = Convert555ToR4G4B4A4(pPal[b^0x1]);	// Remember palette is in different endian order!

				dwByteOffset++;
			}
		}
	}
	pSurf->EndUpdate(&dst);

}


// Used by MarioKart for Cars etc
void ConvertCI8_IA16_16(SurfaceHandler *pSurf,
					   BYTE *pSrc,
					   DWORD dwPitch,
					   WORD * pPal,
					   DWORD dwSrcX, DWORD dwSrcY,
					   DWORD dwWidth, DWORD dwHeight,
					  BOOL bSwapped)
{
	DrawInfo dst;
	LONG nFiddle;

	if (!pSurf->StartUpdate(&dst))
		return;

	if (bSwapped)
	{


		for (DWORD y = 0; y < dwHeight; y++)
		{
			if ((y%2) == 0)
				nFiddle = 0x3;
			else
				nFiddle = 0x7;

			WORD *pD = (WORD *)((BYTE *)dst.lpSurface + y * dst.lPitch);

			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + dwSrcX;

			for (DWORD x = 0; x < dwWidth; x++)
			{
				BYTE b = pSrc[dwByteOffset ^ nFiddle];

				*pD++ = ConvertIA16ToR4G4B4A4(pPal[b^0x1]);	// Remember palette is in different endian order!

				dwByteOffset++;
			}
		}


	}
	else
	{

		for (DWORD y = 0; y < dwHeight; y++)
		{
			WORD *pD = (WORD *)((BYTE *)dst.lpSurface + y * dst.lPitch);

			DWORD dwByteOffset = ((y+dwSrcY) * dwPitch) + dwSrcX;

			for (DWORD x = 0; x < dwWidth; x++)
			{
				BYTE b = pSrc[dwByteOffset ^ 0x3];

				*pD++ = ConvertIA16ToR4G4B4A4(pPal[b^0x1]);	// Remember palette is in different endian order!

				dwByteOffset++;
			}
		}
	}
	pSurf->EndUpdate(&dst);

}

