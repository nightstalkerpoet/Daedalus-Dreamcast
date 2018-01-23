#ifndef PLANE_H
#define PLANE_H

#include "../vector/vector3.h"

typedef struct tPlane
{
	public:
		// Konstruktor/Destruktor
		tPlane( tVector3 vOrigin = tVector3(), tVector3 vNormal = tVector3() );
		~tPlane();
		
		// Methoden
		bool  intersectLine( tVector3 vStart, tVector3 vDir, float *t );

        /*
         * Berechnet den Abstand des Punktes p von der Ebene
         * p: Punkt, dessen Abstand zur Ebene berechnet wird
         * returns: Abstand des Punktes p zur Ebene
         */
        inline float distanceFrom( tVector3 p )
        {
            return a * p.x + b * p.y + c * p.z + d;
        }
		
		// Attribute
		float a, b, c, d;
}tPlane;

#endif
