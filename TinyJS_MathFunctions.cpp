/*
 * TinyJS
 *
 * A single-file Javascript-alike engine
 *
 * -  Math and Trigonometry functions
 *
 * Authored By O.Z.L.B. <ozlbinfo@gmail.com>
 *
 * Copyright (C) 2011 O.Z.L.B.
 *

 * 42TinyJS
 *
 * A fork of TinyJS with the goal to makes a more JavaScript/ECMA compliant engine
 *
 * Authored / Changed By Armin Diedering <armin@diedering.de>
 *
 * Copyright (C) 2010-2012 ardisoft
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cmath>
#include <cstdlib>
#include <sstream>
#include <ctime>
#include "TinyJS.h"

using namespace std;

#define k_E                 exp(1.0)
#define k_PI                3.1415926535897932384626433832795
#define k_LN2               log((double)2)
#define k_LN10              log((double)10)
#define k_LOG2E             (log(k_E)/log((double)2))
#define k_LOG10E            log10(k_E)
#define k_SQRT1_2           sqrt((double)0.5)
#define k_SQRT2             sqrt((double)2)

#define F_ABS(a)            ((a)>=0 ? (a) : (-(a)))
#define F_MIN(a,b)          ((a)>(b) ? (b) : (a))
#define F_MAX(a,b)          ((a)>(b) ? (a) : (b))
#define F_SGN(a)            ((a)>0 ? 1 : ((a)<0 ? -1 : 0 ))
#define F_RNG(a,min,max)    ((a)<(min) ? min : ((a)>(max) ? max : a ))
 
#ifdef _MSC_VER
namespace
{
	double asinh( const double &value ) {
		double returned;

		if(value>0)
			returned = log(value + sqrt(value * value + 1));
		else
			returned = -log(-value + sqrt(value * value + 1));

		return(returned);
	}

	double acosh( const double &value ) {
		double returned;

		if(value>0)
			returned = log(value + sqrt(value * value - 1));
		 else
			returned = -log(-value + sqrt(value * value - 1));

		return(returned);
	}

	 double atanh( double value ) {
		bool neg = value<0;
		if(neg) value=-value;
		double value_x2 = 2.0*value;
		if(value>=0.5)
			value = log(1.0+value_x2/(1.0-value))/2.0;
		 else
			value = log(1.0+value_x2+value_x2*value/(1.0-value))/2.0;
		return(neg ? -value : value);
	}
}
#endif

#define PARAMETER_TO_NUMBER(v,n) CNumber v = c->getArgument(n)->toNumber()
#define RETURN_NAN_IS_NAN(v) do{ if(v.isNaN()) { c->setReturnVar(c->newScriptVar(v)); return; } }while(0)
#define RETURN_NAN_IS_NAN_OR_INFINITY(v) do{ if(v.isNaN() || v.isInfinity()) { c->setReturnVar(c->newScriptVar(v)); return; } }while(0)
#define RETURN_INFINITY_IS_INFINITY(v) do{ if(v.isInfinity()) { c->setReturnVar(c->newScriptVar(v)); return; } }while(0)
#define RETURN_ZERO_IS_ZERO(v) do{ if(v.isZero()) { c->setReturnVar(c->newScriptVar(v)); return; } }while(0)
#define RETURN(a)	do{ c->setReturnVar(c->newScriptVar(a)); return; }while(0)
#define RETURNconst(a)	c->setReturnVar(c->constScriptVar(a))

//Math.abs(x) - returns absolute of given value
static void scMathAbs(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); 
	RETURN(a.sign()<0?-a:a);
}

//Math.round(a) - returns nearest round of given value
static void scMathRound(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a");
	RETURN(a.round());
}

//Math.ceil(a) - returns nearest round of given value
static void scMathCeil(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_INFINITY_IS_INFINITY(a);
	RETURN(a.ceil());
}

//Math.floor(a) - returns nearest round of given value
static void scMathFloor(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); 
	RETURN(a.floor());
}

//Math.min(a,b) - returns minimum of two given values 
static void scMathMin(const CFunctionsScopePtr &c, void *userdata) {
	int length = c->getArgumentsLength();
	CNumber ret(InfinityPositive);
	for(int i=0; i<length; i++)
	{
		PARAMETER_TO_NUMBER(a,i); RETURN_NAN_IS_NAN(a);
		if(ret>a) ret=a;
	}
	RETURN(ret);
}

//Math.max(a,b) - returns maximum of two given values  
static void scMathMax(const CFunctionsScopePtr &c, void *userdata) {
	int length = c->getArgumentsLength();
	CNumber ret(InfinityNegative);
	for(int i=0; i<length; i++)
	{
		PARAMETER_TO_NUMBER(a,i); RETURN_NAN_IS_NAN(a);
		if(ret<a) ret=a;
	}
	RETURN(ret);
}

//Math.range(x,a,b) - returns value limited between two given values  
static void scMathRange(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(x,"x"); RETURN_NAN_IS_NAN(x); 
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	PARAMETER_TO_NUMBER(b,"b"); RETURN_NAN_IS_NAN(b);

	if(a>b) RETURNconst(NaN);
	if(x<a) RETURN(a);
	if(x>b) RETURN(b);
	RETURN(x);	
}

//Math.sign(a) - returns sign of given value (-1==negative,0=zero,1=positive)
static void scMathSign(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	RETURN(a.isZero() ? 0 : a.sign());
}
static void scMathRandom(const CFunctionsScopePtr &c, void *) {
	static int inited=0;
	if(!inited) {
		inited = 1;
		srand((unsigned int)time(NULL));
	}
	RETURN(double(rand())/RAND_MAX);
}

//Math.toDegrees(a) - returns degree value of a given angle in radians
static void scMathToDegrees(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_INFINITY_IS_INFINITY(a); 
	RETURN( (180.0/k_PI)*a );
}

//Math.toRadians(a) - returns radians value of a given angle in degrees
static void scMathToRadians(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_INFINITY_IS_INFINITY(a); 
	RETURN( (k_PI/180.0)*a );
}

//Math.sin(a) - returns trig. sine of given angle in radians
static void scMathSin(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); RETURN_ZERO_IS_ZERO(a);
	RETURN( sin(a.toDouble()) );
}

//Math.asin(a) - returns trig. arcsine of given angle in radians
static void scMathASin(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_ZERO_IS_ZERO(a);
	if(abs(a)>1) RETURNconst(NaN);
	RETURN( asin(a.toDouble()) );
}

//Math.cos(a) - returns trig. cosine of given angle in radians
static void scMathCos(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	if(a.isZero()) RETURN(1);
	RETURN( cos(a.toDouble()) );
}

//Math.acos(a) - returns trig. arccosine of given angle in radians
static void scMathACos(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	if(abs(a)>1) RETURNconst(NaN);
	else if(a==1) RETURN(0);
	RETURN( acos(a.toDouble()) );
}

//Math.tan(a) - returns trig. tangent of given angle in radians
static void scMathTan(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); RETURN_ZERO_IS_ZERO(a);
	RETURN( tan(a.toDouble()) );
}

//Math.atan(a) - returns trig. arctangent of given angle in radians
static void scMathATan(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_ZERO_IS_ZERO(a);
	int infinity=a.isInfinity();
	if(infinity) RETURN(k_PI/(infinity*2));
	RETURN( atan(a.toDouble()) );
}

//Math.atan2(a,b) - returns trig. arctangent of given angle in radians
static void scMathATan2(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a);
	PARAMETER_TO_NUMBER(b,"b"); RETURN_NAN_IS_NAN(b);
	int sign_a = a.sign();
	int sign_b = b.sign();
	if(a.isZero())
		RETURN(sign_a>0 ? (sign_b>0 ? 0.0 : k_PI) : (sign_b>0 ? -0.0 : -k_PI));
	else if(b.isZero())
		RETURN((sign_a>0 ? k_PI : -k_PI)/2.0);
	int infinity_a=a.isInfinity();
	int infinity_b=b.isInfinity();
	if(infinity_a) {
		if(infinity_b>0) RETURN(k_PI/(infinity_a*4)); 
		else if(infinity_b<0) RETURN(3.0*k_PI/(infinity_a*4));
		else RETURN(k_PI/(infinity_a*2));
	} else if(infinity_b>0)
		RETURN(sign_a>0 ? 0.0 : -0.0);
	else if(infinity_b<0)
		RETURN(sign_a>0 ? k_PI : -k_PI);
	RETURN( atan2(a.toDouble(), b.toDouble()) );
}


//Math.sinh(a) - returns trig. hyperbolic sine of given angle in radians
static void scMathSinh(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	RETURN_ZERO_IS_ZERO(a);
	if(abs(a)>1) RETURNconst(NaN);
	RETURN( sinh(a.toDouble()) );
}

//Math.asinh(a) - returns trig. hyperbolic arcsine of given angle in radians
static void scMathASinh(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	RETURN_INFINITY_IS_INFINITY(a);
	RETURN_ZERO_IS_ZERO(a);
	RETURN( asinh(a.toDouble()) );
}

//Math.cosh(a) - returns trig. hyperbolic cosine of given angle in radians
static void scMathCosh(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	if(a.isInfinity()) RETURNconst(InfinityPositive);
	RETURN( cosh(a.toDouble()) );
}

//Math.acosh(a) - returns trig. hyperbolic arccosine of given angle in radians
static void scMathACosh(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	RETURN_INFINITY_IS_INFINITY(a);
	if(abs(a)<1) RETURNconst(NaN);
	RETURN( acosh(a.toDouble()) );
}

//Math.tanh(a) - returns trig. hyperbolic tangent of given angle in radians
static void scMathTanh(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	RETURN_ZERO_IS_ZERO(a);
	if(a.isInfinity()) RETURN(a.sign());
	RETURN( tanh(a.toDouble()) );
}

//Math.atanh(a) - returns trig. hyperbolic arctangent of given angle in radians
static void scMathATanh(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	RETURN_ZERO_IS_ZERO(a);
	CNumber abs_a = abs(a);
	if(abs_a > 1) RETURNconst(NaN);
	if(abs_a == 1) RETURNconst(Infinity(a.sign()));
	RETURN( atanh(a.toDouble()) );
}

//Math.log(a) - returns natural logaritm (base E) of given value
static void scMathLog(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	if(a.isZero()) RETURNconst(InfinityNegative);
	if(a.sign()<0) RETURNconst(NaN);
	if(a.isInfinity()) RETURNconst(InfinityPositive);
	RETURN( log( a.toDouble()) );
}

//Math.log10(a) - returns logaritm(base 10) of given value
static void scMathLog10(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	if(a.isZero()) RETURNconst(InfinityNegative);
	if(a.sign()<0) RETURNconst(NaN);
	if(a.isInfinity()) RETURNconst(InfinityPositive);
	RETURN( log10( a.toDouble()) );
}

//Math.exp(a) - returns e raised to the power of a given number
static void scMathExp(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a);
	if(a.isZero()) RETURN(1);
	int a_i = a.isInfinity();
	if(a_i>0) RETURNconst(InfinityPositive);
	else if(a_i<0) RETURN(0);
	RETURN( exp(a.toDouble()) );
}

//Math.pow(a,b) - returns the result of a number raised to a power (a)^(b)
static void scMathPow(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a");
	PARAMETER_TO_NUMBER(b,"b"); RETURN_NAN_IS_NAN(b); 
	if(b.isZero()) RETURN(1);
	RETURN_NAN_IS_NAN(a);
	if(b==1) RETURN(a);

	int sign;
	CNumber a_abs = abs(a);
	if((sign = b.isInfinity())) {
		if( a_abs==1 ) RETURNconst(NaN);
		else if( (a_abs>1) ^ (sign<0) )	RETURN(b); else RETURN(0);
	} else if((sign = a.isInfinity())) {
		if(sign>0) {
			if(b.sign()>0) RETURN(a); else RETURN(0);
		} else {
			bool b_is_odd_int = ((b+1)/2).isInteger();
			if(b.sign()>0) RETURNconst(b_is_odd_int?InfinityNegative:InfinityPositive);
			else RETURN(b_is_odd_int?CNumber(NegativeZero):CNumber(0));
		}
	} else if(a.isZero()) {
		if(a.isNegativeZero()) {
			bool b_is_odd_int = ((b+1)/2).isInteger();
			if(b.sign()>0) RETURN(b_is_odd_int?CNumber(NegativeZero):CNumber(0));
			else RETURNconst(b_is_odd_int?InfinityNegative:InfinityPositive);
		} else 
			if(b.sign()>0) RETURN(a); else RETURNconst(InfinityPositive);
	}
	if(a.sign()<0 && !b.isInteger()) RETURNconst(NaN);

	RETURN( pow(a.toDouble(), b.toDouble()) );
}

//Math.sqr(a) - returns square of given value
static void scMathSqr(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a");
	RETURN( a*a );
}

//Math.sqrt(a) - returns square root of given value
static void scMathSqrt(const CFunctionsScopePtr &c, void *userdata) {
	PARAMETER_TO_NUMBER(a,"a"); RETURN_NAN_IS_NAN(a); 
	RETURN_ZERO_IS_ZERO(a);
	if(a.sign()<0) RETURNconst(NaN);
	RETURN_INFINITY_IS_INFINITY(a); 
	RETURN( sqrt(a.toDouble()) );
}

// ----------------------------------------------- Register Functions
void registerMathFunctions(CTinyJS *tinyJS) {}
extern "C" void _registerMathFunctions(CTinyJS *tinyJS) {

	 CScriptVarPtr Math = tinyJS->getRoot()->addChild("Math", tinyJS->newScriptVar(Object), SCRIPTVARLINK_CONSTANT);

	 // --- Math and Trigonometry functions ---
	 tinyJS->addNative("function Math.abs(a)", scMathAbs, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.round(a)", scMathRound, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.ceil(a)", scMathCeil, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.floor(a)", scMathFloor, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.min()", scMathMin, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.max()", scMathMax, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.range(x,a,b)", scMathRange, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.sign(a)", scMathSign, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.random(a)", scMathRandom, 0, SCRIPTVARLINK_BUILDINDEFAULT);


// atan2, ceil, floor, random, round, 

	 Math->addChild("LN2", tinyJS->newScriptVar(k_LN2), SCRIPTVARLINK_READONLY);
	 Math->addChild("LN10", tinyJS->newScriptVar(k_LN10), SCRIPTVARLINK_READONLY);
	 Math->addChild("LOG2E", tinyJS->newScriptVar(k_LOG2E), SCRIPTVARLINK_READONLY);
	 Math->addChild("LOG10E", tinyJS->newScriptVar(k_LOG10E), SCRIPTVARLINK_READONLY);
	 Math->addChild("SQRT1_2", tinyJS->newScriptVar(k_SQRT1_2), SCRIPTVARLINK_READONLY);
	 Math->addChild("SQRT2", tinyJS->newScriptVar(k_SQRT2), SCRIPTVARLINK_READONLY);
	 Math->addChild("PI", tinyJS->newScriptVar(k_PI), SCRIPTVARLINK_READONLY);
//    tinyJS->addNative("function Math.PI()", scMathPI, 0);
	 tinyJS->addNative("function Math.toDegrees(a)", scMathToDegrees, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.toRadians(a)", scMathToRadians, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.sin(a)", scMathSin, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.asin(a)", scMathASin, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.cos(a)", scMathCos, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.acos(a)", scMathACos, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.tan(a)", scMathTan, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.atan(a)", scMathATan, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.atan2(a,b)", scMathATan2, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.sinh(a)", scMathSinh, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.asinh(a)", scMathASinh, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.cosh(a)", scMathCosh, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.acosh(a)", scMathACosh, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.tanh(a)", scMathTanh, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.atanh(a)", scMathATanh, 0, SCRIPTVARLINK_BUILDINDEFAULT);
		 
	 Math->addChild("E", tinyJS->newScriptVar(k_E), SCRIPTVARLINK_READONLY);
	 tinyJS->addNative("function Math.log(a)", scMathLog, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.log10(a)", scMathLog10, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.exp(a)", scMathExp, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.pow(a,b)", scMathPow, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 
	 tinyJS->addNative("function Math.sqr(a)", scMathSqr, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	 tinyJS->addNative("function Math.sqrt(a)", scMathSqrt, 0, SCRIPTVARLINK_BUILDINDEFAULT);    
  
}
