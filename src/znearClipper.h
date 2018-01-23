#ifndef ZNEAR_CLIPPER_H
#define ZNEAR_CLIPPER_H

#include "math/vector/vector2.h"
#include "math/vector/vector3.h"

typedef struct tClipVertex
{
    tVector3 vPos;
    tVector2 vTexCoord;
    uint32   uColor;
}tClipVertex;

typedef struct tPtrClipVertex
{
    tVector3 *pPos;
    tVector2 *pTexCoord;
    uint32   *pColor;
}tPtrClipVertex;

class CZNearClipper
{
	public:
		// Konstruktor/Destruktor
		CZNearClipper();
		~CZNearClipper();
		
		// Methoden
		void update( tMatrix *m );

		bool needsClipping( tVector3 *v );
		
		bool allVertsOut();

        int clip( void *ptr[], uint32 stride[] );
        int clipIndexed( void *ptr[], uint32 stride[] );
		
        // Attribute
        tClipVertex m_pVerts[4];
        uint32      m_pIndices[3];
	private:
        // Methoden
        inline void clipVertex( tClipVertex *pVert, tPtrClipVertex *v0, tPtrClipVertex *v1, float *pDist1, float *pDist2 );
        inline void copyVertex( tClipVertex *pDest, tPtrClipVertex *pSource );
        
		// Attribute
        float m_pDists[3];
        float m_pClip[4];
        bool  m_bAllVertsOut;
        bool  m_bIndexed;
};

#endif
