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
 * Copyright (C) 2010-2014 ardisoft
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


/*
 * This is a simple program showing how to use TinyJS
 */

#include "TinyJS.h"
//#include "TinyJS_Functions.h"
//#include "TinyJS_StringFunctions.h"
//#include "TinyJS_MathFunctions.h"
#include <assert.h>
#include <stdio.h>
#include <iostream>

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
	printf("> %s\n", v->getArgument("text")->toString().c_str());
}

void js_dump(const CFunctionsScopePtr &v, void *) {
	v->getContext()->getRoot()->trace(">  ");
}


char *topOfStack;
#define sizeOfStack 1*1024*1024 /* for example 1 MB depend of Compiler-Options */
#define sizeOfSafeStack 50*1024 /* safety area */

int main(int , char **)
{
	char dummy;
	topOfStack = &dummy;
//	printf("%i %i\n", __cplusplus, _MSC_VER);

//	printf("Locale:%s\n",setlocale( LC_ALL, 0 ));
//	setlocale( LC_ALL, ".858" );
//	printf("Locale:%s\n",setlocale( LC_ALL, 0 ));
	CTinyJS *js = new CTinyJS();
	/* add the functions from TinyJS_Functions.cpp */
//	registerFunctions(js);
//	registerStringFunctions(js);
//	registerMathFunctions(js);
	/* Add a native function */
	js->addNative("function print(text)", &js_print, 0);
//  js->addNative("function dump()", &js_dump, js);
	/* Execute out bit of code - we could call 'evaluate' here if
		we wanted something returned */
	js->setStackBase(topOfStack-(sizeOfStack-sizeOfSafeStack));
	try {
		js->execute("var lets_quit = 0; function quit() { lets_quit = 1; }");
		js->execute("print(\"Interactive mode... Type quit(); to exit, or print(...); to print something, or dump() to dump the symbol table!\");");
		js->execute("print(function () {print(\"gen\");yield 5;yield 6;}().next());", "yield-test.js");
		js->execute("for each(i in function () {print(\"gen\");yield 5;yield 6;}()) print(i);", "yield-test.js");
		js->execute("function g(){				\n\n"
			"	throw \"error\"\n"
			"	try{									\n"
			"		yield 1; yield 2				\n"
			"	}finally{							\n"
			"		print(\"finally\")			\n"
			"		yield 3;							\n"
			"		throw StopIteration			\n"
			"	}										\n"
			"	print(\"after finally\")		\n"
			"}t=g()", "test");
	} catch (CScriptException *e) {
		printf("%s\n", e->toString().c_str());
		delete e;
	}
	int lineNumber = 0;
	while (js->evaluate("lets_quit") == "0") {
		std::string buffer;
		if(!std::getline(std::cin, buffer)) break;
		try {
			js->execute(buffer, "console.input", lineNumber++);
		} catch (CScriptException *e) {
			printf("%s\n", e->toString().c_str());
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
