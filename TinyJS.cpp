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
//#define CLEAN(x) do{ CScriptVarLink *__v = x; if (__v && !__v->owned) { delete __v;} }while(0)
/* Create a LINK to point to VAR and free the old link.
 * BUT this is more clever - it tries to keep the old link if it's not owned to save allocations */
//#define CREATE_LINK(LINK, VAR) do{ if (!LINK || LINK->owned) LINK = new CScriptVarLink(VAR); else {LINK->replaceWith(VAR);LINK->name=TINYJS_TEMP_NAME;} }while(0)

// corresponds "CScriptVarLink *link=0;" and "CScriptVarLink *link=Link"
inline CScriptVarSmartLink::CScriptVarSmartLink () : link(0){}
inline CScriptVarSmartLink::CScriptVarSmartLink (CScriptVarLink *Link) : link(Link) {}

// this constructor replace "CScriptVarLink *link=0 ; CREATE_LINK(link, Var);"
inline CScriptVarSmartLink::CScriptVarSmartLink (CScriptVar *Var) : link(0) {
	*this = Var;
}
// this operator replace "CREATE_LINK(link, Var);"
inline CScriptVarSmartLink &CScriptVarSmartLink::operator = (CScriptVar *Var) {
	if (!link || link->owned) 
		link = new CScriptVarLink(Var);
	else {
		link->replaceWith(Var);
		link->name=TINYJS_TEMP_NAME;
	} 
	return *this;
}

// the deconstructor replace the macro CLEAN(x)
inline CScriptVarSmartLink::~CScriptVarSmartLink () {
	if(link && !link->owned) 
		delete link; 
}
// the copy-stuff has a special thing - 
// when copying a SmartLink to an other then the right hand side will lost your link
inline CScriptVarSmartLink::CScriptVarSmartLink (const CScriptVarSmartLink &Link) : link(0) {
	*this = Link; 
}
inline CScriptVarSmartLink &CScriptVarSmartLink::operator = (const CScriptVarSmartLink &Link) { 
	if(link && !link->owned) delete link; 
	link = Link.link; 
	((CScriptVarSmartLink &)Link).link = 0; // explicit cast to a non const ref
	return *this;
}

// this operator corresponds "CLEAN(link); link = Link;"
inline CScriptVarSmartLink &CScriptVarSmartLink::operator = (CScriptVarLink *Link) {
	if(link && !link->owned) delete link; 
	link = Link; 
	return *this;
}

// this operator corresponds "link->replaceWith(Link->var)"
inline CScriptVarSmartLink &CScriptVarSmartLink::operator <<(CScriptVarSmartLink &Link) {
	ASSERT(link && Link.link);
	link->replaceWith(Link->var);
	return *this;
}

using namespace std;

#ifdef __GNUC__
#	define vsprintf_s vsnprintf
#	define vsnprintf_s(buffer, size, len, format, args) vsnprintf(buffer, size, format, args)
#	define sprintf_s snprintf
#	define _strdup strdup
#else
#	define __attribute__(x)
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

string &int2string(int intData, const string &inString=string());
string &int2string(int intData, const string &inString) {
	string &_inString = (string &) inString;
	char buffer[32];
	sprintf_s(buffer, sizeof(buffer), "%d", intData);
	return _inString=buffer;
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

CScriptException::CScriptException(const string &exceptionText, int Pos) {
	text = exceptionText;
	pos = Pos;
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

void CScriptLex::reset(int toPos) {
	dataPos = dataStart+toPos;
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
	if (expected_tk==';' && tk==LEX_EOF) return; // ignore last missing ';'
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
		case LEX_ASTERISKEQUAL : return "*=";
		case LEX_SLASHEQUAL : return "/=";
		case LEX_PERCENTEQUAL : return "%=";

		// reserved words
		case LEX_R_IF : return "if";
		case LEX_R_ELSE : return "else";
		case LEX_R_DO : return "do";
		case LEX_R_WHILE : return "while";
		case LEX_R_FOR : return "for";
		case LEX_R_IN: return "in";
		case LEX_R_BREAK : return "break";
		case LEX_R_CONTINUE : return "continue";
		case LEX_R_FUNCTION : return "function";
		case LEX_R_RETURN : return "return";
		case LEX_R_VAR : return "var";
		case LEX_R_TRUE : return "true";
		case LEX_R_FALSE : return "false";
		case LEX_R_NULL : return "null";
		case LEX_R_UNDEFINED : return "undefined";
		case LEX_R_INFINITY : return "Infinity";
		case LEX_R_NAN : return "NaN";
		case LEX_R_NEW : return "new";
		case LEX_R_TRY : return "try";
		case LEX_R_CATCH : return "catch";
		case LEX_R_FINALLY : return "finally";
		case LEX_R_THROW : return "throw";
		case LEX_R_TYPEOF : return "typeof";
		case LEX_R_VOID : return "void";
		case LEX_R_DELETE : return "delete";
		case LEX_R_INSTANCEOF : return "instanceof";
		case LEX_R_SWITCH : return "switch";
		case LEX_R_CASE : return "case";
		case LEX_R_DEFAULT : return "default";
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
		else if (tkStr=="in") tk = LEX_R_IN;
		else if (tkStr=="break") tk = LEX_R_BREAK;
		else if (tkStr=="continue") tk = LEX_R_CONTINUE;
		else if (tkStr=="function") tk = LEX_R_FUNCTION;
		else if (tkStr=="return") tk = LEX_R_RETURN;
		else if (tkStr=="var") tk = LEX_R_VAR;
		else if (tkStr=="true") tk = LEX_R_TRUE;
		else if (tkStr=="false") tk = LEX_R_FALSE;
		else if (tkStr=="null") tk = LEX_R_NULL;
		else if (tkStr=="undefined") tk = LEX_R_UNDEFINED;
		else if (tkStr=="Infinity") tk = LEX_R_INFINITY;
		else if (tkStr=="NaN") tk = LEX_R_NAN;
		else if (tkStr=="new") tk = LEX_R_NEW;
		else if (tkStr=="try") tk = LEX_R_TRY;
		else if (tkStr=="catch") tk = LEX_R_CATCH;
		else if (tkStr=="finally") tk = LEX_R_FINALLY;
		else if (tkStr=="throw") tk = LEX_R_THROW;
		else if (tkStr=="typeof") tk = LEX_R_TYPEOF;
		else if (tkStr=="void") tk = LEX_R_VOID;
		else if (tkStr=="delete") tk = LEX_R_DELETE;
		else if (tkStr=="instanceof") tk = LEX_R_INSTANCEOF;
		else if (tkStr=="switch") tk = LEX_R_SWITCH;
		else if (tkStr=="case") tk = LEX_R_CASE;
		else if (tkStr=="default") tk = LEX_R_DEFAULT;
	} else if (isNumeric(currCh) || (currCh=='.' && isNumeric(nextCh))) { // Numbers
		if(currCh=='.') tkStr+='0';
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
		} else if (tk=='<') {
			if (currCh=='=') {	// <=
				tk = LEX_LEQUAL;
				getNextCh();
			} else if (currCh=='<') {	// <<
				tk = LEX_LSHIFT;
				getNextCh();
				if (currCh=='=') { // <<=
					tk = LEX_LSHIFTEQUAL;
					getNextCh();
				}
			}
		} else if (tk=='>') {
			if (currCh=='=') {	// >=
				tk = LEX_GEQUAL;
				getNextCh();
			} else if (currCh=='>') {	// >>
				tk = LEX_RSHIFT;
				getNextCh();
				if (currCh=='=') { // >>=
					tk = LEX_RSHIFTEQUAL;
					getNextCh();
				}
			}
		}  else if (tk=='+') {
			if (currCh=='=') {	// +=
				tk = LEX_PLUSEQUAL;
				getNextCh();
			}  else if (currCh=='+') {	// ++
				tk = LEX_PLUSPLUS;
				getNextCh();
			}
		}  else if (tk=='-') {
			if (currCh=='=') {	// -=
				tk = LEX_MINUSEQUAL;
				getNextCh();
			}  else if (currCh=='-') {	// --
				tk = LEX_MINUSMINUS;
				getNextCh();
			}
		} else if (tk=='&') {
			if (currCh=='=') {			// &=
				tk = LEX_ANDEQUAL;
				getNextCh();
			} else if (currCh=='&') {	// &&
				tk = LEX_ANDAND;
				getNextCh();
			}
		} else if (tk=='|') {
			if (currCh=='=') {			// |=
				tk = LEX_OREQUAL;
				getNextCh();
			} else if (currCh=='|') {	// ||
				tk = LEX_OROR;
				getNextCh();
			}
		} else if (tk=='^' && currCh=='=') {
			tk = LEX_XOREQUAL;
			getNextCh();
		} else if (tk=='*' && currCh=='=') {
			tk = LEX_ASTERISKEQUAL;
			getNextCh();
		} else if (tk=='/' && currCh=='=') {
			tk = LEX_SLASHEQUAL;
			getNextCh();
		} else if (tk=='%' && currCh=='=') {
			tk = LEX_PERCENTEQUAL;
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
int CScriptLex::getDataPos() {
	return dataPos;
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
	this->var = var->ref();
	this->owned = false;
	this->owner = 0;
	this->dontDelete = false;
}

CScriptVarLink::CScriptVarLink(const CScriptVarLink &link) {
	// Copy constructor
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	this->name = link.name;
	this->var = link.var->ref();
	this->owned = false;
	this->owner = 0;
	this->dontDelete = false;
}

CScriptVarLink::~CScriptVarLink() {
#if DEBUG_MEMORY
	mark_deallocated(this);
#endif
	var->unref(this->owner);
}

void CScriptVarLink::replaceWith(CScriptVar *newVar) {
	if(!newVar) newVar = new CScriptVar();
	CScriptVar *oldVar = var;
	oldVar->unref(owner);
	var = newVar->ref();
	if(owner) var->recoursionCheck(owner);
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
	internalRefs = 0;
	recursionFlag = 0;
	recursionSet = 0;
	__proto__ = NULL;
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	init();
	flags = SCRIPTVAR_UNDEFINED;
}

CScriptVar::CScriptVar(const string &str) {
	refs = 0;
	internalRefs = 0;
	recursionFlag = 0;
	recursionSet = 0;
	__proto__ = NULL;
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	init();
	flags = SCRIPTVAR_STRING;
	data = str;
}

CScriptVar::CScriptVar(const char *str) {
	refs = 0;
	internalRefs = 0;
	recursionFlag = 0;
	recursionSet = 0;
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
	internalRefs = 0;
	recursionFlag = 0;
	recursionSet = 0;
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
	internalRefs = 0;
	recursionFlag = 0;
	recursionSet = 0;
	__proto__ = NULL;
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	init();
	setDouble(val);
}

CScriptVar::CScriptVar(int val) {
	refs = 0;
	internalRefs = 0;
	recursionFlag = 0;
	recursionSet = 0;
	__proto__ = NULL;
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	init();
	setInt(val);
}

CScriptVar::CScriptVar(bool val) {
	refs = 0;
	internalRefs = 0;
	recursionFlag = 0;
	recursionSet = 0;
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

CScriptVar *CScriptVar::getParameter(int Idx) {
	CScriptVar *arguments = findChildOrCreate(TINYJS_ARGUMENTS_VAR)->var;
	return arguments->findChildOrCreate(int2string(Idx))->var;
}


CScriptVarLink *CScriptVar::findChild(const string &childName) {
	SCRIPTVAR_CHILDS::iterator it = Childs.find(childName);
	if(it != Childs.end())
		return it->second;
	return NULL;
}

CScriptVarLink *CScriptVar::findChildOrCreate(const string &childName, int varFlags) {
	CScriptVarLink *l = findChild(childName);
	if (l) return l;
	return addChild(childName, new CScriptVar(TINYJS_BLANK_DATA, varFlags));
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
	link->owned = true;
	link->owner = this;
	Childs[childName] = link;
	link->var->recoursionCheck(this);
	return link;
}
CScriptVarLink *CScriptVar::addChildNoDup(const std::string &childName, CScriptVar *child) {
	// if no child supplied, create one
	if (!child)
		child = new CScriptVar();

	CScriptVarLink *link = findChild(childName);
	if (link) {
		link->replaceWith(child);
	} else {
		link = new CScriptVarLink(child, childName);
		link->owned = true;
		link->owner = this;
		Childs[childName] = link;
		link->var->recoursionCheck(this);
	}

	return link;
}

bool CScriptVar::removeLink(CScriptVarLink *&link) {
	if (!link) return false;
#ifdef _DEBUG
	if(findChild(link->name) != link)
		ASSERT(0); // removeLink - the link is not atached to this var 
#endif
	Childs.erase(link->name);
	delete link;
	link = 0;
	return true;
}
void CScriptVar::removeAllChildren() {
	SCRIPTVAR_CHILDS::iterator it;
	for(it = Childs.begin(); it!= Childs.end(); ++it) {
		delete it->second;
	}
	Childs.clear();
}

CScriptVar *CScriptVar::getArrayIndex(int idx) {
	CScriptVarLink *link = findChild(int2string(idx));
	if (link) return link->var;
	else return new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_NULL); // undefined
}

void CScriptVar::setArrayIndex(int idx, CScriptVar *value) {
	string sIdx;
	CScriptVarLink *link = findChild(int2string(idx, sIdx));

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
	return highest+1;
}

int CScriptVar::getInt() {
	/* strtol understands about hex and octal */
	if (isInt()) return intData;
//  if (isNull()) return 0;
//  if (isUndefined()) return 0;
	if (isDouble()) return (int)doubleData;
	if (isString()) return strtol(data.c_str(),0,0);
	return 0;
}

bool CScriptVar::getBool() {
	if (isInt() || isBool()) return intData!=0;
//	if (isNull()) return 0;
//	if (isUndefined()) return 0;
	if (isDouble()) return doubleData!=0;
	if (isString()) return data.length()!=0;
	if (isInfinity()) return true;
	return false;
}

double CScriptVar::getDouble() {
	if (isDouble()) return doubleData;
	if (isInt() || isBool()) return intData;
//	if (isNull()) return 0.0;
//	if (isUndefined()) return 0.0;
	if (isString()) return strtod(data.c_str(),0);
	return 0.0; /* or NaN? */
}

const string &CScriptVar::getString() {
	/* Because we can't return a string that is generated on demand.
	 * I should really just use char* :) */
	static string s_null = "null";
	static string s_undefined = "undefined";
	static string s_NaN = "NaN";
	static string s_Infinity = "Infinity";
	static string s_true = "true";
	static string s_false = "false";
	if (isBool()) return getBool() ? s_true : s_false;
	if (isInt()) {
		return int2string(intData, data);
	}
	if (isDouble()) {
		char buffer[32];
		sprintf_s(buffer, sizeof(buffer), "%lf", doubleData);
		data = buffer;
		return data;
	}
	if (isNull()) return s_null;
	if (isNaN()) return s_NaN;
	if (isInfinity()) return s_Infinity;
	if (isUndefined()) return s_undefined;
	// are we just a string here?
	return data;
}

void CScriptVar::setInt(int val) {
	init();
	flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_INTEGER;
	intData = val;
}

void CScriptVar::setBool(bool val) {
	init();
	flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_BOOLEAN;
	intData = val?1:0;
}

void CScriptVar::setDouble(double val) {
	init();
	flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_DOUBLE;
	doubleData = val;
}

void CScriptVar::setString(const string &str) {
	// name sure it's not still a number or integer
	init();
	flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_STRING;
	data = str;
}

void CScriptVar::setUndefined() {
	// name sure it's not still a number or integer
	init();
	flags = (flags&~SCRIPTVAR_VARTYPEMASK) | SCRIPTVAR_UNDEFINED;
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
			case '+':			return new CScriptVar(da+db);
			case '-':			return new CScriptVar(da-db);
			case '*':			return new CScriptVar(da*db);
			case '/':
				if(db==0)
					if(da==0)	return new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_NAN);			// 0/0 = NaN
					else			return new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_INFINITY);	//  /0 = Infinity
				else				return new CScriptVar(da/db);
			case '&':			return new CScriptVar(dai&dbi);
			case '|':			return new CScriptVar(dai|dbi);
			case '^':			return new CScriptVar(dai^dbi);
			case '%':			return new CScriptVar(dai%dbi);
			case '~':			return new CScriptVar(~dai);
			case LEX_LSHIFT:	return new CScriptVar(dai<<dbi);
			case LEX_RSHIFT:	return new CScriptVar(dai>>dbi);
			case LEX_EQUAL:	return new CScriptVar(da==db);
			case LEX_NEQUAL:	return new CScriptVar(da!=db);
			case '<':			return new CScriptVar(da<db);
			case LEX_LEQUAL:	return new CScriptVar(da<=db);
			case '>':			return new CScriptVar(da>db);
			case LEX_GEQUAL:	return new CScriptVar(da>=db);
			default: throw new CScriptException("This operation not supported on the int datatype");
	}
}
static CScriptVar *String2NumericVar(string &numeric) {
	double d;
	char *endptr;//=NULL;
	int i = strtol(numeric.c_str(),&endptr,10);
	if(*endptr == 'x' || *endptr == 'X')
		i = strtol(numeric.c_str(),&endptr,16);
	if(*endptr == '\0')
		return new CScriptVar(i);
	if(*endptr=='.' || *endptr=='e' || *endptr=='E') {
		d = strtod(numeric.c_str(),&endptr);
		if(*endptr == '\0')
			return new CScriptVar(d);
	}
	return NULL;
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
	if((a->isNaN() || a->isUndefined() || b->isNaN() || b->isUndefined()) && !a->isString() && !b->isString()) {
		switch (op) {
			case LEX_NEQUAL:	return new CScriptVar(!(a->isUndefined() && b->isUndefined()));
			case LEX_EQUAL:	return new CScriptVar((a->isUndefined() && b->isUndefined()));
			case LEX_GEQUAL:	
			case LEX_LEQUAL:
			case '<':
			case '>':			return new CScriptVar(false);
			default:				return new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_NAN);
		}
	} else if (a->isNumeric() && b->isNumeric()) {
		if (!a->isDouble() && !b->isDouble()) {
			// use ints
			return DoMaths<int>(a, b, op);
		} else {
			// use doubles
			return DoMaths<double>(a, b, op);
		}
	} else if((a->isInfinity() || b->isInfinity()) && !a->isString() && !b->isString()) {
		switch (op) {
			case LEX_EQUAL:
			case LEX_GEQUAL:	
			case LEX_LEQUAL:	return new CScriptVar(a->isInfinity() == b->isInfinity());
			case LEX_NEQUAL:	return new CScriptVar(a->isInfinity() != b->isInfinity());
			case '<':
			case '>':			return new CScriptVar(false);
			case '/':
			case '-':			if(a->isInfinity() && b->isInfinity()) return new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_NAN);
			default:				return new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_INFINITY);
		}
	} else if (a->isArray()) {
		/* Just check pointers */
		switch (op) {
			case LEX_EQUAL:	return new CScriptVar(a==b);
			case LEX_NEQUAL:	return new CScriptVar(a!=b);
			default:				return new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_NAN);
		}
	} else if (a->isObject()) {
				/* Just check pointers */
		switch (op) {
			case LEX_EQUAL:	return new CScriptVar(a==b);
			case LEX_NEQUAL:	return new CScriptVar(a!=b);
			default:				return new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_NAN);
		}
	} else { //Strings
		string da = a->getString();
		string db = b->getString();
		if((a->isNumeric() || b->isNumeric()) && op != '+')
		{
			CScriptVar *ret = NULL;
			CScriptVar *a_num = String2NumericVar(da);
			CScriptVar *b_num = String2NumericVar(db);
			if(a_num && b_num)
				ret = a_num->mathsOp(b_num, op);
			if(a_num) delete a_num;
			if(b_num) delete b_num;
			if(ret) 
				return ret;
		}
		// use strings
		switch (op) {
			case '+':			return new CScriptVar(da+db, SCRIPTVAR_STRING);
			case LEX_EQUAL:	return new CScriptVar(da==db);
			case LEX_NEQUAL:	return new CScriptVar(da!=db);
			case '<':			return new CScriptVar(da<db);
			case LEX_LEQUAL:	return new CScriptVar(da<=db);
			case '>':			return new CScriptVar(da>db);
			case LEX_GEQUAL:	return new CScriptVar(da>=db);
			default:				return new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_NAN);
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

void CScriptVar::trace(const string &name) {
	trace(string(), name);
}
void CScriptVar::trace(string &indentStr, const string &name) {
	string indent = "  ";
	if(recursionSet) {
		if(recursionFlag) {
			int pos = recursionFlag>>12;
			string indent_r = indentStr.substr(0, pos+1) + string(indentStr.length()-(pos+1), '-');
			TRACE("%s'%s' = '%s' %s\n",
					indent_r.c_str(),
					name.c_str(),
					getString().c_str(),
					getFlagsAsString().c_str());

			if((recursionFlag&0xfff) == 1) {
				indentStr.replace(pos,1," ");
				recursionFlag = 0;
			} else
				recursionFlag--;
			return;
		} else if(!recursionSet->recursionPathBase || internalRefs>1) {
			indent = "| ";
			recursionSet->recursionPathBase = this;
		}
		recursionFlag = indentStr.length()<<12 | (internalRefs&0xfff) ;
	}
	TRACE("%s'%s' = '%s' %s\n",
			indentStr.c_str(),
			name.c_str(),
			getString().c_str(),
			getFlagsAsString().c_str());

	indentStr+=indent;
	for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it != Childs.end(); ++it) {
		it->second->var->trace(indentStr, it->first);
	}
	if(recursionSet) {
		recursionSet->recursionPathBase = 0;
		recursionFlag = 0;
	}
	indentStr = indentStr.substr(0, indentStr.length()-2);
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
	int count = Childs.size();
	for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it != Childs.end(); ++it) {
		funcStr << it->first;
		if(--count) funcStr << ", ";
	}
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
string CScriptVar::getVarType() {
	if(this->isBool())
		return "boolean";
	else if(this->isFunction())
		return "function";
	else if(this->isNumeric() || this->isInfinity() || this->isNaN())
		return "number";
	else if(this->isString())
		return "string";
	else if(this->isUndefined())
		return "undefined";
	return "object"; // Objcect / Array / null
}

void CScriptVar::getJSON(ostringstream &destination, const string linePrefix) {
	if (isObject()) {
		string indentedLinePrefix = linePrefix+"  ";
		// children - handle with bracketed list
		destination << "{ \n";
		int count = Childs.size();
		for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it != Childs.end(); ++it) {
			destination << indentedLinePrefix;
			if (isAlphaNum(it->first))
				destination  << it->first;
			else
				destination  << getJSString(it->first);
			destination  << " : ";
			it->second->var->getJSON(destination, indentedLinePrefix);
			if (--count) destination  << ",\n";
		}
		destination << "\n" << linePrefix << "}";
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
	if(recursionSet) recursionSet->sumRefs++;
	return this;
}
RECURSION_SET_VAR *CScriptVar::unrefInternal(){
	RECURSION_SET_VAR *ret=0;
	if(internalRefs) {
		ASSERT(recursionSet);
		internalRefs--;
		recursionSet->sumInternalRefs--;
		if(internalRefs==0) {
			recursionSet->sumRefs -= refs;
			recursionSet->recursionSet.erase(this);
			if(recursionSet->recursionSet.size()) {
				for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it!= Childs.end(); ++it) {
					if(it->second->var->recursionSet == recursionSet)
						it->second->var->unrefInternal();
				}
				ret = recursionSet;
			} else 
				delete recursionSet;
			recursionSet = 0;
		}
	}
	return ret;
}
void CScriptVar::unref(CScriptVar* Owner) {
	refs--;
	ASSERT(refs>=0); // printf("OMFG, we have unreffed too far!\n");
#if 1
	if(recursionSet) { // this Var is in a Set
		recursionSet->sumRefs--;
		if(Owner && Owner->recursionSet == recursionSet) { // internal Ref
			if(internalRefs==1) {
				if(refs) {
					RECURSION_SET_VAR *old_set = unrefInternal();

					if(old_set) { // we have breaked a recursion bu the recursion Set is not destroyed
						// we needs a rededection of recursions for all Vars in the Set;
						// first we destroy the Set
						for(RECURSION_SET::iterator it= old_set->recursionSet.begin(); it!=old_set->recursionSet.end(); ++it) {
							(*it)->internalRefs = 0;
							(*it)->recursionSet = 0;
						}
						// now we can rededect recursions
						for(RECURSION_SET::iterator it= old_set->recursionSet.begin(); it!=old_set->recursionSet.end(); ++it) {
							if((*it)->recursionSet == 0)
								(*it)->recoursionCheck();
						}
						delete old_set;
					}

				} else {
					removeAllChildren();
					internalRefs--;
					recursionSet->sumInternalRefs--;
					recursionSet->recursionSet.erase(this);
					if(!recursionSet->recursionSet.size())
						delete recursionSet;
					delete this;
				}
			}// otherwise nothing to do
		}
		else { //external Ref
			ASSERT(refs); // by a Set it is not possible to remove an external Ref as the last ref
			if(recursionSet->sumRefs==recursionSet->sumInternalRefs) {
				SCRIPTVAR_CHILDS copyOfChilds(Childs);
				Childs.clear();
				SCRIPTVAR_CHILDS::iterator it;
				for(it = copyOfChilds.begin(); it!= copyOfChilds.end(); ++it) {
					delete it->second;
				}
				copyOfChilds.clear();
			} // otherwise nothing to do
		}
	}
	else 
#endif
		if (refs==0) {
		delete this;
	}
}

int CScriptVar::getRefs() {
	return refs;
}

RECURSION_VECT::iterator find_last(RECURSION_VECT &Vector, const RECURSION_VECT::value_type &Key) {
	for(RECURSION_VECT::reverse_iterator rit=Vector.rbegin(); rit!=Vector.rend(); ++rit) {
		if(*rit==Key) return --rit.base();
	}
	return Vector.end();
}

void CScriptVar::recoursionCheck(CScriptVar *Owner) {
	if(recursionSet && Owner &&recursionSet == Owner->recursionSet) { // special thing for self-linking
		Owner->internalRefs++;
		Owner->recursionSet->sumInternalRefs++;
	} else {
		RECURSION_VECT recursionPath;
		recoursionCheck(recursionPath);
	}
}
void CScriptVar::recoursionCheck(RECURSION_VECT &recursionPath)
{
	if(recursionFlag || (recursionSet && recursionSet->recursionPathBase)) { // recursion found - create a new set
		RECURSION_SET_VAR *new_set;
		RECURSION_VECT::iterator it;

		if(recursionSet) {					// recursion starts by a Set
			new_set = recursionSet;			// we use the old Set as new Set
			new_set->sumInternalRefs++;	// one internal
			this->internalRefs++;			// new internal reference found
			// find the Var of the Set tat is in the Path 
			// +1, because the Set is allready in the new Set
			it = find_last(recursionPath, recursionSet->recursionPathBase)+1;
		} else {													// recursion starts by a Var
			new_set = new RECURSION_SET_VAR(this);	// create a new Set(marks it as "in the Path", with "this as start Var)
			// find the Var in the Path
			it = find_last(recursionPath, this);
		}
		// insert the Path begining by this Var or the next Var after dis Set
		// in the new_set and removes it from the Path
		for(; it!= recursionPath.end(); ++it) {
			if((*it)->recursionSet) {											// insert an existing Set
				RECURSION_SET_VAR *old_set = (*it)->recursionSet;
				new_set->sumInternalRefs += old_set->sumInternalRefs;	// for speed up; we adds the sum of internal refs and the sum of
				new_set->sumRefs += old_set->sumRefs;						// refs to the new Set here instead rather than separately for each Var
				new_set->sumInternalRefs++;									// a new Set in a Set is linked 
				(*it)->internalRefs++;											// over an internale reference
				// insert all Vars of the old Set in the new Set
				for(RECURSION_SET::iterator set_it = old_set->recursionSet.begin(); set_it!= old_set->recursionSet.end(); ++set_it) {
					new_set->recursionSet.insert(*set_it);	// insert this Var to the Set
					(*set_it)->recursionSet = new_set;		// the Var is now in a Set
				}
				delete old_set;
			} else {											// insert this Var in the new set
				new_set->sumInternalRefs++;			// a new Var in a Set is linked 
				(*it)->internalRefs++;					// over an internale reference
				new_set->sumRefs += (*it)->refs;		// adds the Var refs to the Set refs
				new_set->recursionSet.insert(*it);	// insert this Var to the Set
				(*it)->recursionSet = new_set;		// the Var is now in a Set
				(*it)->recursionFlag = 0;				// clear the Var-recursionFlag, because the Var in now in a Set
			}
		}
		// all Vars inserted in the new Set 
		// remove from Path all Vars from the end up to the start of Set (without the start of Set)
		while(recursionPath.back() != new_set->recursionPathBase)
			recursionPath.pop_back(); 
	} else if(recursionSet) {								// found a recursionSet
		recursionPath.push_back(this);	// push "this" in the Path
		recursionSet->recursionPathBase = this;		// of all Vars in the Set is "this" in the Path
		RECURSION_SET_VAR *old_set = recursionSet;
		// a recursionSet is like an one&only Var 
		// goes to all Cilds of the Set
		RECURSION_SET Set(recursionSet->recursionSet);
		for(RECURSION_SET::iterator set_it = Set.begin(); set_it != Set.end(); ++set_it) {
			for(SCRIPTVAR_CHILDS::iterator it = (*set_it)->Childs.begin(); it != (*set_it)->Childs.end(); ++it) {
				if(it->second->var->recursionSet != recursionSet)
					it->second->var->recoursionCheck(recursionPath);
			}
		}
		if(old_set == recursionSet) { // old_set == recursionSet meens this Set is *not* included in an other Set
			recursionSet->recursionPathBase = 0;
			recursionPath.pop_back();
		} // otherwise this Set is included in an other Set and is allready removed from Path
	} else {
		recursionPath.push_back(this);	// push "this" in the Path
		recursionFlag = 1;									// marked this Var as "the Var is in the Path"
		// goes to all Cilds of the Var
		for(SCRIPTVAR_CHILDS::iterator it = Childs.begin(); it != Childs.end(); ++it) {
			it->second->var->recoursionCheck(recursionPath);
		}
		if(recursionFlag) { // recursionFlag!=0 meens this Var is not included in a Set
			recursionFlag = 0;
			ASSERT(recursionPath.back() == this);
			recursionPath.pop_back();
		} else { // otherwise this Var is included in a Set and is allready removed from Path
			ASSERT(recursionSet && recursionSet->recursionPathBase);
			if(recursionSet->recursionPathBase == this) { // the Var is the Base of the Set
				recursionSet->recursionPathBase = 0;
				ASSERT(recursionPath.back() == this);
				recursionPath.pop_back();
			}
		}
	}
}

// ----------------------------------------------------------------------------------- CSCRIPT

CTinyJS::CTinyJS(bool TwoPass) {
	twoPass=TwoPass;
	funcOffset = 0;
	l = NULL;
	runtimeFlags = 0;
	root = (new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT))->ref();
	scopes.push_back(root);
	// Add built-in classes
	stringClass = (new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT))->ref();
	arrayClass = (new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT))->ref();
	objectClass = (new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT))->ref();
	root->addChild("String", stringClass);
	root->addChild("Array", arrayClass);
	root->addChild("Object", objectClass);
	exeption = NULL;
	addNative("function eval(jsCode)", this, &CTinyJS::scEval); // execute the given string and return the resul
}

CTinyJS::~CTinyJS() {
	ASSERT(!l);
	scopes.clear();
	stringClass->unref(0);
	arrayClass->unref(0);
	objectClass->unref(0);
	root->unref(0);

#if DEBUG_MEMORY
	show_allocated();
#endif
}

void CTinyJS::throwError(bool &execute, const string &message, int pos /*=-1*/) {
	if(execute && (runtimeFlags & RUNTIME_CANTHROW)) {
		exeption = (new CScriptVar(message))->ref();
		runtimeFlags |= RUNTIME_THROW;
		execute = false;
		return;
	}
	throw new CScriptException(message, pos);
}

void CTinyJS::trace() {
	root->trace();
}

void CTinyJS::execute(const string &code) {
	evaluateComplex(code);
}

CScriptVarLink CTinyJS::evaluateComplex(const string &code) {
	CScriptLex *oldLex = l;

	l = new CScriptLex(code);
	CScriptVarSmartLink v;
	try {
		bool execute = true;
		bool noexecute = false;
		SET_RUNTIME_PASS_SINGLE;
		if(twoPass)
		{
			SET_RUNTIME_PASS_TWO_1;
			do {
				statement(noexecute);
				while (l->tk==';') l->match(';'); // skip empty statements
			} while (l->tk!=LEX_EOF);
			l->reset();
			SET_RUNTIME_PASS_TWO_2;
		}
		do {
			v = statement(execute);
			while (l->tk==';') l->match(';'); // skip empty statements
		} while (l->tk!=LEX_EOF);
	} catch (CScriptException *e) {
		runtimeFlags = 0; // clean up runtimeFlags
		ostringstream msg;
		msg << "Error " << e->text << " at " << l->getPosition(e->pos);
		delete e;
		delete l;
		l = oldLex;
		throw new CScriptException(msg.str());
	}
	delete l;
	l = oldLex;

	if (v) {
		CScriptVarLink r = *v;
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
	CScriptVar *arguments = new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT);
	funcVar->addChild(TINYJS_ARGUMENTS_VAR, arguments);
	int idx = 0;
	while (l->tk!=')') {
		arguments->addChild(int2string(idx++), new CScriptVar(l->tkStr));
		l->match(LEX_ID);
		if (l->tk!=')') l->match(',');
	}
	l->match(')');
	funcVar->addChild("length", new CScriptVar(idx));
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

CScriptVarSmartLink CTinyJS::parseFunctionDefinition() {
	// actually parse a function...
	int oldFuncOffset = funcOffset;
	funcOffset = l->getDataPos();
	l->match(LEX_R_FUNCTION);
	string funcName = TINYJS_TEMP_NAME;
	/* we can have functions without names */
	if (l->tk==LEX_ID) {
		funcName = l->tkStr;
		l->match(LEX_ID);
	}
	funcOffset -= l->getDataPos();
	CScriptVarSmartLink funcVar = new CScriptVarLink(new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_FUNCTION), funcName);
	parseFunctionArguments(funcVar->var);
	funcOffset += l->getDataPos();
	bool noexecute = false;
	if(IS_RUNTIME_PASS_TWO_2) {
		SAVE_RUNTIME_PASS;
		SET_RUNTIME_PASS_SINGLE;
		block(noexecute);
		RESTORE_RUNTIME_PASS;
	} else if(IS_RUNTIME_PASS_TWO_1) {
		int funcBegin = l->tokenStart;
		CScriptVar *locale = new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT);
		CScriptVarLink *anonymous_link = locale->addChild(TINYJS_ANONYMOUS_VAR, new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT));
		CScriptVarLink *locale_link = funcVar->var->addChild(TINYJS_LOKALE_VAR, locale);
		locale_link->hidden = locale_link->dontEnumerable = true;
		scopes.push_back(locale);
		try {
			block(noexecute);
		} catch(CScriptException *e) {
			scopes.pop_back();
			throw e;
		}
		scopes.pop_back();
		funcVar->var->data = l->getSubString(funcBegin);
		if(anonymous_link->var->Childs.size() == 0)
			locale->removeLink(anonymous_link);
	} else {
		int funcBegin = l->tokenStart;
		block(noexecute);
		funcVar->var->data = l->getSubString(funcBegin);
	}
	funcOffset = oldFuncOffset;

	return funcVar;
}

class CScriptEvalException {
public:
	int runtimeFlags;
	CScriptEvalException(int RuntimeFlags) : runtimeFlags(RuntimeFlags){}
};

// Precedence 2, Precedence 1 & literals
CScriptVarSmartLink CTinyJS::factor(bool &execute) {
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
	if (l->tk==LEX_R_INFINITY) {
		l->match(LEX_R_INFINITY);
		return new CScriptVarLink(new CScriptVar(TINYJS_BLANK_DATA,SCRIPTVAR_INFINITY));
	}
	if (l->tk==LEX_R_NAN) {
		l->match(LEX_R_NAN);
		return new CScriptVarLink(new CScriptVar(TINYJS_BLANK_DATA,SCRIPTVAR_NAN));
	}
	if (l->tk==LEX_ID || l->tk=='(') {
		CScriptVarSmartLink a;
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
			if(l->tkStr == "this") {
				a = new CScriptVarLink(this->root, l->tkStr);
				parent = this->root;
			} else
				a = new CScriptVarLink(new CScriptVar(), l->tkStr);
		}
		string path = a->name;
		if(op==LEX_ID)
			l->match(LEX_ID);
		while (l->tk=='(' || l->tk=='.' || l->tk=='[') {
			if(execute && a->var->isUndefined()) {
				throwError(execute, path + " is undefined", l->tokenEnd);
			}
			if (l->tk=='(') { // ------------------------------------- Function Call
				if (execute) {
					if (!a->var->isFunction()) {
						string errorMsg = "Expecting '";
						errorMsg = errorMsg + a->name + "' to be a function";
						throw new CScriptException(errorMsg.c_str());
					}
					l->match('('); path += '(';
					// create a new symbol table entry for execution of this function
					CScriptVarSmartLink functionRoot(new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_FUNCTION));
					if (parent)
						functionRoot->var->addChildNoDup("this", parent);
//					else
//						functionRoot->var->addChildNoDup("this", this->root);//new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT)); // always add a this-Object
//						functionRoot->var->addChildNoDup("this", new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT)); // always add a this-Object
					// insert locale
					CScriptVarLink *__locale__ = a->var->findChild(TINYJS_LOKALE_VAR);
					if(__locale__) {
						for(SCRIPTVAR_CHILDS::iterator it = __locale__->var->Childs.begin(); it != __locale__->var->Childs.end(); ++it) {
							functionRoot->var->addChild(it->first , it->second->var->deepCopy());
						}
					}
					// grab in all parameters
					CScriptVarLink *arguments_proto = a->var->findChild("arguments");
					int length_proto = arguments_proto ? arguments_proto->var->getChildren() : 0;
					CScriptVar *arguments = new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT);
					int arguments_idx = 0;
					for(; l->tk!=')' || arguments_idx<length_proto; ++arguments_idx) {
						CScriptVarSmartLink value;
						if (l->tk!=')') {
							try {
								value = assignment(execute);
								path += value->var->getString();
							} catch (CScriptException *e) {
								delete arguments;
								throw e;
							}

						} else
							value = new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_UNDEFINED);
						if (execute) {
							string arguments_idx_str; int2string(arguments_idx, arguments_idx_str);
							if (value->var->isBasic()) {
								// pass by value
								value = arguments->addChild(arguments_idx_str , value->var->deepCopy());
							} else {
								// pass by reference
								value = arguments->addChild(arguments_idx_str, value->var);
							}
							CScriptVarLink *argument_name;
							if(arguments_proto && (argument_name = arguments_proto->var->findChild(arguments_idx_str)))
								functionRoot->var->addChildNoDup(argument_name->var->getString(), value->var);
						}
						if (l->tk!=')') { l->match(','); path+=','; }
					}
					l->match(')'); path+=')';
					arguments->addChild("length", new CScriptVar(arguments_idx));
					functionRoot->var->addChild("arguments", arguments);

					// setup a return variable
					CScriptVarSmartLink returnVar;
					if(execute) {
						int old_function_runtimeFlags = runtimeFlags; // save runtimFlags
						runtimeFlags &= ~RUNTIME_LOOP_MASK; // clear LOOP-Flags
						// execute function!
						// add the function's execute space to the symbol table so we can recurse
//						CScriptVarLink *returnVarLink = functionRoot->addChild(TINYJS_RETURN_VAR);
						scopes.push_back(functionRoot->var);
						try {
							if (a->var->isNative()) {
								ASSERT(a->var->jsCallback);
								try {
									if (a->var->isNative_ClassMemberFnc())
										(*a->var->jsCallbackClass)(functionRoot->var, a->var->jsCallbackUserData);
									else
										a->var->jsCallback(functionRoot->var, a->var->jsCallbackUserData);
									// special thinks for eval() // set execute to false when runtimeFlags setted
									runtimeFlags = old_function_runtimeFlags | (runtimeFlags & RUNTIME_THROW); // restore runtimeFlags
									if(runtimeFlags & RUNTIME_THROW) {
										execute = false;
									}
								} catch (CScriptEvalException *e) {
									runtimeFlags = old_function_runtimeFlags; // restore runtimeFlags
									if(e->runtimeFlags & RUNTIME_BREAK) {
										if(runtimeFlags & RUNTIME_CANBREAK) {
											runtimeFlags |= RUNTIME_BREAK;
											execute = false;
										}
										else
											throw new CScriptException("'break' must be inside loop or switch");
									} else if(e->runtimeFlags & RUNTIME_CONTINUE) {
										if(runtimeFlags & RUNTIME_CANCONTINUE) {
											runtimeFlags |= RUNTIME_CONTINUE;
											execute = false;
										}
										else
											throw new CScriptException("'continue' must be inside loop");
									}
								} catch (CScriptVarLink *v) {
									CScriptVarSmartLink e = v;
									if(runtimeFlags & RUNTIME_CANTHROW) {
										runtimeFlags |= RUNTIME_THROW;
										execute = false;
										this->exeption = e->var->ref();
									}
									else
										throw new CScriptException("uncaught exeption: '"+e->var->getString()+"' in: "+a->name+"()");
								}
							} else {
								/* we just want to execute the block, but something could
								 * have messed up and left us with the wrong ScriptLex, so
								 * we want to be careful here... */
								CScriptLex *oldLex = l;
								CScriptLex *newLex = new CScriptLex(a->var->getString());
								l = newLex;
								try {
									SET_RUNTIME_CANRETURN;
									SAVE_RUNTIME_PASS;
									if(__locale__) 
										SET_RUNTIME_PASS_TWO_2;
									block(execute);
									RESTORE_RUNTIME_PASS;
									// because return will probably have called this, and set execute to false
									runtimeFlags = old_function_runtimeFlags | (runtimeFlags & RUNTIME_THROW); // restore runtimeFlags
									if(!(runtimeFlags & RUNTIME_THROW)) {
										execute = true;
									}
								} catch (CScriptException *e) {
									delete newLex;
									l = oldLex;
									throw e;
								}
								delete newLex;
								l = oldLex;
							}
						} catch (CScriptException *e) {
							scopes.pop_back();
							throw e;
						}

						scopes.pop_back();
						/* get the real return var before we remove it from our function */
						if(returnVar = functionRoot->var->findChild(TINYJS_RETURN_VAR))
							returnVar = new CScriptVarLink(returnVar->var);
//						returnVar = functionRoot->getReturnVar();
//						functionRoot->removeLink(returnVarLink);
					}
					if (returnVar)
						a = returnVar;
					else
						a = new CScriptVarLink(
						new CScriptVar());
				} else {
					// function, but not executing - just parse args and be done
					l->match('(');
					while (l->tk != ')') {
						CScriptVarSmartLink value = base(execute);
						if (l->tk!=')') l->match(',');
					}
					l->match(')');
//					if (l->tk == '{') {
//						block(execute, SINGLE_PASS);
//					}
				}
			} else if (l->tk == '.' || l->tk == '[') { // ------------------------------------- Record Access
				string name;
				if(l->tk == '.') {
					l->match('.');
					name = l->tkStr;
					path += "."+name;
					l->match(LEX_ID);
				} else {
					l->match('[');
					CScriptVarSmartLink index = expression(execute);
					name = index->var->getString();
					path += "["+name+"]";
					l->match(']');
				}

				if (execute) {
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
							//child = a->var->addChild(name);
						}
					}
					if(child) {
						parent = a->var;
						a = child;
					} else {
						CScriptVar *owner = a->var;
						a = new CScriptVarLink(new CScriptVar(), name);
						a->owner = owner;
					}
				}
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
			CScriptVarSmartLink a = assignment(execute);
			if (execute) {
				contents->addChild(id, a->var);
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
			CScriptVarSmartLink a = assignment(execute);
			if (execute) {
				contents->addChild(int2string(idx), a->var);
			}
			// no need to clean here, as it will definitely be used
			if (l->tk != ']') l->match(',');
			idx++;
		}
		l->match(']');
		return new CScriptVarLink(contents);
	}
	if (l->tk==LEX_R_FUNCTION) {
		CScriptVarLink *anonymous = scopes.back()->findChild(TINYJS_ANONYMOUS_VAR);
		string anonymous_name = int2string(l->getDataPos()-this->funcOffset);
		CScriptVarSmartLink funcVar = parseFunctionDefinition();
		this->runtimeFlags;
//				funcName = anonymous ? int2string(l->tokenStart) : l->tkStr;

		funcVar->name = TINYJS_TEMP_NAME; // allways anonymous
		if(IS_RUNTIME_PASS_TWO_1)
		{
			CScriptVarLink *anonymous = scopes.back()->findChildOrCreate(TINYJS_ANONYMOUS_VAR, SCRIPTVAR_OBJECT);
			anonymous->var->addChild(anonymous_name, funcVar->var);
		}
		if(IS_RUNTIME_PASS_TWO_2)
		{
			CScriptVarLink *anonymous = scopes.back()->findChild(TINYJS_ANONYMOUS_VAR);
			if(anonymous) 
				funcVar->replaceWith(anonymous->var->findChild(anonymous_name));
		}
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
// R-L: Precedence 4 (TODO: add typeof, void, delete) & Precedence 3 
CScriptVarSmartLink CTinyJS::unary(bool &execute) {
	CScriptVarSmartLink a;
	if (l->tk=='-') {
		l->match('-');
		a = unary(execute);
		if (execute) {
			CheckRightHandVar(execute, a);
			CScriptVar zero(0);
			a = zero.mathsOp(a->var, '-');
		}
	} else if (l->tk=='+') {
		l->match('+');
		a = unary(execute);
		CheckRightHandVar(execute, a);
	} else if (l->tk=='!') {
		l->match('!'); // binary not
		a = unary(execute);
		if (execute) {
			CheckRightHandVar(execute, a);
			CScriptVar zero(0);
			a = a->var->mathsOp(&zero, LEX_EQUAL);
		}
	} else if (l->tk=='~') {
		l->match('~'); // binary neg
		a = unary(execute);
		if (execute) {
			CheckRightHandVar(execute, a);
			a = a->var->mathsOp(NULL, '~');
		}
	} else if (l->tk==LEX_R_TYPEOF) {
		l->match(LEX_R_TYPEOF); // void
		a = unary(execute);
		if (execute) {
			// !!! no right-hand-check by delete
			a = new CScriptVarLink(new CScriptVar(a->var->getVarType(),SCRIPTVAR_STRING));
		}
	} else if (l->tk==LEX_R_VOID) {
		l->match(LEX_R_VOID); // void
		a = unary(execute);
		if (execute) {
			CheckRightHandVar(execute, a);
			a = new CScriptVarLink(new CScriptVar(TINYJS_BLANK_DATA,SCRIPTVAR_UNDEFINED));
		}
	} else if (l->tk==LEX_R_DELETE) {
		l->match(LEX_R_DELETE); // delete
		a = unary(execute);
		if (execute) {
			// !!! no right-hand-check by delete
			if(a->owned && !a->dontDelete) {
				a->owner->removeLink(a.getLink());	// removes the link from owner
				a = new CScriptVarLink(new CScriptVar(true));
			}
			else
				a = new CScriptVarLink(new CScriptVar(false));
		}
	} else if (l->tk==LEX_PLUSPLUS || l->tk==LEX_MINUSMINUS) {
		int op = l->tk;
		l->match(l->tk); // pre increment/decrement
		a = factor(execute);
		if (execute) {
			if(a->owned)
			{
				CScriptVar one(1);
				CScriptVar *res = a->var->mathsOp(&one, op==LEX_PLUSPLUS ? '+' : '-');
				// in-place add/subtract
				a->replaceWith(res);
				a = res;
			}
		}
	} else {
		a = factor(execute);
	}
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
				a = res;
				res->unref(0);
			}
		}
	}
	return a;
}

// L->R: Precedence 5
CScriptVarSmartLink CTinyJS::term(bool &execute) {
	CScriptVarSmartLink a = unary(execute);
	if (l->tk=='*' || l->tk=='/' || l->tk=='%') {
		CheckRightHandVar(execute, a);
		while (l->tk=='*' || l->tk=='/' || l->tk=='%') {
			int op = l->tk;
			l->match(l->tk);
			CScriptVarSmartLink b = unary(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				a = a->var->mathsOp(b->var, op);
			}
		}
	}
	return a;
}

// L->R: Precedence 6
CScriptVarSmartLink CTinyJS::expression(bool &execute) {
	CScriptVarSmartLink a = term(execute);
	if (l->tk=='+' || l->tk=='-') {
		CheckRightHandVar(execute, a);
		while (l->tk=='+' || l->tk=='-') {
			int op = l->tk;
			l->match(l->tk);
			CScriptVarSmartLink b = term(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				 // not in-place, so just replace
				 a = a->var->mathsOp(b->var, op);
			}
		}
	}
	return a;
}

// L->R: Precedence 7 (TODO: add >>>)
CScriptVarSmartLink CTinyJS::binary_shift(bool &execute) {
	CScriptVarSmartLink a = expression(execute);
	if (l->tk==LEX_LSHIFT || l->tk==LEX_RSHIFT) {
		CheckRightHandVar(execute, a);
		while (l->tk==LEX_LSHIFT || l->tk==LEX_RSHIFT) {
			int op = l->tk;
			l->match(l->tk);

			CScriptVarSmartLink b = expression(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, a);
				 // not in-place, so just replace
				 a = a->var->mathsOp(b->var, op);
			}
		}
	}
	return a;
}
// L->R: Precedence 9 & Precedence 8 (TODO: add instanceof and in)
CScriptVarSmartLink CTinyJS::relation(bool &execute, int set, int set_n) {
	CScriptVarSmartLink a = set_n ? relation(execute, set_n, 0) : binary_shift(execute);
	if ((set==LEX_EQUAL && (l->tk==LEX_EQUAL || l->tk==LEX_NEQUAL || l->tk==LEX_TYPEEQUAL || l->tk==LEX_NTYPEEQUAL))
				||	(set=='<' && (l->tk==LEX_LEQUAL || l->tk==LEX_GEQUAL || l->tk=='<' || l->tk=='>' || l->tk == LEX_R_IN))) {
		CheckRightHandVar(execute, a);
		while ((set==LEX_EQUAL && (l->tk==LEX_EQUAL || l->tk==LEX_NEQUAL || l->tk==LEX_TYPEEQUAL || l->tk==LEX_NTYPEEQUAL))
					||	(set=='<' && (l->tk==LEX_LEQUAL || l->tk==LEX_GEQUAL || l->tk=='<' || l->tk=='>' || l->tk == LEX_R_IN))) {
			int op = l->tk;
			l->match(l->tk);
			CScriptVarSmartLink b = set_n ? relation(execute, set_n, 0) : binary_shift(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				if(op == LEX_R_IN) {
					a = new CScriptVarLink(new CScriptVar( b->var->findChild(a->var->getString())!= 0 ));
				} else
					a = a->var->mathsOp(b->var, op);
			}
		}
	}
	return a;
}

// L->R: Precedence 12, Precedence 11 & Precedence 10
CScriptVarSmartLink CTinyJS::logic_binary(bool &execute, int op, int op_n1, int op_n2) {
	CScriptVarSmartLink a = op_n1 ? logic_binary(execute, op_n1, op_n2, 0) : relation(execute);
	if (l->tk==op) {
		CheckRightHandVar(execute, a);
		while (l->tk==op) {
			l->match(l->tk);
			CScriptVarSmartLink b = op_n1 ? logic_binary(execute, op_n1, op_n2, 0) : relation(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				a = a->var->mathsOp(b->var, op);
			}
		}
	}
	return a;
}

// L->R: Precedence 14 & Precedence 13
CScriptVarSmartLink CTinyJS::logic(bool &execute, int op, int op_n) {
	CScriptVarSmartLink a = op_n ? logic(execute, op_n, 0) : logic_binary(execute);
	if (l->tk==op) {
		CheckRightHandVar(execute, a);
		while (l->tk==op) {
			CheckRightHandVar(execute, a);
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
			CScriptVarSmartLink b = op_n ? logic(shortCircuit ? noexecute : execute, op_n, 0) : logic_binary(shortCircuit ? noexecute : execute); // L->R
			CheckRightHandVar(execute, b);
			if (execute && !shortCircuit) {
				if (boolean) {
					CheckRightHandVar(execute, b);
					a = new CScriptVar(a->var->getBool());
					b = new CScriptVar(b->var->getBool());
				}
				a = a->var->mathsOp(b->var, bop);
			}
		}
	}
	return a;
}

// R->L: Precedence 15
CScriptVarSmartLink CTinyJS::condition(bool &execute)
{
	CScriptVarSmartLink a = logic(execute);
	if (l->tk=='?')
	{
		CheckRightHandVar(execute, a);
		l->match(l->tk);
		bool cond = a->var->getBool();
		CScriptVarSmartLink b;
		bool noexecute=false;
		a = condition(cond ? execute : noexecute ); // R->L
		CheckRightHandVar(execute, a);
		l->match(':');
		b = condition(cond ? noexecute : execute); // R-L
		CheckRightHandVar(execute, b);
		if(!cond)
			a=b;
	}
	return a;
}
	
// R->L: Precedence 16
CScriptVarSmartLink CTinyJS::assignment(bool &execute) {
	CScriptVarSmartLink lhs = condition(execute);
	if (l->tk=='=' || l->tk==LEX_PLUSEQUAL || l->tk==LEX_MINUSEQUAL || 
				l->tk==LEX_ASTERISKEQUAL || l->tk==LEX_SLASHEQUAL || l->tk==LEX_PERCENTEQUAL ||
				l->tk==LEX_LSHIFTEQUAL || l->tk==LEX_RSHIFTEQUAL ||
				l->tk==LEX_ANDEQUAL || l->tk==LEX_XOREQUAL || l->tk==LEX_OREQUAL) {
		int op = l->tk;
		int pos = l->tokenEnd;
		l->match(l->tk);
		CScriptVarSmartLink rhs = assignment(execute); // R->L
		if (execute) {
			if (!lhs->owned && lhs->name.length()==0) {
				throw new CScriptException("invalid assignment left-hand side", pos);
			} else if (op != '=' && !lhs->owned) {
				throwError(execute, lhs->name + " is not defined", pos);
			}
			if (op=='=') {
				if (!lhs->owned) {
					CScriptVarLink *realLhs;
					if(lhs->owner)
						realLhs = lhs->owner->addChildNoDup(lhs->name, lhs->var);
					else
						realLhs = root->addChildNoDup(lhs->name, lhs->var);
					lhs = realLhs;
				}
				lhs << rhs;
			} else if (op==LEX_PLUSEQUAL)
				lhs->replaceWith(lhs->var->mathsOp(rhs->var, '+'));
			else if (op==LEX_MINUSEQUAL)
				lhs->replaceWith(lhs->var->mathsOp(rhs->var, '-'));
			else if (op==LEX_ASTERISKEQUAL)
				lhs->replaceWith(lhs->var->mathsOp(rhs->var, '*'));
			else if (op==LEX_SLASHEQUAL)
				lhs->replaceWith(lhs->var->mathsOp(rhs->var, '/'));
			else if (op==LEX_PERCENTEQUAL)
				lhs->replaceWith(lhs->var->mathsOp(rhs->var, '%'));
			else if (op==LEX_LSHIFTEQUAL)
				lhs->replaceWith(lhs->var->mathsOp(rhs->var, LEX_LSHIFT));
			else if (op==LEX_RSHIFTEQUAL)
				lhs->replaceWith(lhs->var->mathsOp(rhs->var, LEX_RSHIFT));
			else if (op==LEX_ANDEQUAL)
				lhs->replaceWith(lhs->var->mathsOp(rhs->var, '&'));
			else if (op==LEX_OREQUAL)
				lhs->replaceWith(lhs->var->mathsOp(rhs->var, '|'));
			else if (op==LEX_XOREQUAL)
				lhs->replaceWith(lhs->var->mathsOp(rhs->var, '^'));
		}
	}
	else 
		CheckRightHandVar(execute, lhs);
	return lhs;
}
// L->R: Precedence 17
CScriptVarSmartLink CTinyJS::base(bool &execute) {
	CScriptVarSmartLink a;
	for(;;)
	{
		a = assignment(execute); // L->R
		if (l->tk == ',') {
			l->match(',');
		} else
			break;
	}
	return a;
}
void CTinyJS::block(bool &execute) {
	l->match('{');
	if (execute || IS_RUNTIME_PASS_TWO_1) {
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

CScriptVarSmartLink CTinyJS::statement(bool &execute) {
	CScriptVarSmartLink ret;
	if (l->tk=='{') {
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
			CScriptVarSmartLink a;
			string var = l->tkStr;
			l->match(LEX_ID);
			if (execute || IS_RUNTIME_PASS_TWO_1) {
				a = scopes.back()->findChildOrCreate(var);
				a->dontDelete = true;
			}// sort out initialiser
			if (l->tk == '=') {
				l->match('=');
				CScriptVarSmartLink var = assignment(execute);
				if (execute)
					a << var;
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
		CScriptVarSmartLink var = base(execute);
		l->match(')');
		bool cond = execute && var->var->getBool();
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
		if(IS_RUNTIME_PASS_TWO_1)
		{
			statement(execute);
			l->match(LEX_R_WHILE);
			l->match('(');
			base(execute);
			l->match(')');
			l->match(';');
		} else {
			bool loopCond = true; 
			bool old_execute = execute;
			int old_loop_runtimeFlags = runtimeFlags & RUNTIME_LOOP_MASK;
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
					// break or continue
					if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
					{
						execute = old_execute;
						if(runtimeFlags & RUNTIME_BREAK)
							loopCond = false;
						runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
					}
					// other stuff e.g return, throw
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
				CScriptVarSmartLink cond = base(execute);
				loopCond = execute && cond->var->getBool();
				if(!whileCond)
				{
					whileCond = l->getSubLex(whileCondStart);
					l->match(')');
					l->match(';');
					oldLex = l;
				}
			}
			runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | old_loop_runtimeFlags;
			l = oldLex;
			delete whileCond;
			delete whileBody;

			if (loopCount<=0) {
				root->trace();
				TRACE("WHILE Loop exceeded %d iterations at %s\n", TINYJS_LOOP_MAX_ITERATIONS, l->getPosition(l->tokenLastEnd).c_str());
				throw new CScriptException("LOOP_ERROR");
			}
		}
	} else if (l->tk==LEX_R_WHILE) {
		// We do repetition by pulling out the string representing our statement
		// there's definitely some opportunity for optimisation here
		l->match(LEX_R_WHILE);
		l->match('(');
		if(IS_RUNTIME_PASS_TWO_1) {
			base(execute);
			l->match(')');
			statement(execute);
		} else {
			int whileCondStart = l->tokenStart;
			bool noexecute = false;
			CScriptVarSmartLink cond = base(execute);
			bool loopCond = execute && cond->var->getBool();
			CScriptLex *whileCond = l->getSubLex(whileCondStart);
			l->match(')');
			int whileBodyStart = l->tokenStart;

			bool old_execute = execute;
			int old_loop_runtimeFlags = runtimeFlags & RUNTIME_LOOP_MASK;
			runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;
			statement(loopCond ? execute : noexecute);
			CScriptLex *whileBody = l->getSubLex(whileBodyStart);
			CScriptLex *oldLex = l;
			if(loopCond && old_execute != execute)
			{
				// break or continue
				if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
				{
					execute = old_execute;
					if(runtimeFlags & RUNTIME_BREAK)
						loopCond = false;
					runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
				}
				// other stuff e.g return, throw
			}

			int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
			while (loopCond && loopCount-->0) {
				whileCond->reset();
				l = whileCond;
				cond = base(execute);
				loopCond = execute && cond->var->getBool();
				if (loopCond) {
					whileBody->reset();
					l = whileBody;
					statement(execute);
					if(!execute)
					{
						// break or continue
						if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
						{
							execute = old_execute;
							if(runtimeFlags & RUNTIME_BREAK)
								loopCond = false;
							runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
						}
						// other stuff e.g return, throw
					}
				}
			}
			runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | old_loop_runtimeFlags;
			l = oldLex;
			delete whileCond;
			delete whileBody;

			if (loopCount<=0) {
				root->trace();
				TRACE("WHILE Loop exceeded %d iterations at %s\n", TINYJS_LOOP_MAX_ITERATIONS, l->getPosition(l->tokenLastEnd).c_str());
				throw new CScriptException("LOOP_ERROR");
			}
		}
	} else if (l->tk==LEX_R_FOR) {
		l->match(LEX_R_FOR);
		bool for_each_in;
		int error_pos = -1;
		if((for_each_in = l->tk == LEX_ID && l->tkStr == "each")) l->match(LEX_ID);
		l->match('(');
		int pos=l->tokenStart;
		bool for_in = false;
		CScriptVarSmartLink for_var;
		CScriptVarSmartLink for_in_var;
		if(l->tk == LEX_ID) {
			for_var = factor(execute);

			if(l->tk == LEX_R_IN) {
				for_in = true;
				l->match(LEX_R_IN);
				for_in_var = factor(execute);
				l->match(')');
			} else {
				error_pos = l->tokenEnd;
				l->reset(pos);
			}
		} 
		if(for_each_in && !for_in)
			throw new CScriptException("invalid for each loop", error_pos);
		else if (for_in) {
			if (execute && !for_var->owned) {
				CScriptVarLink *real_for_var;
				if(for_var->owner)
					real_for_var = for_var->owner->addChildNoDup(for_var->name, for_var->var);
				else
					real_for_var = root->addChildNoDup(for_var->name, for_var->var);
				for_var = real_for_var;
			}
			if(IS_RUNTIME_PASS_TWO_1)
				statement(execute);
			else {
				int forBodyStart = l->tokenStart;
				bool old_execute = execute;
				int old_loop_runtimeFlags = runtimeFlags & RUNTIME_LOOP_MASK;
				runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;
				for(SCRIPTVAR_CHILDS::iterator it = for_in_var->var->Childs.begin(); execute && it != for_in_var->var->Childs.end(); ++it) {
					if (for_each_in)
						for_var->replaceWith(it->second->var);
					else
						for_var->replaceWith(new CScriptVar(it->first));
					l->reset(forBodyStart);
					statement(execute);
					if(!execute)
					{
						// break or continue
						if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
						{
							execute = old_execute;
							if(runtimeFlags & RUNTIME_BREAK)
								break;
							runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
						}
						// other stuff e.g return, throw
					}
				}
				runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | old_loop_runtimeFlags;
			}
		} else {
			statement(execute); // initialisation
			//l->match(';');
			bool noexecute = false;
			int forCondStart = l->tokenStart;
			bool cond_empty = true;
			bool loopCond = true;	// Empty Condition -->always true
			CScriptVarSmartLink cond;
			if(l->tk != ';') {
				cond_empty = false;
				cond = base(execute); // condition
				loopCond = execute && cond->var->getBool();
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
			if(IS_RUNTIME_PASS_TWO_1) {
				delete forCond;
				delete forIter;
				statement(execute);
			} else {
				int forBodyStart = l->tokenStart;
				bool old_execute = execute;
				int old_loop_runtimeFlags = runtimeFlags & RUNTIME_LOOP_MASK;
				runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;
				statement(loopCond ? execute : noexecute);
				CScriptLex *forBody = l->getSubLex(forBodyStart);
				CScriptLex *oldLex = l;
				if(loopCond && old_execute != execute) {
					// break or continue
					if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE)) {
						execute = old_execute;
						if(runtimeFlags & RUNTIME_BREAK)
							loopCond = false;
						runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
					}
					// other stuff e.g return, throw
				}

				if (loopCond && !iter_empty) {
					forIter->reset();
					l = forIter;
					base(execute);
				}
				int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
				while (execute && loopCond && loopCount-->0) {
					if(!cond_empty) {
						forCond->reset();
						l = forCond;
						cond = base(execute);
						loopCond = cond->var->getBool();
					}
					if (execute && loopCond) {
						forBody->reset();
						l = forBody;
						statement(execute);
						if(!execute) {
							// break or continue;
							if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE)) {
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
				runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | old_loop_runtimeFlags;
				l = oldLex;
				delete forCond;
				delete forIter;
				delete forBody;
				if (loopCount<=0) {
					root->trace();
					TRACE("FOR Loop exceeded %d iterations at %s\n", TINYJS_LOOP_MAX_ITERATIONS, l->getPosition(l->tokenLastEnd).c_str());
					throw new CScriptException("LOOP_ERROR");
				}
			}
		}					
	} else if (l->tk==LEX_R_BREAK) {
		l->match(LEX_R_BREAK);
		if(execute) {
			if(runtimeFlags & RUNTIME_CANBREAK)
			{
				runtimeFlags |= RUNTIME_BREAK;
				execute = false;
			}
			else
				throw new CScriptException("'break' must be inside loop or switch");
		}
		l->match(';');
	} else if (l->tk==LEX_R_CONTINUE) {
		l->match(LEX_R_CONTINUE);
		if(execute) {
			if(runtimeFlags & RUNTIME_CANCONTINUE)
			{
				runtimeFlags |= RUNTIME_CONTINUE;
				execute = false;
			}
			else
				throw new CScriptException("'continue' must be inside loop");
		}
		l->match(';');
	} else if (l->tk==LEX_R_RETURN) {
		l->match(LEX_R_RETURN);
		CScriptVarSmartLink result;
		if (l->tk != ';')
			result = base(execute);
		if (execute) {
			if (IS_RUNTIME_CANRETURN) {
				if(result->var) scopes.back()->setReturnVar(result->var);
			} else 
				throw new CScriptException("'return' statement, but not in a function.");
			execute = false;
		}
		l->match(';');
	} else if (l->tk==LEX_R_FUNCTION) {
		CScriptVarSmartLink funcVar = parseFunctionDefinition();
		if ((execute && IS_RUNTIME_PASS_SINGLE) || IS_RUNTIME_PASS_TWO_1) {
			if (funcVar->name == TINYJS_TEMP_NAME)
				throw new CScriptException("Functions defined at statement-level are meant to have a name.");
			else
				scopes.back()->addChildNoDup(funcVar->name, funcVar->var)->dontDelete = true;
		}
	} else if (l->tk==LEX_R_TRY) {
		l->match(LEX_R_TRY);
		bool old_execute = execute;
		// save runtimeFlags
		int old_throw_runtimeFlags = runtimeFlags & RUNTIME_THROW_MASK;
		// set runtimeFlags
		runtimeFlags = (runtimeFlags & ~RUNTIME_THROW_MASK) | RUNTIME_CANTHROW;

		block(execute);
//try{print("try1");try{ print("try2");try{print("try3");throw 3;}catch(a){print("catch3");throw 3;}finally{print("finalli3");}print("after3");}catch(a){print("catch2");}finally{print("finalli2");}print("after2");}catch(a){print("catch1");}finally{print("finalli1");}print("after1");		
		bool isThrow = (runtimeFlags & RUNTIME_THROW) != 0;
		if(isThrow) execute = old_execute;

		// restore runtimeFlags
		runtimeFlags = (runtimeFlags & ~RUNTIME_THROW_MASK) | old_throw_runtimeFlags;

		if(l->tk != LEX_R_FINALLY) // expect catch
		{
			l->match(LEX_R_CATCH);
			l->match('(');
			string exeption_var_name = l->tkStr;
			l->match(LEX_ID);
			l->match(')');
			if(isThrow) {
				CScriptVar *catchScope = new CScriptVar(TINYJS_BLANK_DATA, SCRIPTVAR_OBJECT);
				catchScope->addChild(exeption_var_name, exeption);
				exeption->unref(0); exeption = 0;
				scopes.push_back(catchScope);
				try {
					block(execute);
				} catch (CScriptException *e) {
					scopes.pop_back(); // clean up scopes
					delete catchScope;
					throw e;
				}
				scopes.pop_back();
				delete catchScope;
			} else {
				bool noexecute = false;
				block(noexecute);
			}
		}
		if(l->tk == LEX_R_FINALLY) {
			l->match(LEX_R_FINALLY);
			bool finally_execute = true;
			block(isThrow ? finally_execute : execute);
		}
	} else if (l->tk==LEX_R_THROW) {
		int tokenStart = l->tokenStart;
		l->match(LEX_R_THROW);
		CScriptVarSmartLink a = base(execute);
		if(execute) {
			if(runtimeFlags & RUNTIME_CANTHROW) {
				runtimeFlags |= RUNTIME_THROW;
				execute = false;
				exeption = a->var->ref();
			}
			else
				throw new CScriptException("uncaught exeption: '"+a->var->getString()+"'", tokenStart);
		}
	} else if (l->tk==LEX_R_SWITCH) {
		l->match(LEX_R_SWITCH);
		l->match('(');
		CScriptVarSmartLink SwitchValue = base(execute);
		l->match(')');
		if(execute) {
			// save runtimeFlags
			int old_switch_runtimeFlags = runtimeFlags & RUNTIME_BREAK_MASK;
			// set runtimeFlags
			runtimeFlags = (runtimeFlags & ~RUNTIME_BREAK_MASK) | RUNTIME_CANBREAK;
			bool noexecute = false;
			bool old_execute = execute = false;
			l->match('{');
			if(l->tk == LEX_R_CASE || l->tk == LEX_R_DEFAULT || l->tk == '}') {

				while (l->tk && l->tk!='}') {
					if(l->tk == LEX_R_CASE) {
						l->match(LEX_R_CASE);
						if(execute) {
							base(noexecute);
						} else {
							if(old_execute == execute)
								old_execute = execute = true;
							CScriptVarSmartLink CaseValue = base(execute);
							if(execute) {
								CaseValue = CaseValue->var->mathsOp(SwitchValue->var, LEX_EQUAL);
								old_execute = execute = CaseValue->var->getBool();
							}
						}
						l->match(':');
					} else if(l->tk == LEX_R_DEFAULT) {
						l->match(LEX_R_DEFAULT);
						l->match(':');
						if(!old_execute && !execute )
							old_execute = execute = true;
					}
					statement(execute);
				}
				l->match('}');
				if(runtimeFlags & RUNTIME_BREAK)
					execute = true;
				// restore runtimeFlags
				runtimeFlags = (runtimeFlags & ~RUNTIME_BREAK_MASK) | old_switch_runtimeFlags;
			} else
				throw new CScriptException("invalid switch statement");
		} else
			block(execute);
	} else if(l->tk != LEX_EOF) {
		/* Execute a simple statement that only contains basic arithmetic... */
		ret = base(execute);
		l->match(';');
	} else
		l->match(LEX_EOF);
	return ret;
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
	CScriptVarLink *v = scopes.back()->findChild(childName);
	if(!v && scopes.front() != scopes.back())
		v = scopes.front()->findChild(childName);
	return v;
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
	std::string code = c->getParameter("jsCode")->getString();
	CScriptVar *scEvalScope = scopes.back(); // save scope
	scopes.pop_back(); // go back to the callers scope
	try {
		CScriptLex *oldLex = l;
		l = new CScriptLex(code);
		CScriptVarSmartLink returnVar;
		runtimeFlags |= RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;

		SAVE_RUNTIME_PASS;
		SET_RUNTIME_PASS_SINGLE;
		try {
			bool execute = true;
			bool noexecute = false;
			if(twoPass)
			{
				SET_RUNTIME_PASS_TWO_1;
				do {
					statement(noexecute);
					while (l->tk==';') l->match(';'); // skip empty statements
				} while (l->tk!=LEX_EOF);
				l->reset();
				SET_RUNTIME_PASS_TWO_2;
			}
			do {
				returnVar = statement(execute);
				while (l->tk==';') l->match(';'); // skip empty statements
			} while (l->tk!=LEX_EOF);
		} catch (CScriptException *e) {
			delete l;
			l = oldLex;
			throw e;
		}
		delete l;
		l = oldLex;
		RESTORE_RUNTIME_PASS;
		
		this->scopes.push_back(scEvalScope); // restore Scopes;
		if(returnVar)
			c->setReturnVar(returnVar->var);

		// check of exeptions
		int exeption = runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE);
		runtimeFlags &= ~RUNTIME_LOOP_MASK;
		if(exeption) throw new CScriptEvalException(exeption);

	} catch (CScriptException *e) {
		scopes.push_back(scEvalScope); // restore Scopes;
		e->pos = -1;
		throw e;
	}
}
