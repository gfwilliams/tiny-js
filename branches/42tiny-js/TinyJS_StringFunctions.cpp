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
/*
 * 42TinyJS
 *
 * A fork of TinyJS
 *
 * Copyright (C) 2010 ardisoft
 *
 * Authored By Armin Diedering <armin@diedering.de>
 *
 */

#include <algorithm>
#include "TinyJS.h"

using namespace std;
// ----------------------------------------------- Actual Functions

static void scStringCharAt(const CFunctionsScopePtr &c, void *) {
	string str = c->getParameter("this")->getString();
	int p = c->getParameter("pos")->getInt();
	if (p>=0 && p<(int)str.length())
		c->setReturnVar(c->newScriptVar(str.substr(p, 1)));
	else
		c->setReturnVar(c->newScriptVar(""));
}

static void scStringCharCodeAt(const CFunctionsScopePtr &c, void *) {
	string str = c->getParameter("this")->getString();
	int p = c->getParameter("pos")->getInt();
	if (p>=0 && p<(int)str.length())
		c->setReturnVar(c->newScriptVar(str.at(p)));
	else
		c->setReturnVar(c->newScriptVar(0));
}

static void scStringConcat(const CFunctionsScopePtr &c, void *userdata) {
	int length = c->getParameterLength();
	string str = c->getParameter("this")->getString();
	for(int i=(int)userdata; i<length; i++)
		str.append(c->getParameter(i)->getString());
	c->setReturnVar(c->newScriptVar(str));
}

static void scStringIndexOf(const CFunctionsScopePtr &c, void *userdata) {
	string str = c->getParameter("this")->getString();
	string search = c->getParameter("search")->getString();
	size_t p = (userdata==0) ? str.find(search) : str.rfind(search);
	int val = (p==string::npos) ? -1 : p;
	c->setReturnVar(c->newScriptVar(val));
}

static void scStringSlice(const CFunctionsScopePtr &c, void *userdata) {
	string str = c->getParameter("this")->getString();
	int length = c->getParameterLength()-((int)userdata & 1);
	bool slice = ((int)userdata & 2) == 0;
	int start = c->getParameter("start")->getInt();
	int end = (int)str.size();
	if(slice && start<0) start = str.size()+start;
	if(length>1) {
		end = c->getParameter("end")->getInt();
		if(slice && end<0) end = str.size()+end;
	}
	if(!slice && end < start) { end^=start; start^=end; end^=start; }
	if(start<0) start = 0;
	if(start>=(int)str.size()) 
		c->setReturnVar(c->newScriptVar(""));
	else if(end <= start)
		c->setReturnVar(c->newScriptVar(""));
	else
		c->setReturnVar(c->newScriptVar(str.substr(start, end-start)));
}

static void scStringSplit(const CFunctionsScopePtr &c, void *) {
	string str = c->getParameter("this")->getString();
	CScriptVarPtr sep_var = c->getParameter("separator");
	CScriptVarPtr limit_var = c->getParameter("limit");
	int limit = limit_var->isUndefined() ? 0x7fffffff : limit_var->getInt();

	CScriptVarPtr result(newScriptVar(c->getContext(), Array));
	c->setReturnVar(result);
	if(limit == 0 || !str.size())
		return;
	else if(sep_var->isUndefined()) {
		result->setArrayIndex(0, c->newScriptVar(str));
		return;
	}
	string sep = sep_var->getString();
	if(sep.size() == 0) {
		for(int i=0; i<min((int)sep.size(), limit); ++i)
			result->setArrayIndex(i, c->newScriptVar(str.substr(i,1)));
		return;
	}
	int length = 0;
	string::size_type pos = 0, last_pos=0;
	do {
		pos = str.find(sep, last_pos);
		result->setArrayIndex(length++, c->newScriptVar(str.substr(last_pos ,pos-last_pos)));
		if(length == limit || pos == string::npos) break;
		last_pos = pos+sep.size();
	} while (last_pos < str.size());
}

static void scStringSubstr(const CFunctionsScopePtr &c, void *userdata) {
	string str = c->getParameter("this")->getString();
	int length = c->getParameterLength()-(int)userdata;
	int start = c->getParameter("start")->getInt();
	if(start<0 || start>=(int)str.size()) 
		c->setReturnVar(c->newScriptVar(""));
	else if(length>1) {
		int length = c->getParameter("length")->getInt();
		c->setReturnVar(c->newScriptVar(str.substr(start, length)));
	} else
		c->setReturnVar(c->newScriptVar(str.substr(start)));
}

static void scStringToLowerCase(const CFunctionsScopePtr &c, void *) {
	string str = c->getParameter("this")->getString();
	transform(str.begin(), str.end(), str.begin(), ::tolower);
	c->setReturnVar(c->newScriptVar(str));
}

static void scStringToUpperCase(const CFunctionsScopePtr &c, void *) {
	string str = c->getParameter("this")->getString();
	transform(str.begin(), str.end(), str.begin(), ::toupper);
	c->setReturnVar(c->newScriptVar(str));
}

static void scStringTrim(const CFunctionsScopePtr &c, void *userdata) {
	string str = c->getParameter("this")->getString();
	string::size_type start = 0;
	string::size_type end = string::npos;
	if((((int)userdata) & 2) == 0) {
		start = str.find_first_not_of(" \t\r\n");
		if(start == string::npos) start = 0;
	}
	if((((int)userdata) & 1) == 0) {
		end = str.find_last_not_of(" \t\r\n");
		if(end != string::npos) end = 1+end-start;
	}
	c->setReturnVar(c->newScriptVar(str.substr(start, end)));
}



static void scCharToInt(const CFunctionsScopePtr &c, void *) {
	string str = c->getParameter("ch")->getString();;
	int val = 0;
	if (str.length()>0)
		val = (int)str.c_str()[0];
	c->setReturnVar(c->newScriptVar(val));
}


static void scStringFromCharCode(const CFunctionsScopePtr &c, void *) {
	char str[2];
	str[0] = c->getParameter("char")->getInt();
	str[1] = 0;
	c->setReturnVar(c->newScriptVar(str));
}


// ----------------------------------------------- Register Functions
void registerStringFunctions(CTinyJS *tinyJS) {
	CScriptVarPtr fnc;
	// charAt
	tinyJS->addNative("function String.prototype.charAt(pos)", scStringCharAt, 0);
	tinyJS->addNative("function String.charAt(this,pos)", scStringCharAt, 0);
	// charCodeAt
	tinyJS->addNative("function String.prototype.charCodeAt(pos)", scStringCharCodeAt, 0);
	tinyJS->addNative("function String.charCodeAt(this,pos)", scStringCharCodeAt, 0);
	// concat
	tinyJS->addNative("function String.prototype.concat()", scStringConcat, 0);
	tinyJS->addNative("function String.concat(this)", scStringConcat, (void*)1);
	// indexOf
	tinyJS->addNative("function String.prototype.indexOf(search)", scStringIndexOf, 0); // find the position of a string in a string, -1 if not
	tinyJS->addNative("function String.indexOf(this,search)", scStringIndexOf, 0); // find the position of a string in a string, -1 if not
	// lastIndexOf
	tinyJS->addNative("function String.prototype.lastIndexOf(search)", scStringIndexOf, (void*)-1); // find the last position of a string in a string, -1 if not
	tinyJS->addNative("function String.lastIndexOf(this,search)", scStringIndexOf, (void*)-1); // find the last position of a string in a string, -1 if not
	// slice
	tinyJS->addNative("function String.prototype.slice(start,end)", scStringSlice, 0); // find the last position of a string in a string, -1 if not
	tinyJS->addNative("function String.slice(this,start,end)", scStringSlice, (void*)1); // find the last position of a string in a string, -1 if not
	// split
	tinyJS->addNative("function String.prototype.split(separator,limit)", scStringSplit, 0);
	tinyJS->addNative("function String.split(this,separator,limit)", scStringSplit, 0);
	// substr
	tinyJS->addNative("function String.prototype.substr(start,length)", scStringSubstr, 0);
	tinyJS->addNative("function String.substr(this,start,length)", scStringSubstr, (void*)1);
	// substring
	tinyJS->addNative("function String.prototype.substring(start,end)", scStringSlice, (void*)2);
	tinyJS->addNative("function String.substring(this,start,end)", scStringSlice, (void*)3);
	// toLowerCase
	tinyJS->addNative("function String.prototype.toLowerCase()", scStringToLowerCase, 0);
	tinyJS->addNative("function String.toLowerCase(this)", scStringToLowerCase, 0);
	// toUpperCase
	tinyJS->addNative("function String.prototype.toUpperCase()", scStringToUpperCase, 0);
	tinyJS->addNative("function String.toUpperCase(this)", scStringToUpperCase, 0);
	// trim
	tinyJS->addNative("function String.prototype.trim()", scStringTrim, 0);
	tinyJS->addNative("function String.trim(this)", scStringTrim, 0);
	// trimLeft
	tinyJS->addNative("function String.prototype.trimLeft()", scStringTrim, (void*)1);
	tinyJS->addNative("function String.trimLeft(this)", scStringTrim, (void*)1);
	// trimRight
	tinyJS->addNative("function String.prototype.trimRight()", scStringTrim, (void*)2);
	tinyJS->addNative("function String.trimRight(this)", scStringTrim, (void*)2);

	tinyJS->addNative("function charToInt(ch)", scCharToInt, 0); //  convert a character to an int - get its value
	
	tinyJS->addNative("function String.prototype.fromCharCode(char)", scStringFromCharCode, 0);
}

