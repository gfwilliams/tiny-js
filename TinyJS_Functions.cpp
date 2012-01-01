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

#include <math.h>
#include <cstdlib>
#include <sstream>
#include <time.h>
#include "TinyJS_Functions.h"

using namespace std;
// ----------------------------------------------- Actual Functions

static void scTrace(const CFunctionsScopePtr &c, void * userdata) {
	CTinyJS *js = (CTinyJS*)userdata;
	if(c->getParameterLength())
		c->getParameter(0)->trace();
	else
		js->getRoot()->trace("root");
}

static void scObjectDump(const CFunctionsScopePtr &c, void *) {
	c->getParameter("this")->trace("> ");
}

static void scObjectClone(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr obj = c->getParameter("this");
	c->setReturnVar(obj->clone());
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

static void scIntegerValueOf(const CFunctionsScopePtr &c, void *) {
	string str = c->getParameter("str")->getString();

	int val = 0;
	if (str.length()==1)
		val = str.operator[](0);
	c->setReturnVar(c->newScriptVar(val));
}

static void scJSONStringify(const CFunctionsScopePtr &c, void *) {
	string indent = "   ", indentString;
	c->setReturnVar(c->newScriptVar(c->getParameter("obj")->getParsableString(indentString, indent)));
}

static void scArrayContains(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getParameter("obj");
	CScriptVarPtr arr = c->getParameter("this");

	int l = arr->getArrayLength();
	CScriptVarSmartLink equal;
	for (int i=0;i<l;i++) {
		equal = obj->mathsOp(arr->getArrayIndex(i), LEX_EQUAL);
		if((*equal)->getBool()) {
			c->setReturnVar(c->newScriptVar(true));
			return;
		}
	}
	c->setReturnVar(c->newScriptVar(false));
}

static void scArrayRemove(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getParameter("obj");
	CScriptVarPtr arr = c->getParameter("this");
	int i;
	vector<int> removedIndices;

	int l = arr->getArrayLength();
	CScriptVarSmartLink equal;
	for (i=0;i<l;i++) {
		equal = obj->mathsOp(arr->getArrayIndex(i), LEX_EQUAL);
		if((*equal)->getBool()) {
			removedIndices.push_back(i);
		}
	}
	if(removedIndices.size()) {
		vector<int>::iterator remove_it = removedIndices.begin();
		int next_remove = *remove_it;
		int next_insert = *remove_it++;
		for (i=next_remove;i<l;i++) {

			CScriptVarLink *link = arr->findChild(int2string(i));
			if(i == next_remove) {
				if(link) arr->removeLink(link);
				if(remove_it != removedIndices.end())
					next_remove = *remove_it++;
			} else {
				if(link) {
					arr->setArrayIndex(next_insert++, link);
					arr->removeLink(link);
				}	
			}
		}
	}
}

static void scArrayJoin(const CFunctionsScopePtr &c, void *data) {
	string sep = c->getParameter("separator")->getString();
	CScriptVarPtr arr = c->getParameter("this");

	ostringstream sstr;
	int l = arr->getArrayLength();
	for (int i=0;i<l;i++) {
		if (i>0) sstr << sep;
		sstr << arr->getArrayIndex(i)->getString();
	}

	c->setReturnVar(c->newScriptVar(sstr.str()));
}

// ----------------------------------------------- Register Functions
void registerFunctions(CTinyJS *tinyJS) {
	tinyJS->addNative("function trace()", scTrace, tinyJS);
	tinyJS->addNative("function Object.prototype.dump()", scObjectDump, 0);
	tinyJS->addNative("function Object.prototype.clone()", scObjectClone, 0);

	tinyJS->addNative("function Integer.valueOf(str)", scIntegerValueOf, 0); // value of a single character
	tinyJS->addNative("function JSON.stringify(obj, replacer)", scJSONStringify, 0); // convert to JSON. replacer is ignored at the moment
	tinyJS->addNative("function Array.prototype.contains(obj)", scArrayContains, 0);
	tinyJS->addNative("function Array.prototype.remove(obj)", scArrayRemove, 0);
	tinyJS->addNative("function Array.prototype.join(separator)", scArrayJoin, 0);
}

