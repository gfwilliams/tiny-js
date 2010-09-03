// Javascript.cpp : Definiert den Einstiegspunkt für die Konsolenanwendung.
//

#include "targetver.h"
#include "stdafx.h"
#include "42tiny-js.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <TinyJS.h>
#include <TinyJS_Functions.h>

#ifdef __GNUC__
#	define UNUSED(x) __attribute__((__unused__))
#else
#	ifndef UNUSED
#		define UNUSED
#		pragma warning( disable : 4100 ) /* unreferenced formal parameter */
#	endif
#endif


// Das einzige Anwendungsobjekt

CWinApp theApp;

using namespace std;
//const char *code = "var a = 5; if (a==5) a=4; else a=3;";
//const char *code = "{ var a = 4; var b = 1; while (a>0) { b = b * 2; a = a - 1; } var c = 5; }";
//const char *code = "{ var b = 1; for (var i=0;i<4;i=i+1) b = b * 2; }";
const char *code = "function myfunc(x, y) { return x + y; } var a = myfunc(1,2); print(a);";

void js_print(CScriptVar *v, void *UNUSED(userdata)) {
    printf("> %s\n", v->getParameter("text")->getString().c_str());
}
void js_dump(CScriptVar *UNUSED(v), void *userdata) {
    CTinyJS *js = (CTinyJS*)userdata;
    js->root->trace(">  ");
}

int _tmain(int UNUSED(argc), TCHAR* UNUSED(argv[]), TCHAR* UNUSED(envp[]))
{
	int nRetCode = 0;

	// MFC initialisieren und drucken. Bei Fehlschlag Fehlermeldung aufrufen.
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: Den Fehlercode an Ihre Anforderungen anpassen.
		_tprintf(_T("Schwerwiegender Fehler bei der MFC-Initialisierung\n"));
		nRetCode = 1;
	}
	else
	{
		// TODO: Hier den Code für das Verhalten der Anwendung schreiben.
		 CTinyJS js;
		  /* add the functions from TinyJS_Functions.cpp */
		  registerFunctions(&js);
		  /* Add a native function */
		  js.addNative("function print(text)", &js_print, 0);
		  js.addNative("function dump()", &js_dump, &js);
		  /* Execute out bit of code - we could call 'evaluate' here if
			 we wanted something returned */
		  try {
			js.execute("var lets_quit = 0; function quit() { lets_quit = 1; }");
			js.execute("print(\"Interactive mode... Type quit(); to exit, or print(...); to print something, or dump() to dump the symbol table!\");");
		  } catch (CScriptException *e) {
			printf("ERROR: %s\n", e->text.c_str());
		  }

		  while (js.evaluate("lets_quit") == "0") {
			char buffer[2048];
			printf("> ");
			fgets ( buffer, sizeof(buffer), stdin );
			try {
			  js.execute(buffer);
			} catch (CScriptException *e) {
			  printf("ERROR: %s\n", e->text.c_str());
			}
		  }
		  return 0;
	}

	return nRetCode;
}
