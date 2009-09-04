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

#include "TinyJS_Functions.h"
#include <math.h>
#include <cstdlib>

using namespace std;
// ----------------------------------------------- Actual Functions

void scTrace(CScriptVar *c) {
    while (c->parent) c = c->parent;
    c->trace();
}

void scRand(CScriptVar *c) {
    c->getChild(TINYJS_RETURN_VAR)->setDouble((double)rand()/RAND_MAX);
}

void scRandInt(CScriptVar *c) {
    int min = c->getChild("min")->getInt();
    int max = c->getChild("max")->getInt();
    int val = min + (int)((long)rand()*(1+max-min)/RAND_MAX);
    if (val>max) val=max;
    c->getChild(TINYJS_RETURN_VAR)->setInt(val);
}

void scCharToInt(CScriptVar *c) {
    string str = c->getChild("ch")->getString();;
    int val = 0;
    if (str.length()>0)
        val = (int)str.c_str()[0];
    c->getChild(TINYJS_RETURN_VAR)->setInt(val);
}

void scStrLen(CScriptVar *c) {
    string str = c->getChild("str")->getString();;
    int val = str.length();
    c->getChild(TINYJS_RETURN_VAR)->setInt(val);
}

void scStrPos(CScriptVar *c) {
    string str = c->getChild("string")->getString();
    string search = c->getChild("search")->getString();
    size_t p = str.find(search);
    int val = (p==string::npos) ? -1 : p;
    c->getChild(TINYJS_RETURN_VAR)->setInt(val);
}

void scAtoi(CScriptVar *c) {
    int val = c->getChild("str")->getInt();
    c->getChild(TINYJS_RETURN_VAR)->setInt(val);
}

// ----------------------------------------------- Register Functions
void registerFunctions(CTinyJS *tinyJS) {
    tinyJS->addNative("function trace()", scTrace);
    tinyJS->addNative("function rand()", scRand);
    tinyJS->addNative("function randInt(min, max)", scRandInt);
    tinyJS->addNative("function charToInt(ch)", scCharToInt); //  convert a character to an int - get its value
    tinyJS->addNative("function strlen(str)", scStrLen); // length of a string
    tinyJS->addNative("function strpos(string,search)", scStrPos); // find the position of a string in a string, -1 if not
    tinyJS->addNative("function atoi(str)", scAtoi); // string to int
}
