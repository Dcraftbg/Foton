/*===============================================================================
 Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
===============================================================================*/

	//----------------------------------------------------------------------
	// variables, structures, definitions
	//----------------------------------------------------------------------
	#ifndef	LIB_MATH
		#include	"./math.h"
	#endif

int8_t lib_math_compare_double( double f1, double f2 ) {
	// lower than
	if( (f1 - LIB_MATH_EPSILON) < f2 ) return -1;

	// greater than
	if( (f1 + LIB_MATH_EPSILON) > f2 ) return 1;

	// equal
	return 0;
}

double cos( double x ) {
	// keep in range
	x = fmod( x, 360.0f );
	if(x > 180.0f) x -= 360.0f;

	// convert to radians
	x = x * LIB_MATH_PI / 180.0f;

	if( x < 0.0f ) x = -x;

	if( 0 <= lib_math_compare_double( x, LIB_MATH_PI_2x ) )
		do {
			x -= LIB_MATH_PI_2x;
		} while( 0 <= lib_math_compare_double( x, LIB_MATH_PI_2x ) );

	if( (0 <= lib_math_compare_double( x, LIB_MATH_PI ) ) && ( -1 == lib_math_compare_double( x, LIB_MATH_PI_2x )) ) {
		x -= LIB_MATH_PI;
		
		return ((-1) * (1.0f - (x * x / 2.0f) * (1.0f - (x * x / 12.0f) * (1.0f - ( x * x / 30.0f) * (1.0f - ( x * x / 56.0f ) * (1.0f - ( x * x / 90.0f) * (1.0f - (x * x / 132.0f) * (1.0f - ( x * x / 182.0f)))))))));
	}

	return 1.0f - (x * x / 2.0f) * ( 1.0f - (x * x / 12.0f) * ( 1.0f - (x * x / 30.0f) * (1.0f - (x * x / 56.0f) * (1.0f - ( x * x / 90.0f) * (1.0f - (x * x / 132.0f) * (1.0f - ( x * x / 182.0f)))))));
}

double sin( double x ) { return cos( x - 90 ); }

double tan( double x ) { return sin( x ) / cos( x ); }

double ctan( double x ) { return 1.0f / tan( x ); }