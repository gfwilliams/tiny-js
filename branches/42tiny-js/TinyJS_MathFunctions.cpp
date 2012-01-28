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
 * Copyright (C) 2010 ardisoft
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

#include <math.h>
#include <cstdlib>
#include <sstream>
#include <ctime>
#include "TinyJS_MathFunctions.h"

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
 
//CScriptVar shortcut macro
#define scIsInt(a)          ( c->getArgument(a)->isInt() )
#define scIsDouble(a)       ( c->getArgument(a)->isDouble() )  
#define scGetInt(a)         ( c->getArgument(a)->getInt() )
#define scGetDouble(a)      ( c->getArgument(a)->getDouble() )  
#define scReturnInt(a)      ( c->setReturnVar(c->newScriptVar((int)a)) )
#define scReturnDouble(a)   ( c->setReturnVar(c->newScriptVar((double)a)) )
#define scReturnNaN()       do { c->setReturnVar(c->constScriptVar(NaN)); return; } while(0)

#ifdef _MSC_VER
namespace
{
	 double asinh( const double &value )
	 {
		  double returned;

		  if(value>0)
		  returned = log(value + sqrt(value * value + 1));
		  else
		  returned = -log(-value + sqrt(value * value + 1));

		  return(returned);
	 }

	 double acosh( const double &value )
	 {
		  double returned;

		  if(value>0)
		  returned = log(value + sqrt(value * value - 1));
		  else
		  returned = -log(-value + sqrt(value * value - 1));

		  return(returned);
	 }
}
#endif

#define GET_PARAMETER_AS_NUMERIC_VAR(v,n) CScriptVarPtr v = c->getArgument(n)->getNumericVar()
#define RETURN_NAN_IS_NAN(v) do{ if(v->isNaN()) { c->setReturnVar(v); return; } }while(0)
#define RETURN_NAN_IS_NAN_OR_INFINITY(v) do{ if(v->isNaN() || v->isInfinity()) { c->setReturnVar(v); return; } }while(0)
#define RETURN_INFINITY_IS_INFINITY(v) do{ if(v->isInfinity()) { c->setReturnVar(v); return; } }while(0)

#define GET_DOUBLE(v) v->getDouble()

//Math.abs(x) - returns absolute of given value
static void scMathAbs(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a);
	if(a->getInt() < 0) {
		CScriptVarPtr zero = c->newScriptVar(0);
		c->setReturnVar(zero->mathsOp(a, '-'));
	} else
		c->setReturnVar(a);
}

//Math.round(a) - returns nearest round of given value
static void scMathRound(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_INFINITY_IS_INFINITY(a);
	scReturnInt( (int)floor( GET_DOUBLE(a)+0.5 ) );
}

//Math.ceil(a) - returns nearest round of given value
static void scMathCeil(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_INFINITY_IS_INFINITY(a);
	scReturnInt( (int)ceil( GET_DOUBLE(a) ) );
}

//Math.floor(a) - returns nearest round of given value
static void scMathFloor(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_INFINITY_IS_INFINITY(a);
	scReturnInt( (int)floor( GET_DOUBLE(a) ) );
}

//Math.min(a,b) - returns minimum of two given values 
static void scMathMin(const CFunctionsScopePtr &c, void *userdata) {
	int length = c->getArgumentsLength();
	CScriptVarLinkPtr ret(c->constScriptVar(InfinityPositive));
	for(int i=0; i<length; i++)
	{
		GET_PARAMETER_AS_NUMERIC_VAR(a,i);RETURN_NAN_IS_NAN(a);
		if(a->isInfinity() < 0)  { c->setReturnVar(a); return; } 
		CScriptVarPtr result = a->mathsOp(ret, '<');
		if(result->getBool())
			ret = a;
	}
	c->setReturnVar(ret);
}

//Math.max(a,b) - returns maximum of two given values  
static void scMathMax(const CFunctionsScopePtr &c, void *userdata) {
	int length = c->getArgumentsLength();
	CScriptVarLinkPtr ret(c->constScriptVar(InfinityNegative));
	for(int i=0; i<length; i++)
	{
		GET_PARAMETER_AS_NUMERIC_VAR(a,i);RETURN_NAN_IS_NAN(a);
		if(a->isInfinity() > 0)  { c->setReturnVar(a); return; } 
		CScriptVarPtr result = a->mathsOp(ret, '>');
		if(result->getBool())
			ret = a;
	}
	c->setReturnVar(ret);
}

//Math.range(x,a,b) - returns value limited between two given values  
static void scMathRange(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(x,"x"); RETURN_NAN_IS_NAN(x); 
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); 
	GET_PARAMETER_AS_NUMERIC_VAR(b,"b"); RETURN_NAN_IS_NAN(b);

	CScriptVarPtr check;
	bool check_bool;

	check = a->mathsOp(b, LEX_LEQUAL);
	check_bool = check->getBool();
	if(!check_bool) scReturnNaN();
	
	check = x->mathsOp(a, '<');
	check_bool = check->getBool(); 
	if(check_bool) { c->setReturnVar(a); return; }

	check = x->mathsOp(b, '>');
	check_bool = check->getBool(); 
	c->setReturnVar((check_bool ? b:x));
}

//Math.sign(a) - returns sign of given value (-1==negative,0=zero,1=positive)
static void scMathSign(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); 
	double a_d = GET_DOUBLE(a);
	scReturnInt(a_d>0.0?1:a_d<0.0?-1:0);
}
static void init_rand(){
	static int inited=0;
	if(!inited) {
		inited = 1;
		srand((unsigned int)time(NULL));
	}
}
static void scMathRandom(const CFunctionsScopePtr &c, void *) {
	init_rand();
	scReturnDouble((double)rand()/RAND_MAX);
}

//Math.toDegrees(a) - returns degree value of a given angle in radians
static void scMathToDegrees(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_INFINITY_IS_INFINITY(a); 
	scReturnDouble( (180.0/k_PI)*( GET_DOUBLE(a) ) );
}

//Math.toRadians(a) - returns radians value of a given angle in degrees
static void scMathToRadians(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_INFINITY_IS_INFINITY(a); 
	scReturnDouble( (k_PI/180.0)*( GET_DOUBLE(a) ) );
}

//Math.sin(a) - returns trig. sine of given angle in radians
static void scMathSin(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( sin( GET_DOUBLE(a) ) );
}

//Math.asin(a) - returns trig. arcsine of given angle in radians
static void scMathASin(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( asin( GET_DOUBLE(a) ) );
}

//Math.cos(a) - returns trig. cosine of given angle in radians
static void scMathCos(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( cos( GET_DOUBLE(a) ) );
}

//Math.acos(a) - returns trig. arccosine of given angle in radians
static void scMathACos(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( acos( GET_DOUBLE(a) ) );
}

//Math.tan(a) - returns trig. tangent of given angle in radians
static void scMathTan(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( tan( GET_DOUBLE(a) ) );
}

//Math.atan(a) - returns trig. arctangent of given angle in radians
static void scMathATan(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( atan( GET_DOUBLE(a) ) );
}

//Math.sinh(a) - returns trig. hyperbolic sine of given angle in radians
static void scMathSinh(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( sinh( GET_DOUBLE(a) ) );
}

//Math.asinh(a) - returns trig. hyperbolic arcsine of given angle in radians
static void scMathASinh(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( asinh( GET_DOUBLE(a) ) );
}

//Math.cosh(a) - returns trig. hyperbolic cosine of given angle in radians
static void scMathCosh(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( cosh( GET_DOUBLE(a) ) );
}

//Math.acosh(a) - returns trig. hyperbolic arccosine of given angle in radians
static void scMathACosh(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( acosh( GET_DOUBLE(a) ) );
}

//Math.tanh(a) - returns trig. hyperbolic tangent of given angle in radians
static void scMathTanh(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( tanh( GET_DOUBLE(a) ) );
}

//Math.atan(a) - returns trig. hyperbolic arctangent of given angle in radians
static void scMathATanh(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN_OR_INFINITY(a); 
	scReturnDouble( atan( GET_DOUBLE(a) ) );
}

//Math.log(a) - returns natural logaritm (base E) of given value
static void scMathLog(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); 
	int a_i = a->isInfinity();
	double a_d = a->getDouble();
	if(a_i>0) { c->setReturnVar(c->constScriptVar(InfinityPositive)); return; }
	else if(a_i<0 || a_d<0.0) scReturnNaN();
	scReturnDouble( log( a_d ) );
}

//Math.log10(a) - returns logaritm(base 10) of given value
static void scMathLog10(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); 
	int a_i = a->isInfinity();
	double a_d = a->getDouble();
	if(a_i>0) { c->setReturnVar(c->constScriptVar(InfinityPositive)); return; }
	else if(a_i<0 || a_d<0.0) scReturnNaN();
	scReturnDouble( log10( a_d ) );
}

//Math.exp(a) - returns e raised to the power of a given number
static void scMathExp(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a);
	int a_i = a->isInfinity();
	if(a_i>0) { c->setReturnVar(c->constScriptVar(InfinityPositive)); return; }
	else if(a_i<0) { c->setReturnVar(c->newScriptVar(0)); return; }
	scReturnDouble( exp( GET_DOUBLE(a) ) );
}

//Math.pow(a,b) - returns the result of a number raised to a power (a)^(b)
static void scMathPow(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a);
	GET_PARAMETER_AS_NUMERIC_VAR(b,"b"); RETURN_NAN_IS_NAN(b); 
	int a_i = a->isInfinity(), b_i = b->isInfinity();
	double a_d = a->getDouble(), b_d = b->getDouble();
	if(b_i>0) {
		if(a_i || a_d>1.0 || a_d<-1.0) { c->setReturnVar(c->constScriptVar(InfinityPositive)); return; }
		else if(a_i==0 && (a_d==1.0 || a_d==-1.0)) { c->setReturnVar(c->newScriptVar(1)); return; }
		if(a_i==0 && a_d<1.0 && a_d>-1.0) { c->setReturnVar(c->newScriptVar(0)); return; }
	} else if(b_i<0) { c->setReturnVar(c->newScriptVar(0)); return; }
	else if(b_d == 0.0)  { c->setReturnVar(c->newScriptVar(1)); return; }

	if(a_i) a_d = a_i;
	double result = pow(a_d, b_d);
	if(a_i) { c->setReturnVar(c->constScriptVar(Infinity(result>=0.0?1:-1))); return; }
	scReturnDouble( result );
}

//Math.sqr(a) - returns square of given value
static void scMathSqr(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_INFINITY_IS_INFINITY(a); 
	scReturnDouble( ( GET_DOUBLE(a) * GET_DOUBLE(a) ) );
}

//Math.sqrt(a) - returns square root of given value
static void scMathSqrt(const CFunctionsScopePtr &c, void *userdata) {
	GET_PARAMETER_AS_NUMERIC_VAR(a,"a"); RETURN_NAN_IS_NAN(a); RETURN_INFINITY_IS_INFINITY(a); 
	scReturnDouble( sqrt( GET_DOUBLE(a) ) );
}

// ----------------------------------------------- Register Functions
void registerMathFunctions(CTinyJS *tinyJS) {

	 CScriptVarPtr Math = tinyJS->getRoot()->addChild("Math", tinyJS->newScriptVar(Object));

	 // --- Math and Trigonometry functions ---
	 tinyJS->addNative("function Math.abs(a)", scMathAbs, 0);
	 tinyJS->addNative("function Math.round(a)", scMathRound, 0);
	 tinyJS->addNative("function Math.ceil(a)", scMathCeil, 0);
	 tinyJS->addNative("function Math.floor(a)", scMathFloor, 0);
	 tinyJS->addNative("function Math.min()", scMathMin, 0);
	 tinyJS->addNative("function Math.max()", scMathMax, 0);
	 tinyJS->addNative("function Math.range(x,a,b)", scMathRange, 0);
	 tinyJS->addNative("function Math.sign(a)", scMathSign, 0);
	 tinyJS->addNative("function Math.random(a)", scMathRandom, 0);


// atan2, ceil, floor, random, round, 

	 Math->addChild("LN2", tinyJS->newScriptVar(k_LN2), SCRIPTVARLINK_ENUMERABLE);
	 Math->addChild("LN10", tinyJS->newScriptVar(k_LN10), SCRIPTVARLINK_ENUMERABLE);
	 Math->addChild("LOG2E", tinyJS->newScriptVar(k_LOG2E), SCRIPTVARLINK_ENUMERABLE);
	 Math->addChild("LOG10E", tinyJS->newScriptVar(k_LOG10E), SCRIPTVARLINK_ENUMERABLE);
	 Math->addChild("SQRT1_2", tinyJS->newScriptVar(k_SQRT1_2), SCRIPTVARLINK_ENUMERABLE);
	 Math->addChild("SQRT2", tinyJS->newScriptVar(k_SQRT2), SCRIPTVARLINK_ENUMERABLE);
	 Math->addChild("PI", tinyJS->newScriptVar(k_PI), SCRIPTVARLINK_ENUMERABLE);
//    tinyJS->addNative("function Math.PI()", scMathPI, 0);
	 tinyJS->addNative("function Math.toDegrees(a)", scMathToDegrees, 0);
	 tinyJS->addNative("function Math.toRadians(a)", scMathToRadians, 0);
	 tinyJS->addNative("function Math.sin(a)", scMathSin, 0);
	 tinyJS->addNative("function Math.asin(a)", scMathASin, 0);
	 tinyJS->addNative("function Math.cos(a)", scMathCos, 0);
	 tinyJS->addNative("function Math.acos(a)", scMathACos, 0);
	 tinyJS->addNative("function Math.tan(a)", scMathTan, 0);
	 tinyJS->addNative("function Math.atan(a)", scMathATan, 0);
	 tinyJS->addNative("function Math.sinh(a)", scMathSinh, 0);
	 tinyJS->addNative("function Math.asinh(a)", scMathASinh, 0);
	 tinyJS->addNative("function Math.cosh(a)", scMathCosh, 0);
	 tinyJS->addNative("function Math.acosh(a)", scMathACosh, 0);
	 tinyJS->addNative("function Math.tanh(a)", scMathTanh, 0);
	 tinyJS->addNative("function Math.atanh(a)", scMathATanh, 0);
		 
	 Math->addChild("E", tinyJS->newScriptVar(k_E), SCRIPTVARLINK_ENUMERABLE);
//	 tinyJS->addNative("function Math.E()", scMathE, 0);
	 tinyJS->addNative("function Math.log(a)", scMathLog, 0);
	 tinyJS->addNative("function Math.log10(a)", scMathLog10, 0);
	 tinyJS->addNative("function Math.exp(a)", scMathExp, 0);
	 tinyJS->addNative("function Math.pow(a,b)", scMathPow, 0);
	 
	 tinyJS->addNative("function Math.sqr(a)", scMathSqr, 0);
	 tinyJS->addNative("function Math.sqrt(a)", scMathSqrt, 0);    
  
}
