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

#include <assert.h>
#include "TinyJS.h"
#include "TinyJS_Functions.h"

//const char *code = "var a = 5; if (a==5) a=4; else a=3;";
//const char *code = "{ var a = 4; var b = 1; while (a>0) { b = b * 2; a = a - 1; } var c = 5; }";
//const char *code = "{ var b = 1; for (var i=0;i<4;i=i+1) b = b * 2; }";
const char *code = "function myfunc(x, y) { return x + y; } var a = myfunc(1,2); print(a);";

void js_print(CScriptVar *v) {
    printf(">%s\n", v->findChild("text")->getString().c_str());
}

int main(int argc, char **argv)
{
  try {
    CTinyJS s;

    /* add the functions from TinyJS_Functions.cpp */
    registerFunctions(&s);
    /* Add a native function */
    s.addNative("function print(text)", &js_print);
    /* Execute out bit of code - we could call 'evaluate' here if
       we wanted something returned */
    s.execute(code);

    printf("Symbol table contents:\n");
    s.trace();
  } catch (CScriptException *e) {
    printf("ERROR: %s\n", e->text.c_str());
  }
  return 0;
}
