#ifndef VECTOR4_H
#define VECTOR4_H

#include "../matrix/matrix.h"

// Vorwärtsdefinition für Zirkelbezug
struct tMatrix;

typedef struct tVector4
{
	public:
		// Konstruktor/Destruktor
		tVector4( float x = 0.0f, float y = 0.0f, float z = 0.0f, float w = 1.0f );
		~tVector4();

		// Operatoren
		bool operator == ( tVector4 );

		// Methoden
		tVector4	translateCoord( tMatrix m );
		tVector4	rotateCoord( tMatrix m );
		tVector4	transformCoord( tMatrix m );

		// Attribute
		float x, y, z, w;
}tVector4;

#endif
