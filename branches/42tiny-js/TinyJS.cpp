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

/* Version 0.1  :	(gw) First published on Google Code
	Version 0.11 :	Making sure the 'root' variable never changes
						'symbol_base' added for the current base of the sybmbol table
	Version 0.12 :	Added findChildOrCreate, changed string passing to use references
						Fixed broken string encoding in getJSString()
						Removed getInitCode and added getJSON instead
						Added nil
						Added rough JSON parsing
						Improved example app
	Version 0.13 :	Added tokenEnd/tokenLastEnd to lexer to avoid parsing whitespace
						Ability to define functions without names
						Can now do "var mine = function(a,b) { ... };"
						Slightly better 'trace' function
						Added findChildOrCreateByPath function
						Added simple test suite
						Added skipping of blocks when not executing
	Version 0.14 : Added parsing of more number types
						Added parsing of string defined with '
						Changed nil to null as per spec, added 'undefined'
						Now set variables with the correct scope, and treat unknown
						as 'undefined' rather than failing
						Added proper (I hope) handling of null and undefined
						Added === check
	Version 0.15 :	Fix for possible memory leaks
	Version 0.16 :	Removal of un-needed findRecursive calls
						symbol_base removed and replaced with 'scopes' stack
						Added reference counting a proper tree structure
						(Allowing pass by reference)
						Allowed JSON output to output IDs, not strings
						Added get/set for array indices
						Changed Callbacks to include user data pointer
						Added some support for objects
						Added more Java-esque builtin functions
	Version 0.17 :	Now we don't deepCopy the parent object of the class
						Added JSON.stringify and eval()
						Nicer JSON indenting
						Fixed function output in JSON
						Added evaluateComplex
						Fixed some reentrancy issues with evaluate/execute
	Version 0.18 :	Fixed some issues with code being executed when it shouldn't
	Version 0.19 :	Added array.length
						Changed '__parent' to 'prototype' to bring it more in line with javascript
	Version 0.20 :	Added '%' operator
	Version 0.21 :	Added array type
						String.length() no more - now String.length
						Added extra constructors to reduce confusion
						Fixed checks against undefined

						
	Forked to 42tiny-js by Armin diedering
	R3					Added boolean datatype (in string operators its shown as 'true'/'false' but in math as '1'/'0'
						Added '~' operator
						Added bit-shift operators '<<' '>>'
						Added assignment operators '<<=' '>>=' '|=' '&=' '^='
						Addet comma operator like this 'var i=1,j,k=9;' or 'for(i=0,j=12; i<10; i++, j++)' works
						Added Conditional operator ( ? : )
						Added automatic cast from doubles to integer e.G. by logic and binary operators
						Added pre-increment/-decrement like '++i'/'--i' operator
						Fixed post-increment/decrement now returns the previous value 
						Fixed throws an Error when invalid using of post/pre-increment/decrement (like this 5++ works no more) 
						Fixed memoryleak (unref arrayClass at deconstructor of JS CTinyJS)
						Fixed unary operator handling (like this '-~-5' works now)
						Fixed operator prority order -> ','
												-> '=' '+=' '-=' '<<=' '>>=' '&=' '^=' '|='
												-> '? :' -> '||' -> '&&' -> '|' -> '^' -> '&' 
												-> ['==' '===' '!=' '!=='] 
												-> [ '<' '<=' '=>' '>'] 
												-> ['<<' '>>'] -> [ '*' '/' '%']
												-> ['!' '~' '-' '++' '--']
					Added do-while-loop ( do .... while(..); )
					Added break and continue statements for loops
	R4				Changed "owned"-member of CScriptVarLink from bool to a pointer of CScriptVarowned
					Added some Visual Studio Preprocessor stuff
					Added now alowed stuff like this (function (v) {print(v);})(12);
					Remove unneeded and wrong deepCopy by assignment operator '='


	NOTE: This doesn't support constructors for objects
				Recursive loops of data such as a.foo = a; fail to be garbage collected
				'length' cannot be set
				There is no ternary operator implemented yet
 */


#ifdef _MSC_VER
#	include "targetver.h"
#	include <afx.h>
#endif
#include <assert.h>
#include <string>
#include <string.h>
#include <sstream>
#include <cstdlib>
#include <stdio.h>
#include "TinyJS.h"

#ifdef _DEBUG
#	ifdef _MSC_VER
#		define new DEBUG_NEW
#	else
#		define DEBUG_MEMORY 1
#	endif
#endif


#ifndef ASSERT
#	define ASSERT(X) assert(X)
#endif
/* Frees the given link IF it isn't owned by anything else */
#define CLEAN(x) do{ CScriptVarLink *__v = x; if (__v && !__v->owned) { delete __v; } }while(0)
/* Create a LINK to point to VAR and free the old link.
 * BUT this is more clever - it tries to keep the old link if it's not owned to save allocations */
#define CREATE_LINK(LINK, VAR) { if (!LINK || LINK->owned) LINK = new CScriptVarLink(VAR); else LINK->replaceWith(VAR); }


using namespace std;

#ifdef __GNUC__
#define vsprintf_s vsnprintf
#define sprintf_s snprintf
#define _strdup strdup
#endif

// ----------------------------------------------------------------------------------- Memory Debug

//#define DEBUG_MEMORY 1

#if DEBUG_MEMORY

vector<CScriptVar*> allocatedVars;
vector<CScriptVarLink*> allocatedLinks;

void mark_allocated(CScriptVar *v) {
	allocatedVars.push_back(v);
}

void mark_deallocated(CScriptVar *v) {
	for (size_t i=0;i<allocatedVars.size();i++) {
		if (allocatedVars[i] == v) {
			allocatedVars.erase(allocatedVars.begin()+i);
			break;
		}
	}
}

void mark_allocated(CScriptVarLink *v) {
	allocatedLinks.push_back(v);
}

void mark_deallocated(CScriptVarLink *v) {
	for (size_t i=0;i<allocatedLinks.size();i++) {
		if (allocatedLinks[i] == v) {
			allocatedLinks.erase(allocatedLinks.begin()+i);
			break;
		}
	}
}

void show_allocated() {
	for (size_t i=0;i<allocatedVars.size();i++) {
		printf("ALLOCATED, %d refs\n", allocatedVars[i]->getRefs());
		allocatedVars[i]->trace("  ");
	}
	for (size_t i=0;i<allocatedLinks.size();i++) {
		printf("ALLOCATED LINK %s, allocated[%d] to \n", allocatedLinks[i]->name.c_str(), allocatedLinks[i]->var->getRefs());
		allocatedLinks[i]->var->trace("  ");
	}
	allocatedVars.clear();
	allocatedLinks.clear();
}
#endif

// ----------------------------------------------------------------------------------- Utils
bool isWhitespace(char ch) {
	return (ch==' ') || (ch=='\t') || (ch=='\n') || (ch=='\r');
}

bool isNumeric(char ch) {
	return (ch>='0') && (ch<='9');
}
bool isNumber(const string &str) {
	for (size_t i=0;i<str.size();i++)
		if (!isNumeric(str[i])) return false;
	return true;
}
bool isHexadecimal(char ch) {
	return ((ch>='0') && (ch<='9')) ||
			 ((ch>='a') && (ch<='f')) ||
			 ((ch>='A') && (ch<='F'));
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

/** Is the string alphanumeric */
bool isAlphaNum(const std::string &str) {
	if (str.size()==0) return true;
	if (!isAlpha(str[0])) return false;
	for (size_t i=0;i<str.size();i++)
		if (!(isAlpha(str[i]) || isNumeric(str[i])))
			return false;
	return true;
}

// ----------------------------------------------------------------------------------- CSCRIPTEXCEPTION

CScriptException::CScriptException(const string &exceptionText) {
	text = exceptionText;
}

// ----------------------------------------------------------------------------------- CSCRIPTLEX

CScriptLex::CScriptLex(const string &input) {
	data = _strdup(input.c_str());
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
			 << " at " << getPosition(tokenStart) << " in '" << data << "'";
			throw new CScriptException(errorString.str());
	}
	getNextToken();
}

string CScriptLex::getTokenStr(int token) {
	if (token>32 && token<128) {
			char buf[4] = "' '";
			buf[1] = (char)token;
			return buf;
	}
	switch (token) {
		case LEX_EOF : return "EOF";
		case LEX_ID : return "ID";
		case LEX_INT : return "INT";
		case LEX_FLOAT : return "FLOAT";
		case LEX_STR : return "STRING";
		case LEX_EQUAL : return "==";
		case LEX_TYPEEQUAL : return "===";
		case LEX_NEQUAL : return "!=";
		case LEX_NTYPEEQUAL : return "!==";
		case LEX_LEQUAL : return "<=";
		case LEX_LSHIFT : return "<<";
		case LEX_LSHIFTEQUAL : return "<<=";
		case LEX_GEQUAL : return ">=";
		case LEX_RSHIFT : return ">>";
		case LEX_RSHIFTEQUAL : return ">>=";
		case LEX_PLUSEQUAL : return "+=";
		case LEX_MINUSEQUAL : return "-=";
		case LEX_PLUSPLUS : return "++";
		case LEX_MINUSMINUS : return "--";
		case LEX_ANDEQUAL : return "&=";
		case LEX_ANDAND : return "&&";
		case LEX_OREQUAL : return "|=";
		case LEX_OROR : return "||";
		case LEX_XOREQUAL : return "^=";
		// reserved words
		case LEX_R_IF : return "if";
		case LEX_R_ELSE : return "else";
		case LEX_R_DO : return "do";
		case LEX_R_WHILE : return "while";
		case LEX_R_FOR : return "for";
		case LEX_R_BREAK : return "break";
		case LEX_R_CONTINUE : return "continue";
		case LEX_R_FUNCTION : return "function";
		case LEX_R_RETURN : return "return";
		case LEX_R_VAR : return "var";
		case LEX_R_TRUE : return "true";
		case LEX_R_FALSE : return "false";
		case LEX_R_NULL : return "null";
		case LEX_R_UNDEFINED : return "undefined";
		case LEX_R_NEW : return "new";
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
		else if (tkStr=="do") tk = LEX_R_DO;
		else if (tkStr=="while") tk = LEX_R_WHILE;
		else if (tkStr=="for") tk = LEX_R_FOR;
		else if (tkStr=="break") tk = LEX_R_BREAK;
		else if (tkStr=="continue") tk = LEX_R_CONTINUE;
		else if (tkStr=="function") tk = LEX_R_FUNCTION;
		else if (tkStr=="return") tk = LEX_R_RETURN;
		else if (tkStr=="var") tk = LEX_R_VAR;
		else if (tkStr=="true") tk = LEX_R_TRUE;
		else if (tkStr=="false") tk = LEX_R_FALSE;
		else if (tkStr=="null") tk = LEX_R_NULL;
		else if (tkStr=="undefined") tk = LEX_R_UNDEFINED;
		else if (tkStr=="new") tk = LEX_R_NEW;
	} else if (isNumeric(currCh)) { // Numbers
		bool isHex = false;
		if (currCh=='0') { tkStr += currCh; getNextCh(); }
		if (currCh=='x') {
			isHex = true;
			tkStr += currCh; getNextCh();
		}
		tk = LEX_INT;
		while (isNumeric(currCh) || (isHex && isHexadecimal(currCh))) {
			tkStr += currCh;
			getNextCh();
		}
		if (!isHex && currCh=='.') {
			tk = LEX_FLOAT;
			tkStr += '.';
			getNextCh();
			while (isNumeric(currCh)) {
				tkStr += currCh;
				getNextCh();
			}
		}
		// do fancy e-style floating point
		if (!isHex && currCh=='e') {
			tk = LEX_FLOAT;
			tkStr += currCh; getNextCh();
			if (currCh=='-') { tkStr += currCh; getNextCh(); }
			while (isNumeric(currCh)) {
				tkStr += currCh; getNextCh();
			}
		}
	} else if (currCh=='"') {
		// strings...
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
	} else if (currCh=='\'') {
		// strings again...
		getNextCh();
		while (currCh && currCh!='\'') {
			if (currCh == '\\') {
				getNextCh();
				switch (currCh) {
					case 'n' : tkStr += '\n'; break;
					case '\'' : tkStr += '\''; break;
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
		if (tk=='=' && currCh=='=') { // ==
			tk = LEX_EQUAL;
			getNextCh();
			if (currCh=='=') { // ===
				tk = LEX_TYPEEQUAL;
				getNextCh();
			}
		} else if (tk=='!' && currCh=='=') { // !=
			tk = LEX_NEQUAL;
			getNextCh();
			if (currCh=='=') { // !==
				tk = LEX_NTYPEEQUAL;
				getNextCh();
			}
		} else if (tk=='<' && currCh=='=') {
			tk = LEX_LEQUAL;
			getNextCh();
		} else if (tk=='<' && currCh=='<') {
			tk = LEX_LSHIFT;
			getNextCh();
			if (currCh=='=') { // <<=
				tk = LEX_LSHIFTEQUAL;
				getNextCh();
			}
		} else if (tk=='>' && currCh=='=') {
			tk = LEX_GEQUAL;
			getNextCh();
		} else if (tk=='>' && currCh=='>') {
			tk = LEX_RSHIFT;
			getNextCh();
			if (currCh=='=') { // <<=
				tk = LEX_RSHIFTEQUAL;
				getNextCh();
			}
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
		} else if (tk=='&' && currCh=='=') {
			tk = LEX_ANDEQUAL;
			getNextCh();
		} else if (tk=='&' && currCh=='&') {
			tk = LEX_ANDAND;
			getNextCh();
		} else if (tk=='|' && currCh=='=') {
			tk = LEX_OREQUAL;
			getNextCh();
		} else if (tk=='|' && currCh=='|') {
			tk = LEX_OROR;
			getNextCh();
		} else if (tk=='^' && currCh=='=') {
			tk = LEX_XOREQUAL;
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
			/* save a memory alloc by using our data array to create the substring */
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
	if (pos<0) pos=tokenLastEnd;
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

// ----------------------------------------------------------------------------------- CSCRIPTVARLINK

CScriptVarLink::CScriptVarLink(CScriptVar *var, const std::string &name) {
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	this->name = name;
	this->nextSibling = 0;
	this->prevSibling = 0;
	this->var = var->ref();
	this->owned = NULL;
}

CScriptVarLink::CScriptVarLink(const CScriptVarLink &link) {
	// Copy constructor
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	this->name = link.name;
	this->nextSibling = 0;
	this->prevSibling = 0;
	this->var = link.var->ref();
	this->owned = NULL;
}

CScriptVarLink::~CScriptVarLink() {
#if DEBUG_MEMORY
	mark_deallocated(this);
#endif
	var->unref();
}

void CScriptVarLink::replaceWith(CScriptVar *newVar) {
	CScriptVar *oldVar = var;
	var = newVar->ref();
	oldVar->unref();
}

void CScriptVarLink::replaceWith(CScriptVarLink *newVar) {
	if (newVar)
		replaceWith(newVar->var);
	else
		replaceWith(new CScriptVar());
}

// ----------------------------------------------------------------------------------- CSCRIPTVAR

CScriptVar::CScriptVar() {
	refs = 0;
	__proto__ = NULL;
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	init();
	flags = SCRIPTVAR_UNDEFINED;
}

CScriptVar::CScriptVar(const string &str) {
	refs = 0;
	__proto__ = NULL;
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	init();
	flags = SCRIPTVAR_STRING;
	data = str;
}


CScriptVar::CScriptVar(const string &varData, int varFlags) {
	refs = 0;
	__proto__ = NULL;
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	init();
	flags = varFlags;
	if (varFlags & SCRIPTVAR_INTEGER) {
		intData = strtol(varData.c_str(),0,0);
	} else if (varFlags & SCRIPTVAR_DOUBLE) {
		doubleData = strtod(varData.c_str(),0);
	} else
		data = varData;
}

CScriptVar::CScriptVar(double val) {
	refs = 0;
	__proto__ = NULL;
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	init();
	setDouble(val);
}

CScriptVar::CScriptVar(int val) {
	refs = 0;
	__proto__ = NULL;
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	init();
	setInt(val);
}

CScriptVar::CScriptVar(bool val) {
	refs = 0;
	__proto__ = NULL;
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	init();
	setBool(val);
}

CScriptVar::~CScriptVar(void) {
#if DEBUG_MEMORY
	mark_deallocated(this);
#endif
	removeAllChildren();
	if(flags & SCRIPTVAR_NATIVE_MFNC)
		delete jsCallbackClass;
}

void CScriptVar::init() {
////    firstChild = 0;
////    lastChild = 0;
	flags = 0;
	jsCallback = 0;
	jsCallbackUserData = 0;
	data = TINYJS_BLANK_DATA;
	intData = 0;
	doubleData = 0;
}

CScriptVar *CScriptVar::getReturnVar() {
	return getParameter(TINYJS_RETURN_VAR);
}

void CScriptVar::setReturnVar(CScriptVar *var) {
	findChildOrCreate(TINYJS_RETURN_VAR)->replaceWith(var);
}


CScriptVar *CScriptVar::getParameter(const std::string &name) {
	return findChildOrCreate(name)->var;
}


CScriptVarLink *CScriptVar::findChild(const string &childName) {
	SCRIPTVAR_CHILDS::iterator it = Childs.find(childName);
	if(it != Childs.end())
		return it->second;
////
/*
	CScriptVarLink *v = firstChild;
	while (v) {
			if (v->name.compare(childName)==0)
				return v;
			v = v->nextSibling;
	}
*/
////
	return NULL;
}

CScriptVarLink *CScriptVar::findChildOrCreate(const string &childName, int varFlags) {
	CScriptVarLink *l = findChild(childName);
	if (l) return l;
	return addChild(childName, new CScriptVar(TINYJS_BLANK_DATA, varFlags));
////
/*

	return addChild(childName, new CScriptVar(TINYJS_BLANK_DATA, varFlags));
*/
////
}

CScriptVarLink *CScriptVar::findChildOrCreateByPath(const std::string &path) {
  size_t p = path.find('.');
  if (p == string::npos)
	return findChildOrCreate(path);

  return findChildOrCreate(path.substr(0,p), SCRIPTVAR_OBJECT)->var->
				findChildOrCreateByPath(path.substr(p+1));
}

CScriptVarLink *CScriptVar::addChild(const std::string &childName, CScriptVar *child) {
  if (isUndefined()) {
	flags = SCRIPTVAR_OBJECT;
  }
	// if no child supplied, create one
	if (!child)
		child = new CScriptVar();
#ifdef _DEBUG
	if(findChild(childName))
		ASSERT(0); // addCild - the child exists 
#endif
	CScriptVarLink *link = new CScriptVarLink(child, childName);
	link->owned = this;
	return Childs[childName] = link;
////
/*
	if (lastChild) {
			lastChild->nextSibling = link;
			link->prevSibling = lastChild;
			lastChild = link;
	} else {
			firstChild = link;
			lastChild = link;
	}
	return link;
*/
////
}
CScriptVarLink *CScriptVar::addChildNoDup(const std::string &childName, CScriptVar *child) {
	// if no child supplied, create one
	if (!child)
		child = new CScriptVar();

	CScriptVarLink *v = findChild(childName);
	if (v) {
			v->replaceWith(child);
	} else {
			CScriptVarLink *link = new CScriptVarLink(child, childName);
			link->owned = this;
		v = Childs[childName] = link;
////        v = addChild(childName, child);
	}

	return v;
}

void CScriptVar::removeChild(CScriptVar *child) {
	for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it != Childs.end(); ++it) {
		if(it->second->var == child) {
			removeLink(it->second);
			break;
		}
	}
	ASSERT(0);// removeChild - the child is not atached to this var 
////
/*
	CScriptVarLink *link = firstChild;
	while (link) {
			if (link->var == child)
				break;
			link = link->nextSibling;
	}
	ASSERT(link);
	removeLink(link);
*/
////
}

void CScriptVar::removeLink(CScriptVarLink *link) {
	if (!link) return;
#ifdef _DEBUG
	if(findChild(link->name) != link)
		ASSERT(0); // removeLink - the link is not atached to this var 
#endif
	Childs.erase(link->name);
////
/*
	if (link->nextSibling)
		link->nextSibling->prevSibling = link->prevSibling;
	if (link->prevSibling)
		link->prevSibling->nextSibling = link->nextSibling;
	if (lastChild == link)
			lastChild = link->prevSibling;
	if (firstChild == link)
			firstChild = link->nextSibling;
*/
////
	delete link;
}

void CScriptVar::removeAllChildren() {
	SCRIPTVAR_CHILDS::iterator it;
	while((it = Childs.begin()) != Childs.end()) {
		delete it->second;
		Childs.erase(it);
	}
////
/*
	CScriptVarLink *c = firstChild;
	while (c) {
			CScriptVarLink *t = c->nextSibling;
			delete c;
			c = t;
	}
	firstChild = 0;
	lastChild = 0;
*/
////
}

CScriptVar *CScriptVar::getArrayIndex(int idx) {
	char sIdx[64];
	sprintf_s(sIdx, sizeof(sIdx), "%d", idx);
	CScriptVarLink *link = findChild(sIdx);
	if (link) return link->var;
	else return new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_NULL); // undefined
}

void CScriptVar::setArrayIndex(int idx, CScriptVar *value) {
	char sIdx[64];
	sprintf_s(sIdx, sizeof(sIdx), "%d", idx);
	CScriptVarLink *link = findChild(sIdx);

	if (link) {
		if (value->isUndefined())
			removeLink(link);
		else
			link->replaceWith(value);
	} else {
		if (!value->isUndefined())
			addChild(sIdx, value);
	}
}

int CScriptVar::getArrayLength() {
	int highest = -1;
	if (!isArray()) return 0;

	for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it != Childs.end(); ++it) {
		if (isNumber(it->first)) {
			int val = atoi(it->first.c_str());
			if (val > highest) highest = val;
		}
	}
////
/*
	CScriptVarLink *link = firstChild;
	while (link) {
		if (isNumber(link->name)) {
			int val = atoi(link->name.c_str());
			if (val > highest) highest = val;
		}
		link = link->nextSibling;
	}
*/
////
	return highest+1;
}
//// moved to TinyJS.h
/*
int CScriptVar::getChildren() {
	int n = 0;
	CScriptVarLink *link = firstChild;
	while (link) {
		n++;
		link = link->nextSibling;
	}
	return n;
}
*/
////

int CScriptVar::getInt() {
	/* strtol understands about hex and octal */
	if (isInt()) return intData;
//  if (isNull()) return 0;
//  if (isUndefined()) return 0;
	if (isDouble()) return (int)doubleData;
	return 0;
}

bool CScriptVar::getBool() {
	if (isInt()) return intData!=0;
	if (isNull()) return 0;
	if (isUndefined()) return 0;
	if (isDouble()) return doubleData!=0;
	if (isString()) return data.length()!=0;
	return 0;
}

double CScriptVar::getDouble() {
	if (isDouble()) return doubleData;
	if (isInt() || isBool()) intData;
	if (isNull()) return 0;
	if (isUndefined()) return 0;
	return 0; /* or NaN? */
}

const string &CScriptVar::getString() {
	/* Because we can't return a string that is generated on demand.
	 * I should really just use char* :) */
	static string s_null = "null";
	static string s_undefined = "undefined";
	static string s_true = "true";
	static string s_false = "false";
	if (isBool()) return getInt() ? s_true : s_false;
	if (isInt()) {
		char buffer[32];
		sprintf_s(buffer, sizeof(buffer), "%ld", intData);
		data = buffer;
		return data;
	}
	if (isDouble()) {
		char buffer[32];
		sprintf_s(buffer, sizeof(buffer), "%lf", doubleData);
		data = buffer;
		return data;
	}
	if (isNull()) return s_null;
	if (isUndefined()) return s_undefined;
	// are we just a string here?
	return data;
}

void CScriptVar::setInt(int val) {
	flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_INTEGER;
	intData = val;
	doubleData = 0;
	data = TINYJS_BLANK_DATA;
}

void CScriptVar::setBool(bool val) {
	flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_BOOLEAN;
	intData = val?1:0;
	doubleData = 0;
	data = TINYJS_BLANK_DATA;
}

void CScriptVar::setDouble(double val) {
	flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_DOUBLE;
	doubleData = val;
	intData = 0;
	data = TINYJS_BLANK_DATA;
}

void CScriptVar::setString(const string &str) {
	// name sure it's not still a number or integer
	flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_STRING;
	data = str;
	intData = 0;
	doubleData = 0;
}

void CScriptVar::setUndefined() {
	// name sure it's not still a number or integer
	flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_UNDEFINED;
	data = TINYJS_BLANK_DATA;
	intData = 0;
	doubleData = 0;
	removeAllChildren();
}

template <class T>
CScriptVar *DoMaths(CScriptVar *a, CScriptVar *b, int op)
{
	int dai = a->getInt();// use int when needed
	int dbi = b->getInt();
	T da = (T)(a->isDouble()?a->getDouble():dai);
	T db = (T)(b->isDouble()?b->getDouble():dbi);
	switch (op) {
			case '+': return new CScriptVar(da+db);
			case '-': return new CScriptVar(da-db);
			case '*': return new CScriptVar(da*db);
			case '/': return new CScriptVar(da/db);
			case '&': return new CScriptVar(dai&dbi);
			case '|': return new CScriptVar(dai|dbi);
			case '^': return new CScriptVar(dai^dbi);
			case '%': return new CScriptVar(dai%dbi);
			case '~': return new CScriptVar(~dai);
			case LEX_LSHIFT:    return new CScriptVar(dai<<dbi);
			case LEX_RSHIFT:    return new CScriptVar(dai>>dbi);
			case LEX_EQUAL:     return new CScriptVar(da==db);
			case LEX_NEQUAL:    return new CScriptVar(da!=db);
			case '<':     return new CScriptVar(da<db);
			case LEX_LEQUAL:    return new CScriptVar(da<=db);
			case '>':     return new CScriptVar(da>db);
			case LEX_GEQUAL:    return new CScriptVar(da>=db);
			default: throw new CScriptException("This operation not supported on the int datatype");
	}
}

CScriptVar *CScriptVar::mathsOp(CScriptVar *b, int op) {
	CScriptVar *a = this;
	// TODO Equality checks on classes/structures
	// Type equality check
	if (op == LEX_TYPEEQUAL || op == LEX_NTYPEEQUAL) {
		// check type first, then call again to check data
		bool eql = ((a->flags & SCRIPTVAR_VARTYPEMASK) ==
						(b->flags & SCRIPTVAR_VARTYPEMASK)) &&
						a->mathsOp(b, LEX_EQUAL);
		if (op == LEX_TYPEEQUAL)
			return new CScriptVar(eql);
		else
			return new CScriptVar(!eql);
	}
	// do maths...
	if (a->isUndefined() && b->isUndefined()) {
		if (op == LEX_EQUAL) return new CScriptVar(true);
		else if (op == LEX_NEQUAL) return new CScriptVar(false);
		else return new CScriptVar(); // undefined
	} else if ((a->isNumeric() || a->isUndefined()) &&
					(b->isNumeric() || b->isUndefined())) {
		if (!a->isDouble() && !b->isDouble()) {
			// use ints
			return DoMaths<int>(a, b, op);
		} else {
			// use doubles
			return DoMaths<double>(a, b, op);
		}			

	} else if (a->isArray()) {
		/* Just check pointers */
		switch (op) {
				 case LEX_EQUAL: return new CScriptVar(a==b);
				 case LEX_NEQUAL: return new CScriptVar(a!=b);
				 default: throw new CScriptException("This operation not supported on the Array datatype");
		}
	} else if (a->isObject()) {
				/* Just check pointers */
		switch (op) {
			 case LEX_EQUAL: return new CScriptVar(a==b);
			 case LEX_NEQUAL: return new CScriptVar(a!=b);
			 default: throw new CScriptException("This operation not supported on the Object datatype");
		}
	} else {
		string da = a->getString();
		string db = b->getString();
		// use strings
		switch (op) {
			case '+':           return new CScriptVar(da+db, SCRIPTVAR_STRING);
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

void CScriptVar::copySimpleData(CScriptVar *val) {
	data = val->data;
	intData = val->intData;
	doubleData = val->doubleData;
	flags = (flags & ~SCRIPTVAR_VARTYPEMASK) | (val->flags & SCRIPTVAR_VARTYPEMASK);
}

void CScriptVar::copyValue(CScriptVar *val) {
	if (val) {
		copySimpleData(val);
		// remove all current children
		removeAllChildren();
		// copy children of 'val'
////
/*
		CScriptVarLink *child = val->firstChild;
		while (child) {
			CScriptVar *copied;
			// don't copy the 'parent' object...
			if (child->name != TINYJS_PROTOTYPE_CLASS)
				copied = child->var->deepCopy();
			else
				copied = child->var;

			addChild(child->name, copied);

			child = child->nextSibling;
		}
*/
////
		for(SCRIPTVAR_CHILDS::iterator it = val->Childs.begin(); it != val->Childs.end(); ++it) {
			CScriptVar *copied;
			// don't copy the 'parent' object...
			if (it->first != TINYJS_PROTOTYPE_CLASS)
				copied = it->second->var->deepCopy();
			else
				copied = it->second->var;

			addChild(it->first, copied);
		}
	} else {
		setUndefined();
	}
}

CScriptVar *CScriptVar::deepCopy() {
	CScriptVar *newVar = new CScriptVar();
	newVar->copySimpleData(this);
	// copy children
////
/*
	CScriptVarLink *child = firstChild;
	while (child) {
			CScriptVar *copied;
			// don't copy the 'parent' object...
			if (child->name != TINYJS_PROTOTYPE_CLASS)
				copied = child->var->deepCopy();
			else
				copied = child->var;

			newVar->addChild(child->name, copied);
			child = child->nextSibling;
	}
*/
////
	for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it != Childs.end(); ++it) {
		CScriptVar *copied;
		// don't copy the 'parent' object...
		if (it->first != TINYJS_PROTOTYPE_CLASS)
			copied = it->second->var->deepCopy();
		else
			copied = it->second->var;

		newVar->addChild(it->first, copied);
	}
	return newVar;
}

void CScriptVar::trace(string indentStr, const string &name) {
	TRACE("%s'%s' = '%s' %s\n",
			indentStr.c_str(),
			name.c_str(),
			getString().c_str(),
			getFlagsAsString().c_str());
	string indent = indentStr+" ";
////
/*
	CScriptVarLink *link = firstChild;
	while (link) {
		link->var->trace(indent, link->name);
		link = link->nextSibling;
	}
*/
////
	for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it != Childs.end(); ++it) {
		it->second->var->trace(indent, it->first);
	}
}

string CScriptVar::getFlagsAsString() {
  string flagstr = "";
  if (flags&SCRIPTVAR_FUNCTION) flagstr = flagstr + "FUNCTION ";
  if (flags&SCRIPTVAR_OBJECT) flagstr = flagstr + "OBJECT ";
  if (flags&SCRIPTVAR_ARRAY) flagstr = flagstr + "ARRAY ";
  if (flags&SCRIPTVAR_NATIVE) flagstr = flagstr + "NATIVE ";
  if (flags&SCRIPTVAR_DOUBLE) flagstr = flagstr + "DOUBLE ";
  if (flags&SCRIPTVAR_INTEGER) flagstr = flagstr + "INTEGER ";
  if (flags&SCRIPTVAR_BOOLEAN) flagstr = flagstr + "BOOLEAN ";
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
	SCRIPTVAR_CHILDS::iterator last_it = --(Childs.end());//// --last_it;
	for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it != Childs.end(); ++it) {
		funcStr << it->first;
		if (it != last_it) funcStr << ", ";
	}
////
/*
	CScriptVarLink *link = firstChild;
	while (link) {
		funcStr << link->name;
		if (link->nextSibling) funcStr << ",";
		link = link->nextSibling;
	}
*/
////
	// add function body
	funcStr << ") " << getString();
	return funcStr.str();
  }
  // if it is a string then we quote it
  if (isString())
	return getJSString(getString());
  if (isNull())
		return "null";
  return "undefined";
}

void CScriptVar::getJSON(ostringstream &destination, const string linePrefix) {
	if (isObject()) {
		string indentedLinePrefix = linePrefix+"  ";
		// children - handle with bracketed list
		destination << "{ \n";
		SCRIPTVAR_CHILDS::iterator last_it = --(Childs.end());//// --last_it;
		for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it != Childs.end(); ++it) {
			destination << indentedLinePrefix;
			if (isAlphaNum(it->first))
				destination  << it->first;
			else
				destination  << getJSString(it->first);
			destination  << " : ";
			it->second->var->getJSON(destination, indentedLinePrefix);
			if (it != last_it) {
				destination  << ",\n";
			}
		}
		destination << "\n" << linePrefix << "}";
////
/*
		CScriptVarLink *link = firstChild;
		while (link) {
			destination << indentedLinePrefix;
			if (isAlphaNum(link->name))
				destination  << link->name;
			else
				destination  << getJSString(link->name);
			destination  << " : ";
			link->var->getJSON(destination, indentedLinePrefix);
			link = link->nextSibling;
			if (link) {
				destination  << ",\n";
			}
		}
		destination << "\n" << linePrefix << "}";
*/
////
	} else if (isArray()) {
		string indentedLinePrefix = linePrefix+"  ";
		destination << "[\n";
		int len = getArrayLength();
		if (len>10000) len=10000; // we don't want to get stuck here!

		for (int i=0;i<len;i++) {
			getArrayIndex(i)->getJSON(destination, indentedLinePrefix);
			if (i<len-1) destination  << ",\n";
		}

		destination << "\n" << linePrefix << "]";
	} else {
		// no children or a function... just write value directly
		destination << getParsableString();
	}
}


void CScriptVar::setCallback(JSCallback callback, void *userdata) {
	jsCallback = callback;
	jsCallbackUserData = userdata;
	flags = (flags & ~SCRIPTVAR_NATIVE) | SCRIPTVAR_NATIVE_FNC;
}
void CScriptVar::setCallback(NativeFncBase *callback, void *userdata) {
	jsCallbackClass = callback;
	jsCallbackUserData = userdata;
	flags = (flags & ~SCRIPTVAR_NATIVE) | SCRIPTVAR_NATIVE_MFNC;
}

CScriptVar *CScriptVar::ref() {
	refs++;
	return this;
}

void CScriptVar::unref() {
	if (refs<=0) printf("OMFG, we have unreffed too far!\n");
	if ((--refs)==0) {
		delete this;
	}
}

int CScriptVar::getRefs() {
	return refs;
}


// ----------------------------------------------------------------------------------- CSCRIPT

CTinyJS::CTinyJS() {
	l = NULL;
	runtimeFlags = 0;
	root = (new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT))->ref();
	// Add built-in classes
	stringClass = (new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT))->ref();
	arrayClass = (new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT))->ref();
	objectClass = (new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT))->ref();
	root->addChild("String", stringClass);
	root->addChild("Array", arrayClass);
	root->addChild("Object", objectClass);
	addNative("function eval(jsCode)", this, &CTinyJS::scEval); // execute the given string and return the resul
}

CTinyJS::~CTinyJS() {
	ASSERT(!l);
	scopes.clear();
	stringClass->unref();
	arrayClass->unref();
	objectClass->unref();
	root->unref();

#if DEBUG_MEMORY
	show_allocated();
#endif
}

void CTinyJS::trace() {
	root->trace();
}

void CTinyJS::execute(const string &code) {
	CScriptLex *oldLex = l;
	vector<CScriptVar*> oldScopes = scopes;
	l = new CScriptLex(code);
	scopes.clear();
	scopes.push_back(root);
	try {
			bool execute = true;
			while (l->tk) statement(execute);
	} catch (CScriptException *e) {
			ostringstream msg;
			msg << "Error " << e->text << " at " << l->getPosition();
			delete l;
			l = oldLex;
			throw new CScriptException(msg.str());
	}
	delete l;
	l = oldLex;
	scopes = oldScopes;
}

CScriptVarLink CTinyJS::evaluateComplex(const string &code) {
	CScriptLex *oldLex = l;
	vector<CScriptVar*> oldScopes = scopes;

	l = new CScriptLex(code);
	scopes.clear();
	scopes.push_back(root);
	CScriptVarLink *v = 0;
	try {
			bool execute = true;
			do {
				CLEAN(v);
				v = base(execute);
				if (l->tk!=LEX_EOF) l->match(';');
			} while (l->tk!=LEX_EOF);
	} catch (CScriptException *e) {
			ostringstream msg;
			msg << "Error " << e->text << " at " << l->getPosition();
			delete l;
			l = oldLex;
			throw new CScriptException(msg.str());
	}
	delete l;
	l = oldLex;
	scopes = oldScopes;

	if (v) {
			CScriptVarLink r = *v;
			CLEAN(v);
			return r;
	}
	// return undefined...
	return CScriptVarLink(new CScriptVar());
}

string CTinyJS::evaluate(const string &code) {
	return evaluateComplex(code).var->getString();
}

void CTinyJS::parseFunctionArguments(CScriptVar *funcVar) {
  l->match('(');
  while (l->tk!=')') {
		funcVar->addChildNoDup(l->tkStr);
		l->match(LEX_ID);
		if (l->tk!=')') l->match(',');
  }
  l->match(')');
}
void CTinyJS::addNative(const string &funcDesc, JSCallback ptr, void *userdata) {
	CScriptVar *funcVar = addNative(funcDesc);
	funcVar->setCallback(ptr, userdata);
}
void CTinyJS::addNative(const string &funcDesc, NativeFncBase *ptr, void *userdata) {
	CScriptVar *funcVar = addNative(funcDesc);
	funcVar->setCallback(ptr, userdata);
}
CScriptVar *CTinyJS::addNative(const string &funcDesc) {
	CScriptLex *oldLex = l;
	l = new CScriptLex(funcDesc);
	CScriptVar *base = root;

	l->match(LEX_R_FUNCTION);
	string funcName = l->tkStr;
	l->match(LEX_ID);
	/* Check for dots, we might want to do something like function String.substring ... */
	while (l->tk == '.') {
		l->match('.');
		CScriptVarLink *link = base->findChild(funcName);
		// if it doesn't exist, make an object class
		if (!link) link = base->addChild(funcName, new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT));
		base = link->var;
		funcName = l->tkStr;
		l->match(LEX_ID);
	}

	CScriptVar *funcVar = new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_FUNCTION);
	parseFunctionArguments(funcVar);
	delete l;
	l = oldLex;

	base->addChild(funcName, funcVar);
	return funcVar;
}

CScriptVarLink *CTinyJS::parseFunctionDefinition() {
  // actually parse a function...
  l->match(LEX_R_FUNCTION);
  string funcName = TINYJS_TEMP_NAME;
  /* we can have functions without names */
  if (l->tk==LEX_ID) {
	funcName = l->tkStr;
	l->match(LEX_ID);
  }
  CScriptVarLink *funcVar = new CScriptVarLink(new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_FUNCTION), funcName);
  parseFunctionArguments(funcVar->var);
  int funcBegin = l->tokenStart;
  bool noexecute = false;
  block(noexecute);
  funcVar->var->data = l->getSubString(funcBegin);
  return funcVar;
}

CScriptVarLink *CTinyJS::factor(bool &execute) {
	if (l->tk==LEX_R_TRUE) {
			l->match(LEX_R_TRUE);
			return new CScriptVarLink(new CScriptVar(true));
	}
	if (l->tk==LEX_R_FALSE) {
			l->match(LEX_R_FALSE);
			return new CScriptVarLink(new CScriptVar(false));
	}
	if (l->tk==LEX_R_NULL) {
			l->match(LEX_R_NULL);
			return new CScriptVarLink(new CScriptVar(TINYJS_BLANK_DATA,SCRIPTVAR_NULL));
	}
	if (l->tk==LEX_R_UNDEFINED) {
			l->match(LEX_R_UNDEFINED);
			return new CScriptVarLink(new CScriptVar(TINYJS_BLANK_DATA,SCRIPTVAR_UNDEFINED));
	}
	if (l->tk==LEX_ID || l->tk=='(') {
			CScriptVarLink *a;
		int op = l->tk;
		if(l->tk=='(')
		{
			l->match('(');
			a = base(execute);
			l->match(')');
		}
		else 
			a = execute ? findInScopes(l->tkStr) : new CScriptVarLink(new CScriptVar());
			/* The parent if we're executing a method call */
			CScriptVar *parent = 0;

			if (execute && !a) {
				/* Variable doesn't exist! JavaScript says we should create it
				 * (we won't add it here. This is done in the assignment operator)*/
				a = new CScriptVarLink(new CScriptVar(), l->tkStr);
			}
		if(op==LEX_ID)
			l->match(LEX_ID);
			while (l->tk=='(' || l->tk=='.' || l->tk=='[') {
				if (l->tk=='(') { // ------------------------------------- Function Call
				if (execute) {
				if (!a->var->isFunction()) {
				string errorMsg = "Expecting '";
				errorMsg = errorMsg + a->name + "' to be a function";
				throw new CScriptException(errorMsg.c_str());
				}
				l->match('(');
				// create a new symbol table entry for execution of this function
				CScriptVar *functionRoot = new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_FUNCTION);
				if (parent)
					functionRoot->addChildNoDup("this", parent);
				else
					functionRoot->addChildNoDup("this", new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT)); // always add a this-Object
				// grab in all parameters
				for(SCRIPTVAR_CHILDS::iterator it = a->var->Childs.begin(); it != a->var->Childs.end(); ++it) {
					CScriptVarLink *value = assignment(execute);
					if (execute) {
						if (value->var->isBasic()) {
							// pass by value
							functionRoot->addChild( it->first, value->var->deepCopy());
						} else {
							// pass by reference
							functionRoot->addChild(it->first, value->var);
						}
					}
					CLEAN(value);
					if (l->tk!=')') l->match(',');
				}
////
/*
				CScriptVarLink *v = a->var->firstChild;
				while (v) {
				CScriptVarLink *value = base(execute);
				if (execute) {
				if (value->var->isBasic()) {
				// pass by value
				functionRoot->addChild(v->name, value->var->deepCopy());
				} else {
				// pass by reference
				functionRoot->addChild(v->name, value->var);
				}
				}
				CLEAN(value);
				if (l->tk!=')') l->match(',');
				v = v->nextSibling;
				}
*/
////
				l->match(')');
				// setup a return variable
				CScriptVarLink *returnVar = NULL;
				// execute function!
				// add the function's execute space to the symbol table so we can recurse
				CScriptVarLink *returnVarLink = functionRoot->addChild(TINYJS_RETURN_VAR);
				scopes.push_back(functionRoot);

				if (a->var->isNative()) {
				ASSERT(a->var->jsCallback);
					if (a->var->isNative_ClassMemberFnc())
						(*a->var->jsCallbackClass)(functionRoot, a->var->jsCallbackUserData);
				else
						a->var->jsCallback(functionRoot, a->var->jsCallbackUserData);
				} else {
				/* we just want to execute the block, but something could
				 * have messed up and left us with the wrong ScriptLex, so
				 * we want to be careful here... */
				CScriptException *exception = 0;
				CScriptLex *oldLex = l;
				CScriptLex *newLex = new CScriptLex(a->var->getString());
				l = newLex;
				try {
				block(execute);
				// because return will probably have called this, and set execute to false
				execute = true;
				} catch (CScriptException *e) {
				exception = e;
				}
				delete newLex;
				l = oldLex;

				if (exception)
				throw exception;
				}

				scopes.pop_back();
				/* get the real return var before we remove it from our function */
				returnVar = new CScriptVarLink(returnVarLink->var);
				functionRoot->removeLink(returnVarLink);
				delete functionRoot;
				if (returnVar)
				a = returnVar;
				else
				a = new CScriptVarLink(new CScriptVar());
				} else {
				// function, but not executing - just parse args and be done
				l->match('(');
				while (l->tk != ')') {
				CScriptVarLink *value = base(execute);
				CLEAN(value);
				if (l->tk!=')') l->match(',');
				}
				l->match(')');
				if (l->tk == '{') {
				block(execute);
				}
				}
				} else if (l->tk == '.') { // ------------------------------------- Record Access
				l->match('.');
				if (execute) {
				const string &name = l->tkStr;
				CScriptVarLink *child = a->var->findChild(name);
				if (!child) child = findInParentClasses(a->var, name);
				if (!child) {
				/* if we haven't found this defined yet, use the built-in
				 'length' properly */
				if (a->var->isArray() && name == "length") {
				int l = a->var->getArrayLength();
				child = new CScriptVarLink(new CScriptVar(l));
				} else if (a->var->isString() && name == "length") {
				int l = a->var->getString().size();
				child = new CScriptVarLink(new CScriptVar(l));
				} else {
				child = a->var->addChild(name);
				}
				}
				parent = a->var;
				a = child;
				}
				l->match(LEX_ID);
				} else if (l->tk == '[') { // ------------------------------------- Array Access
				l->match('[');
				CScriptVarLink *index = expression(execute);
				l->match(']');
				if (execute) {
				CScriptVarLink *child = a->var->findChildOrCreate(index->var->getString());
				parent = a->var;
				a = child;
				}
				CLEAN(index);
				} else ASSERT(0);
			}
			return a;
	}
	if (l->tk==LEX_INT || l->tk==LEX_FLOAT) {
			CScriptVar *a = new CScriptVar(l->tkStr,
				((l->tk==LEX_INT)?SCRIPTVAR_INTEGER:SCRIPTVAR_DOUBLE));
			l->match(l->tk);
			return new CScriptVarLink(a);
	}
	if (l->tk==LEX_STR) {
			CScriptVar *a = new CScriptVar(l->tkStr, SCRIPTVAR_STRING);
			l->match(LEX_STR);
			return new CScriptVarLink(a);
	}
	if (l->tk=='{') {
			CScriptVar *contents = new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT);
			/* JSON-style object definition */
			l->match('{');
			while (l->tk != '}') {
				string id = l->tkStr;
				// we only allow strings or IDs on the left hand side of an initialisation
				if (l->tk==LEX_STR) l->match(LEX_STR);
				else l->match(LEX_ID);
				l->match(':');
				if (execute) {
				CScriptVarLink *a = base(execute);
				contents->addChild(id, a->var);
				CLEAN(a);
				}
				// no need to clean here, as it will definitely be used
				if (l->tk != '}') l->match(',');
			}

			l->match('}');
			return new CScriptVarLink(contents);
	}
	if (l->tk=='[') {
			CScriptVar *contents = new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_ARRAY);
			/* JSON-style array */
			l->match('[');
			int idx = 0;
			while (l->tk != ']') {
				if (execute) {
				char idx_str[16]; // big enough for 2^32
				sprintf_s(idx_str, sizeof(idx_str), "%d",idx);

				CScriptVarLink *a = base(execute);
				contents->addChild(idx_str, a->var);
				CLEAN(a);
				}
				// no need to clean here, as it will definitely be used
				if (l->tk != ']') l->match(',');
				idx++;
			}
			l->match(']');
			return new CScriptVarLink(contents);
	}
	if (l->tk==LEX_R_FUNCTION) {
		CScriptVarLink *funcVar = parseFunctionDefinition();
			if (funcVar->name != TINYJS_TEMP_NAME)
				TRACE("Functions not defined at statement-level are not meant to have a name");
			return funcVar;
	}
	if (l->tk==LEX_R_NEW) {
		// new -> create a new object
		l->match(LEX_R_NEW);
		const string &className = l->tkStr;
		if (execute) {
			CScriptVarLink *objClass = findInScopes(className);
			if (!objClass) {
				TRACE("%s is not a valid class name", className.c_str());
				return new CScriptVarLink(new CScriptVar());
			}
			l->match(LEX_ID);
			CScriptVar *obj = new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT);
			obj->addChild(TINYJS_PROTOTYPE_CLASS, objClass->var);
			if (l->tk == '(') {
				l->match('(');
				l->match(')');
			}
			// TODO: Object constructors
			return new CScriptVarLink(obj);
		} else {
			l->match(LEX_ID);
			if (l->tk == '(') {
				l->match('(');
			while(l->tk != ')') l->match(l->tk);
			l->match(')');
			}
		}
	}
	// Nothing we can do here... just hope it's the end...
	l->match(LEX_EOF);
	return 0;
}
CScriptVarLink *CTinyJS::unary(bool &execute) {
	CScriptVarLink *a;
	if (l->tk=='-') {
			l->match('-');
			a = unary(execute);
			if (execute) {
				CScriptVar zero(0);
				CScriptVar *res = zero.mathsOp(a->var, '-');
				CREATE_LINK(a, res);
			}
	} else
	if (l->tk=='+') {
			l->match('+');
			a = unary(execute);
	} else
	if (l->tk=='!') {
			l->match('!'); // binary not
			a = unary(execute);
			if (execute) {
				CScriptVar zero(0);
				CScriptVar *res = a->var->mathsOp(&zero, LEX_EQUAL);
				CREATE_LINK(a, res);
			}
	} else
	if (l->tk=='~') {
			l->match('~'); // binary neg
			a = unary(execute);
			if (execute) {
				CScriptVar *res = a->var->mathsOp(NULL, '~');
				CREATE_LINK(a, res);
			}
	} else
	// pre increment/decrement
	if (l->tk==LEX_PLUSPLUS || l->tk==LEX_MINUSMINUS) {
		int op = l->tk;
		l->match(l->tk);
			a = factor(execute);
		if (execute) {
			if(a->owned)
			{
				CScriptVar one(1);
				CScriptVar *res = a->var->mathsOp(&one, op==LEX_PLUSPLUS ? '+' : '-');
				// in-place add/subtract
				a->replaceWith(res);
				CREATE_LINK(a, res);
			}
		}
	} 
	else
			a = factor(execute);

	// post increment/decrement
	if (l->tk==LEX_PLUSPLUS || l->tk==LEX_MINUSMINUS) {
		int op = l->tk;
			l->match(l->tk);
			if (execute) {
			if(a->owned)
			{
//				TRACE("post-increment of %s and a is %sthe owner\n", a->name.c_str(), a->owned?"":"not ");
				CScriptVar one(1);
				CScriptVar *res = a->var->ref();
				CScriptVar *new_a = a->var->mathsOp(&one, op==LEX_PLUSPLUS ? '+' : '-');
				a->replaceWith(new_a);
				CREATE_LINK(a, res);
				res->unref();
			}
			else
			{
				ostringstream errorString;
				errorString << "Post-" << (l->tk==LEX_PLUSPLUS ? "Increment" : "Decrement") << ": ";
				if(a->name.length())
					errorString << a->name << " is not defined";
				else
					errorString << "invalid " << (l->tk==LEX_PLUSPLUS ? "increment" : "decrement") << " operand";
//				errorString << " at " << (l->getPosition(l->tokenStart-2));
				throw new CScriptException(errorString.str());
			}
			}
	}
	
	
	return a;
}

CScriptVarLink *CTinyJS::term(bool &execute) {
	CScriptVarLink *a = unary(execute);
	while (l->tk=='*' || l->tk=='/' || l->tk=='%') {
			int op = l->tk;
			l->match(l->tk);
			CScriptVarLink *b = unary(execute);
			if (execute) {
				CScriptVar *res = a->var->mathsOp(b->var, op);
				CREATE_LINK(a, res);
			}
			CLEAN(b);
	}
	return a;
}

CScriptVarLink *CTinyJS::expression(bool &execute) {
	CScriptVarLink *a = term(execute);
	while (l->tk=='+' || l->tk=='-') {
			int op = l->tk;
			l->match(l->tk);
			CScriptVarLink *b = term(execute);
			if (execute) {
				 // not in-place, so just replace
				 CScriptVar *res = a->var->mathsOp(b->var, op);
				 CREATE_LINK(a, res);
			}
			CLEAN(b);
	}
	return a;
}

CScriptVarLink *CTinyJS::binary_shift(bool &execute) {
	CScriptVarLink *a = expression(execute);
	while (l->tk==LEX_LSHIFT || l->tk==LEX_RSHIFT) {
			int op = l->tk;
			l->match(l->tk);

		CScriptVarLink *b = expression(execute);
			if (execute) {
				 // not in-place, so just replace
				 CScriptVar *res = a->var->mathsOp(b->var, op);
				 CREATE_LINK(a, res);
			}
			CLEAN(b);
	}
	return a;
}

CScriptVarLink *CTinyJS::relation(bool &execute, int set, int set_n) {
	CScriptVarLink *a = set_n ? relation(execute, set_n, 0) : binary_shift(execute);
//    CScriptVarLink *a = expression(execute);
	CScriptVarLink *b;
	while ((set==LEX_EQUAL && (l->tk==LEX_EQUAL || l->tk==LEX_NEQUAL || l->tk==LEX_TYPEEQUAL || l->tk==LEX_NTYPEEQUAL))
				||	(set=='<' && (l->tk==LEX_LEQUAL || l->tk==LEX_GEQUAL || l->tk=='<' || l->tk=='>'))) {
		int op = l->tk;
		l->match(l->tk);
		b = set_n ? relation(execute, set_n, 0) : binary_shift(execute);
		if (execute) {
			CScriptVar *res = a->var->mathsOp(b->var, op);
			CREATE_LINK(a,res);
		}
		CLEAN(b);
	}
	return a;
}

CScriptVarLink *CTinyJS::logic_binary(bool &execute, int op, int op_n1, int op_n2) {
	CScriptVarLink *a = op_n1 ? logic_binary(execute, op_n1, op_n2, 0) : relation(execute);
	CScriptVarLink *b;
	while (l->tk==op) {
		l->match(l->tk);
		b = op_n1 ? logic_binary(execute, op_n1, op_n2, 0) : relation(execute);
		if (execute) {
			CScriptVar *res = a->var->mathsOp(b->var, op);
			CREATE_LINK(a, res);
		}
		CLEAN(b);
	}
	return a;
}

CScriptVarLink *CTinyJS::logic(bool &execute, int op, int op_n) {
	CScriptVarLink *a = op_n ? logic(execute, op_n, 0) : logic_binary(execute);
	CScriptVarLink *b;
	while (l->tk==op) {
		bool noexecute = false;
		int bop = l->tk;
		l->match(l->tk);
		bool shortCircuit = false;
		bool boolean = false;
		// if we have short-circuit ops, then if we know the outcome
		// we don't bother to execute the other op. Even if not
		// we need to tell mathsOp it's an & or |
		if (op==LEX_ANDAND) {
			bop = '&';
			shortCircuit = !a->var->getBool();
			boolean = true;
		} else if (op==LEX_OROR) {
			bop = '|';
			shortCircuit = a->var->getBool();
			boolean = true;
		}
		b = op_n ? logic(shortCircuit ? noexecute : execute, op_n, 0) : logic_binary(shortCircuit ? noexecute : execute);
		if (execute && !shortCircuit) {
			if (boolean) {
				CScriptVar *newa = new CScriptVar(a->var->getBool());
				CScriptVar *newb = new CScriptVar(b->var->getBool());
				CREATE_LINK(a, newa);
				CREATE_LINK(b, newb);
			}
			CScriptVar *res = a->var->mathsOp(b->var, bop);
			CREATE_LINK(a, res);
		}
		CLEAN(b);
	}
	return a;
}
CScriptVarLink *CTinyJS::condition(bool &execute)
{
	CScriptVarLink *a = logic(execute);
	if (l->tk=='?')
	{
		l->match(l->tk);
		bool cond = a->var->getBool();
		CLEAN(a);
		CScriptVarLink *b;
		bool noexecute=false;
		a = condition(cond ? execute : noexecute );
		l->match(':');
		b = condition(cond ? noexecute : execute);
		if(cond)
			CLEAN(b);
		else
		{
			CLEAN(a);
			a=b;
		}
	}
	return a;
}
	
CScriptVarLink *CTinyJS::assignment(bool &execute) {
	CScriptVarLink *lhs = condition(execute);
	if (l->tk=='=' || l->tk==LEX_PLUSEQUAL || l->tk==LEX_MINUSEQUAL || 
			l->tk==LEX_LSHIFTEQUAL || l->tk==LEX_RSHIFTEQUAL ||
			l->tk==LEX_ANDEQUAL || l->tk==LEX_OREQUAL || l->tk==LEX_XOREQUAL) {
		/* If we're assigning to this and we don't have a parent,
		 * add it to the symbol table root as per JavaScript. */
		if (execute && !lhs->owned) {
			if (lhs->name.length()>0) {
				CScriptVarLink *realLhs = root->addChildNoDup(lhs->name, lhs->var);
				CLEAN(lhs);
				lhs = realLhs;
			} else
			TRACE("Trying to assign to an un-named type\n");
		}

		int op = l->tk;
		l->match(l->tk);
		CScriptVarLink *rhs = assignment(execute);
		if (execute) {
			if (op=='=') {
				lhs->replaceWith(rhs);
			} else if (op==LEX_PLUSEQUAL) {
				CScriptVar *res = lhs->var->mathsOp(rhs->var, '+');
				lhs->replaceWith(res);
			} else if (op==LEX_MINUSEQUAL) {
				CScriptVar *res = lhs->var->mathsOp(rhs->var, '-');
				lhs->replaceWith(res);
			} else if (op==LEX_LSHIFTEQUAL) {
				CScriptVar *res = lhs->var->mathsOp(rhs->var, LEX_LSHIFT);
				lhs->replaceWith(res);
			} else if (op==LEX_RSHIFTEQUAL) {
				CScriptVar *res = lhs->var->mathsOp(rhs->var, LEX_RSHIFT);
				lhs->replaceWith(res);
			} else if (op==LEX_ANDEQUAL) {
				CScriptVar *res = lhs->var->mathsOp(rhs->var, '&');
				lhs->replaceWith(res);
			} else if (op==LEX_OREQUAL) {
				CScriptVar *res = lhs->var->mathsOp(rhs->var, '|');
				lhs->replaceWith(res);
			} else if (op==LEX_XOREQUAL) {
				CScriptVar *res = lhs->var->mathsOp(rhs->var, '^');
				lhs->replaceWith(res);
			}
		}
		CLEAN(rhs);
	}
	return lhs;
}
CScriptVarLink *CTinyJS::base(bool &execute) {
	CScriptVarLink *a;
	for(;;)
	{
		a = assignment(execute);
		if (l->tk == ',') {
			l->match(',');
			CLEAN(a);
		} else
			break;
	}
	return a;
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
			l->tk==LEX_PLUSPLUS ||
			l->tk==LEX_MINUSMINUS ||
			l->tk=='+' ||
			l->tk=='-' ||
			l->tk=='(') {
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
		for(;;)
		{
			CScriptVarLink *a = 0;
			if (execute)
				a = scopes.back()->findChildOrCreate(l->tkStr);
			l->match(LEX_ID);
			// now do stuff defined with dots
			while (l->tk == '.') {
				l->match('.');
				if (execute) {
					CScriptVarLink *lastA = a;
					a = lastA->var->findChildOrCreate(l->tkStr);
				}
				l->match(LEX_ID);
			}
			// sort out initialiser
			if (l->tk == '=') {
				l->match('=');
				CScriptVarLink *var = assignment(execute);
				if (execute)
					a->replaceWith(var);
				CLEAN(var);
			}
			if (l->tk == ',')
				l->match(',');
			else
				break;
		}
			l->match(';');
	} else if (l->tk==LEX_R_IF) {
		l->match(LEX_R_IF);
		l->match('(');
		CScriptVarLink *var = base(execute);
		l->match(')');
		bool cond = execute && var->var->getBool();
		CLEAN(var);
		bool noexecute = false; // because we need to be abl;e to write to it
		statement(cond ? execute : noexecute);
		if (l->tk==LEX_R_ELSE) {
			l->match(LEX_R_ELSE);
			statement(cond ? noexecute : execute);
		}
	} else if (l->tk==LEX_R_DO) {
		// We do repetition by pulling out the string representing our statement
		// there's definitely some opportunity for optimisation here
		l->match(LEX_R_DO);
		bool loopCond = true; 
		bool old_execute = execute;
		int old_runtimeFlags = runtimeFlags;
		runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;
		int whileBodyStart = l->tokenStart;
		int whileCondStart = 0;
		CScriptLex *oldLex = NULL;
		CScriptLex *whileBody = NULL;
		CScriptLex *whileCond = NULL;
		int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
		while (loopCond && loopCount-->0) 
		{
			if(whileBody)
			{
				whileBody->reset();
				l = whileBody;
			}
			statement(execute);
			if(!whileBody) whileBody = l->getSubLex(whileBodyStart);
			if(old_execute && !execute)
			{
				// break;
				if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
				{
					execute = old_execute;
					if(runtimeFlags & RUNTIME_BREAK)
						loopCond = false;
					runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
				}
				// return, break or continue;
			}

			if(!whileCond)
			{
				l->match(LEX_R_WHILE);
				l->match('(');
				whileCondStart = l->tokenStart;
			}
			else
			{
				whileCond->reset();
				l = whileCond;
			}
			CScriptVarLink *cond = base(execute);
			loopCond = execute && cond->var->getBool();
			CLEAN(cond);
			if(!whileCond)
			{
				whileCond = l->getSubLex(whileCondStart);
				l->match(')');
				l->match(';');
				oldLex = l;
			}
		}
		runtimeFlags = old_runtimeFlags;
			l = oldLex;
			delete whileCond;
			delete whileBody;

			if (loopCount<=0) {
				root->trace();
				TRACE("WHILE Loop exceeded %d iterations at %s\n", TINYJS_LOOP_MAX_ITERATIONS, l->getPosition(l->tokenLastEnd).c_str());
				throw new CScriptException("LOOP_ERROR");
			}
	} else if (l->tk==LEX_R_WHILE) {
		// We do repetition by pulling out the string representing our statement
		// there's definitely some opportunity for optimisation here
		l->match(LEX_R_WHILE);
		l->match('(');
		int whileCondStart = l->tokenStart;
		bool noexecute = false;
		CScriptVarLink *cond = base(execute);
		bool loopCond = execute && cond->var->getBool();
		CLEAN(cond);
		CScriptLex *whileCond = l->getSubLex(whileCondStart);
		l->match(')');
		int whileBodyStart = l->tokenStart;

		bool old_execute = execute;
		int old_runtimeFlags = runtimeFlags;
		runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;
		statement(loopCond ? execute : noexecute);
		CScriptLex *whileBody = l->getSubLex(whileBodyStart);
		CScriptLex *oldLex = l;
		if(loopCond && old_execute != execute)
		{
			// break;
			if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
			{
				execute = old_execute;
				if(runtimeFlags & RUNTIME_BREAK)
					loopCond = false;
				runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
			}
			// return, break or continue;
		}

		int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
		while (loopCond && loopCount-->0) {
			whileCond->reset();
			l = whileCond;
			cond = base(execute);
			loopCond = execute && cond->var->getBool();
			CLEAN(cond);
			if (loopCond) {
				whileBody->reset();
				l = whileBody;
				statement(execute);
				if(!execute)
				{
					// break;
					if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
					{
						execute = old_execute;
						if(runtimeFlags & RUNTIME_BREAK)
							loopCond = false;
						runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
					}
					// return, break or continue;
				}
			}
		}
		runtimeFlags = old_runtimeFlags;
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
		bool noexecute = false;
		int forCondStart = l->tokenStart;
		bool cond_empty = true;
		bool loopCond = true;	// Empty Condition -->always true
		CScriptVarLink *cond;
		if(l->tk != ';')
		{
			cond_empty = false;
			cond = base(execute); // condition
			loopCond = execute && cond->var->getBool();
			CLEAN(cond);
		}
		CScriptLex *forCond = l->getSubLex(forCondStart);
		l->match(';');
		bool iter_empty = true;
		CScriptLex *forIter=NULL;
		if(l->tk != ')')
		{
			iter_empty = false;
			int forIterStart = l->tokenStart;
			base(noexecute); // iterator
			forIter = l->getSubLex(forIterStart);
		}
		l->match(')');
		int forBodyStart = l->tokenStart;
		bool old_execute = execute;
		int old_runtimeFlags = runtimeFlags;
		runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;
		statement(loopCond ? execute : noexecute);
		CScriptLex *forBody = l->getSubLex(forBodyStart);
		CScriptLex *oldLex = l;
		if(loopCond && old_execute != execute)
		{
			// break;
			if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
			{
				execute = old_execute;
				if(runtimeFlags & RUNTIME_BREAK)
					loopCond = false;
				runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
			}
			// return, break or continue;
		}

		if (loopCond && !iter_empty) {
			forIter->reset();
			l = forIter;
			base(execute);
		}
		int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
		while (execute && loopCond && loopCount-->0) {
			if(!cond_empty)
			{
				forCond->reset();
				l = forCond;
				cond = base(execute);
				loopCond = cond->var->getBool();
				CLEAN(cond);
			}
			if (execute && loopCond) {
				forBody->reset();
				l = forBody;
				statement(execute);
				if(!execute)
				{
					// break or continue;
					if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
					{
						execute = old_execute;
						if(runtimeFlags & RUNTIME_BREAK)
							loopCond = false;
						runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
					}
					// return nothing to do
				}
			}
			if (execute && loopCond && !iter_empty) {
				forIter->reset();
				l = forIter;
				base(execute);
			}
		}
		runtimeFlags = old_runtimeFlags;
		l = oldLex;
		delete forCond;
		delete forIter;
		delete forBody;
		if (loopCount<=0) {
			root->trace();
			TRACE("FOR Loop exceeded %d iterations at %s\n", TINYJS_LOOP_MAX_ITERATIONS, l->getPosition(l->tokenLastEnd).c_str());
			throw new CScriptException("LOOP_ERROR");
		}
	} else if (l->tk==LEX_R_BREAK) {
		l->match(LEX_R_BREAK);
		if(runtimeFlags & RUNTIME_CANBREAK)
		{
			runtimeFlags |= RUNTIME_BREAK;
			execute = false;
		}
		l->match(';');
	} else if (l->tk==LEX_R_CONTINUE) {
		l->match(LEX_R_CONTINUE);
		if(runtimeFlags & RUNTIME_CANCONTINUE)
		{
			runtimeFlags |= RUNTIME_CONTINUE;
			execute = false;
		}
		l->match(';');
	} else if (l->tk==LEX_R_RETURN) {
		l->match(LEX_R_RETURN);
		CScriptVarLink *result = 0;
		if (l->tk != ';')
			result = base(execute);
		if (execute) {
			CScriptVarLink *resultVar = scopes.back()->findChild(TINYJS_RETURN_VAR);
		if (resultVar)
			resultVar->replaceWith(result);
		else
			TRACE("RETURN statement, but not in a function.\n");
			execute = false;
		}
		CLEAN(result);
		l->match(';');
	} else if (l->tk==LEX_R_FUNCTION) {
		CScriptVarLink *funcVar = parseFunctionDefinition();
		if (execute) {
			if (funcVar->name == TINYJS_TEMP_NAME)
				TRACE("Functions defined at statement-level are meant to have a name\n");
			else
				scopes.back()->addChildNoDup(funcVar->name, funcVar->var);
		}
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
		CScriptVarLink *varl = var->findChild(el);
		var = varl?varl->var:0;
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

/// Finds a child, looking recursively up the scopes
CScriptVarLink *CTinyJS::findInScopes(const std::string &childName) {
	for (int s=scopes.size()-1;s>=0;s--) {
		CScriptVarLink *v = scopes[s]->findChild(childName);
		if (v) return v;
	}
	return NULL;

}

/// Look up in any parent classes of the given object
CScriptVarLink *CTinyJS::findInParentClasses(CScriptVar *object, const std::string &name) {
	// Look for links to actual parent classes
	CScriptVarLink *parentClass = object->findChild(TINYJS_PROTOTYPE_CLASS);
	while (parentClass) {
		CScriptVarLink *implementation = parentClass->var->findChild(name);
		if (implementation) return implementation;
		parentClass = parentClass->var->findChild(TINYJS_PROTOTYPE_CLASS);
	}
	// else fake it for strings and finally objects
	if (object->isString()) {
		CScriptVarLink *implementation = stringClass->findChild(name);
		if (implementation) return implementation;
	}
	if (object->isArray()) {
		CScriptVarLink *implementation = arrayClass->findChild(name);
		if (implementation) return implementation;
	}
	CScriptVarLink *implementation = objectClass->findChild(name);
	if (implementation) return implementation;

	return 0;
}

void CTinyJS::scEval(CScriptVar *c, void *data) {
	std::string str = c->getParameter("jsCode")->getString();
	c->setReturnVar(evaluateComplex(str).var);
}
