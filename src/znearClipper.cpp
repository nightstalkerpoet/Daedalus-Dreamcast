#include "znearclipper.h"

#include <kos.h>

static matrix_t m __attribute__((aligned(32)));

/*
 * Standardkonstruktor
 */
CZNearClipper::CZNearClipper()
{
}

/*
 * Destruktor
 */
CZNearClipper::~CZNearClipper()
{
}

/*
 * Aktualisiert die Ebenengleichung der ZNearPlane
 */
void CZNearClipper::update( tMatrix *m )
{
	 m_pClip[0] = m->m[0][3] + m->m[0][2];
	 m_pClip[1] = m->m[1][3] + m->m[1][2];
	 m_pClip[2] = m->m[2][3] + m->m[2][2];
	 m_pClip[3] = m->m[3][3] + m->m[3][2];
}

/*
 * Clippt einen Vertex anhand der ¸bergebenen Distanzen
 * pPosOUT: Zeiger auf den Vektor, welcher die Positionsdaten des neuen Vertex aufnimmt
 * pTexCoordOUT: Zeiger auf den Vektor, welcher die Textur-Koordinate des neuen Vertex aufnimmt
 * pPos1: Zeiger auf die Positionsdaten des ersten Vertex
 * pTexCoord1: Zeiger auf die Textur-Koordinate des ersten Vertex
 * pDist1: Zeiger auf die Distanz des ersten Vertex
 * pPos2: Zeiger auf die Positionsdaten des zweiten Vertex
 * pTexCoord2: Zeiger auf die Textur-Koordinate des zweiten Vertex
 * pDist2: Zeiger auf die Distanz des zweiten Vertex
 */
inline void CZNearClipper::clipVertex( tClipVertex *pVert, tPtrClipVertex *v0, tPtrClipVertex *v1, float *pDist1, float *pDist2 )
{    
    // Faktor zum Interpolieren berechnen 
    float t = (*pDist1) / ( (*pDist1) - (*pDist2) );
    
    // Position linear interpolieren
    pVert->vPos.x = v0->pPos->x + t * ( v1->pPos->x - v0->pPos->x );
    pVert->vPos.y = v0->pPos->y + t * ( v1->pPos->y - v0->pPos->y );
    pVert->vPos.z = v0->pPos->z + t * ( v1->pPos->z - v0->pPos->z );

    // Textur-Koordinate linear interpolieren
    pVert->vTexCoord.x = v0->pTexCoord->x + t * ( v1->pTexCoord->x - v0->pTexCoord->x );
    pVert->vTexCoord.y = v0->pTexCoord->y + t * ( v1->pTexCoord->y - v0->pTexCoord->y );     
    
    // TODO: Farbe linear interpolieren
}

/*
 * Kopiert die Werte des Source-Vertex in den Dest-Vertex
 * pSource: Quelle, die kopiert wird
 * pDest: Ziel in welches kopiert wird
 */
inline void CZNearClipper::copyVertex( tClipVertex *pDest, tPtrClipVertex *pSource )
{    
     pDest->vPos      = *pSource->pPos;
     pDest->vTexCoord = *pSource->pTexCoord;
     pDest->uColor    = *pSource->pColor;
}

/*
 * ‹berpr¸ft, ob ein Dreieck geclippt werden muss
 * remarks: Berechnet auﬂerdem die Abst‰nde der Vertices von der ZNearPlane
 */
bool CZNearClipper::needsClipping( tVector3 *v )
{
    // Speichern, dass das Dreieck nicht indiziert ist
    m_bIndexed = false;
    
    // Speichert, ob das Dreieck geclippt werden muss 
    bool bNeedsClipping = false;
    
    // Speichert, ob alle Vertices auﬂerhalb der ZNearPlane liegen
    m_bAllVertsOut = true;
    
    for( register int i = 0; i < 3; i++ )
    {
        // Aktuellen Vertex einlesen
        tVector3 *pVert = &v[i];
        
        // Vertex ¸berpr¸fen
        m_pDists[i] = m_pClip[0] * pVert->x + m_pClip[1] * pVert->y + m_pClip[2] * pVert->z + m_pClip[3];
        if( m_pDists[i] <= 0.0f )
            bNeedsClipping = true;
        else
            m_bAllVertsOut = false;
    }
    
    return bNeedsClipping;  
}

/*
 * Gibt zur¸ck, ob alle Vertices des Dreiecks auﬂerhalb der ZNearPlane liegen
 * remarks: Darf erst nach dem Aufruf der Funktion needsClipping, f¸r das entsprechende Dreieck, aufgerufen werden
 */
bool CZNearClipper::allVertsOut()
{
     return m_bAllVertsOut;
}

/*
 * Clippt ein Dreieck
 * remarks: Vor dem Aufruf dieser Funktion muss f¸r das zu clippende Dreieck 
 * die Funktion needsClipping aufgerufen worden sein.
 */
int CZNearClipper::clip( void *ptr[], uint32 stride[] )
{       
    // Zeiger auf den aktuellen Vertex speichern
    /*tClipVertex *pClipVert = m_pVerts;
    tPtrClipVertex v0;
    tPtrClipVertex v1;
    
    for( int i0 = 0; i0 < 3; i0++ )
    {
        // Index f¸r den jeweils folgenden Vertex berechnen
        int i1 = (i0 + 1) % 3;
        
        // Index im Vertex-Array berechnen
        int vi0 = i0;
        int vi1 = i1;
        if( m_bIndexed )
        {
            vi0 = m_pIndices[vi0];
            vi1 = m_pIndices[vi1];
        }
        
        // Die beiden Vertices zum Vergleichen einlesen
        v0.pPos      = (tVector3 *)(((char *)ptr[ATLANTIS_VERTEX_I])   + vi0 * stride[ATLANTIS_VERTEX_I] );
        v0.pTexCoord = (tVector2 *)(((char *)ptr[ATLANTIS_TEXCOORD_I]) + vi0 * stride[ATLANTIS_TEXCOORD_I] );
        v0.pColor   =  (uint32 *)  (((char *)ptr[ATLANTIS_COLOR_I])    + vi0 * stride[ATLANTIS_COLOR_I] );
        v1.pPos      = (tVector3 *)(((char *)ptr[ATLANTIS_VERTEX_I])   + vi1 * stride[ATLANTIS_VERTEX_I] );
        v1.pTexCoord = (tVector2 *)(((char *)ptr[ATLANTIS_TEXCOORD_I]) + vi1 * stride[ATLANTIS_TEXCOORD_I] );
        v1.pColor   =  (uint32 *)  (((char *)ptr[ATLANTIS_COLOR_I])    + vi1 * stride[ATLANTIS_COLOR_I] );
         
         // Alle F‰lle des Sutherland-Hodgeman Algorithmus durchgehen
         if( m_pDists[i0] <= 0.0f )
         {
            if( m_pDists[i1] > 0.0f )
            {
                // Dieser Vertex ist auﬂerhalb der ZNearPlane, der n‰chste innerhalb
                clipVertex( pClipVert, &v0, &v1, m_pDists + i0, m_pDists + i1 );
                pClipVert++;
                copyVertex( pClipVert, &v1 );
                pClipVert++;
            }
         }
         else if( m_pDists[i1] <= 0.0f )
         {
            // Dieser Vertex ist innerhalb der ZNearPlane, der n‰chste auﬂerhalb
            clipVertex( pClipVert, &v0, &v1, m_pDists + i0, m_pDists + i1 );
            pClipVert++;
         }
         else
         {
            // Beide Vertices sind innerhalb der ZNearPlane
            copyVertex( pClipVert, &v1 );
            pClipVert++;
         }
    }
    
    // Anzahl neu erstellter Vertices zur¸ckgeben
    return pClipVert - m_pVerts;   */
    return 0;
}
