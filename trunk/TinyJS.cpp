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

/* Version 0.1  :  (gw) First published on Google Code
   Version 0.11 :  Making sure the 'root' variable never changes
                   'symbol_base' added for the current base of the sybmbol table
   Version 0.12 :  Added findChildOrCreate, changed string passing to use references
                   Fixed broken string encoding in getJSString()
                   Removed getInitCode and added getJSON instead
                   Added nil
                   Added rough JSON parsing
                   Improved example app
   Version 0.13 :  Added tokenEnd/tokenLastEnd to lexer to avoid parsing whitespace
                   Ability to define functions without names
                   Can now do "var mine = function(a,b) { ... };"
                   Slightly better 'trace' function
                   Added findChildOrCreateByPath function
                   Added simple test suite
                   Added skipping of blocks when not executing

    NOTE: Currently TinyJS passes by VALUE for function, and never copies
          references - which is a major difference between this and proper
          JavaScript.
          It also doesn't support class prototypes, or the 'new' operator
 */

#include "TinyJS.h"
#include <assert.h>

#define ASSERT(X) assert(X)
#define CLEAN(x) { CScriptVar *__v = x; if (__v && !__v->parent) delete __v; }

#include <string>
#include <string.h>
#include <sstream>
#include <cstdlib>

using namespace std;

#ifdef __GNUC__
#define vsprintf_s vsnprintf
#define sprintf_s snprintf
#endif

// ----------------------------------------------------------------------------------- Utils
bool isWhitespace(char ch) {
    return (ch==' ') || (ch=='\t') || (ch=='\n') || (ch=='\r');
}

bool isNumeric(char ch) {
    return (ch>='0') && (ch<='9');
}
bool isAlpha(char ch) {
    return ((ch>='a') && (ch<='z')) || ((ch>='A') && (ch<='Z')) || ch=='_';
}

bool isIDString(const char *s) {
    if (!isAlpha(*s))
        return false;
    while (*s) {
        if (!(isAlpha(*s) || isNumeric(*s)))
            return false;
        s++;
    }
    return true;
}

void replace(string &str, char textFrom, const char *textTo) {
    int sLen = strlen(textTo);
    size_t p = str.find(textFrom);
    while (p != string::npos) {
        str = str.substr(0, p) + textTo + str.substr(p+1);
        p = str.find(textFrom, p+sLen);
    }
}

/// convert the given string into a quoted string suitable for javascript
std::string getJSString(const std::string &str) {
    std::string nStr = str;
    for (size_t i=0;i<nStr.size();i++) {
      const char *replaceWith = "";
      bool replace = true;

      switch (nStr[i]) {
        case '\\': replaceWith = "\\\\"; break;
        case '\n': replaceWith = "\\n"; break;
        case '"': replaceWith = "\\\""; break;
        default: replace=false;
      }

      if (replace) {
        nStr = nStr.substr(0, i) + replaceWith + nStr.substr(i+1);
        i += strlen(replaceWith)-1;
      }
    }
    return "\"" + nStr + "\"";
}

// ----------------------------------------------------------------------------------- CSCRIPTEXCEPTION

CScriptException::CScriptException(const string &exceptionText) {
    text = exceptionText;
}

// ----------------------------------------------------------------------------------- CSCRIPTLEX

CScriptLex::CScriptLex(const string &input) {
    data = strdup(input.c_str());
    dataOwned = true;
    dataStart = 0;
    dataEnd = strlen(data);
    reset();
}

CScriptLex::CScriptLex(CScriptLex *owner, int startChar, int endChar) {
    data = owner->data;
    dataOwned = false;
    dataStart = startChar;
    dataEnd = endChar;
    reset();
}

CScriptLex::~CScriptLex(void)
{
    if (dataOwned)
        free((void*)data);
}

void CScriptLex::reset() {
    dataPos = dataStart;
    tokenStart = 0;
    tokenEnd = 0;
    tokenLastEnd = 0;
    tk = 0;
    tkStr = "";
    getNextCh();
    getNextCh();
    getNextToken();
}

void CScriptLex::match(int expected_tk) {
    if (tk!=expected_tk) {
        ostringstream errorString;
        errorString << "Got " << getTokenStr(tk) << " expected " << getTokenStr(expected_tk)
         << " at " << getPosition(tokenStart) << "in '" << data << "'";
        throw new CScriptException(errorString.str());
    }
    getNextToken();
}

string CScriptLex::getTokenStr(int token) {
    if (token>32 && token<128) {
        char buf[4] = "' '";
        buf[1] = token;
        return buf;
    }
    switch (token) {
        case LEX_EOF : return "EOF";
        case LEX_ID : return "ID";
        case LEX_INT : return "INT";
        case LEX_FLOAT : return "FLOAT";
        case LEX_STR : return "STRING";
        case LEX_EQUAL : return "==";
        case LEX_NEQUAL : return "!=";
        case LEX_LEQUAL : return "<=";
        case LEX_GEQUAL : return ">=";
        case LEX_PLUSEQUAL : return "+=";
        case LEX_MINUSEQUAL : return "-=";
        case LEX_PLUSPLUS : return "++";
        case LEX_MINUSMINUS : return "--";
        case LEX_ANDAND : return "&&";
        case LEX_OROR : return "||";
                // reserved words
        case LEX_R_IF : return "if";
        case LEX_R_ELSE : return "else";
        case LEX_R_WHILE : return "while";
        case LEX_R_FOR : return "for";
        case LEX_R_FUNCTION : return "function";
        case LEX_R_RETURN : return "return";
        case LEX_R_VAR : return "var";
        case LEX_R_TRUE : return "true";
        case LEX_R_FALSE : return "false";
        case LEX_R_NIL : return "nil";
    }

    ostringstream msg;
    msg << "?[" << token << "]";
    return msg.str();
}

void CScriptLex::getNextCh() {
    currCh = nextCh;
    if (dataPos < dataEnd)
        nextCh = data[dataPos];
    else
        nextCh = 0;
    dataPos++;
}

void CScriptLex::getNextToken() {
    tk = LEX_EOF;
    tkStr.clear();
    while (currCh && isWhitespace(currCh)) getNextCh();
    // newline comments
    if (currCh=='/' && nextCh=='/') {
        while (currCh && currCh!='\n') getNextCh();
        getNextCh();
        getNextToken();
        return;
    }
    // block comments
    if (currCh=='/' && nextCh=='*') {
        while (currCh && (currCh!='*' || nextCh!='/')) getNextCh();
        getNextCh();
        getNextCh();
        getNextToken();
        return;
    }
    // record beginning of this token
    tokenStart = dataPos-2;
    // tokens
    if (isAlpha(currCh)) { //  IDs
        while (isAlpha(currCh) || isNumeric(currCh)) {
            tkStr += currCh;
            getNextCh();
        }
        tk = LEX_ID;
             if (tkStr=="if") tk = LEX_R_IF;
        else if (tkStr=="else") tk = LEX_R_ELSE;
        else if (tkStr=="while") tk = LEX_R_WHILE;
        else if (tkStr=="for") tk = LEX_R_FOR;
        else if (tkStr=="function") tk = LEX_R_FUNCTION;
        else if (tkStr=="return") tk = LEX_R_RETURN;
        else if (tkStr=="var") tk = LEX_R_VAR;
        else if (tkStr=="true") tk = LEX_R_TRUE;
        else if (tkStr=="false") tk = LEX_R_FALSE;
        else if (tkStr=="nil") tk = LEX_R_NIL;
    } else if (isNumeric(currCh)) { // Numbers
        tk = LEX_INT;
        while (isNumeric(currCh)) {
            tkStr += currCh;
            getNextCh();
        }
        if (currCh=='.') {
            tk = LEX_FLOAT;
            tkStr += '.';
            getNextCh();
            while (isNumeric(currCh)) {
                tkStr += currCh;
                getNextCh();
            }
        }
    } else if (currCh=='"') {
        getNextCh();
        while (currCh && currCh!='"') {
            if (currCh == '\\') {
                getNextCh();
                switch (currCh) {
                case 'n' : tkStr += '\n'; break;
                case '"' : tkStr += '"'; break;
                case '\\' : tkStr += '\\'; break;
                default: tkStr += currCh;
                }
            } else {
                tkStr += currCh;
            }
            getNextCh();
        }
        getNextCh();
        tk = LEX_STR;
    } else {
        // single chars
        tk = currCh;
        if (currCh) getNextCh();
        if (tk=='=' && currCh=='=') {
            tk = LEX_EQUAL;
            getNextCh();
        } else if (tk=='!' && currCh=='=') {
            tk = LEX_NEQUAL;
            getNextCh();
        } else if (tk=='<' && currCh=='=') {
            tk = LEX_LEQUAL;
            getNextCh();
        } else if (tk=='>' && currCh=='=') {
            tk = LEX_GEQUAL;
            getNextCh();
        }  else if (tk=='+' && currCh=='=') {
            tk = LEX_PLUSEQUAL;
            getNextCh();
        }  else if (tk=='-' && currCh=='=') {
            tk = LEX_MINUSEQUAL;
            getNextCh();
        }  else if (tk=='+' && currCh=='+') {
            tk = LEX_PLUSPLUS;
            getNextCh();
        }  else if (tk=='-' && currCh=='-') {
            tk = LEX_MINUSMINUS;
            getNextCh();
        } else if (tk=='&' && currCh=='&') {
            tk = LEX_ANDAND;
            getNextCh();
        } else if (tk=='|' && currCh=='|') {
            tk = LEX_OROR;
            getNextCh();
        }
    }
    /* This isn't quite right yet */
    tokenLastEnd = tokenEnd;
    tokenEnd = dataPos-3;
}

string CScriptLex::getSubString(int lastPosition) {
    int lastCharIdx = tokenLastEnd+1;
    if (lastCharIdx < dataEnd) {
        /* save a memory alloc by using our data array to create the
           substring */
        char old = data[lastCharIdx];
        data[lastCharIdx] = 0;
        std::string value = &data[lastPosition];
        data[lastCharIdx] = old;
        return value;
    } else {
        return std::string(&data[lastPosition]);
    }
}


CScriptLex *CScriptLex::getSubLex(int lastPosition) {
    int lastCharIdx = tokenLastEnd+1;
    if (lastCharIdx < dataEnd)
        return new CScriptLex( this, lastPosition, lastCharIdx);
    else
        return new CScriptLex( this, lastPosition, dataEnd );
}

string CScriptLex::getPosition(int pos) {
    int line = 1,col = 1;
    for (int i=0;i<pos;i++) {
        char ch;
        if (i < dataEnd)
            ch = data[i];
        else
            ch = 0;
        col++;
        if (ch=='\n') {
            line++;
            col = 0;
        }
    }
    char buf[256];
    sprintf_s(buf, 256, "(line: %d, col: %d)", line, col);
    return buf;
}


// ----------------------------------------------------------------------------------- CSCRIPTVAR

CScriptVar::CScriptVar(void) {
    init();
    name=TINYJS_TEMP_NAME;
    data=TINYJS_BLANK_DATA;
}

CScriptVar::~CScriptVar(void) {
    removeAllChildren();
}

CScriptVar::CScriptVar(string varName, string varData, int varFlags) {
    init();
    name = varName;
    flags = varFlags;
    data = varData;
}

CScriptVar::CScriptVar(double val) {
    init();
    setDouble(val);
}

CScriptVar::CScriptVar(int val) {
    init();
    setInt(val);
}

void CScriptVar::init() {
    firstChild = 0;
    lastChild = 0;
    nextSibling = 0;
    prevSibling = 0;
    parent = 0;
    flags = 0;
    jsCallback = 0;
}

CScriptVar *CScriptVar::findChild(const string &childName) {
    CScriptVar *v = firstChild;
    while (v) {
        if (v->name.compare(childName)==0)
            return v;
        v = v->nextSibling;
    }
    return 0;
}

CScriptVar *CScriptVar::findChildOrCreate(const string &childName) {
    CScriptVar *v = findChild(childName);
    if (!v) {
      v = new CScriptVar(childName);
      addChild(v);
    }
    return v;
}

CScriptVar *CScriptVar::findChildOrCreateByPath(const std::string &path) {
  size_t p = path.find('.');
  if (p == string::npos)
    return findChildOrCreate(path);

  return findChildOrCreate(path.substr(0,p))->
            findChildOrCreateByPath(path.substr(p+1));
}

CScriptVar *CScriptVar::findRecursive(const string &childName) {
    CScriptVar *v = findChild(childName);
    if (!v && parent)
        v = parent->findRecursive(childName);
    return v;
}

void CScriptVar::addChild(CScriptVar *child) {
    ASSERT(!child->parent && !child->nextSibling && !child->prevSibling);
    child->parent = this;
    if (lastChild) {
        lastChild->nextSibling = child;
        child->prevSibling = lastChild;
        lastChild = child;
    } else {
        firstChild = child;
        lastChild = child;
    }
}

void CScriptVar::addNamedChild(const std::string &childName, CScriptVar *child) {
  if (!child) {
    // not given anything - just add empty child
    addChild(new CScriptVar(childName));
  } else if (child->parent) {
    // current element has an owner, so copy it
    CScriptVar *c = new CScriptVar(childName);
    c->copyValue(child);
    addChild(c);
  } else {
    // no owners - just change name and add it
    child->name = childName;
    addChild(child);
  }
}

CScriptVar *CScriptVar::addChildNoDup(CScriptVar *child) {
    ASSERT(!child->parent);
    CScriptVar *v = findChild(child->getName());
    if (v) {
        v->copyValue(child);
        delete child;
    } else {
        addChild(child);
        v = child;
    }

    return v;
}

void CScriptVar::removeChild(CScriptVar *child) {
    ASSERT(child->parent == this);
    if (child->nextSibling)
        child->nextSibling->prevSibling = child->prevSibling;
    if (child->prevSibling)
        child->prevSibling->nextSibling = child->nextSibling;
    if (lastChild == child)
        lastChild = child->prevSibling;
    if (firstChild == child)
        firstChild = child->nextSibling;
    child->prevSibling = 0;
    child->nextSibling = 0;
    child->parent = 0;
}

void CScriptVar::removeAllChildren() {
    CScriptVar *c = firstChild;
    while (c) {
        CScriptVar *t = c->nextSibling;
        delete c;
        c = t;
    }
    firstChild = 0;
    lastChild = 0;
}

CScriptVar *CScriptVar::getRoot() {
    CScriptVar *p = this;
    while (p->parent) p = p->parent;
    return p;
}

const string &CScriptVar::getName() {
    return name;
}

int CScriptVar::getInt() {
    return atoi(data.c_str());
}

double CScriptVar::getDouble() {
    return atof(data.c_str());
}

const string &CScriptVar::getString() {
    return data;
}

void CScriptVar::setInt(int val) {
    char buf[256];
    sprintf(buf, "%d", val);
    flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_INTEGER;
    data = buf;
}

void CScriptVar::setDouble(double val) {
    char buf[256];
    sprintf(buf, "%lf", val);
    flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_DOUBLE;
    data = buf;
}

void CScriptVar::setString(const string &str) {
    // name sure it's not still a number or integer
    flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_STRING;
    data = str;
}

void CScriptVar::setVoid() {
    // name sure it's not still a number or integer
    flags &= ~SCRIPTVAR_VARTYPEMASK;
    data = TINYJS_BLANK_DATA;
    removeAllChildren();
}

CScriptVar *CScriptVar::mathsOp(CScriptVar *b, int op) {
    CScriptVar *a = this;
    // if these vars have nothing in them and we add a number...
    if (a->data.empty() && b->isNumeric())
        a->setInt(0);
    if (b->data.empty() && a->isNumeric())
        b->setInt(0);
    // do maths...
    if (a->isNumeric() && b->isNumeric()) {
        if (a->isInt() && b->isInt()) {
            // use ints
            int da = a->getInt();
            int db = b->getInt();
            switch (op) {
                case '+': return new CScriptVar(da+db);
                case '-': return new CScriptVar(da-db);
                case '*': return new CScriptVar(da*db);
                case '/': return new CScriptVar(da/db);
                case '&': return new CScriptVar(da&db);
                case '|': return new CScriptVar(da|db);
                case '^': return new CScriptVar(da^db);
                case '%': return new CScriptVar(da%db);
                case LEX_EQUAL:     return new CScriptVar(da==db);
                case LEX_NEQUAL:    return new CScriptVar(da!=db);
                case '<':     return new CScriptVar(da<db);
                case LEX_LEQUAL:    return new CScriptVar(da<=db);
                case '>':     return new CScriptVar(da>db);
                case LEX_GEQUAL:    return new CScriptVar(da>=db);
                default: throw new CScriptException("This operation not supported on the int datatype");
            }
        } else {
            // use doubles
            double da = a->getDouble();
            double db = b->getDouble();
            switch (op) {
                case '+': return new CScriptVar(da+db);
                case '-': return new CScriptVar(da-db);
                case '*': return new CScriptVar(da*db);
                case '/': return new CScriptVar(da/db);
                case LEX_EQUAL:     return new CScriptVar(da==db);
                case LEX_NEQUAL:    return new CScriptVar(da!=db);
                case '<':     return new CScriptVar(da<db);
                case LEX_LEQUAL:    return new CScriptVar(da<=db);
                case '>':     return new CScriptVar(da>db);
                case LEX_GEQUAL:    return new CScriptVar(da>=db);
                default: throw new CScriptException("This operation not supported on the double datatype");
            }
        }
    } else {
       string da = a->getString();
       string db = b->getString();
       // use strings
       switch (op) {
           case '+':           return new CScriptVar(TINYJS_TEMP_NAME, da+db);
           case LEX_EQUAL:     return new CScriptVar(da==db);
           case LEX_NEQUAL:    return new CScriptVar(da!=db);
           case '<':     return new CScriptVar(da<db);
           case LEX_LEQUAL:    return new CScriptVar(da<=db);
           case '>':     return new CScriptVar(da>db);
           case LEX_GEQUAL:    return new CScriptVar(da>=db);
           default: throw new CScriptException("This operation not supported on the string datatype");
       }
    }
    ASSERT(0);
    return 0;
}

void CScriptVar::copyValue(CScriptVar *val) {
    if (val) {
      // we *don't* copy the name
      data = val->data;
      flags = (flags & ~SCRIPTVAR_VARTYPEMASK) | (val->flags & SCRIPTVAR_VARTYPEMASK);
      // copy children
      removeAllChildren();
      CScriptVar *child = val->firstChild;
      while (child) {
          addChild(child->deepCopy());
          child = child->nextSibling;
      }
    } else {
      setVoid();
    }
}

CScriptVar *CScriptVar::deepCopy() {
    CScriptVar *newVar = new CScriptVar(name, data, flags);
    // copy children
    CScriptVar *child = firstChild;
    while (child) {
        newVar->addChild(child->deepCopy());
        child = child->nextSibling;
    }
    return newVar;
}

void CScriptVar::trace(string indentStr) {
    TRACE("%s'%s' = '%s' %s\n",
        indentStr.c_str(),
        name.c_str(),
        data.c_str(),
        getFlagsAsString().c_str());
    string indent = indentStr+" ";
    CScriptVar *child = firstChild;
    while (child) {
        child->trace(indent);
        child = child->nextSibling;
    }
}

string CScriptVar::getFlagsAsString() {
  string flagstr = "";
  if (flags&SCRIPTVAR_FUNCTION) flagstr = flagstr + "FUNCTION ";
  if (flags&SCRIPTVAR_PARAMETER) flagstr = flagstr + "PARAMETER ";
  if (flags&SCRIPTVAR_NATIVE) flagstr = flagstr + "NATIVE ";
  if (flags&SCRIPTVAR_DOUBLE) flagstr = flagstr + "DOUBLE ";
  if (flags&SCRIPTVAR_INTEGER) flagstr = flagstr + "INTEGER ";
  if (flags&SCRIPTVAR_STRING) flagstr = flagstr + "STRING ";
  return flagstr;
}

string CScriptVar::getParsableString() {
  // Numbers can just be put in directly
  if (isNumeric())
    return getString();
  if (isFunction()) {
    ostringstream funcStr;
    funcStr << "function (";
    // get list of parameters
    CScriptVar *child = firstChild;
    bool first = true;
    while (child) {
      if (child->isParameter()) {
        if (!first) { funcStr << ","; first = false; }
        funcStr << child->getName();
      }
      child = child->nextSibling;
    }
    // add function body
    funcStr << ")" << getString();
    return funcStr.str();
  }
  // if it is a string then we quote it
  if (isString())
    return getJSString(getString());
  return "nil";
}

void CScriptVar::getJSON(ostringstream &destination) {
    if (firstChild) {
      // children - handle with bracketed list
      destination << " { \n";
      CScriptVar *child = firstChild;
      while (child) {
        destination  << getJSString(child->name) << " : ";
          child->getJSON(destination);
          child = child->nextSibling;
          if (child)
            destination  << ",\n";
          else
            destination  << "\n";
      }
      destination << " }\n";
    } else {
      // no children, just write value directly
      destination << getParsableString();
    }
}


void CScriptVar::setCallback(JSCallback callback) {
    jsCallback = callback;
}

// ----------------------------------------------------------------------------------- CSCRIPT

CTinyJS::CTinyJS() {
    l = 0;
    root = symbol_base = new CScriptVar("root", "");
}

CTinyJS::~CTinyJS() {
    ASSERT(!l);
    symbol_base = 0;
    delete root;
}

void CTinyJS::trace() {
    root->trace();
}

void CTinyJS::execute(const string &code) {
    CScriptLex *oldLex = l;
    l = new CScriptLex(code);
    symbol_base = root;
    try {
        bool execute = true;
        while (l->tk) statement(execute);
    } catch (CScriptException *e) {
        ostringstream msg;
        msg << "Error " << e->text << " at " << l->getPosition(l->tokenLastEnd);
        delete l;
        l = oldLex;
        throw new CScriptException(msg.str());
    }
    delete l;
    l = oldLex;
}

string CTinyJS::evaluate(const string &code) {
    CScriptLex *oldLex = l;
    l = new CScriptLex(code);
    symbol_base = root;
    CScriptVar *v = 0;
    try {
        bool execute = true;
        v = base(execute);
    } catch (CScriptException *e) {
        ostringstream msg;
        msg << "Error " << e->text << " at " << l->getPosition(l->tokenLastEnd);
        delete l;
        l = oldLex;
        throw new CScriptException(msg.str());
    }
    delete l;
    l = oldLex;

    string result = v ? v->getString() : "";
    CLEAN(v);

    return result;
}

void CTinyJS::parseFunctionArguments(CScriptVar *funcVar) {
  l->match('(');
  while (l->tk!=')') {
      CScriptVar *funcParam = new CScriptVar(l->tkStr, "", SCRIPTVAR_PARAMETER);
      funcVar->addChildNoDup(funcParam);
      l->match(LEX_ID);
      if (l->tk!=')') l->match(',');
  }
  l->match(')');
}

void CTinyJS::addNative(const string &funcDesc, JSCallback ptr) {
    CScriptLex *oldLex = l;
    l = new CScriptLex(funcDesc);
    l->match(LEX_R_FUNCTION);
    CScriptVar *funcVar = new CScriptVar(l->tkStr, "", SCRIPTVAR_FUNCTION | SCRIPTVAR_NATIVE);
    funcVar->setCallback(ptr);
    l->match(LEX_ID);
    parseFunctionArguments(funcVar);
    delete l;
    l = oldLex;

    root->addChild(funcVar);
}

CScriptVar *CTinyJS::parseFunctionDefinition() {
  // actually parse a function...
  l->match(LEX_R_FUNCTION);
  string funcName = TINYJS_TEMP_NAME;
  /* we can have functions without names */
  if (l->tk==LEX_ID) {
    funcName = l->tkStr;
    l->match(LEX_ID);
  }
  CScriptVar *funcVar = new CScriptVar(funcName, "", SCRIPTVAR_FUNCTION);
  parseFunctionArguments(funcVar);
  int funcBegin = l->tokenStart;
  bool noexecute = false;
  block(noexecute);
  funcVar->data = l->getSubString(funcBegin);
  return funcVar;
}

CScriptVar *CTinyJS::factor(bool &execute) {
    if (l->tk=='(') {
        l->match('(');
        CScriptVar *a = base(execute);
        l->match(')');
        return a;
    }
    if (l->tk==LEX_R_TRUE) {
        l->match(LEX_R_TRUE);
        return new CScriptVar(1);
    }
    if (l->tk==LEX_R_FALSE) {
        l->match(LEX_R_FALSE);
        return new CScriptVar(0);
    }
    if (l->tk==LEX_R_NIL) {
        l->match(LEX_R_NIL);
        return new CScriptVar();
    }
    if (l->tk==LEX_ID) {
        CScriptVar *a = symbol_base->findRecursive(l->tkStr);
        if (!a) {
          /* Variable doesn't exist! */
            if (execute) {
                ostringstream msg;
                msg << "Unknown ID '" << l->tkStr << "'";
                throw new CScriptException(msg.str());
            }
            a = new CScriptVar(l->tkStr);
        }
        l->match(LEX_ID);
        while (l->tk=='(' || l->tk=='.' || l->tk=='[') {
            if (l->tk=='(') { // ------------------------------------- Function Call
                if (!a->isFunction()) {
                    string errorMsg = "Expecting '";
                    errorMsg = errorMsg + a->getName() + "' to be a function";
                    throw new CScriptException(errorMsg.c_str());
                }
                l->match('(');
                // create a new symbol table entry for execution of this function
                CScriptVar *functionRoot = new CScriptVar("__EXECUTESTACK__", "", SCRIPTVAR_FUNCTION);
                // grab in all parameters
                CScriptVar *v = a->firstChild;
                while (v) {
                    if (v->isParameter()) {
                        CScriptVar *value = base(execute);
                        if (execute) {
                            CScriptVar *param = new CScriptVar(v->getName(), "", SCRIPTVAR_PARAMETER);
                            param->copyValue(value);
                            functionRoot->addChild(param);
                        }
                        CLEAN(value);
                        if (l->tk!=')') l->match(',');
                    }
                    v = v->nextSibling;
                }
                l->match(')');
                // setup a return variable
                CScriptVar *returnVar = new CScriptVar(TINYJS_RETURN_VAR, "");
                // execute function!
                if (execute) {
                    // add the function's execute space to the symbol table so we can recurse
                    CScriptVar *old_symbol_base = symbol_base;
                    symbol_base->addChild(functionRoot);
                    functionRoot->addChild(returnVar);
                    symbol_base = functionRoot;

                    if (a->isNative()) {
                        ASSERT(a->jsCallback);
                        a->jsCallback(functionRoot);
                    } else {
                        CScriptLex *oldLex = l;
                        fflush(stdout);
                        l = new CScriptLex(a->getString());
                        block(execute);
                        // because return will probably have called this, and set execute to false
                        execute = true;
                        delete l;
                        l = oldLex;
                    }
                    symbol_base = old_symbol_base;
                    symbol_base->removeChild(functionRoot);
                    functionRoot->removeChild(returnVar);
                }
                delete functionRoot;
                a = returnVar;
            } else if (l->tk == '.') { // ------------------------------------- Record Access
                l->match('.');
                CScriptVar *child = a->findRecursive(l->tkStr);
                if (!child) {
                    // TODO: Error here if this doesn't exist??
                    child = new CScriptVar(l->tkStr, "");
                    a->addChild(child);
                }
                l->match(LEX_ID);
                a = child;
            } else if (l->tk == '[') { // ------------------------------------- Array Access
                l->match('[');
                CScriptVar *index = expression(execute);
                l->match(']');
                CScriptVar *child = a->findRecursive(index->getString());
                if (!child) {
                    child = new CScriptVar(index->getString(), "");
                    a->addChild(child);
                }
                CLEAN(index);

                a = child;
            } else ASSERT(0);
        }
        return a;
    }
    if (l->tk==LEX_INT || l->tk==LEX_FLOAT) {
        CScriptVar *a = new CScriptVar(
            TINYJS_TEMP_NAME, l->tkStr,
            ((l->tk==LEX_INT)?SCRIPTVAR_INTEGER:SCRIPTVAR_DOUBLE));
        l->match(l->tk);
        return a;
    }
    if (l->tk==LEX_STR) {
        CScriptVar *a = new CScriptVar(TINYJS_TEMP_NAME, l->tkStr, 0);
        l->match(LEX_STR);
        return a;
    }
    if (l->tk=='{') {
        CScriptVar *contents = new CScriptVar();
        /* JSON-style object definition */
        l->match('{');
        while (l->tk != '}') {
          string id = l->tkStr;
          // we only allow strings or IDs on the left hand side of an initialisation
          if (l->tk==LEX_STR) l->match(LEX_STR);
          else l->match(LEX_ID);
          l->match(':');
          CScriptVar *a = base(execute);
          contents->addNamedChild(id, a);
          // no need to clean here, as it will definitely be used
          if (l->tk != '}') l->match(',');
        }

        l->match('}');
        return contents;
    }
    if (l->tk=='[') {
        CScriptVar *contents = new CScriptVar();
        /* JSON-style array */
        l->match('[');
        int idx = 0;
        while (l->tk != ']') {
          char idx_str[16]; // big enough for 2^32
          sprintf(idx_str,"%d",idx);

          CScriptVar *a = base(execute);
          contents->addNamedChild(idx_str, a);
          // no need to clean here, as it will definitely be used
          if (l->tk != ']') l->match(',');
          idx++;
        }
        l->match(']');
        return contents;
    }
    if (l->tk==LEX_R_FUNCTION) {
        CScriptVar *funcVar = parseFunctionDefinition();
        if (funcVar->name != TINYJS_TEMP_NAME)
          TRACE("Functions not defined at statement-level are not meant to have a name");
        return funcVar;
    }
    // Nothing we can do here... just hope it's the end...
    l->match(LEX_EOF);
    return new CScriptVar();
}

CScriptVar *CTinyJS::unary(bool &execute) {
    CScriptVar *a;
    if (l->tk=='!') {
        l->match('!'); // binary not
        a = factor(execute);
        if (execute) {
            CScriptVar zero(0);
            CScriptVar *res = a->mathsOp(&zero, LEX_EQUAL);
            CLEAN(a);
            a = res;
        }
    } else
        a = factor(execute);
    return a;
}

CScriptVar *CTinyJS::term(bool &execute) {
    CScriptVar *a = unary(execute);
    while (l->tk=='*' || l->tk=='/') {
        int op = l->tk;
        l->match(l->tk);
        CScriptVar *b = unary(execute);
        if (execute) {
            CScriptVar *res = a->mathsOp(b, op);
            CLEAN(a);
            CLEAN(b);
            a = res;
        } else {
            CLEAN(b);
        }
    }
    return a;
}

CScriptVar *CTinyJS::expression(bool &execute) {
    bool negate = false;
    if (l->tk=='-') {
        l->match('-');
        negate = true;
    }
    CScriptVar *a = term(execute);
    if (negate) {
        CScriptVar *zero = new CScriptVar(0);
        CScriptVar *res = zero->mathsOp(a, '-');
        CLEAN(a);
        CLEAN(zero);
        a = res;
    }

    while (l->tk=='+' || l->tk=='-' ||
        l->tk==LEX_PLUSPLUS || l->tk==LEX_MINUSMINUS) {
        int op = l->tk;
        l->match(l->tk);
        if (op==LEX_PLUSPLUS || op==LEX_MINUSMINUS) {
            if (execute) {
                CScriptVar one(1);
                CScriptVar *res = a->mathsOp(&one, op==LEX_PLUSPLUS ? '+' : '-');
                a->copyValue(res);
                CLEAN(res);
            }
        } else {
            CScriptVar *b = term(execute);
            if (execute) {
                CScriptVar *res = a->mathsOp(b, op);
                CLEAN(a);
                CLEAN(b);
                a = res;
            } else {
                CLEAN(b);
            }
        }
    }
    return a;
}

CScriptVar *CTinyJS::condition(bool &execute) {
    CScriptVar *a = expression(execute);
    CScriptVar *b;
    while (l->tk==LEX_EQUAL || l->tk==LEX_NEQUAL ||
           l->tk==LEX_LEQUAL || l->tk==LEX_GEQUAL ||
           l->tk=='<' || l->tk=='>') {
        int op = l->tk;
        l->match(l->tk);
        b = expression(execute);
        if (execute) {
            CScriptVar *res = a->mathsOp(b, op);
            CLEAN(a);
            CLEAN(b);
            a = res;
        } else {
            CLEAN(b);
        }
    }
    return a;
}

CScriptVar *CTinyJS::logic(bool &execute) {
    CScriptVar *a = condition(execute);
    CScriptVar *b;
    while (l->tk=='&' || l->tk=='|' || l->tk=='^' || l->tk==LEX_ANDAND || l->tk==LEX_OROR) {
        bool noexecute = false;
        int op = l->tk;
        l->match(l->tk);
        bool shortCircuit = false;
        bool boolean = false;
        // if we have short-circuit ops, then if we know the outcome
        // we don't bother to execute the other op. Even if not
        // we need to tell mathsOp it's an & or |
        if (op==LEX_ANDAND) {
            op = '&';
            shortCircuit = !a->getBool();
            boolean = true;
        } else if (op==LEX_OROR) {
            op = '|';
            shortCircuit = a->getBool();
            boolean = true;
        }
        b = condition(shortCircuit ? noexecute : execute);
        if (execute && !shortCircuit) {
            if (boolean) {
              CScriptVar *newa = new CScriptVar(a->getBool());
              CScriptVar *newb = new CScriptVar(b->getBool());
              CLEAN(a);
              CLEAN(b);
              a = newa;
              b = newb;
            }
            CScriptVar *res = a->mathsOp(b, op);
            CLEAN(a);
            CLEAN(b);
            a = res;
        } else {
            CLEAN(b);
        }
    }
    return a;
}

CScriptVar *CTinyJS::base(bool &execute) {
    CScriptVar *lhs = logic(execute);
    if (l->tk=='=' || l->tk==LEX_PLUSEQUAL || l->tk==LEX_MINUSEQUAL) {
        int op = l->tk;
        l->match(l->tk);
        CScriptVar *rhs = base(execute);
        if (execute) {
            if (op=='=') {
                lhs->copyValue(rhs);
            } else if (op==LEX_PLUSEQUAL) {
                CScriptVar *res = lhs->mathsOp(rhs, '+');
                lhs->copyValue(res);
                CLEAN(res);
            } else if (op==LEX_MINUSEQUAL) {
                CScriptVar *res = lhs->mathsOp(rhs, '-');
                lhs->copyValue(res);
                CLEAN(res);
            } else ASSERT(0);
        }
        CLEAN(rhs);
    }
    return lhs;
}

void CTinyJS::block(bool &execute) {
    // TODO: fast skip of blocks
    l->match('{');
    if (execute) {
      while (l->tk && l->tk!='}')
        statement(execute);
      l->match('}');
    } else {
      int brackets = 1;
      while (l->tk && brackets) {
        if (l->tk == '{') brackets++;
        if (l->tk == '}') brackets--;
        l->match(l->tk);
      }
    }

}

void CTinyJS::statement(bool &execute) {
    if (l->tk==LEX_ID ||
        l->tk==LEX_INT ||
        l->tk==LEX_FLOAT ||
        l->tk==LEX_STR ||
        l->tk=='-') {
        /* Execute a simple statement that only contains basic arithmetic... */
        CLEAN(base(execute));
        l->match(';');
    } else if (l->tk=='{') {
        /* A block of code */
        block(execute);
    } else if (l->tk==';') {
        /* Empty statement - to allow things like ;;; */
        l->match(';');
    } else if (l->tk==LEX_R_VAR) {
        /* variable creation. TODO - we need a better way of parsing the left
         * hand side. Maybe just have a flag called can_create_var that we
         * set and then we parse as if we're doing a normal equals.*/
        l->match(LEX_R_VAR);
        CScriptVar *a = 0;
        if (execute)
          a = symbol_base->findChildOrCreate(l->tkStr);
        l->match(LEX_ID);
        // now do stuff defined with dots
        while (l->tk == '.') {
            l->match('.');
            if (execute) {
                CScriptVar *lastA = a;
                a = lastA->findChildOrCreate(l->tkStr);
            }
            l->match(LEX_ID);
        }
        // sort out initialiser
        if (l->tk == '=') {
            l->match('=');
            CScriptVar *var = base(execute);
            if (execute)
                a->copyValue(var);
            CLEAN(var);
        }
        l->match(';');
    } else if (l->tk==LEX_R_IF) {
        l->match(LEX_R_IF);
        l->match('(');
        CScriptVar *var = base(execute);
        l->match(')');
        bool cond = execute && var->getBool();
        CLEAN(var);
        bool noexecute = false; // because we need to be abl;e to write to it
        statement(cond ? execute : noexecute);
        if (l->tk==LEX_R_ELSE) {
            l->match(LEX_R_ELSE);
            statement(cond ? noexecute : execute);
        }
    } else if (l->tk==LEX_R_WHILE) {
        // We do repetition by pulling out the string representing our statement
        // there's definitely some opportunity for optimisation here
        l->match(LEX_R_WHILE);
        l->match('(');
        int whileCondStart = l->tokenStart;
        bool noexecute = false;
        CScriptVar *cond = base(execute);
        bool loopCond = execute && cond->getBool();
        CLEAN(cond);
        CScriptLex *whileCond = l->getSubLex(whileCondStart);
        l->match(')');
        int whileBodyStart = l->tokenStart;
        statement(loopCond ? execute : noexecute);
        CScriptLex *whileBody = l->getSubLex(whileBodyStart);
        CScriptLex *oldLex = l;
        int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
        while (loopCond && loopCount-->0) {
            whileCond->reset();
            l = whileCond;
            cond = base(execute);
            loopCond = execute && cond->getBool();
            CLEAN(cond);
            if (loopCond) {
                whileBody->reset();
                l = whileBody;
                statement(execute);
            }
        }
        l = oldLex;
        delete whileCond;
        delete whileBody;

        if (loopCount<=0) {
            root->trace();
            TRACE("WHILE Loop exceeded %d iterations at %s\n", TINYJS_LOOP_MAX_ITERATIONS, l->getPosition(l->tokenLastEnd).c_str());
            throw new CScriptException("LOOP_ERROR");
        }
    } else if (l->tk==LEX_R_FOR) {
        l->match(LEX_R_FOR);
        l->match('(');
        statement(execute); // initialisation
        //l->match(';');
        int forCondStart = l->tokenStart;
        bool noexecute = false;
        CScriptVar *cond = base(execute); // condition
        bool loopCond = execute && cond->getBool();
        CLEAN(cond);
        CScriptLex *forCond = l->getSubLex(forCondStart);
        l->match(';');
        int forIterStart = l->tokenStart;
        base(noexecute); // iterator
        CScriptLex *forIter = l->getSubLex(forIterStart);
        l->match(')');
        int forBodyStart = l->tokenStart;
        statement(loopCond ? execute : noexecute);
        CScriptLex *forBody = l->getSubLex(forBodyStart);
        CScriptLex *oldLex = l;
        if (loopCond) {
            forIter->reset();
            l = forIter;
            base(execute);
        }
        int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
        while (execute && loopCond && loopCount-->0) {
            forCond->reset();
            l = forCond;
            cond = base(execute);
            loopCond = cond->getBool();
            CLEAN(cond);
            if (execute && loopCond) {
                forBody->reset();
                l = forBody;
                statement(execute);
            }
            if (execute && loopCond) {
                forIter->reset();
                l = forIter;
                base(execute);
            }
        }
        l = oldLex;
        delete forCond;
        delete forIter;
        delete forBody;
        if (loopCount<=0) {
            root->trace();
            TRACE("FOR Loop exceeded %d iterations at %s\n", TINYJS_LOOP_MAX_ITERATIONS, l->getPosition(l->tokenLastEnd).c_str());
            throw new CScriptException("LOOP_ERROR");
        }
    } else if (l->tk==LEX_R_RETURN) {
        l->match(LEX_R_RETURN);
        CScriptVar *result = 0;
        if (l->tk != ';')
          result = base(execute);
        if (execute) {
            CScriptVar *resultVar = symbol_base->findChild(TINYJS_RETURN_VAR);
            ASSERT(resultVar);
            resultVar->copyValue(result);
            execute = false;
        }
        CLEAN(result);
        l->match(';');
    } else if (l->tk==LEX_R_FUNCTION) {
        CScriptVar *funcVar = parseFunctionDefinition();
        if (execute) {
          if (funcVar->name == TINYJS_TEMP_NAME)
            TRACE("Functions defined at statement-level are meant to have a name");
          else
            symbol_base->addChildNoDup(funcVar);
        } else
          CLEAN(funcVar);
    } else l->match(LEX_EOF);
}

/// Get the value of the given variable, or return 0
const string *CTinyJS::getVariable(const string &path) {
    // traverse path
    size_t prevIdx = 0;
    size_t thisIdx = path.find('.');
    if (thisIdx == string::npos) thisIdx = path.length();
    CScriptVar *var = root;
    while (var && prevIdx<path.length()) {
        string el = path.substr(prevIdx, thisIdx-prevIdx);
        var = var->findChild(el);
        prevIdx = thisIdx+1;
        thisIdx = path.find('.', prevIdx);
        if (thisIdx == string::npos) thisIdx = path.length();
    }
    // return result
    if (var)
        return &var->getString();
    else
        return 0;
}
