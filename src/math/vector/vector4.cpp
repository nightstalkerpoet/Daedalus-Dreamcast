#include "vector4.h"
#include <math.h>

/*
 * Initialisierungskonstruktor
 * x: X-Komponente des Vektors
 * y: Y-Komponente des Vektors
 * z: Z-Komponente des Vektors
 */
tVector4::tVector4( float x, float y, float z, float w )
{
	this->x = x;
	this->y = y;
	this->z = z;
	this->w = w;
}

tVector4::~tVector4()
{
}

/*
 * Überladener Vergleichsoperator, welcher zwei Vektoren auf Gleichheit überprüft
 * other: Vektor, mit dem der linksseitige Operant verglichen wird
 * returns: true, bei Gleichheit der Objekte, ansonsten false
 */
bool tVector4::operator == ( tVector4 other )
{
	if( other.x == x && other.y == y && other.z == z && other.w == w )
		return true;
	else
		return false;
}

/*
 * Verschiebt den Vektor um die angegebene Matrix
 * m: Matrix, durch welche der Vektor verschoben wird
 * returns: Verschobener Vektor
 */
tVector4 tVector4::translateCoord( tMatrix m )
{
	// Vektor verschieben und zurückgeben
	return tVector4( x + m.m[3][0], y + m.m[3][1], z + m.m[3][2], w + m.m[3][3] );
}

/*
 * Rotiert den Vektor um die angegebene Matrix
 * m: Matrix, durch welche der Vektor rotiert wird
 * returns: Rotierter Vektor
 */
tVector4 tVector4::rotateCoord( tMatrix m )
{
	// Speichert den Ergebnisvektor
	tVector4 erg;

	erg.x = m.m[0][0] * x + m.m[1][0] * y + m.m[2][0] * z;
	erg.y = m.m[0][1] * x + m.m[1][1] * y + m.m[2][1] * z;
	erg.z = m.m[0][2] * x + m.m[1][2] * y + m.m[2][2] * z;
	erg.w = m.m[0][3] * x + m.m[1][3] * y + m.m[2][3] * z;

	// Ergebnisvektor zurückgeben
	return erg;
}

/*
 * Transformiert den Vektor durch die angegebene Matrix
 * m: Matrix, durch welche der Vektor transformiert wird
 * returns: Transformierter Vektor
 */
tVector4 tVector4::transformCoord( tMatrix m )
{
	// Vektor verschieben
	tVector4 erg = rotateCoord( m );

	// Vektor rotieren
	erg = erg.translateCoord( m );

	// Ergebnis zurückgeben
	return erg;
}
