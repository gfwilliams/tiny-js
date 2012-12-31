/*
 * TinyJS
 *
 * A single-file Javascript-alike engine
 *
 * Authored By Gordon Williams <gw@pur3.co.uk>
 *
 * Copyright (C) 2009 Pur3 Ltd
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


#include <math.h>
#include <cstdlib>
#include <sstream>
#include <time.h>
#include "TinyJS.h"

using namespace std;
// ----------------------------------------------- Actual Functions

static void scTrace(const CFunctionsScopePtr &c, void * userdata) {
	CTinyJS *js = (CTinyJS*)userdata;
	if(c->getArgumentsLength())
		c->getArgument(0)->trace();
	else
		js->getRoot()->trace("root");
}

static void scObjectDump(const CFunctionsScopePtr &c, void *) {
	c->getArgument("this")->trace("> ");
}

static void scObjectClone(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr obj = c->getArgument("this");
	c->setReturnVar(obj->clone());
}

static void scIntegerValueOf(const CFunctionsScopePtr &c, void *) {
	string str = c->getArgument("str")->toString();

	int val = 0;
	if (str.length()==1)
		val = str.operator[](0);
	c->setReturnVar(c->newScriptVar(val));
}

static void scJSONStringify(const CFunctionsScopePtr &c, void *) {
	uint32_t UniqueID = c->getContext()->getUniqueID();
	bool hasRecursion=false;
	c->setReturnVar(c->newScriptVar(c->getArgument("obj")->getParsableString("", "   ", UniqueID, hasRecursion)));
	if(hasRecursion) c->throwError(TypeError, "cyclic object value");
}

static void scArrayContains(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getArgument("obj");
	CScriptVarPtr arr = c->getArgument("this");

	int l = arr->getArrayLength();
	CScriptVarPtr equal = c->constScriptVar(Undefined);
	for (int i=0;i<l;i++) {
		equal = obj->mathsOp(arr->getArrayIndex(i), LEX_EQUAL);
		if(equal->toBoolean()) {
			c->setReturnVar(c->constScriptVar(true));
			return;
		}
	}
	c->setReturnVar(c->constScriptVar(false));
}

static void scArrayRemove(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getArgument("obj");
	CScriptVarPtr arr = c->getArgument("this");
	int i;
	vector<int> removedIndices;

	int l = arr->getArrayLength();
	CScriptVarPtr equal = c->constScriptVar(Undefined);
	for (i=0;i<l;i++) {
		equal = obj->mathsOp(arr->getArrayIndex(i), LEX_EQUAL);
		if(equal->toBoolean()) {
			removedIndices.push_back(i);
		}
	}
	if(removedIndices.size()) {
		vector<int>::iterator remove_it = removedIndices.begin();
		int next_remove = *remove_it;
		int next_insert = *remove_it++;
		for (i=next_remove;i<l;i++) {

			CScriptVarLinkPtr link = arr->findChild(int2string(i));
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
	string sep = c->getArgument("separator")->toString();
	CScriptVarPtr arr = c->getArgument("this");

	ostringstream sstr;
	int l = arr->getArrayLength();
	for (int i=0;i<l;i++) {
		if (i>0) sstr << sep;
		sstr << arr->getArrayIndex(i)->toString();
	}

	c->setReturnVar(c->newScriptVar(sstr.str()));
}

// ----------------------------------------------- Register Functions
void registerFunctions(CTinyJS *tinyJS) {
}
extern "C" void _registerFunctions(CTinyJS *tinyJS) {
	tinyJS->addNative("function trace()", scTrace, tinyJS, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Object.prototype.dump()", scObjectDump, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Object.prototype.clone()", scObjectClone, 0, SCRIPTVARLINK_BUILDINDEFAULT);

	tinyJS->addNative("function Integer.valueOf(str)", scIntegerValueOf, 0, SCRIPTVARLINK_BUILDINDEFAULT); // value of a single character
	tinyJS->addNative("function JSON.stringify(obj, replacer)", scJSONStringify, 0, SCRIPTVARLINK_BUILDINDEFAULT); // convert to JSON. replacer is ignored at the moment
	tinyJS->addNative("function Array.prototype.contains(obj)", scArrayContains, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.remove(obj)", scArrayRemove, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.join(separator)", scArrayJoin, 0, SCRIPTVARLINK_BUILDINDEFAULT);
}

