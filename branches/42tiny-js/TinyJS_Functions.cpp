/*
 * TinyJS
 *
 * A single-file Javascript-alike engine
 *
 * Authored By Gordon Williams <gw@pur3.co.uk>
 *
 * Copyright (C) 2009 Pur3 Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef _MSC_VER
#	include "targetver.h"
#	include <afx.h>
#endif
#include <math.h>
#include <cstdlib>
#include <sstream>
#include "TinyJS_Functions.h"

#ifdef _DEBUG
#	ifdef _MSC_VER
#		define new DEBUG_NEW
#	endif
#endif

using namespace std;
// ----------------------------------------------- Actual Functions

void scTrace(CScriptVar *UNUSED(c), void * userdata) {
	CTinyJS *js = (CTinyJS*)userdata;
	js->root->trace();
}

void scObjectDump(CScriptVar *c, void *) {
	c->getParameter("this")->trace("> ");
}

void scObjectClone(CScriptVar *c, void *) {
	CScriptVar *obj = c->getParameter("this");
	c->getReturnVar()->copyValue(obj);
}

void scMathRand(CScriptVar *c, void *) {
	c->getReturnVar()->setDouble((double)rand()/RAND_MAX);
}

void scMathRandInt(CScriptVar *c, void *) {
	int min = c->getParameter("min")->getInt();
	int max = c->getParameter("max")->getInt();
	int val = min + (int)((long)rand()*(1+max-min)/RAND_MAX);
	if (val>max) val=max;
	c->getReturnVar()->setInt(val);
}

void scCharToInt(CScriptVar *c, void *) {
	string str = c->getParameter("ch")->getString();;
	int val = 0;
	if (str.length()>0)
		val = (int)str.c_str()[0];
	c->getReturnVar()->setInt(val);
}

void scStringIndexOf(CScriptVar *c, void *) {
	string str = c->getParameter("this")->getString();
	string search = c->getParameter("search")->getString();
	size_t p = str.find(search);
	int val = (p==string::npos) ? -1 : p;
	c->getReturnVar()->setInt(val);
}

void scStringSubstring(CScriptVar *c, void *) {
	string str = c->getParameter("this")->getString();
	int lo = c->getParameter("lo")->getInt();
	int hi = c->getParameter("hi")->getInt();

	int l = hi-lo;
	if (l>0 && lo>=0 && lo+l<=(int)str.length())
		c->getReturnVar()->setString(str.substr(lo, l));
	else
		c->getReturnVar()->setString("");
}

void scStringCharAt(CScriptVar *c, void *) {
	string str = c->getParameter("this")->getString();
	int p = c->getParameter("pos")->getInt();
	if (p>=0 && p<(int)str.length())
		c->getReturnVar()->setString(str.substr(p, 1));
	else
		c->getReturnVar()->setString("");
}

void scIntegerParseInt(CScriptVar *c, void *) {
	string str = c->getParameter("str")->getString();
	int val = strtol(str.c_str(),0,0);
	c->getReturnVar()->setInt(val);
}

void scIntegerValueOf(CScriptVar *c, void *) {
	string str = c->getParameter("str")->getString();

	int val = 0;
	if (str.length()==1)
		val = str[0];
	c->getReturnVar()->setInt(val);
}

void scJSONStringify(CScriptVar *c, void *) {
	std::ostringstream result;
	c->getParameter("obj")->getJSON(result);
	c->getReturnVar()->setString(result.str());
}

void scEval(CScriptVar *c, void *data) {
	CTinyJS *tinyJS = (CTinyJS *)data;
	std::string str = c->getParameter("jsCode")->getString();
	c->setReturnVar(tinyJS->evaluateComplex(str).var);
}

// ----------------------------------------------- Register Functions
void registerFunctions(CTinyJS *tinyJS) {
//    tinyJS->addNative("function eval(jsCode)", scEval, tinyJS); // execute the given string and return the result
	tinyJS->addNative("function trace()", scTrace, tinyJS);
	tinyJS->addNative("function Object.dump()", scObjectDump, 0);
	tinyJS->addNative("function Object.clone()", scObjectClone, 0);
	tinyJS->addNative("function Math.rand()", scMathRand, 0);
	tinyJS->addNative("function Math.randInt(min, max)", scMathRandInt, 0);
	tinyJS->addNative("function charToInt(ch)", scCharToInt, 0); //  convert a character to an int - get its value
	tinyJS->addNative("function String.indexOf(search)", scStringIndexOf, 0); // find the position of a string in a string, -1 if not
	tinyJS->addNative("function String.substring(lo,hi)", scStringSubstring, 0);
	tinyJS->addNative("function String.charAt(pos)", scStringCharAt, 0);
	tinyJS->addNative("function Integer.parseInt(str)", scIntegerParseInt, 0); // string to int
	tinyJS->addNative("function Integer.valueOf(str)", scIntegerValueOf, 0); // value of a single character
	tinyJS->addNative("function JSON.stringify(obj, replacer)", scJSONStringify, 0); // convert to JSON. replacer is ignored at the moment
	// JSON.parse is left out as you can (unsafely!) use eval instead
}

