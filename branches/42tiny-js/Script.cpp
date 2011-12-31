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
 * This is a simple program showing how to use TinyJS
 */

#include "TinyJS.h"
#include "TinyJS_Functions.h"
#include "TinyJS_StringFunctions.h"
#include "TinyJS_MathFunctions.h"
#include <assert.h>
#include <stdio.h>
	
#ifdef _DEBUG
#	ifndef _MSC_VER
#		define DEBUG_MEMORY 1
#	endif
#endif

//const char *code = "var a = 5; if (a==5) a=4; else a=3;";
//const char *code = "{ var a = 4; var b = 1; while (a>0) { b = b * 2; a = a - 1; } var c = 5; }";
//const char *code = "{ var b = 1; for (var i=0;i<4;i=i+1) b = b * 2; }";
const char *code = "function myfunc(x, y) { return x + y; } var a = myfunc(1,2); print(a);";

void js_print(const CFunctionsScopePtr &v, void *) {
	printf("> %s\n", v->getParameter("text")->getString().c_str());
}

void js_dump(const CFunctionsScopePtr &v, void *) {
	v->getContext()->getRoot()->trace(">  ");
}


int main(int , char **)
{
	CTinyJS *js = new CTinyJS();
	/* add the functions from TinyJS_Functions.cpp */
	registerFunctions(js);
	registerStringFunctions(js);
	registerMathFunctions(js);
	/* Add a native function */
	js->addNative("function print(text)", &js_print, 0);
//  js->addNative("function dump()", &js_dump, js);
	/* Execute out bit of code - we could call 'evaluate' here if
		we wanted something returned */
	try {
		js->execute("var lets_quit = 0; function quit() { lets_quit = 1; }");
		js->execute("print(\"Interactive mode... Type quit(); to exit, or print(...); to print something, or dump() to dump the symbol table!\");");
	} catch (CScriptException *e) {
		printf("ERROR: %s\n", e->text.c_str());
		delete e;
	}

	while (js->evaluate("lets_quit") == "0") {
		char buffer[2048];
		fgets ( buffer, sizeof(buffer), stdin );
		try {
			js->execute(buffer);
		} catch (CScriptException *e) {
			printf("ERROR: %s\n", e->text.c_str());
			delete e;
		}
	}
	delete js;
#ifdef _WIN32
#ifdef _DEBUG
//  _CrtDumpMemoryLeaks();
/*
	no dump momoryleaks here
	_CrtSetDbgFlag(..) force dump memoryleake after call of all global deconstructors
*/
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
#endif
	return 0;
}
