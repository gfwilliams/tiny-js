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
						'symbol_base' added for the current base of the symbol table
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
						Added comma operator like this 'var i=1,j,k=9;' or 'for(i=0,j=12; i<10; i++, j++)' works
						Added Conditional operator ( ? : )
						Added automatic cast from doubles to integer e.G. by logic and binary operators
						Added pre-increment/-decrement like '++i'/'--i' operator
						Fixed post-increment/decrement now returns the previous value 
						Fixed throws an Error when invalid using of post/pre-increment/decrement (like this 5++ works no more) 
						Fixed memoryleak (unref arrayClass at deconstructor of JS CTinyJS)
						Fixed unary operator handling (like this '-~-5' works now)
						Fixed operator priority order -> ','
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
					Added now allowed stuff like this (function (v) {print(v);})(12);
					Remove unneeded and wrong deepCopy by assignment operator '='


	NOTE: This doesn't support constructors for objects
				Recursive loops of data such as a.foo = a; fail to be garbage collected
				'length' cannot be set
				There is no ternary operator implemented yet
 */

#ifdef _DEBUG
#	ifndef _MSC_VER
#		define DEBUG_MEMORY 1
#	endif
#endif
#include <assert.h>
#include <string.h>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <memory> // for auto_ptr
#include <algorithm>

#include "TinyJS.h"

#ifndef ASSERT
#	define ASSERT(X) assert(X)
#endif

inline CScriptVarSmartLink &CScriptVarSmartLink::operator = (CScriptVarPtr Var) {
	if(link && !link->IsOwned()) delete link; 
	link = new CScriptVarLink(Var);
/*
	if (!link || link->IsOwned()) 
		link = new CScriptVarLink(Var);
	else {
		link->replaceWith(Var);
		link->name=TINYJS_TEMP_NAME;
	} 
*/
	return *this;
}
inline CScriptVarSmartLink &CScriptVarSmartLink::operator = (const CScriptVarSmartLink &Link) { 
	if(link && !link->IsOwned()) delete link; 
	link = Link.link; 
	((CScriptVarSmartLink &)Link).link = 0; // explicit cast to a non const ref
	return *this;
}

// this operator corresponds "CLEAN(link); link = Link;"
inline CScriptVarSmartLink &CScriptVarSmartLink::operator = (CScriptVarLink *Link) {
	if(link && !link->IsOwned()) delete link; 
	link = Link; 
	return *this;
}
// this operator corresponds "link->replaceWith(Link->get())"
inline CScriptVarSmartLink &CScriptVarSmartLink::operator <<(CScriptVarSmartLink &Link) {
	ASSERT(link && Link.link);
	link->replaceWith(Link.link);
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
		printf("ALLOCATED LINK %s, allocated[%d] to \n", allocatedLinks[i]->name.c_str(), (*allocatedLinks[i])->getRefs());
		(*allocatedLinks[i])->trace("  ");
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
	return ((ch>='0') && (ch<='9')) || ((ch>='a') && (ch<='f')) || ((ch>='A') && (ch<='F'));
}
bool isOctal(char ch) {
	return ((ch>='0') && (ch<='7'));
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
string int2string(int intData) {
	char buffer[32];
	sprintf_s(buffer, sizeof(buffer), "%d", intData);
	return buffer;
}
string float2string(const double &floatData) {
	char buffer[32];
	sprintf_s(buffer, sizeof(buffer), "%f", floatData);
	return buffer;
}
/// convert the given string into a quoted string suitable for javascript
string getJSString(const string &str) {
	string nStr = str;
	for (size_t i=0;i<nStr.size();i++) {
		const char *replaceWith = "";
		bool replace = true;

		switch (nStr[i]) {
			case '\\': replaceWith = "\\\\"; break;
			case '\n': replaceWith = "\\n"; break;
			case '\r': replaceWith = "\\r"; break;
			case '\a': replaceWith = "\\a"; break;
			case '\b': replaceWith = "\\b"; break;
			case '\f': replaceWith = "\\f"; break;
			case '\t': replaceWith = "\\t"; break;
			case '\v': replaceWith = "\\v"; break;
			case '"': replaceWith = "\\\""; break;
//			case '\'': replaceWith = "\\'"; break;
			default: {
					int nCh = ((int)nStr[i]) & 0xff;
					if(nCh<32 || nCh>127) {
						char buffer[5];
						sprintf_s(buffer, sizeof(buffer), "\\x%02X", nCh);
						replaceWith = buffer;
					} else replace=false;
				}
		}

		if (replace) {
			nStr = nStr.substr(0, i) + replaceWith + nStr.substr(i+1);
			i += strlen(replaceWith)-1;
		}
	}
	return "\"" + nStr + "\"";
}

/** Is the string alphanumeric */
bool isAlphaNum(const string &str) {
	if (str.size()==0) return true;
	if (!isAlpha(str[0])) return false;
	for (size_t i=0;i<str.size();i++)
		if (!(isAlpha(str[i]) || isNumeric(str[i])))
			return false;
	return true;
}

// ----------------------------------------------------------------------------------- CSCRIPTEXCEPTION
/*
CScriptException::CScriptException(const string &exceptionText, int Pos) {
	text = exceptionText;
	pos = Pos;
}
*/
// ----------------------------------------------------------------------------------- CSCRIPTLEX

CScriptLex::CScriptLex(const char *Code, const string &File, int Line, int Column) : data(Code) {
	currentFile = File;
	currentLineStart = tokenStart = data;
	reset(data, Line, data);
}


void CScriptLex::reset(const char *toPos, int Line, const char *LineStart) {
	dataPos = toPos;
	tokenStart = data;
	tk = 0;
	tkStr = "";
	currentLine = Line;
	currentLineStart = LineStart;
	currCh = nextCh = 0;
	getNextCh(); // currCh
	getNextCh(); // nextCh
	getNextToken();
}

void CScriptLex::check(int expected_tk) {
	if (expected_tk==';' && tk==LEX_EOF) return; // ignore last missing ';'
	if (tk!=expected_tk) {
		ostringstream errorString;
		errorString << "Got " << getTokenStr(tk) << " expected " << getTokenStr(expected_tk);
		throw new CScriptException(errorString.str(), currentFile, currentLine, currentColumn());
	}
}
void CScriptLex::match(int expected_tk) {
	check(expected_tk);
	getNextToken();
}

string CScriptLex::getTokenStr(int token) {
	if (token>32 && token<128) {
		//			char buf[4] = "' '";
		//			buf[1] = (char)token;
			char buf[2] = " ";
			buf[0] = (char)token;
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
		case LEX_RSHIFTU : return ">>>";
		case LEX_RSHIFTUEQUAL : return ">>>=";
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
		case LEX_T_FOR_IN : return "for";
		case LEX_T_FOR_EACH_IN : return "for each";
		case LEX_R_IN: return "in";
		case LEX_R_BREAK : return "break";
		case LEX_R_CONTINUE : return "continue";
		case LEX_R_FUNCTION : return "function";
		case LEX_T_FUNCTION_OPERATOR : return "function";
		case LEX_T_GET : return "get";
		case LEX_T_SET : return "set";

		case LEX_R_RETURN : return "return";
		case LEX_R_VAR : return "var";
		case LEX_R_LET : return "let";
		case LEX_R_WITH : return "with";
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

		case LEX_T_SKIP : return "LEX_SKIP";
		case LEX_T_LABEL: return "LABEL";
	}

	ostringstream msg;
	msg << "?[" << token << "]";
	return msg.str();
}

void CScriptLex::getNextCh() {
	if(currCh == '\n') { // Windows or Linux
		currentLine++;
		tokenStart = currentLineStart = dataPos - (nextCh == LEX_EOF ?  0 : 1);
	}
	currCh = nextCh;
	if ( (nextCh = *dataPos) != LEX_EOF ) dataPos++; // stay on EOF
	if(currCh == '\r') { // Windows or Mac
		if(nextCh == '\n')
			getNextCh(); // Windows '\r\n\' --> skip '\r'
		else
			currCh = '\n'; // Mac (only '\r') --> convert '\r' to '\n'
	}
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
	tokenStart = dataPos - (nextCh == LEX_EOF ? (currCh == LEX_EOF ? 0 : 1) : 2);
	// tokens
	if (isAlpha(currCh)) { //  IDs
		while (isAlpha(currCh) || isNumeric(currCh)) {
			tkStr += currCh;
			getNextCh();
		}
		tk = LEX_ID;
			  if (tkStr=="if")			tk = LEX_R_IF;
		else if (tkStr=="else")			tk = LEX_R_ELSE;
		else if (tkStr=="do")			tk = LEX_R_DO;
		else if (tkStr=="while")		tk = LEX_R_WHILE;
		else if (tkStr=="for")			tk = LEX_R_FOR;
		else if (tkStr=="in")			tk = LEX_R_IN;
		else if (tkStr=="break")		tk = LEX_R_BREAK;
		else if (tkStr=="continue")	tk = LEX_R_CONTINUE;
		else if (tkStr=="function")	tk = LEX_R_FUNCTION;
		else if (tkStr=="return")		tk = LEX_R_RETURN;
		else if (tkStr=="var")			tk = LEX_R_VAR;
		else if (tkStr=="let")			tk = LEX_R_LET;
		else if (tkStr=="with")			tk = LEX_R_WITH;
		else if (tkStr=="true")			tk = LEX_R_TRUE;
		else if (tkStr=="false")		tk = LEX_R_FALSE;
		else if (tkStr=="null")			tk = LEX_R_NULL;
		else if (tkStr=="undefined")	tk = LEX_R_UNDEFINED;
		else if (tkStr=="Infinity")	tk = LEX_R_INFINITY;
		else if (tkStr=="NaN")			tk = LEX_R_NAN;
		else if (tkStr=="new")			tk = LEX_R_NEW;
		else if (tkStr=="try")			tk = LEX_R_TRY;
		else if (tkStr=="catch")		tk = LEX_R_CATCH;
		else if (tkStr=="finally")		tk = LEX_R_FINALLY;
		else if (tkStr=="throw")		tk = LEX_R_THROW;
		else if (tkStr=="typeof")		tk = LEX_R_TYPEOF;
		else if (tkStr=="void")			tk = LEX_R_VOID;
		else if (tkStr=="delete")		tk = LEX_R_DELETE;
		else if (tkStr=="instanceof")	tk = LEX_R_INSTANCEOF;
		else if (tkStr=="switch")		tk = LEX_R_SWITCH;
		else if (tkStr=="case")			tk = LEX_R_CASE;
		else if (tkStr=="default")		tk = LEX_R_DEFAULT;
	} else if (isNumeric(currCh) || (currCh=='.' && isNumeric(nextCh))) { // Numbers
		if(currCh=='.') tkStr+='0';
		bool isHex = false;
		if (currCh=='0') { tkStr += currCh; getNextCh(); }
		if (currCh=='x' || currCh=='X') {
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
		if (!isHex && (currCh=='e' || currCh=='E')) {
			tk = LEX_FLOAT;
			tkStr += currCh; getNextCh();
			if (currCh=='-') { tkStr += currCh; getNextCh(); }
			while (isNumeric(currCh)) {
				tkStr += currCh; getNextCh();
			}
		}
	} else if (currCh=='"' || currCh=='\'') {	// strings...
		char endCh = currCh;
		getNextCh();
		while (currCh && currCh!=endCh && currCh!='\n') {
			if (currCh == '\\') {
				getNextCh();
				switch (currCh) {
					case '\n' : break; // ignore newline after '\'
					case 'n': tkStr += '\n'; break;
					case 'r': tkStr += '\r'; break;
					case 'a': tkStr += '\a'; break;
					case 'b': tkStr += '\b'; break;
					case 'f': tkStr += '\f'; break;
					case 't': tkStr += '\t'; break;
					case 'v': tkStr += '\v'; break;
					case 'x': { // hex digits
						getNextCh();
						char buf[3]="\0\0";
						for(int i=0; i<2 && isHexadecimal(currCh); i++)
						{
							buf[i] = currCh; getNextCh();
						}
						tkStr += (char)strtol(buf, 0, 16);
					}
					default: {
						if(isOctal(currCh)) {
							char buf[4]="\0\0\0";
							for(int i=0; i<3 && isOctal(currCh); i++)
							{
								buf[i] = currCh; getNextCh();
							}
							tkStr += (char)strtol(buf, 0, 8);
						}
						else tkStr += currCh;
					}
				}
			} else {
				tkStr += currCh;
			}
			getNextCh();
		}
		if(currCh != endCh)
			throw new CScriptException("unterminated string literal", currentFile, currentLine, currentColumn());
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
				} else if (currCh=='>') { // >>>
					tk = LEX_RSHIFTU;
					getNextCh();
					if (currCh=='=') { // >>>=
						tk = LEX_RSHIFTUEQUAL;
						getNextCh();
					}				
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
}

// ----------------------------------------------------------------------------------- CSCRIPTTOKEN
CScriptToken::CScriptToken(CScriptLex *l, int Match) : line(l->currentLine), column(l->currentColumn()), token(l->tk), intData(0)
{
	if(token == LEX_INT)
		intData = strtol(l->tkStr.c_str(),0,0);
	else if(LEX_TOKEN_DATA_FLOAT(token))
		floatData = new double(strtod(l->tkStr.c_str(),0));
	else if(LEX_TOKEN_DATA_STRING(token)) {
		stringData = new CScriptTokenDataString(l->tkStr);
		stringData->ref();
	} else if(LEX_TOKEN_DATA_FUNCTION(token)) {
		fncData = new CScriptTokenDataFnc;
		fncData->ref();
	}
	if(Match>=0)
		l->match(Match);
	else
		l->match(l->tk); 
}
CScriptToken::CScriptToken(uint16_t Tk, int IntData) : line(0), column(0), token(Tk) {
	if (LEX_TOKEN_DATA_SIMPLE(token)) {
		intData = IntData;
	} else if (LEX_TOKEN_DATA_FUNCTION(token)) {
		fncData = new CScriptTokenDataFnc;
		fncData->ref();
	} else 
		ASSERT(0);
}


CScriptToken::CScriptToken(uint16_t Tk, const string &TkStr) : line(0), column(0), token(Tk) {
	ASSERT(LEX_TOKEN_DATA_STRING(token));
	stringData = new CScriptTokenDataString(TkStr);
	stringData->ref();
}

CScriptToken &CScriptToken::operator =(const CScriptToken &Copy)
{
	clear();
	line			= Copy.line;
	column		= Copy.column; 
	token			= Copy.token;
	if(LEX_TOKEN_DATA_FLOAT(token))
		floatData = new double(*Copy.floatData);
	else if(LEX_TOKEN_DATA_STRING(token)) {
		stringData = Copy.stringData;
		stringData->ref();
	} else if(LEX_TOKEN_DATA_FUNCTION(token)) {
		fncData = Copy.fncData;
		fncData->ref();
	} else
		intData	= Copy.intData;
	return *this;
}
string CScriptToken::getParsableString(string &indentString, int &newln, const string &indent) {
	string OutString;
	string nl = indent.size() ? "\n" : " ";
	int last_newln = newln;
	if(newln&2) OutString.append(nl);
	if(newln&1) OutString.append(indentString);
	newln = 0;

	if(LEX_TOKEN_DATA_STRING(token))
		OutString.append(String()).append(" ");
	else if(LEX_TOKEN_DATA_FLOAT(token))
		OutString.append(float2string(Float())).append(" ");
	else if(token == LEX_INT)
		OutString.append(int2string(Int())).append(" ");
	else if(LEX_TOKEN_DATA_FUNCTION(token)) {
		OutString.append("function ");
		if(token == LEX_R_FUNCTION)
			OutString.append(Fnc().name);
		OutString.append("(");
		if(Fnc().parameter.size()) {
			OutString.append(Fnc().parameter.front());
			for(vector<string>::iterator it=Fnc().parameter.begin()+1; it!=Fnc().parameter.end(); ++it)
				OutString.append(", ").append(*it);
		}
		OutString.append(") ");
		for(TOKEN_VECT::iterator it=Fnc().body.begin(); it != Fnc().body.end(); ++it) {
			OutString.append(it->getParsableString(indentString, newln, indent));
		}
	} else if(token == '{') {
		OutString.append("{");
		indentString.append(indent);
		newln = 1|2|4;
	} else if(token == '}') {
		if(last_newln==(1|2|4)) OutString =  "";
		indentString.resize(indentString.size() - min(indentString.size(),indent.size()));
		OutString.append("}");
		newln = 1|2;
	} else {
		OutString.append(CScriptLex::getTokenStr(token));
		if(token==';') newln=1|2; else OutString.append(" ");
	}
	return OutString;
}
string CScriptToken::getParsableString(const string indent) {
	string indentString;
	int newln = 0;
	return getParsableString(indentString, newln, indent);
}
void CScriptToken::print(string &indent )
{
	if(LEX_TOKEN_DATA_STRING(token))
		printf("%s%s ", indent.c_str(), String().c_str());
	else if(LEX_TOKEN_DATA_FLOAT(token))
		printf("%s%f ", indent.c_str(), Float());
	else if(token == LEX_INT)
		printf("%s%d ", indent.c_str(), Int());
	else if(LEX_TOKEN_DATA_FUNCTION(token)) {
		if(token == LEX_R_FUNCTION)
			printf("%sfunction %s(", indent.c_str(), Fnc().name.c_str());
		else 
			printf("%sfunction (", indent.c_str());
		if(Fnc().parameter.size()) {
			printf("%s", Fnc().parameter.front().c_str());
			for(vector<string>::iterator it=Fnc().parameter.begin()+1; it!=Fnc().parameter.end(); ++it)
				printf(",%s", it->c_str());
		}
		printf(")");
		for(TOKEN_VECT::iterator it=Fnc().body.begin(); it != Fnc().body.end(); ++it)
			it->print (indent);
	} else if(token == '{') {
		printf("%s{\n", indent.c_str());
		indent += "   ";
	} else if(token == '}') {
		indent.resize(indent.size()-3);
		printf("}\n");
	} else {
		printf("%s%s ", indent.c_str(), CScriptLex::getTokenStr(token).c_str());
		if(token==';') printf("\n");
	}
}
void CScriptToken::clear()
{
	if(LEX_TOKEN_DATA_STRING(token))
		stringData->unref();
	else if(LEX_TOKEN_DATA_FLOAT(token))
		delete floatData;
	else if(LEX_TOKEN_DATA_FUNCTION(token))
		fncData->unref();
	token = 0;
}

// ----------------------------------------------------------------------------------- CSCRIPTTOKENIZER
CScriptTokenizer::CScriptTokenizer() : l(0), prevPos(&tokens) {
}
CScriptTokenizer::CScriptTokenizer(CScriptLex &Lexer) : l(0), prevPos(&tokens) {
	tokenizeCode(Lexer);
}
CScriptTokenizer::CScriptTokenizer(const char *Code, const string &File, int Line, int Column) : l(0), prevPos(&tokens) {
	CScriptLex lexer(Code, File, Line, Column);
	tokenizeCode(lexer);
}
void CScriptTokenizer::tokenizeCode(CScriptLex &Lexer) {
	try {
		l=&Lexer;
		tokens.clear();
		tokenScopeStack.clear();
		vector<int> blockStart(1, tokens.size()), marks;
		bool statement = true;
		if(l->tk == '§') { // special-Token at Start means the code begins not at Statement-Level
			l->match('§');
			statement = false;
		}
		do {
			tokenizeToken(tokens, statement, blockStart, marks);
		} while (l->tk!=LEX_EOF);
		pushToken(tokens); // add LEX_EOF-Token
		TOKEN_VECT(tokens).swap(tokens);//	tokens.shrink_to_fit();
		pushTokenScope(tokens);
		currentFile = l->currentFile;
		tk = getToken().token;
	} catch (CScriptException *e) {
		ostringstream msg;
		msg << "Error " << e->text;
		if(e->line >= 0) msg << " at Line:" << e->line+1 << " Column:" << e->column+1;
		if(e->file.length()) msg << " in " << e->file;
		delete e;
		l=0;
		throw new CScriptException(msg.str(),"");
	}
}

void CScriptTokenizer::getNextToken() {
	prevPos = tokenScopeStack.back();
	if(getToken().token == LEX_EOF) 
		return;
	ScriptTokenPosition &_TokenPos = tokenScopeStack.back();
	_TokenPos.pos++;
	if(_TokenPos.pos == _TokenPos.tokens->end())
		tokenScopeStack.pop_back();
//	ScriptTokenPosition &TokenPos = tokenScopeStack.back();
	tk = getToken().token;
}



void CScriptTokenizer::match(int ExpectedToken) {
	if(check(ExpectedToken))
		getNextToken();
}
bool CScriptTokenizer::check(int ExpectedToken) {
	int currentToken = getToken().token;
	if (ExpectedToken==';' && (currentToken==LEX_EOF || currentToken=='}')) return false; // ignore last missing ';'
	if (currentToken!=ExpectedToken) {
		ostringstream errorString;
		errorString << "Got " << CScriptLex::getTokenStr(currentToken) << " expected " << CScriptLex::getTokenStr(ExpectedToken);
		throw new CScriptException(errorString.str(), currentFile, currentLine(), currentColumn());
	}
	return true;
}
void CScriptTokenizer::pushTokenScope(TOKEN_VECT &Tokens) {
	tokenScopeStack.push_back(ScriptTokenPosition(&Tokens));
	tk = getToken().token;
}

void CScriptTokenizer::setPos(ScriptTokenPosition &TokenPos) {
	ASSERT( TokenPos.tokens == tokenScopeStack.back().tokens);
	tokenScopeStack.back().pos = TokenPos.pos;
	tk = getToken().token;
}
void CScriptTokenizer::skip(int Tokens) {
	tokenScopeStack.back().pos+=Tokens;
	tk = getToken().token;
}
void CScriptTokenizer::tokenizeTry(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	if(!Statement)	throwTokenNotExpected();
	int tk = l->tk;
	Marks.push_back(pushToken(Tokens)); // push Token & push tokenBeginIdx
	Statement = true;
	l->check('{');
//	BlockStart.push_back(Tokens.size()); // set a blockStart
	tokenizeBlock(Tokens, Statement, BlockStart, Marks);
//	BlockStart.pop_back();
	
	int tokenBeginIdx = Marks.back();
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;
	
	if(l->tk == LEX_R_CATCH && tk == LEX_R_TRY)
		tokenizeTry(Tokens, Statement, BlockStart, Marks);
	else if(l->tk == LEX_R_FINALLY && (tk == LEX_R_TRY || tk == LEX_R_CATCH))
		tokenizeTry(Tokens, Statement, BlockStart, Marks);
}
static inline void setTokenSkip(TOKEN_VECT &Tokens, vector<int> &Marks) {
	int tokenBeginIdx = Marks.back();
	Marks.pop_back();
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;
}
void CScriptTokenizer::tokenizeSwitch(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	if(!Statement)	throwTokenNotExpected();

	Marks.push_back(pushToken(Tokens)); // push Token & push tokenBeginIdx

	Statement = false;
	l->check('(');
	tokenizeBlock(Tokens, Statement, BlockStart, Marks);

	Statement = true;
	Marks.push_back(pushToken(Tokens, '{')); // push Token & push blockBeginIdx
	BlockStart.push_back(Tokens.size());	// push Block-Start (one Token after '{')

	for(bool hasDefault=false;;) {
		if( l->tk == LEX_R_CASE || l->tk == LEX_R_DEFAULT) {
			if(l->tk == LEX_R_CASE) {
				Marks.push_back(pushToken(Tokens)); // push Token & push caseBeginIdx
				Statement = false;
				while(l->tk != ':' && l->tk != LEX_EOF )
					tokenizeToken(Tokens, Statement, BlockStart, Marks); 
				setTokenSkip(Tokens, Marks);
			} else { // default
				if(hasDefault) throw new CScriptException("more than one switch default", l->currentFile, l->currentLine, l->currentColumn());
				hasDefault = true;
				pushToken(Tokens);
			}
			
			Marks.push_back(pushToken(Tokens, ':'));
			Statement = true;
			while(l->tk != '}' && l->tk != LEX_R_CASE && l->tk != LEX_R_DEFAULT && l->tk != LEX_EOF )
				tokenizeStatement(Tokens, Statement, BlockStart, Marks); 
			setTokenSkip(Tokens, Marks);
		} else if(l->tk == '}') {
			break;
		} else
			throw new CScriptException("invalid switch statement", l->currentFile, l->currentLine, l->currentColumn());
	}
	pushToken(Tokens, '}');
	BlockStart.pop_back();
	Statement = true;

	int tokenBeginIdx = Marks.back(); // get block begin
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;

	tokenBeginIdx = Marks.back(); // get switch gegin
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;
}
void CScriptTokenizer::tokenizeWith(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	if(!Statement)	throwTokenNotExpected();

	Marks.push_back(pushToken(Tokens)); // push Token & push tokenBeginIdx

	Statement = false;
	l->check('(');
	tokenizeBlock(Tokens, Statement, BlockStart, Marks);
	Statement = true;

	BlockStart.push_back(Tokens.size()); // set a blockStart
	tokenizeStatement(Tokens, Statement, BlockStart, Marks);
	BlockStart.pop_back();

	int tokenBeginIdx = Marks.back();
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;
}
void CScriptTokenizer::tokenizeWhile(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	if(!Statement)	throwTokenNotExpected();

	Marks.push_back(pushToken(Tokens)); // push Token & push tokenBeginIdx
	Statement = false;
	l->check('(');
	tokenizeBlock(Tokens, Statement, BlockStart, Marks);

	Marks.push_back(Tokens.size()); // push skiperBeginIdx
	Tokens.push_back(CScriptToken(LEX_T_SKIP)); // skip 

	Statement = true;

	BlockStart.push_back(Tokens.size()); // set a blockStart
	tokenizeStatement(Tokens, Statement, BlockStart, Marks);
	BlockStart.pop_back();

	int tokenBeginIdx = Marks.back(); // set skipper
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;

	tokenBeginIdx = Marks.back();
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;
}
void CScriptTokenizer::tokenizeDo(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	if(!Statement)	throwTokenNotExpected();
	Marks.push_back(pushToken(Tokens)); // push Token & push tokenBeginIdx

	BlockStart.push_back(Tokens.size()); // set a blockStart
	tokenizeStatement(Tokens, Statement, BlockStart, Marks);
	BlockStart.pop_back();
	pushToken(Tokens, LEX_R_WHILE);
	Statement = false;
	l->check('(');
	tokenizeBlock(Tokens, Statement, BlockStart, Marks);
	pushToken(Tokens, ';');
	Statement = true;

	int tokenBeginIdx = Marks.back();
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;
}

void CScriptTokenizer::tokenizeIf(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	if(!Statement)	throwTokenNotExpected();

	Marks.push_back(pushToken(Tokens)); // push Token & push tokenBeginIdx

	Statement = false;
	l->check('(');
	tokenizeBlock(Tokens, Statement, BlockStart, Marks);

	Marks.push_back(Tokens.size()); // push skiperBeginIdx
	Tokens.push_back(CScriptToken(LEX_T_SKIP)); // skip 

	Statement = true;
	BlockStart.push_back(Tokens.size()); // set a blockStart
	tokenizeStatement(Tokens, Statement, BlockStart, Marks);
	BlockStart.pop_back();

	int tokenBeginIdx = Marks.back(); // set skipper
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;

	if(l->tk == LEX_R_ELSE) {
		Marks.push_back(pushToken(Tokens)); // push Token & push tokenBeginIdx
		
		BlockStart.push_back(Tokens.size()); // set a blockStart
		tokenizeStatement(Tokens, Statement, BlockStart, Marks);
		BlockStart.pop_back();

		tokenBeginIdx = Marks.back();
		Marks.pop_back(); // clean-up Marks
		Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;
	}

	tokenBeginIdx = Marks.back();
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;
}

static void fix_BlockStarts_Marks(vector<int> &BlockStart, vector<int> &Marks, int start, int diff) {
	for(vector<int>::iterator it = BlockStart.begin(); it != BlockStart.end(); ++it)
		if(*it >= start) *it += diff;
	for(vector<int>::iterator it = Marks.begin(); it != Marks.end(); ++it)
		if(*it >= start) *it += diff;
}
void CScriptTokenizer::tokenizeFor(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	if(!Statement)	throwTokenNotExpected();

	const char *prev_pos=l->tokenStart;
	const char *prev_line_start=l->currentLineStart;
	int prev_line = l->currentLine;
	bool for_in, for_each_in;
	l->match(LEX_R_FOR);
	if((for_in = for_each_in = (l->tk == LEX_ID && l->tkStr == "each"))) {
		l->match(LEX_ID);
	}
	if(!for_in) {
		l->match('(');
		int parentheses = 0;
		while(l->tk != ')' || parentheses)
		{
			if(l->tk == '(') parentheses++;
			else if(l->tk == ')') parentheses--;
			else if(l->tk == LEX_R_IN) {
				for_in = true;
				break;
			} else if(l->tk==LEX_EOF) {
				l->match(')');
			}
			l->match(l->tk);
		}
	}
	l->reset(prev_pos, prev_line, prev_line_start);


	l->match(LEX_R_FOR);
	if(for_each_in) {
		l->match(LEX_ID);
	}

	Marks.push_back(Tokens.size()); // push tokenBeginIdx
	Tokens.push_back(CScriptToken(for_in?(for_each_in?LEX_T_FOR_EACH_IN:LEX_T_FOR_IN):LEX_R_FOR));

	BlockStart.push_back(pushToken(Tokens, '(')+1); // set BlockStart - no forwarde for(let ...
	Statement = true;
	if(for_in) {
		if(l->tk == LEX_R_VAR) {
			l->match(LEX_R_VAR);

			int tokenInsertIdx = BlockStart.front();
			Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(LEX_R_VAR));
			Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(LEX_ID, l->tkStr));
			Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(';'));
			fix_BlockStarts_Marks(BlockStart, Marks, BlockStart.front(), 3);
		} else if(l->tk == LEX_R_LET) {
			pushToken(Tokens, LEX_R_LET);
		}
		pushToken(Tokens, LEX_ID);
		pushToken(Tokens, LEX_R_IN);
		
	}
	while(l->tk != ')' && l->tk != LEX_EOF ) {
		tokenizeToken(Tokens, Statement, BlockStart, Marks); 
	}
	BlockStart.pop_back(); // pop_back / "no forwarde for(let ... "-prevention
	pushToken(Tokens, ')');
	Statement = true;
	BlockStart.push_back(Tokens.size()); // set a blockStart
	tokenizeStatement(Tokens, Statement, BlockStart, Marks);
	BlockStart.pop_back();

	int tokenBeginIdx = Marks.back();
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;
}
void CScriptTokenizer::tokenizeFunction(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	bool forward = false;
	int tk = l->tk;
	if(tk == LEX_ID && (l->tkStr=="get"||l->tkStr=="set")) {
		string tkStr = l->tkStr;
		tk = tkStr=="get"?LEX_T_GET:LEX_T_SET;
		l->match(l->tk);
		if(l->tk != LEX_ID) { // is not a getter or setter 
			tokens.push_back(CScriptToken(LEX_ID, tkStr));
			return;
		}
	} else {
		l->match(LEX_R_FUNCTION);
		if(!Statement) tk = LEX_T_FUNCTION_OPERATOR;
	}
	if(tk == LEX_R_FUNCTION) // only forward functions 
		forward = TOKEN_VECT::size_type(BlockStart.front()) != Tokens.size();

	CScriptToken FncToken(tk);
	CScriptTokenDataFnc &FncData = FncToken.Fnc();
	if(l->tk == LEX_ID) {
		FncData.name = l->tkStr;
		l->match(LEX_ID);
	} else {
		//ASSERT(Statement == false);
	}

	l->match('(');
	while(l->tk != ')') {
		FncData.parameter.push_back(l->tkStr);
		l->match(LEX_ID);
		if (l->tk==',') {
			l->match(','); 
		}
	}
	l->match(')');
	FncData.file = l->currentFile;
	FncData.line = l->currentLine;

	vector<int> functionBlockStart, marks;
	functionBlockStart.push_back(FncData.body.size()+1);
	l->check('{');
	tokenizeBlock(FncData.body, Statement, functionBlockStart, marks);

	if(forward) {
		int tokenInsertIdx = BlockStart.back();
		Tokens.insert(Tokens.begin()+BlockStart.front(), FncToken);
		fix_BlockStarts_Marks(BlockStart, Marks, tokenInsertIdx, 1);
	}
	else
		Tokens.push_back(FncToken);
}

void CScriptTokenizer::tokenizeLet(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {

	bool forward = TOKEN_VECT::size_type(BlockStart.back()) != Tokens.size();
	bool let_is_Statement = Statement, expression=false;

	Marks.push_back(pushToken(Tokens)); // push Token & push BeginIdx
	Statement = false;

	if(l->tk == '(' || !let_is_Statement) {
		expression = true;
		pushToken(Tokens, '(');
	}
	vector<string> vars;
	for(;;) {
		vars.push_back(l->tkStr);
		pushToken(Tokens, LEX_ID);
		if(l->tk=='=') {
			pushToken(Tokens);
			for(;;) {
				if(l->tk == (expression?')':';') || l->tk == ',' || l->tk == LEX_EOF) break;
				tokenizeToken(Tokens, Statement, BlockStart, Marks);
			}
		}
		if(l->tk==',')
			pushToken(Tokens);
		else
			break;
	}
	if(expression) {
		pushToken(Tokens, ')');
		Statement = let_is_Statement;
		if(Statement) 
			tokenizeStatement(Tokens, Statement, BlockStart, Marks);
		int tokenBeginIdx = Marks.back();
		Marks.pop_back(); // clean-up Marks
		Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;
	} else {
		pushToken(Tokens, ';');
		Statement = true;

		int tokenBeginIdx = Marks.back();
		Marks.pop_back(); // clean-up Marks
		Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;

		if(forward) // copy a let-deklaration at the begin of the last block
		{
			int tokenInsertIdx = tokenBeginIdx = BlockStart.back();

			Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(LEX_R_VAR));
			Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(LEX_ID, vars.front()));
			for(vector<string>::iterator it = vars.begin()+1; it != vars.end(); ++it) {
				Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(','));
				Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(LEX_ID, *it));
			}
			Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(';'));// insert at the begin var name; 
			Tokens[tokenBeginIdx].Int() = tokenInsertIdx-tokenBeginIdx;

			fix_BlockStarts_Marks(BlockStart, Marks, tokenBeginIdx, tokenInsertIdx-tokenBeginIdx);
		}
	}
}

void CScriptTokenizer::tokenizeVar(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	if(!Statement)	throwTokenNotExpected();

	bool forward = TOKEN_VECT::size_type(BlockStart.front()) != Tokens.size(); // forwarde only if the var-Statement not the first Statement of the Scope

	Marks.push_back(pushToken(Tokens)); // push Token & push BeginIdx

	Statement = false;
	vector<string> vars;
	for(;;) 
	{
		vars.push_back(l->tkStr);
		pushToken(Tokens, LEX_ID);
		if(l->tk=='=') {
			pushToken(Tokens);
			for(;;) {
				if(l->tk == ';' || l->tk == ',' || l->tk == LEX_EOF) break;
				tokenizeToken(Tokens, Statement, BlockStart, Marks);
			}
		}
		if(l->tk==',')
			pushToken(Tokens);
		else
			break;
	}
	pushToken(Tokens, ';');
	Statement = true;

	int tokenBeginIdx = Marks.back();
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;

	if(forward) // copy a var-deklaration at the begin of the scope
	{
		int tokenInsertIdx = tokenBeginIdx = BlockStart.front();

		Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(LEX_R_VAR));
		Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(LEX_ID, vars.front()));
		for(vector<string>::iterator it = vars.begin()+1; it != vars.end(); ++it) {
			Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(','));
			Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(LEX_ID, *it));
		}
		Tokens.insert(Tokens.begin()+tokenInsertIdx++, CScriptToken(';'));// insert at the begin var name; 
		Tokens[tokenBeginIdx].Int() = tokenInsertIdx-tokenBeginIdx;

		fix_BlockStarts_Marks(BlockStart, Marks, tokenBeginIdx, tokenInsertIdx-tokenBeginIdx);
	}
}


void CScriptTokenizer::tokenizeBlock(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	int tkEnd = 0;
	bool Statement_at_end = false;
	switch(l->tk)
	{
	case'{': tkEnd = '}'; break;
	case'(': tkEnd = ')'; break;
	case'[': tkEnd = ']'; break;
	}
	Marks.push_back(pushToken(Tokens)); // push Token & push BeginIdx

	if(tkEnd == '}') // { ... } Block
	{
		if( (Statement_at_end = Statement) )		// Statment-Level after '}' is the same Level as at start ('{')
			BlockStart.push_back(Tokens.size());	// push Block-Start (one Token after '{')
	} else
		Statement = false;
	while(l->tk != tkEnd && l->tk != LEX_EOF) tokenizeToken(Tokens, Statement, BlockStart, Marks);
	pushToken(Tokens, tkEnd);

	if(tkEnd == '}' && Statement_at_end) { // { ... } Block
		BlockStart.pop_back(); // clean-up BlockStarts
	}

	int tokenBeginIdx = Marks.back();
	Marks.pop_back(); // clean-up Marks
	Tokens[tokenBeginIdx].Int() = Tokens.size()-tokenBeginIdx;

	Statement = Statement_at_end;
}
void CScriptTokenizer::tokenizeStatement(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	do {
		tokenizeToken(Tokens, Statement, BlockStart, Marks); 
	} while(Statement == false && l->tk != LEX_EOF ); // tokenize one Statement
	Statement = true;
}
void CScriptTokenizer::tokenizeToken(TOKEN_VECT &Tokens, bool &Statement, vector<int> &BlockStart, vector<int> &Marks) {
	switch(l->tk)
	{
	case '(':
	case '[':
	case '{':				tokenizeBlock(Tokens, Statement, BlockStart, Marks); break;
	case LEX_R_VAR:		tokenizeVar(Tokens, Statement, BlockStart, Marks); break;
	case LEX_R_LET:		tokenizeLet(Tokens, Statement, BlockStart, Marks); break;
	case LEX_R_FUNCTION:	tokenizeFunction(Tokens, Statement, BlockStart, Marks); break;
	case LEX_R_FOR:		tokenizeFor(Tokens, Statement, BlockStart, Marks); break;
	case LEX_R_IF:			tokenizeIf(Tokens, Statement, BlockStart, Marks); break;
	case LEX_R_DO:			tokenizeDo(Tokens, Statement, BlockStart, Marks); break;
	case LEX_R_WHILE:		tokenizeWhile(Tokens, Statement, BlockStart, Marks); break;
	case LEX_R_WITH:		tokenizeWith(Tokens, Statement, BlockStart, Marks); break;
	case LEX_R_SWITCH:	tokenizeSwitch(Tokens, Statement, BlockStart, Marks); break;
	case LEX_R_TRY:		tokenizeTry(Tokens, Statement, BlockStart, Marks); break;
	case LEX_ID: {
			string str = l->tkStr;
			if(!Statement && (str=="get"||str=="set")) {
				tokenizeFunction(Tokens, Statement, BlockStart, Marks);
				break;
			}
			pushToken(Tokens);
			if(Statement && l->tk==':') { // label
				Tokens[Tokens.size()-1].token = LEX_T_LABEL; // change LEX_ID to LEX_T_LABEL
				pushToken(Tokens);
				break;
			}
		}
		Statement = false;
		break;
	default:
		Statement = l->tk==';'; // after ';' allways Statement-Level
		pushToken(Tokens);
	}
#if 0	
	if(l->tk == '{' || l->tk == '(' || l->tk == '[') {	// block
		tokenizeBlock(Tokens, Statement, BlockStart, Marks);
	} else if(l->tk == LEX_R_VAR) {
		tokenizeVar(Tokens, Statement, BlockStart, Marks);
	} else if(l->tk == LEX_R_FUNCTION) {
		tokenizeFunction(Tokens, Statement, BlockStart, Marks);
	} else if(l->tk == LEX_R_FOR) {
		tokenizeFor(Tokens, Statement, BlockStart, Marks);
	} else if(l->tk == LEX_R_IF) {
		tokenizeIf(Tokens, Statement, BlockStart, Marks);
	} else if(l->tk == LEX_R_DO) {
		tokenizeDo(Tokens, Statement, BlockStart, Marks);
	} else if(l->tk == LEX_R_WHILE) {
		tokenizeWhile(Tokens, Statement, BlockStart, Marks);
	} else if(l->tk == LEX_R_WITH) {
		tokenizeWith(Tokens, Statement, BlockStart, Marks);
	} else if(l->tk == LEX_R_TRY) {
		tokenizeTry(Tokens, Statement, BlockStart, Marks);
	} else if(l->tk == LEX_ID && (l->tkStr=="get"||l->tkStr=="set")) {
		string::const_iterator prev_pos=l->tokenStart;
		string::const_iterator prev_line_start=l->currentLineStart;
		int prev_line = l->currentLine;
		string tkStr = l->tkStr;
		l->match(l->tk);
		bool is_set_get = l->tk == LEX_ID; // true if set/get LEX_ID
		l->reset(prev_pos, prev_line, prev_line_start);
		if(is_set_get) {
			l->tk = tkStr=="get"?LEX_T_GET:LEX_T_SET;
			tokenizeFunction(Tokens, Statement, BlockStart, Marks);
		}
		else
			pushToken(Tokens);
	} else {
		Statement = l->tk==';'; // after ';' allways Statement-Level
		pushToken(Tokens);
	}
#endif
}
int CScriptTokenizer::pushToken(TOKEN_VECT &Tokens, int Match) {
	Tokens.push_back(CScriptToken(l, Match));
	return Tokens.size()-1;
}

void CScriptTokenizer::throwTokenNotExpected() {
	throw new CScriptException("'"+CScriptLex::getTokenStr(l->tk)+"' was not expected", l->currentFile, l->currentLine, l->currentColumn());
}

// ----------------------------------------------------------------------------------- CSCRIPTVARLINK

CScriptVarLink::CScriptVarLink(const CScriptVarPtr &Var, const string &Name /*=TINYJS_TEMP_NAME*/, int Flags /*=SCRIPTVARLINK_DEFAULT*/) 
	: name(Name), owner(0), flags(Flags&~SCRIPTVARLINK_OWNED) {
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	var = Var->ref();
}

CScriptVarLink::CScriptVarLink(const CScriptVarLink &link) 
	: name(link.name), owner(0), flags(SCRIPTVARLINK_DEFAULT) {
	// Copy constructor
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	var = link.var->ref();
}

CScriptVarLink::~CScriptVarLink() {
#if DEBUG_MEMORY
	mark_deallocated(this);
#endif
	var->unref(this->owner);
}

CScriptVarPtr CScriptVarLink::getValue() {
	if(var->isAccessor()) {
		CScriptVarSmartLink getter = var->findChild(TINYJS_ACCESSOR_GET_VAR);
		if(getter) {
			vector<CScriptVarPtr> Params;
			bool execute;
			ASSERT(this->owner);
			return (*getter)->getContext()->callFunction(getter, Params, owner, execute);
		}
		return var->newScriptVar(Undefined);
	} else {
		return var;
	}
}

void CScriptVarLink::replaceWith(const CScriptVarPtr &newVar) {
	if(!newVar) ASSERT(0);//newVar = new CScriptVar();
	if(var->isAccessor()) {
		CScriptVarSmartLink setter = var->findChild(TINYJS_ACCESSOR_SET_VAR);
		if(setter) {
			vector<CScriptVarPtr> Params(1, newVar);
			bool execute;
			ASSERT(this->owner);
			(*setter)->getContext()->callFunction(setter, Params, owner, execute);
		}
	} else {
		CScriptVar *oldVar = var;
		var = newVar->ref();
		oldVar->unref(owner);
		if(owner) var->recoursionCheck(owner);
	}
}

void CScriptVarLink::replaceWith(CScriptVarLink *newVar) {
	if (newVar)
		replaceWith(newVar->getValue());
	else
		ASSERT(0);//replaceWith(new CScriptVar());
}

// ----------------------------------------------------------------------------------- CSCRIPTVAR
CScriptVar::CScriptVar(CTinyJS *Context, const CScriptVarPtr &Prototype) : context(Context) , temporaryID(0) {
	if(context->first) {
		next = context->first;
		next->prev = this;
	} else {
		next = 0;
	}
	context->first = this;
	prev = 0;
	refs = 0;
	internalRefs = 0;
	recursionFlag = 0;
	recursionSet = 0;
	if(Prototype)
		addChild(TINYJS___PROTO___VAR, Prototype, 0);
	else if(context->objectPrototype)
		addChild(TINYJS___PROTO___VAR, context->objectPrototype, 0);
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
}
CScriptVar::CScriptVar(const CScriptVar &Copy) : context(Copy.context), temporaryID(0) {
	if(context->first) {
		next = context->first;
		next->prev = this;
	} else {
		next = 0;
	}
	context->first = this;
	prev = 0;
	refs = 0;
	internalRefs = 0;
	recursionFlag = 0;
	recursionSet = 0;
	SCRIPTVAR_CHILDS_cit it;
	for(it = Copy.Childs.begin(); it!= Copy.Childs.end(); ++it) {
		addChild((*it)->name, (*it), (*it)->flags);
	}

#if DEBUG_MEMORY
	mark_allocated(this);
#endif
}
CScriptVar::~CScriptVar(void) {
#if DEBUG_MEMORY
	mark_deallocated(this);
#endif
	removeAllChildren();
	if(prev)
		prev->next = next;
	else
		context->first = next;
	if(next)
		next->prev = prev;
}

/// Type
bool CScriptVar::isInt()			{return false;}
bool CScriptVar::isBool()			{return false;}
bool CScriptVar::isDouble()		{return false;}
bool CScriptVar::isString()		{return false;}
bool CScriptVar::isNumber()		{return false;}
bool CScriptVar::isNumeric()		{return false;}
bool CScriptVar::isFunction()		{return false;}
bool CScriptVar::isObject()		{return false;}
bool CScriptVar::isArray()			{return false;}
bool CScriptVar::isNative()		{return false;}
bool CScriptVar::isUndefined()	{return false;}
bool CScriptVar::isNull()			{return false;}
bool CScriptVar::isNaN()			{return false;}
int CScriptVar::isInfinity()		{ return 0; } ///< +1==POSITIVE_INFINITY, -1==NEGATIVE_INFINITY, 0==is not an InfinityVar
bool CScriptVar::isAccessor()		{return false;}


/// Value
int CScriptVar::getInt() {return 0;}
bool CScriptVar::getBool() {return false;}
double CScriptVar::getDouble() {return 0.0;}
CScriptTokenDataFnc *CScriptVar::getFunctionData() { return 0; }
string CScriptVar::getString() {return "";}

string CScriptVar::getParsableString(const string &indentString, const string &indent) {
	return getString();
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
	return "object"; // Object / Array / null
}
///< returns an Integer, a Double, an Infinity or a NaN
CScriptVarLink *CScriptVar::getNumericVar()
{
	if(isNumber() || isInfinity())
		return new CScriptVarLink(this);
	else if(isNull())
		return new CScriptVarLink(newScriptVar(0));
	else if(isBool())
		return new CScriptVarLink(newScriptVar(getInt()));
/* ToDo
	else if(Var->isObject())
		return ToDo call Object.valueOff();
	else if(Var->isArray())
		return ToDo call Object.valueOff();
*/
	else if(isString()) {
		string the_string = getString();
		double d;
		char *endptr;//=NULL;
		int i = strtol(the_string.c_str(),&endptr,0);
		if(*endptr == '\0')
			return new CScriptVarLink(newScriptVar(i));
		if(*endptr=='.' || *endptr=='e' || *endptr=='E') {
			d = strtod(the_string.c_str(),&endptr);
			if(*endptr == '\0')
				return new CScriptVarLink(newScriptVar(d));
		}
	}
	return new CScriptVarLink(newScriptVar(NaN));
}


////// Childs

/// find
static bool compare_child_name(CScriptVarLink *Link, const string &Name) {
	return Link->name < Name;
}

CScriptVarLink *CScriptVar::findChild(const string &childName) {
	if(Childs.empty()) return 0;
	SCRIPTVAR_CHILDS_it it = lower_bound(Childs.begin(), Childs.end(),
		childName.c_str(),
		compare_child_name);
	if(it != Childs.end() && (*it)->name == childName)
		return *it;
	return 0;
}
CScriptVarLink *CScriptVar::findChildByPath(const string &path) {
	string::size_type p = path.find('.');
	CScriptVarLink *child;
	if (p == string::npos)
		return findChild(path);
	if( (child = findChild(path.substr(0,p))) )
		(*child)->findChildByPath(path.substr(p+1));
	return 0;
}

CScriptVarLink *CScriptVar::findChildOrCreate(const string &childName/*, int varFlags*/) {
	CScriptVarLink *l = findChild(childName);
	if (l) return l;
	return addChild(childName, newScriptVar(Undefined));
	//	return addChild(childName, new CScriptVar(context, TINYJS_BLANK_DATA, varFlags));
}

CScriptVarLink *CScriptVar::findChildOrCreateByPath(const string &path) {
	string::size_type p = path.find('.');
	if (p == string::npos)
		return findChildOrCreate(path);
	string childName(path, 0, p);
	CScriptVarLink *l = findChild(childName);
	if (!l) l = addChild(childName, newScriptVar(Object));
	return (*l)->findChildOrCreateByPath(path.substr(p+1));
}

/// add & remove
CScriptVarLink *CScriptVar::addChild(const string &childName, const CScriptVarPtr &child, int linkFlags /*= SCRIPTVARLINK_DEFAULT*/) {
#ifdef _DEBUG
	if(findChild(childName))
		ASSERT(0); // addCild - the child exists 
#endif
	CScriptVarLink *link = new CScriptVarLink(child?child:newScriptVar(Undefined), childName, linkFlags);
	link->owner = this;
	link->SetOwned(true);
	SCRIPTVAR_CHILDS_it it = lower_bound(Childs.begin(), Childs.end(), childName, compare_child_name);
	if(it == Childs.end() || (*it)->name != childName) {
		it = Childs.insert(it, link);
	}
	(*link)->recoursionCheck(this);
	return link;
}
CScriptVarLink *CScriptVar::addChildNoDup(const string &childName, const CScriptVarPtr &child, int linkFlags /*= SCRIPTVARLINK_DEFAULT*/) {
	// if no child supplied, create one

	CScriptVarLink *link = findChild(childName);
	if (link) {
		if(link->operator->() != child.operator->()) link->replaceWith(child);
	} else {
		link = new CScriptVarLink(child, childName, linkFlags);
		link->owner = this;
		link->SetOwned(true);
		SCRIPTVAR_CHILDS_it it = lower_bound(Childs.begin(), Childs.end(), childName, compare_child_name);
		if(it == Childs.end() || (*it)->name != childName) {
			it = Childs.insert(it, link);
		}
		(*link)->recoursionCheck(this);
	}

	return link;
}

bool CScriptVar::removeLink(CScriptVarLink *&link) {
	if (!link) return false;
#ifdef _DEBUG
	if(findChild(link->name) != link)
		ASSERT(0); // removeLink - the link is not atached to this var 
#endif
	SCRIPTVAR_CHILDS_it it = lower_bound(Childs.begin(), Childs.end(), link->name, compare_child_name);
	if(it != Childs.end() && (*it)->name == link->name) {
		Childs.erase(it);
	}
	delete link;
	link = 0;
	return true;
}
void CScriptVar::removeAllChildren() {
	SCRIPTVAR_CHILDS_it it;
	for(it = Childs.begin(); it!= Childs.end(); ++it) {
		delete *it;
	}
	Childs.clear();
}


/// funcions for FUNCTION

void CScriptVar::setReturnVar(const CScriptVarPtr &var) {
	CScriptVarLink *l = findChild(TINYJS_RETURN_VAR);
	if (l) l->replaceWith(var);
	else addChild(TINYJS_RETURN_VAR, var);
}

CScriptVarPtr CScriptVar::getParameter(const string &name) {
	return findChildOrCreate(name);
}

CScriptVarPtr CScriptVar::getParameter(int Idx) {
	CScriptVarLink *arguments = findChildOrCreate(TINYJS_ARGUMENTS_VAR);
	return (*arguments)->findChildOrCreate(int2string(Idx));
}
int CScriptVar::getParameterLength() {
	CScriptVarLink *arguments = findChildOrCreate(TINYJS_ARGUMENTS_VAR);
	return (*(*arguments)->findChildOrCreate("length"))->getInt();
}




CScriptVarPtr CScriptVar::getArrayIndex(int idx) {
	CScriptVarLink *link = findChild(int2string(idx));
	if (link) return link;
	else return newScriptVar(Undefined); // undefined
}

void CScriptVar::setArrayIndex(int idx, const CScriptVarPtr &value) {
	string sIdx = int2string(idx);
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

	for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
		if (::isNumber((*it)->name)) {
			int val = atoi((*it)->name.c_str());
			if (val > highest) highest = val;
		}
	}
	return highest+1;
}

template <typename T>
CScriptVarPtr DoMaths(CScriptVarPtr &a, CScriptVarPtr &b, int op)
{
	int dai = a->getInt();// use int when needed
	int dbi = b->getInt();
	T da = (T)(a->isDouble()?a->getDouble():dai);
	T db = (T)(b->isDouble()?b->getDouble():dbi);
	switch (op) {
			case '+':			return a->newScriptVar(da+db);
			case '-':			return a->newScriptVar(da-db);
			case '*':			return a->newScriptVar(da*db);
			case '/':
				if(db==0) {
					if(da==0) 	return a->newScriptVar(NaN);			// 0/0 = NaN
					else 			return a->newScriptVar(Infinity(da<0 ? -1 : 1));	//  /0 = Infinity
				} else			return a->newScriptVar(da/db);
			case '%':
				if(db==0) 		return a->newScriptVar(NaN);			// 0/0 = NaN
				else				return a->newScriptVar(dai%dbi);
			case '&':			return a->newScriptVar(dai&dbi);
			case '|':			return a->newScriptVar(dai|dbi);
			case '^':			return a->newScriptVar(dai^dbi);
			case '~':			return a->newScriptVar(~dai);
			case LEX_LSHIFT:	return a->newScriptVar(dai<<dbi);
			case LEX_RSHIFT:	return a->newScriptVar(dai>>dbi);
			case LEX_RSHIFTU:	return a->newScriptVar((int)(((unsigned int)dai)>>dbi));
			case LEX_EQUAL:	return a->newScriptVar(da==db);
			case LEX_NEQUAL:	return a->newScriptVar(da!=db);
			case '<':			return a->newScriptVar(da<db);
			case LEX_LEQUAL:	return a->newScriptVar(da<=db);
			case '>':			return a->newScriptVar(da>db);
			case LEX_GEQUAL:	return a->newScriptVar(da>=db);
			default: throw new CScriptException("This operation not supported on the int datatype");
	}
}
#define RETURN_NAN return newScriptVar(NaN)
CScriptVarPtr CScriptVar::mathsOp(const CScriptVarPtr &b, int op) {
	CScriptVarPtr a = this;
	// TODO Equality checks on classes/structures
	// Type equality check
	if (op == LEX_TYPEEQUAL || op == LEX_NTYPEEQUAL) {
		// check type first, then call again to check data
		bool eql = false;
		if(!a->isNaN() && !b->isNaN() && typeid(*a.getVar()) == typeid(*b.getVar()) ) {
			CScriptVarPtr e = a->mathsOp(b, LEX_EQUAL);
			eql = e->getBool();
		}
		if (op == LEX_TYPEEQUAL)
			return newScriptVar(eql);
		else
			return newScriptVar(!eql);
	}
	// do maths...
	bool a_isString = a->isString();
	bool b_isString = b->isString();
	// special for strings and string '+'
	// both a String or one a String and op='+'
	if( (a_isString && b_isString) || ((a_isString || b_isString) && op == '+')) {
		string da = a->getString();
		string db = b->getString();
		switch (op) {
		case '+':			return newScriptVar(da+db);
		case LEX_EQUAL:	return newScriptVar(da==db);
		case LEX_NEQUAL:	return newScriptVar(da!=db);
		case '<':			return newScriptVar(da<db);
		case LEX_LEQUAL:	return newScriptVar(da<=db);
		case '>':			return newScriptVar(da>db);
		case LEX_GEQUAL:	return newScriptVar(da>=db);
		default:				RETURN_NAN;
		}
	}
	// special for undefined and null
	else if( (a->isUndefined() || a->isNull()) && (b->isUndefined() || b->isNull()) ) {
		switch (op) {
		case LEX_NEQUAL:	return newScriptVar( !( ( a->isUndefined() || a->isNull() ) && ( b->isUndefined() || b->isNull() ) ) );
		case LEX_EQUAL:	return newScriptVar(    ( a->isUndefined() || a->isNull() ) && ( b->isUndefined() || b->isNull() )   );
		case LEX_GEQUAL:	
		case LEX_LEQUAL:
		case '<':
		case '>':			return newScriptVar(false);
		default:				RETURN_NAN;
		}
	}
	if (a->isArray() && b->isArray()) {
		/* Just check pointers */
		switch (op) {
		case LEX_EQUAL:	return newScriptVar(a==b);
		case LEX_NEQUAL:	return newScriptVar(a!=b);
		}
	} else if (a->isObject() && b->isObject()) {
		/* Just check pointers */
		switch (op) {
		case LEX_EQUAL:	return newScriptVar(a==b);
		case LEX_NEQUAL:	return newScriptVar(a!=b);
		}
	}
	
	// gets only an Integer, a Double, in Infinity or a NaN
	CScriptVarSmartLink a_l = a->getNumericVar();
	CScriptVarSmartLink b_l = b->getNumericVar();
	{
		CScriptVarPtr a = a_l;
		CScriptVarPtr b = b_l;

		if( a->isNaN() || b->isNaN() ) {
			switch (op) {
			case LEX_NEQUAL:	return newScriptVar(true);
			case LEX_EQUAL:
			case LEX_GEQUAL:	
			case LEX_LEQUAL:
			case '<':
			case '>':			return newScriptVar(false);
			default:				RETURN_NAN;
			}
		}
		else if((a->isInfinity() || b->isInfinity())) {
			int tmp = 0;
			int a_i=a->isInfinity(), a_sig=a->getInt()>0?1:-1;
			int b_i=b->isInfinity(), b_sig=a->getInt()>0?1:-1;
			switch (op) {
			case LEX_EQUAL:	return newScriptVar(a_i == b_i);
			case LEX_GEQUAL:	
			case '>':			return newScriptVar(a_i >= b_i);
			case LEX_LEQUAL:	
			case '<':			return newScriptVar(a_i <= b_i);
			case LEX_NEQUAL:	return newScriptVar(a_i != b_i);
			case '+':			if(a_i && b_i && a_i != b_i) RETURN_NAN;
									return newScriptVar(Infinity(b_i?b_i:a_i));
			case '-':			if(a_i && a_i == b_i) RETURN_NAN;
									return newScriptVar(Infinity(b_i?-b_i:a_i));
			case '*':			tmp = a->getInt() * b->getInt();
				if(tmp == 0)	RETURN_NAN;
									return newScriptVar(Infinity(tmp));
			case '/':			if(a_i && b_i) RETURN_NAN;
				if(b_i)			return newScriptVar(0);
									return newScriptVar(Infinity(a_sig*b_sig));
			case '%':			if(a_i) RETURN_NAN;
									return newScriptVar(Infinity(a_sig));
			case '&':			return newScriptVar( 0);
			case '|':
			case '^':			if(a_i && b_i) return newScriptVar( 0);
									return newScriptVar(a_i?b->getInt():a->getInt());
			case LEX_LSHIFT:
			case LEX_RSHIFT:	
			case LEX_RSHIFTU:	if(a_i) return newScriptVar(0);
									return newScriptVar(a->getInt());
			default:				throw new CScriptException("This operation not supported on the int datatype");
			}
		} else {
			if (!a->isDouble() && !b->isDouble()) {
				// use ints
				return DoMaths<int>(a, b, op);
			} else {
				// use doubles
				return DoMaths<double>(a, b, op);
			}
		}
	}
	
	ASSERT(0);
	return CScriptVarPtr();
}

void CScriptVar::trace(const string &name) {
	string indentStr;
	trace(indentStr, name);
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
	for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
		if((*it)->IsEnumerable())
			(*(*it))->trace(indentStr, (*it)->name);
	}
	if(recursionSet) {
		recursionSet->recursionPathBase = 0;
		recursionFlag = 0;
	}
	indentStr = indentStr.substr(0, indentStr.length()-2);
}

string CScriptVar::getFlagsAsString() {
	string flagstr = "";
	if (isFunction()) flagstr = flagstr + "FUNCTION ";
	if (isObject()) flagstr = flagstr + "OBJECT ";
	if (isArray()) flagstr = flagstr + "ARRAY ";
	if (isNative()) flagstr = flagstr + "NATIVE ";
	if (isDouble()) flagstr = flagstr + "DOUBLE ";
	if (isInt()) flagstr = flagstr + "INTEGER ";
	if (isBool()) flagstr = flagstr + "BOOLEAN ";
	if (isString()) flagstr = flagstr + "STRING ";
	if (isNaN()) flagstr = flagstr + "NaN ";
	if (isInfinity()) flagstr = flagstr + "INFINITY ";
	return flagstr;
}

#ifdef TODO
void CScriptVar::getJSON(ostringstream &destination, const string linePrefix) {
	if (isObject()) {
		string indentedLinePrefix = linePrefix+"  ";
		// children - handle with bracketed list
		destination << "{ \n";
		int count = Childs.size();
		for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
			destination << indentedLinePrefix;
			if (isAlphaNum((*it)->name))
				destination  << (*it)->name;
			else
				destination  << getJSString((*it)->name);
			destination  << " : ";
			(*(*it))->getJSON(destination, indentedLinePrefix);
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
#endif


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
				for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it!= Childs.end(); ++it) {
					if((*(*it))->recursionSet == recursionSet)
						(*(*it))->unrefInternal();
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
	if(recursionSet) { // this Var is in a Set
		recursionSet->sumRefs--;
		if(Owner && Owner->recursionSet == recursionSet) { // internal Ref
			if(internalRefs==1) {
				if(refs) {
					RECURSION_SET_VAR *old_set = unrefInternal();

					if(old_set) { // we have breaked a recursion bu the recursion Set is not destroyed
						// we needs a redetection of recursions for all Vars in the Set;
						// first we destroy the Set
						for(RECURSION_SET::iterator it= old_set->recursionSet.begin(); it!=old_set->recursionSet.end(); ++it) {
							(*it)->internalRefs = 0;
							(*it)->recursionSet = 0;
						}
						// now we can redetect recursions
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
				SCRIPTVAR_CHILDS_t copyOfChilds(Childs);
				Childs.clear();
				SCRIPTVAR_CHILDS_it it;
				for(it = copyOfChilds.begin(); it!= copyOfChilds.end(); ++it) {
					delete *it;
				}
				copyOfChilds.clear();
			} // otherwise nothing to do
		}
	}
	else if (refs==0) {
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
	return;
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
			// +1, because the Set is already in the new Set
			it = find_last(recursionPath, recursionSet->recursionPathBase)+1;
		} else {													// recursion starts by a Var
			new_set = new RECURSION_SET_VAR(this);	// create a new Set(marks it as "in the Path", with "this as start Var)
			// find the Var in the Path
			it = find_last(recursionPath, this);
		}
		// insert the Path beginning by this Var or the next Var after dis Set
		// in the new_set and removes it from the Path
		for(; it!= recursionPath.end(); ++it) {
			if((*it)->recursionSet) {											// insert an existing Set
				RECURSION_SET_VAR *old_set = (*it)->recursionSet;
				new_set->sumInternalRefs += old_set->sumInternalRefs;	// for speed up; we adds the sum of internal refs and the sum of
				new_set->sumRefs += old_set->sumRefs;						// refs to the new Set here instead rather than separately for each Var
				new_set->sumInternalRefs++;									// a new Set in a Set is linked 
				(*it)->internalRefs++;											// over an internal reference
				// insert all Vars of the old Set in the new Set
				for(RECURSION_SET::iterator set_it = old_set->recursionSet.begin(); set_it!= old_set->recursionSet.end(); ++set_it) {
					new_set->recursionSet.insert(*set_it);	// insert this Var to the Set
					(*set_it)->recursionSet = new_set;		// the Var is now in a Set
				}
				delete old_set;
			} else {											// insert this Var in the new set
				new_set->sumInternalRefs++;			// a new Var in a Set is linked 
				(*it)->internalRefs++;					// over an internal reference
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
		// goes to all Childs of the Set
		RECURSION_SET Set(recursionSet->recursionSet);
		for(RECURSION_SET::iterator set_it = Set.begin(); set_it != Set.end(); ++set_it) {
			for(SCRIPTVAR_CHILDS_it it = (*set_it)->Childs.begin(); it != (*set_it)->Childs.end(); ++it) {
				if((*(*it))->recursionSet != recursionSet)
					(*(*it))->recoursionCheck(recursionPath);
			}
		}
		if(old_set == recursionSet) { // old_set == recursionSet means this Set is *not* included in an other Set
			recursionSet->recursionPathBase = 0;
			recursionPath.pop_back();
		} // otherwise this Set is included in an other Set and is already removed from Path
	} else {
		recursionPath.push_back(this);	// push "this" in the Path
		recursionFlag = 1;									// marked this Var as "the Var is in the Path"
		// goes to all Childs of the Var
		for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
			(*(*it))->recoursionCheck(recursionPath);
		}
		if(recursionFlag) { // recursionFlag!=0 means this Var is not included in a Set
			recursionFlag = 0;
			ASSERT(recursionPath.back() == this);
			recursionPath.pop_back();
		} else { // otherwise this Var is included in a Set and is already removed from Path
			ASSERT(recursionSet && recursionSet->recursionPathBase);
			if(recursionSet->recursionPathBase == this) { // the Var is the Base of the Set
				recursionSet->recursionPathBase = 0;
				ASSERT(recursionPath.back() == this);
				recursionPath.pop_back();
			}
		}
	}
}
void CScriptVar::setTempraryIDrecursive(int ID) {
	if(temporaryID != ID) {
		temporaryID = ID;
		for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
			(*(*it))->setTempraryIDrecursive(ID);
		}
	}
}


////////////////////////////////////////////////////////////////////////// CScriptVarObject

declare_dummy_t(Object);
//CScriptVarObject::CScriptVarObject(CTinyJS *Context, bool withPrototype=true) : CScriptVar(Context, withPrototype?Context->objectPrototype:CScriptVarPtr()) {}
CScriptVarObject::~CScriptVarObject() {}
CScriptVarPtr CScriptVarObject::clone() { return new CScriptVarObject(*this); }
bool CScriptVarObject::isObject() { return true; }
string CScriptVarObject::getString() { return "[ Object ]"; };
string CScriptVarObject::getParsableString(const string &indentString, const string &indent) {
	ostringstream destination;
	string nl = indent.size() ? "\n" : " ";

	destination << "{";
	if(Childs.size()) {
		string new_indentString = indentString + indent;
		int count = 0;
		destination << nl;
		for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
			if((*it)->IsEnumerable()) {
				if (count++) destination  << "," << nl;
				if (isAlphaNum((*it)->name))
					destination << new_indentString << (*it)->name;
				else
					destination << new_indentString << "\"" << getJSString((*it)->name) << "\"";
				destination  << " : ";
				destination << (*(*it))->getParsableString(new_indentString, indent);
			}
		}
		destination << nl << indentString;
	}
	destination << "}";
	return destination.str();
}
string CScriptVarObject::getVarType() { return "object"; }


////////////////////////////////////////////////////////////////////////// CScriptVarAccessor

declare_dummy_t(Accessor);
CScriptVarAccessor::~CScriptVarAccessor() {}
CScriptVarPtr CScriptVarAccessor::clone() { return new CScriptVarAccessor(*this); }
bool CScriptVarAccessor::isAccessor() { return true; }
string CScriptVarAccessor::getString() { return "[ Object ]"; };
string CScriptVarAccessor::getParsableString(const string &indentString, const string &indent) {

	/*
	ostringstream destination;
	string nl = indent.size() ? "\n" : " ";

	destination << "{";
	int count = Childs.size();
	if(count) {
		string new_indentString = indentString + indent;

		destination << nl;
		for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
			if (isAlphaNum((*it)->name))
				destination << new_indentString << (*it)->name;
			else
				destination << new_indentString << "\"" << getJSString((*it)->name) << "\"";
			destination  << " : ";
			destination << (*(*it))->getParsableString(new_indentString, indent);
			if (--count) destination  << "," << nl;
		}
		destination << nl << indentString;
	}
	destination << "}";
	return destination.str();
*/
	return "";
}
string CScriptVarAccessor::getVarType() { return "accessor"; }



////////////////////////////////////////////////////////////////////////// CScriptVarArray

declare_dummy_t(Array);
CScriptVarArray::CScriptVarArray(CTinyJS *Context) : CScriptVar(Context, Context->arrayPrototype) {
	CScriptVarLink *acc = addChild("length", newScriptVar(Accessor), 0);
	CScriptVarFunctionPtr getter(::newScriptVar(Context, this, &CScriptVarArray::native_Length, 0));
	getter->setFunctionData(new CScriptTokenDataFnc);
	(*acc)->addChild(TINYJS_ACCESSOR_GET_VAR, getter, 0);
}

CScriptVarArray::~CScriptVarArray() {}
CScriptVarPtr CScriptVarArray::clone() { return new CScriptVarArray(*this); }
bool CScriptVarArray::isArray() { return true; }
string CScriptVarArray::getString() { 
	ostringstream destination;
	int len = getArrayLength();
	for (int i=0;i<len;i++) {
		destination << getArrayIndex(i)->getString();
		if (i<len-1) destination  << ", ";
	}
	return destination.str();
}
string CScriptVarArray::getVarType() { return "object"; }
string CScriptVarArray::getParsableString(const string &indentString, const string &indent) {
	ostringstream destination;
	string nl = indent.size() ? "\n" : " ";

	destination << "[";
	int len = getArrayLength();
	if(len) {
		destination << nl;
		string new_indentString = indentString + indent;
		for (int i=0;i<len;i++) {
			destination << new_indentString << getArrayIndex(i)->getParsableString(new_indentString, indent);
			if (i<len-1) destination  << "," << nl;
		}
		destination << nl << indentString;
	}
	destination << "]";
	return destination.str();
}

void CScriptVarArray::native_Length(const CFunctionsScopePtr &c, void *data)
{
	c->setReturnVar(newScriptVar(c->getParameter("this")->getArrayLength()));
}

////////////////////////////////////////////////////////////////////////// CScriptVarNull

declare_dummy_t(Null);
CScriptVarNull::~CScriptVarNull() {}
CScriptVarPtr CScriptVarNull::clone() { return new CScriptVarNull(*this); }
bool CScriptVarNull::isNull() { return true; }
string CScriptVarNull::getString() { return "null"; };
string CScriptVarNull::getVarType() { return "null"; }


////////////////////////////////////////////////////////////////////////// CScriptVarUndefined

declare_dummy_t(Undefined);
CScriptVarUndefined::~CScriptVarUndefined() {}
CScriptVarPtr CScriptVarUndefined::clone() { return new CScriptVarUndefined(*this); }
bool CScriptVarUndefined::isUndefined() { return true; }
string CScriptVarUndefined::getString() { return "undefined"; };
string CScriptVarUndefined::getVarType() { return "undefined"; }


////////////////////////////////////////////////////////////////////////// CScriptVarNaN

declare_dummy_t(NaN);
CScriptVarNaN::~CScriptVarNaN() {}
CScriptVarPtr CScriptVarNaN::clone() { return new CScriptVarNaN(*this); }
bool CScriptVarNaN::isNaN() { return true; }
string CScriptVarNaN::getString() { return "NaN"; };
string CScriptVarNaN::getVarType() { return "number"; }


////////////////////////////////////////////////////////////////////////// CScriptVarString

CScriptVarString::CScriptVarString(CTinyJS *Context, const std::string &Data) : CScriptVar(Context, Context->stringPrototype), data(Data) {
	CScriptVarLink *acc = addChild("length", newScriptVar(Accessor), 0);
	CScriptVarFunctionPtr getter(::newScriptVar(Context, this, &CScriptVarString::native_Length, 0));
	getter->setFunctionData(new CScriptTokenDataFnc);
	(*acc)->addChild(TINYJS_ACCESSOR_GET_VAR, getter, 0);
}
CScriptVarString::~CScriptVarString() {}
CScriptVarPtr CScriptVarString::clone() { return new CScriptVarString(*this); }
bool CScriptVarString::isString() { return true; }
int CScriptVarString::getInt() {return strtol(data.c_str(),0,0); }
bool CScriptVarString::getBool() {return data.length()!=0;}
double CScriptVarString::getDouble() {return strtod(data.c_str(),0);}
string CScriptVarString::getString() { return data; }
string CScriptVarString::getVarType() { return "string"; }
string CScriptVarString::getParsableString() { return getJSString(data); }

void CScriptVarString::native_Length(const CFunctionsScopePtr &c, void *data)
{
	c->setReturnVar(newScriptVar((int)this->data.size()));
}


////////////////////////////////////////////////////////////////////////// CScriptVarIntegerBase

CScriptVarIntegerBase::CScriptVarIntegerBase(CTinyJS *Context, int Data) : CScriptVar(Context, Context->numberPrototype), data(Data) {}
CScriptVarIntegerBase::~CScriptVarIntegerBase() {}
int CScriptVarIntegerBase::getInt() {return data; }
bool CScriptVarIntegerBase::getBool() {return data!=0;}
double CScriptVarIntegerBase::getDouble() {return data;}
string CScriptVarIntegerBase::getString() {return int2string(data);}
string CScriptVarIntegerBase::getVarType() { return "number"; }


////////////////////////////////////////////////////////////////////////// CScriptVarInteger

CScriptVarInteger::~CScriptVarInteger() {}
CScriptVarPtr CScriptVarInteger::clone() { return new CScriptVarInteger(*this); }
bool CScriptVarInteger::isNumber() { return true; }
bool CScriptVarInteger::isInt() { return true; }


////////////////////////////////////////////////////////////////////////// CScriptVarBool

CScriptVarBool::~CScriptVarBool() {}
CScriptVarPtr CScriptVarBool::clone() { return new CScriptVarBool(*this); }
bool CScriptVarBool::isBool() { return true; }
string CScriptVarBool::getString() {return data!=0?"true":"false";}
string CScriptVarBool::getVarType() { return "boolean"; }


////////////////////////////////////////////////////////////////////////// CScriptVarInfinity

Infinity InfinityPositive(1);
Infinity InfinityNegative(-1);
CScriptVarInfinity::~CScriptVarInfinity() {}
CScriptVarPtr CScriptVarInfinity::clone() { return new CScriptVarInfinity(*this); }
int CScriptVarInfinity::isInfinity() { return data; }
string CScriptVarInfinity::getString() {return data<0?"-Infinity":"Infinity";}


////////////////////////////////////////////////////////////////////////// CScriptVarDouble

CScriptVarDouble::CScriptVarDouble(CTinyJS *Context, double Data) : CScriptVar(Context, Context->numberPrototype), data(Data) {}
CScriptVarDouble::~CScriptVarDouble() {}
CScriptVarPtr CScriptVarDouble::clone() { return new CScriptVarDouble(*this); }
bool CScriptVarDouble::isNumber() { return true; }
bool CScriptVarDouble::isDouble() { return true; }

int CScriptVarDouble::getInt() {return (int)data; }
bool CScriptVarDouble::getBool() {return data!=0.0;}
double CScriptVarDouble::getDouble() {return data;}
string CScriptVarDouble::getString() {return float2string(data);}
string CScriptVarDouble::getVarType() { return "number"; }


////////////////////////////////////////////////////////////////////////// CScriptVarFunction

CScriptVarFunction::CScriptVarFunction(CTinyJS *Context, CScriptTokenDataFnc *Data) : CScriptVar(Context, Context->functionPrototype), data(0) { 
	setFunctionData(Data); 
}
CScriptVarFunction::~CScriptVarFunction() { setFunctionData(0); }
CScriptVarPtr CScriptVarFunction::clone() { return new CScriptVarFunction(*this); }
bool CScriptVarFunction::isFunction() { return true; }

string CScriptVarFunction::getString() {return "[ Function ]";}
string CScriptVarFunction::getVarType() { return "function"; }
string CScriptVarFunction::getParsableBlockString(TOKEN_VECT::iterator &it, TOKEN_VECT::iterator end, const string indentString, const string indent) {
	ostringstream destination;
	string nl = indent.size() ? "\n" : " ";
	string my_indentString = indentString;
	bool add_nl=false, block_start=false;

	for(; it != end; ++it) {

		string OutString;
		if(add_nl) OutString.append(nl).append(my_indentString);
		bool old_block_start = block_start;
		add_nl = block_start = false;
		if(LEX_TOKEN_DATA_STRING(it->token))
			OutString.append(it->String()).append(" ");
		else if(LEX_TOKEN_DATA_FLOAT(it->token))
			OutString.append(float2string(it->Float())).append(" ");
		else if(it->token == LEX_INT)
			OutString.append(int2string(it->Int())).append(" ");
		else if(LEX_TOKEN_DATA_FUNCTION(it->token)) {
			OutString.append("function ");
			if(it->token == LEX_R_FUNCTION)
				OutString.append(data->name);
			OutString.append("(");
			if(data->parameter.size()) {
				OutString.append(data->parameter.front());
				for(vector<string>::iterator it=data->parameter.begin()+1; it!=data->parameter.end(); ++it)
					OutString.append(", ").append(*it);
			}
			OutString.append(") ");
			TOKEN_VECT::iterator it=data->body.begin();
			OutString += getParsableBlockString(it, data->body.end(), indentString, indent);

		} else if(it->token == '{') {
			OutString.append("{");
			my_indentString.append(indent);
			add_nl = block_start = true;
		} else if(it->token == '}') {
			my_indentString.resize(my_indentString.size() - min(my_indentString.size(),indent.size()));
			if(old_block_start) 
				OutString =  "}";
			else
				OutString = nl + my_indentString + "}";
			add_nl = true;
		} else if(it->token == LEX_T_SKIP) {
			// ignore SKIP-Token
		} else {
			OutString.append(CScriptLex::getTokenStr(it->token));
			if(it->token==';') add_nl=true; else OutString.append(" ");
		}
		destination << OutString;
	}
	return destination.str();
}

string CScriptVarFunction::getParsableString(const string &indentString, const string &indent) {
	ostringstream destination;
	string nl = indent.size() ? "\n" : " ";

	destination << "function "<<data->name<<"(";

	// get list of parameters
	if(data->parameter.size()) {
		destination << data->parameter.front();
		for(vector<string>::iterator it = data->parameter.begin()+1; it != data->parameter.end(); ++it) {
			destination << ", " << *it;
		}
	}

	// add function body
	destination << ") ";


	if(isNative()) {
		destination << "{ /* native Code */ }";
	} else {
		TOKEN_VECT::iterator it=data->body.begin();
		destination << getParsableBlockString(it, data->body.end(), indentString, indent);
	}
	return destination.str();
}
CScriptTokenDataFnc *CScriptVarFunction::getFunctionData() { return data; }

void CScriptVarFunction::setFunctionData(CScriptTokenDataFnc *Data) {
	if(data) { data->unref(); data = 0; }
	if(Data) { 
		data = Data; data->ref(); 
		addChildNoDup("length", newScriptVar((int)data->parameter.size()), 0);
	}
}


////////////////////////////////////////////////////////////////////////// CScriptVarFunctionNative

CScriptVarFunctionNative::~CScriptVarFunctionNative() {}
bool CScriptVarFunctionNative::isNative() { return true; }
string CScriptVarFunctionNative::getString() {return "[ Function Native ]";}


////////////////////////////////////////////////////////////////////////// CScriptVarFunctionNativeCallback

CScriptVarFunctionNativeCallback::~CScriptVarFunctionNativeCallback() {}
CScriptVarPtr CScriptVarFunctionNativeCallback::clone() { return new CScriptVarFunctionNativeCallback(*this); }
void CScriptVarFunctionNativeCallback::callFunction(const CFunctionsScopePtr &c) { jsCallback(c, jsUserData); }


////////////////////////////////////////////////////////////////////////// CScriptVarScope


declare_dummy_t(Scope);
CScriptVarScope::~CScriptVarScope() {}
CScriptVarPtr CScriptVarScope::clone() { return CScriptVarPtr(); }
bool CScriptVarScope::isObject() { return false; }
CScriptVarPtr CScriptVarScope::scopeVar() { return this; }	///< to create var like: var a = ...
CScriptVarPtr CScriptVarScope::scopeLet() { return this; }	///< to create var like: let a = ...
CScriptVarLink *CScriptVarScope::findInScopes(const string &childName) { 
	return  CScriptVar::findChild(childName); 
}
CScriptVarScopePtr CScriptVarScope::getParent() { return CScriptVarScopePtr(); } ///< no Parent


////////////////////////////////////////////////////////////////////////// CScriptVarScopeFnc

declare_dummy_t(ScopeFnc);
CScriptVarScopeFnc::~CScriptVarScopeFnc() {}
CScriptVarLink *CScriptVarScopeFnc::findInScopes(const string &childName) { 
	CScriptVarLink * ret = findChild(childName); 
	if( !ret ) {
		if(closure) ret = CScriptVarScopePtr(closure)->findInScopes(childName);
		else ret = context->getRoot()->findChild(childName);
	}
	return ret;
}


////////////////////////////////////////////////////////////////////////// CScriptVarScopeLet

declare_dummy_t(ScopeLet);
CScriptVarScopeLet::CScriptVarScopeLet(const CScriptVarScopePtr &Parent) // constructor for LetScope
	: CScriptVarScope(Parent->getContext()), parent(addChild(TINYJS_SCOPE_PARENT_VAR, Parent, 0)) {}

CScriptVarScopeLet::~CScriptVarScopeLet() {}
CScriptVarPtr CScriptVarScopeLet::scopeVar() {						// to create var like: var a = ...
	return getParent()->scopeVar(); 
}
CScriptVarScopePtr CScriptVarScopeLet::getParent() { return parent; }
CScriptVarLink *CScriptVarScopeLet::findInScopes(const string &childName) { 
	CScriptVarLink *ret = findChild(childName); 
	if( !ret ) ret = getParent()->findInScopes(childName);
	return ret;
}

////////////////////////////////////////////////////////////////////////// CScriptVarScopeWith

declare_dummy_t(ScopeWith);
CScriptVarScopeWith::~CScriptVarScopeWith() {}
CScriptVarPtr CScriptVarScopeWith::scopeLet() { 							// to create var like: let a = ...
	return getParent()->scopeLet();
}
CScriptVarLink *CScriptVarScopeWith::findInScopes(const string &childName) { 
	CScriptVarLink * ret = (*with)->findChild(childName); 
	if( !ret ) ret = getParent()->findInScopes(childName);
	return ret;
}

// ----------------------------------------------------------------------------------- CSCRIPT
bool CTinyJS::noexecute = false; 
CTinyJS::CTinyJS() {
	CScriptVarPtr var;
	t = 0;
	runtimeFlags = 0;
	first = 0;
	uniqueID = 0;

	
	//////////////////////////////////////////////////////////////////////////
	// Object-Prototype
	objectPrototype = newScriptVar(Object);

	//////////////////////////////////////////////////////////////////////////
	// Scopes
	root = ::newScriptVar(this, Scope);
	_scopes.push_back(root);


	//////////////////////////////////////////////////////////////////////////
	// Add built-in classes


	//////////////////////////////////////////////////////////////////////////
	// Object
	var = addNative("function Object()", this, &CTinyJS::native_Object);
	var->addChild(TINYJS_PROTOTYPE_CLASS, objectPrototype);
	addNative("function Object.prototype.hasOwnProperty(prop)", this, &CTinyJS::native_Object_hasOwnProperty); // execute the given string and return the result
	
	//////////////////////////////////////////////////////////////////////////
	// Array
	arrayPrototype = newScriptVar(Object);
	var = addNative("function Array()", this, &CTinyJS::native_Array);
	var->addChild(TINYJS_PROTOTYPE_CLASS, arrayPrototype);

	//////////////////////////////////////////////////////////////////////////
	// String
	stringPrototype = newScriptVar(Object);
	var = addNative("function String()", this, &CTinyJS::native_String);
	var->addChild(TINYJS_PROTOTYPE_CLASS, stringPrototype);

	//////////////////////////////////////////////////////////////////////////
	// Number
	numberPrototype = newScriptVar(Object);
	var = addNative("function Number()", this, &CTinyJS::native_String);
	var->addChild(TINYJS_PROTOTYPE_CLASS, numberPrototype);

	//////////////////////////////////////////////////////////////////////////
	// Function
	functionPrototype = newScriptVar(Object);
	var = addNative("function Function(params, body)", this, &CTinyJS::native_Function); // execute the given string and return the result
	var->addChild(TINYJS_PROTOTYPE_CLASS, functionPrototype);
	addNative("function Function.prototype.call(objc)", this, &CTinyJS::native_Function_call); // execute the given string and return the result
	addNative("function Function.prototype.apply(objc, args)", this, &CTinyJS::native_Function_apply); // execute the given string and return the result

	//////////////////////////////////////////////////////////////////////////
	addNative("function eval(jsCode)", this, &CTinyJS::native_Eval); // execute the given string and return the result
	addNative("function JSON.parse(text, reviver)", this, &CTinyJS::native_JSON_parse); // execute the given string and return the result

}

CTinyJS::~CTinyJS() {
	ASSERT(!t);
	root->removeAllChildren();
	ClearLostVars();
#ifdef _DEBUG
	for(CScriptVar *p = first; p; p=p->next)
	{
		if(objectPrototype != CScriptVarPtr(p) &&
			arrayPrototype != CScriptVarPtr(p) &&
			numberPrototype != CScriptVarPtr(p) &&
			stringPrototype != CScriptVarPtr(p) &&
			functionPrototype != CScriptVarPtr(p) && 
			root != CScriptVarPtr(p) 
			)
			printf("%p\n", p);
	}
#endif
#if DEBUG_MEMORY
	show_allocated();
#endif
}

void CTinyJS::throwError(bool &execute, const string &message) {
	if(execute && (runtimeFlags & RUNTIME_CANTHROW)) {
		exceptionVar = newScriptVar(message);
		runtimeFlags |= RUNTIME_THROW;
		execute = false;
		return;
	}
	throw new CScriptException(message, t->currentFile, t->currentLine(), t->currentColumn());
}

void CTinyJS::throwError(bool &execute, const string &message, CScriptTokenizer::ScriptTokenPosition &Pos) {
	if(execute && (runtimeFlags & RUNTIME_CANTHROW)) {
		exceptionVar = newScriptVar(message);
		runtimeFlags |= RUNTIME_THROW;
		execute = false;
		return;
	}
	throw new CScriptException(message, t->currentFile, Pos.currentLine(), Pos.currentColumn());
}

void CTinyJS::trace() {
	root->trace();
}

void CTinyJS::execute(CScriptTokenizer &Tokenizer) {
	evaluateComplex(Tokenizer);
}

void CTinyJS::execute(const char *Code, const string &File, int Line, int Column) {
	evaluateComplex(Code, File, Line, Column);
}

void CTinyJS::execute(const string &Code, const string &File, int Line, int Column) {
	evaluateComplex(Code, File, Line, Column);
}

CScriptVarLink CTinyJS::evaluateComplex(CScriptTokenizer &Tokenizer) {
	CScriptVarSmartLink v;
	t = &Tokenizer;
	try {
		bool execute = true;
		do {
			v = execute_statement(execute);
			while (t->tk==';') t->match(';'); // skip empty statements
		} while (t->tk!=LEX_EOF);
	} catch (CScriptException *e) {
		runtimeFlags = 0; // clean up runtimeFlags
		ostringstream msg;
		msg << "Error " << e->text;
		if(e->line >= 0) msg << " at Line:" << e->line+1 << " Column:" << e->column+1;
		if(e->file.length()) msg << " in " << e->file;
		delete e;
		t=0;
		throw new CScriptException(msg.str(),"");
	}
	t=0;
	
	ClearLostVars(v);

	int UniqueID = getUniqueID(); 
	root->setTempraryIDrecursive(UniqueID);
	if(v) (*v)->setTempraryIDrecursive(UniqueID);
	for(CScriptVar *p = first; p; p=p->next)
	{
		if(p->temporaryID != UniqueID)
			printf("%p\n", p);
	}

	if (v) {
		CScriptVarLink r = *v;
		return r;
	}
	// return undefined...
	return CScriptVarLink(newScriptVar(Undefined));
}
CScriptVarLink CTinyJS::evaluateComplex(const char *Code, const string &File, int Line, int Column) {
	CScriptTokenizer Tokenizer(Code, File, Line, Column);
	return evaluateComplex(Tokenizer);
}
CScriptVarLink CTinyJS::evaluateComplex(const string &Code, const string &File, int Line, int Column) {
	CScriptTokenizer Tokenizer(Code.c_str(), File, Line, Column);
	return evaluateComplex(Tokenizer);
}

string CTinyJS::evaluate(CScriptTokenizer &Tokenizer) {
	return evaluateComplex(Tokenizer).getVarPtr()->getString();
}
string CTinyJS::evaluate(const char *Code, const string &File, int Line, int Column) {
	return evaluateComplex(Code, File, Line, Column).getVarPtr()->getString();
}
string CTinyJS::evaluate(const string &Code, const string &File, int Line, int Column) {
	return evaluate(Code.c_str(), File, Line, Column);
}

CScriptVarFunctionNativePtr CTinyJS::addNative(const string &funcDesc, JSCallback ptr, void *userdata) {
	return addNative(funcDesc, ::newScriptVar(this, ptr, userdata));
}

CScriptVarFunctionNativePtr CTinyJS::addNative(const string &funcDesc, CScriptVarFunctionNativePtr Var) {
	CScriptLex lex(funcDesc.c_str());
	CScriptVarPtr base = root;

	lex.match(LEX_R_FUNCTION);
	string funcName = lex.tkStr;
	lex.match(LEX_ID);
	/* Check for dots, we might want to do something like function String.substring ... */
	while (lex.tk == '.') {
		lex.match('.');
		CScriptVarLink *link = base->findChild(funcName);
		// if it doesn't exist, make an object class
		if (!link) link = base->addChild(funcName, newScriptVar(Object));
		base = link->getVarPtr();
		funcName = lex.tkStr;
		lex.match(LEX_ID);
	}

	auto_ptr<CScriptTokenDataFnc> pFunctionData(new CScriptTokenDataFnc);
	lex.match('(');
	while (lex.tk!=')') {
		pFunctionData->parameter.push_back(lex.tkStr);
		lex.match(LEX_ID);
		if (lex.tk!=')') lex.match(',');
	}
	lex.match(')');
	Var->setFunctionData(pFunctionData.release());
	base->addChild(funcName,  Var);
	return Var;

}

CScriptVarSmartLink CTinyJS::parseFunctionDefinition(CScriptToken &FncToken) {
	CScriptTokenDataFnc &Fnc = FncToken.Fnc();
	string fncName = (FncToken.token == LEX_T_FUNCTION_OPERATOR) ? TINYJS_TEMP_NAME : Fnc.name;
	CScriptVarSmartLink funcVar = new CScriptVarLink(newScriptVar(&Fnc), fncName);
	if(scope() != root)
		funcVar->getVarPtr()->addChild(TINYJS_FUNCTION_CLOSURE_VAR, scope(), 0);
	return funcVar;
}
/*
CScriptVarSmartLink CTinyJS::parseFunctionDefinition() {
	CScriptTokenDataFnc &Fnc = t->getToken().Fnc();
	string fncName = (t->tk == LEX_T_FUNCTION_FORCE_ANONYMOUS) ? TINYJS_TEMP_NAME : Fnc.name;
	CScriptVarSmartLink funcVar = new CScriptVarLink(new CScriptVar(Fnc), fncName);
	(*funcVar)->addChild("length", new CScriptVar((int)Fnc.parameter.size()));
	return funcVar;
}
*/

CScriptVarSmartLink CTinyJS::parseFunctionsBodyFromString(const string &Parameter, const string &FncBody) {
	string Fnc = "function ("+Parameter+"){"+FncBody+"}";
	CScriptTokenizer tokenizer(Fnc.c_str());
	return parseFunctionDefinition(tokenizer.getToken());
}

class CScriptEvalException {
public:
	int runtimeFlags;
	CScriptEvalException(int RuntimeFlags) : runtimeFlags(RuntimeFlags){}
};

CScriptVarPtr CTinyJS::callFunction(const CScriptVarPtr &Function, std::vector<CScriptVarPtr> &Arguments, const CScriptVarPtr& This, bool &execute) {
//CScriptVarSmartLink CTinyJS::callFunction(CScriptVarSmartLink &Function, vector<CScriptVarSmartLink> &Arguments, const CScriptVarPtr& This, bool &execute) {
	ASSERT(Function->isFunction());

	CScriptVarScopeFncPtr functionRoot(::newScriptVar(this, ScopeFnc, Function->findChild(TINYJS_FUNCTION_CLOSURE_VAR)));
	functionRoot->addChild("this", This);
	CScriptVarSmartLink arguments = functionRoot->addChild(TINYJS_ARGUMENTS_VAR, newScriptVar(Object));
	CScriptTokenDataFnc *Fnc = Function->getFunctionData();
	if(Fnc->name.size()) functionRoot->addChild(Fnc->name, Function);

	int length_proto = Fnc->parameter.size();
	int length_arguments = Arguments.size();
	int length = max(length_proto, length_arguments);
	for(int arguments_idx = 0; arguments_idx<length; ++arguments_idx) {
		string arguments_idx_str = int2string(arguments_idx);
		CScriptVarSmartLink value;
		if(arguments_idx < length_arguments) {
			value = (*arguments)->addChild(arguments_idx_str, Arguments[arguments_idx]);
		} else {
			value = newScriptVar(Undefined);
		}
		if(arguments_idx < length_proto) {
			functionRoot->addChildNoDup(Fnc->parameter[arguments_idx], value);
		}
	}
	(*arguments)->addChild("length", newScriptVar(length_arguments));
	CScriptVarSmartLink returnVar;

	int old_function_runtimeFlags = runtimeFlags; // save runtimeFlags
	runtimeFlags &= ~RUNTIME_LOOP_MASK; // clear LOOP-Flags
	// execute function!
	// add the function's execute space to the symbol table so we can recurse
	// scopes.push_back(SCOPES::SCOPE_TYPE_FUNCTION, functionRoot->get());
	CScopeControl ScopeControl(this);
	ScopeControl.addFncScope(functionRoot);
	if (Function->isNative()) {
		try {
			CScriptVarFunctionNativePtr(Function)->callFunction(functionRoot);
//				((CScriptVarFunctionNative*)*Function.getVar())->callFunction(functionRoot.getVar());
			runtimeFlags = old_function_runtimeFlags | (runtimeFlags & RUNTIME_THROW); // restore runtimeFlags
			if(runtimeFlags & RUNTIME_THROW) {
				execute = false;
			}
		} catch (CScriptEvalException *e) {
			int exeption_runtimeFlags = e->runtimeFlags;
			delete e;
			runtimeFlags = old_function_runtimeFlags; // restore runtimeFlags
			if(exeption_runtimeFlags & RUNTIME_BREAK) {
				delete e;
				if(runtimeFlags & RUNTIME_CANBREAK) {
					runtimeFlags |= RUNTIME_BREAK;
					execute = false;
				}
				else
					throw new CScriptException("'break' must be inside loop or switch");
			} else if(exeption_runtimeFlags & RUNTIME_CONTINUE) {
				if(runtimeFlags & RUNTIME_CANCONTINUE) {
					runtimeFlags |= RUNTIME_CONTINUE;
					execute = false;
				}
				else
					throw new CScriptException("'continue' must be inside loop");
			}
		} catch (CScriptVarPtr v) {
			if(runtimeFlags & RUNTIME_CANTHROW) {
				runtimeFlags |= RUNTIME_THROW;
				execute = false;
				exceptionVar = v;
			}
			else {
				string e = "uncaught exception: '"+v->getString()+"' in: "+Function->getFunctionData()->name+"()";
				throw new CScriptException(e);
			}
		}
	} else {
		/* we just want to execute the block, but something could
			* have messed up and left us with the wrong ScriptLex, so
			* we want to be careful here... */
		string oldFile = t->currentFile;
		t->currentFile = Fnc->file;
		t->pushTokenScope(Fnc->body);
		SET_RUNTIME_CANRETURN;
		execute_block(execute);
		t->currentFile = oldFile;

		// because return will probably have called this, and set execute to false
		runtimeFlags = old_function_runtimeFlags | (runtimeFlags & RUNTIME_THROW); // restore runtimeFlags
		if(!(runtimeFlags & RUNTIME_THROW)) {
			execute = true;
		}
	}
	/* get the real return var before we remove it from our function */
	if(returnVar = functionRoot->findChild(TINYJS_RETURN_VAR))
		return returnVar->getVarPtr();
	else
		return newScriptVar(Undefined);
}


/*
CScriptVarSmartLink CTinyJS::setValue( CScriptVarSmartLink Var )
{
	if((*Var)->isAccessor()) {

	}
}

CScriptVarSmartLink CTinyJS::getValue( CScriptVarSmartLink Var, bool execute )
{
	if((*Var)->isAccessor()) {
		CScriptVarLink *get = (*Var)->findChild("__accessor_get__");
		if(get) {
		} else
			return CScriptVarSmartLink(new CScriptVar());
	} else
		return Var;
}
*/
CScriptVarSmartLink CTinyJS::execute_literals(bool &execute) {
	if(t->tk == LEX_ID) {
		CScriptVarSmartLink a;
		if(execute) {
			a = findInScopes(t->tkStr());
			if (!a) {
				/* Variable doesn't exist! JavaScript says we should create it
				 * (we won't add it here. This is done in the assignment operator)*/
				if(t->tkStr() == "this") {
					a = new CScriptVarLink(root); // fake this
				} else {
					a = new CScriptVarLink(newScriptVar(Undefined), t->tkStr());
				}
			}
			t->match(t->tk);
			return a;
		}
		t->match(t->tk);
	} else if (t->tk==LEX_INT) {
		CScriptVarPtr a = newScriptVar(t->getToken().Int());
		t->match(t->tk);
		return new CScriptVarLink(a);
	} else if (t->tk==LEX_FLOAT) {
		CScriptVarPtr a = newScriptVar(t->getToken().Float());
		t->match(t->tk);
		return new CScriptVarLink(a);
	} else if (t->tk==LEX_STR) {
		CScriptVarPtr a = newScriptVar(t->getToken().String());
		t->match(LEX_STR);
		return new CScriptVarLink(a);
	} else if (t->tk=='{') {
		if(execute) {
			CScriptVarPtr contents(newScriptVar(Object));
			/* JSON-style object definition */
			t->match('{');
			while (t->tk != '}') {
				// we only allow strings or IDs on the left hand side of an initialisation
				if (t->tk==LEX_STR || t->tk==LEX_ID) {
					string id = t->tkStr();
					t->match(t->tk);
					t->match(':');
					CScriptVarSmartLink a = execute_assignment(execute);
					if (execute) {
						contents->addChildNoDup(id, a->getValue());
					}
				} else if(t->tk==LEX_T_GET || t->tk==LEX_T_SET) {
					CScriptTokenDataFnc &Fnc = t->getToken().Fnc();
					if((t->tk == LEX_T_GET && Fnc.parameter.size()==0) || (t->tk == LEX_T_SET && Fnc.parameter.size()==1)) {
						CScriptVarSmartLink funcVar = parseFunctionDefinition(t->getToken());
						CScriptVarLink *child = contents->findChild(Fnc.name);
						if(child && !(*child)->isAccessor()) child = 0;
						if(!child) child = contents->addChildNoDup(Fnc.name, newScriptVar(Accessor));
						child->getVarPtr()->addChildNoDup((t->tk==LEX_T_GET?TINYJS_ACCESSOR_GET_VAR:TINYJS_ACCESSOR_SET_VAR), funcVar->getVarPtr());
					}
					t->match(t->tk);
				}
				else
					t->match(LEX_ID);
				// no need to clean here, as it will definitely be used
				if (t->tk != ',') break;
				t->match(',');
			}

			t->match('}');
			return new CScriptVarLink(contents);
		} else
			t->skip(t->getToken().Int());
	} else if (t->tk=='[') {
		if(execute) {
			CScriptVarPtr contents(newScriptVar(Array));
			/* JSON-style array */
			t->match('[');
			int idx = 0;
			while (t->tk != ']') {
				CScriptVarSmartLink a = execute_assignment(execute);
				if (execute) {
					contents->addChild(int2string(idx), a->getValue());
				}
				// no need to clean here, as it will definitely be used
				if (t->tk != ']') t->match(',');
				idx++;
			}
			t->match(']');
	//		return new CScriptVarLink(contents.release());
			return new CScriptVarLink(contents);
		} else
			t->skip(t->getToken().Int());
	} else if (t->tk==LEX_R_LET) { // let as expression
		if(execute) {
			t->match(LEX_R_LET);
			t->match('(');
			CScopeControl ScopeControl(this);
			ScopeControl.addLetScope();
			for(;;) {
				CScriptVarSmartLink a;
				string var = t->tkStr();
				t->match(LEX_ID);
				a = scope()->scopeLet()->findChildOrCreate(var);
				a->SetDeletable(false);
				// sort out initialiser
				if (t->tk == '=') {
					t->match('=');
					CScriptVarSmartLink var = execute_assignment(execute);
					a << var;
				}
				if (t->tk == ',') 
					t->match(',');
				else
					break;
			}
			t->match(')');
			return new CScriptVarLink(execute_base(execute));
		} else {
			t->skip(t->getToken().Int());
			execute_base(execute);
		}
	} else if (t->tk==LEX_R_FUNCTION) {
		ASSERT(0); // no function as operator
	} else if (t->tk==LEX_T_FUNCTION_OPERATOR) {
		if(execute) {
			CScriptVarSmartLink a = parseFunctionDefinition(t->getToken());
			t->match(t->tk);
			return a;
		}
		t->match(t->tk);
	} else if (t->tk==LEX_R_NEW) {
		// new -> create a new object
		t->match(LEX_R_NEW);
		CScriptVarSmartLink parent = execute_literals(execute);
		CScriptVarSmartLink objClass = execute_member(parent, execute);
		if (execute) {
			if((*objClass)->isFunction()) {
				CScriptVarPtr obj(newScriptVar(Object));
				CScriptVarLink *prototype = (*objClass)->findChild(TINYJS_PROTOTYPE_CLASS);
				if(prototype) obj->addChildNoDup(TINYJS___PROTO___VAR, prototype);
				vector<CScriptVarPtr> arguments;
				if (t->tk == '(') {
					t->match('(');
					while(t->tk!=')') {
						CScriptVarPtr value = execute_assignment(execute);
						if (execute) {
							arguments.push_back(value);
						}
						if (t->tk!=')') { t->match(','); }
					}
					t->match(')');
				}
				if(execute) {
					CScriptVarPtr returnVar = callFunction(objClass->getVarPtr(), arguments, obj, execute);
					if(returnVar->isArray() || returnVar->isObject())
						return CScriptVarSmartLink(returnVar);
					return CScriptVarSmartLink(obj);
				}
			} else {
				throwError(execute, objClass->name + " is not a constructor");
			}
		} else {
			if (t->tk == '(') {
				t->match('(');
				while(t->tk != ')') execute_base(execute);
				t->match(')');
			}
		}
	} else if (t->tk==LEX_R_TRUE) {
		t->match(LEX_R_TRUE);
		return new CScriptVarLink(newScriptVar(true));
	} else if (t->tk==LEX_R_FALSE) {
		t->match(LEX_R_FALSE);
		return new CScriptVarLink(newScriptVar(false));
	} else if (t->tk==LEX_R_NULL) {
		t->match(LEX_R_NULL);
		return new CScriptVarLink(newScriptVar(Null));
	} else if (t->tk==LEX_R_UNDEFINED) {
		t->match(LEX_R_UNDEFINED);
		return new CScriptVarLink(newScriptVar(Undefined));
	} else if (t->tk==LEX_R_INFINITY) {
		t->match(LEX_R_INFINITY);
		return new CScriptVarLink(newScriptVar(InfinityPositive));
	} else if (t->tk==LEX_R_NAN) {
		t->match(LEX_R_NAN);
		return new CScriptVarLink(newScriptVar(NaN));
	} else if (t->tk=='(') {
		t->match('(');
		CScriptVarSmartLink a = execute_base(execute);
		t->match(')');
		return a;
	} else 
		t->match(LEX_EOF);
	return new CScriptVarLink(newScriptVar(Undefined));

}
CScriptVarSmartLink CTinyJS::execute_member(CScriptVarSmartLink &parent, bool &execute) {
	CScriptVarSmartLink a = parent;
	if(t->tk == '.' || t->tk == '[') {
//		string path = a->name;
//		CScriptVar *parent = 0;
		while(t->tk == '.' || t->tk == '[') {
		
			if(execute && ((*a)->isUndefined() || (*a)->isNull())) {
				throwError(execute, a->name + " is " + (*a)->getString());
			}
			string name;
			if(t->tk == '.') {
				t->match('.');
				name = t->tkStr();
//				path += "."+name;
				t->match(LEX_ID);
			} else {
				t->match('[');
				CScriptVarSmartLink index = execute_expression(execute);
				name = (*index)->getString();
//				path += "["+name+"]";
				t->match(']');
			}
			if (execute) {
				bool first_in_prototype = false;
				CScriptVarLink *child = (*a)->findChild(name);
				if ( !child && (child = findInPrototypeChain(a, name)) )
					first_in_prototype = true;
				if (!child) {
					/* if we haven't found this defined yet, use the built-in
						'length' properly */
					if ((*a)->isArray() && name == "length") {
						int l = (*a)->getArrayLength();
						child = new CScriptVarLink(newScriptVar(l));
					} else if ((*a)->isString() && name == "length") {
						int l = (*a)->getString().size();
						child = new CScriptVarLink(newScriptVar(l));
					} else {
						//child = (*a)->addChild(name);
					}
				}
				if(child) {
					parent = a;
					if(first_in_prototype) {
						a = new CScriptVarLink(child->getVarPtr(), child->name);
						a->owner = parent->operator->(); // fake owner - but not set Owned -> for assignment stuff
					} else
						a = child;
				} else {
					CScriptVar *owner = a->operator->();
					a = new CScriptVarLink(newScriptVar(Undefined), name);
					a->owner = owner;  // fake owner - but not set Owned -> for assignment stuff
				}
			}
		}
	}
	return a;
}

CScriptVarSmartLink CTinyJS::execute_function_call(bool &execute) {
	CScriptVarSmartLink parent = execute_literals(execute);
	CScriptVarSmartLink a = execute_member(parent, execute);
	while (t->tk == '(') {
		if (execute) {
			if (!a->getValue()->isFunction()) {
				string errorMsg = "Expecting '";
				errorMsg = errorMsg + a->name + "' to be a function";
				throw new CScriptException(errorMsg.c_str());
			}
			t->match('('); // path += '(';

			// grab in all parameters
			vector<CScriptVarPtr> arguments;
			while(t->tk!=')') {
				CScriptVarSmartLink value = execute_assignment(execute);
//				path += (*value)->getString();
				if (execute) {
					arguments.push_back(value->getValue());
				}
				if (t->tk!=')') { t->match(','); /*path+=',';*/ }
			}
			t->match(')'); //path+=')';
			// setup a return variable
			CScriptVarSmartLink returnVar;
			if(execute) {
				// if no parent use the root-scope
				CScriptVarPtr This(parent ? parent->getVarPtr() : (CScriptVarPtr )root);
				a = callFunction(a->getValue(), arguments, This, execute);
			}
		} else {
			// function, but not executing - just parse args and be done
			t->match('(');
			while (t->tk != ')') {
				CScriptVarSmartLink value = execute_base(execute);
				//	if (t->tk!=')') t->match(',');
			}
			t->match(')');
		}
		a = execute_member(a, execute);
	}
	return a;
}
// R->L: Precedence 3 (in-/decrement) ++ --
// R<-L: Precedence 4 (unary) ! ~ + - typeof void delete 
CScriptVarSmartLink CTinyJS::execute_unary(bool &execute) {
	CScriptVarSmartLink a;
	if (t->tk=='-') {
		t->match('-');
		a = execute_unary(execute);
		if (execute) {
			CheckRightHandVar(execute, a);
			CScriptVarPtr zero = newScriptVar(0);
			a = zero->mathsOp(a->getValue(), '-');
		}
	} else if (t->tk=='+') {
		t->match('+');
		a = execute_unary(execute);
		CheckRightHandVar(execute, a);
	} else if (t->tk=='!') {
		t->match('!'); // binary not
		a = execute_unary(execute);
		if (execute) {
			CheckRightHandVar(execute, a);
			CScriptVarPtr zero = newScriptVar(0);
			a = a->getValue()->mathsOp(zero, LEX_EQUAL);
		}
	} else if (t->tk=='~') {
		t->match('~'); // binary neg
		a = execute_unary(execute);
		if (execute) {
			CheckRightHandVar(execute, a);
			CScriptVarPtr zero = newScriptVar(0);
			a = a->getValue()->mathsOp(zero, '~');
		}
	} else if (t->tk==LEX_R_TYPEOF) {
		t->match(LEX_R_TYPEOF); // void
		a = execute_unary(execute);
		if (execute) {
			// !!! no right-hand-check by delete
			a = new CScriptVarLink(newScriptVar(a->getValue()->getVarType()));
		}
	} else if (t->tk==LEX_R_VOID) {
		t->match(LEX_R_VOID); // void
		a = execute_unary(execute);
		if (execute) {
			CheckRightHandVar(execute, a);
			a = new CScriptVarLink(newScriptVar(Undefined));
		}
	} else if (t->tk==LEX_R_DELETE) {
		t->match(LEX_R_DELETE); // delete
		a = execute_unary(execute);
		if (execute) {
			// !!! no right-hand-check by delete
			if(a->IsOwned() && a->IsDeletable()) {
				a->owner->removeLink(a.getLink());	// removes the link from owner
				a = new CScriptVarLink(newScriptVar(true));
			}
			else
				a = new CScriptVarLink(newScriptVar(false));
		}
	} else if (t->tk==LEX_PLUSPLUS || t->tk==LEX_MINUSMINUS) {
		int op = t->tk;
		t->match(t->tk); // pre increment/decrement
		a = execute_function_call(execute);
		if (execute) {
			if(a->IsOwned() && a->IsWritable())
			{
				CScriptVarPtr one = newScriptVar(1);
				CScriptVarPtr res = a->getValue()->mathsOp(one, op==LEX_PLUSPLUS ? '+' : '-');
				// in-place add/subtract
				a->replaceWith(res);
				a = res;
			}
		}
	} else {
		a = execute_function_call(execute);
	}
	// post increment/decrement
	if (t->tk==LEX_PLUSPLUS || t->tk==LEX_MINUSMINUS) {
		int op = t->tk;
		t->match(t->tk);
		if (execute) {
			if(a->IsOwned() && a->IsWritable())
			{
//				TRACE("post-increment of %s and a is %sthe owner\n", a->name.c_str(), a->owned?"":"not ");
				CScriptVarPtr one = newScriptVar(1);
				CScriptVarPtr res = a->getVarPtr();
				CScriptVarPtr new_a = a->getValue()->mathsOp(one, op==LEX_PLUSPLUS ? '+' : '-');
				a->replaceWith(new_a);
				a = res;
			}
		}
	}
	return a;
}

// L->R: Precedence 5 (term) * / %
CScriptVarSmartLink CTinyJS::execute_term(bool &execute) {
	CScriptVarSmartLink a = execute_unary(execute);
	if (t->tk=='*' || t->tk=='/' || t->tk=='%') {
		CheckRightHandVar(execute, a);
		while (t->tk=='*' || t->tk=='/' || t->tk=='%') {
			int op = t->tk;
			t->match(t->tk);
			CScriptVarSmartLink b = execute_unary(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				a = a->getValue()->mathsOp(b->getValue(), op);
			}
		}
	}
	return a;
}

// L->R: Precedence 6 (addition/subtraction) + -
CScriptVarSmartLink CTinyJS::execute_expression(bool &execute) {
	CScriptVarSmartLink a = execute_term(execute);
	if (t->tk=='+' || t->tk=='-') {
		CheckRightHandVar(execute, a);
		while (t->tk=='+' || t->tk=='-') {
			int op = t->tk;
			t->match(t->tk);
			CScriptVarSmartLink b = execute_term(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				 // not in-place, so just replace
				 a = a->getValue()->mathsOp(b->getValue(), op);
			}
		}
	}
	return a;
}

// L->R: Precedence 7 (bitwise shift) << >> >>>
CScriptVarSmartLink CTinyJS::execute_binary_shift(bool &execute) {
	CScriptVarSmartLink a = execute_expression(execute);
	if (t->tk==LEX_LSHIFT || t->tk==LEX_RSHIFT || t->tk==LEX_RSHIFTU) {
		CheckRightHandVar(execute, a);
		while (t->tk>=LEX_SHIFTS_BEGIN && t->tk<=LEX_SHIFTS_END) {
			int op = t->tk;
			t->match(t->tk);

			CScriptVarSmartLink b = execute_expression(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, a);
				 // not in-place, so just replace
				 a = a->getValue()->mathsOp(b->getValue(), op);
			}
		}
	}
	return a;
}
// L->R: Precedence 8 (relational) < <= > <= in (TODO: instanceof)
// L->R: Precedence 9 (equality) == != === !===
CScriptVarSmartLink CTinyJS::execute_relation(bool &execute, int set, int set_n) {
	CScriptVarSmartLink a = set_n ? execute_relation(execute, set_n, 0) : execute_binary_shift(execute);
	if ((set==LEX_EQUAL && t->tk>=LEX_RELATIONS_1_BEGIN && t->tk<=LEX_RELATIONS_1_END)
				||	(set=='<' && (t->tk==LEX_LEQUAL || t->tk==LEX_GEQUAL || t->tk=='<' || t->tk=='>' || t->tk == LEX_R_IN))) {
		CheckRightHandVar(execute, a);
		while ((set==LEX_EQUAL && t->tk>=LEX_RELATIONS_1_BEGIN && t->tk<=LEX_RELATIONS_1_END)
					||	(set=='<' && (t->tk==LEX_LEQUAL || t->tk==LEX_GEQUAL || t->tk=='<' || t->tk=='>' || t->tk == LEX_R_IN))) {
			int op = t->tk;
			t->match(t->tk);
			CScriptVarSmartLink b = set_n ? execute_relation(execute, set_n, 0) : execute_binary_shift(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				if(op == LEX_R_IN) {
					a = new CScriptVarLink(newScriptVar( findInPrototypeChain(b->getValue(), a->getValue()->getString())!= 0 ));
				} else
					a = a->getValue()->mathsOp(b->getValue(), op);
			}
		}
	}
	return a;
}

// L->R: Precedence 10 (bitwise-and) &
// L->R: Precedence 11 (bitwise-xor) ^
// L->R: Precedence 12 (bitwise-or) |
CScriptVarSmartLink CTinyJS::execute_binary_logic(bool &execute, int op, int op_n1, int op_n2) {
	CScriptVarSmartLink a = op_n1 ? execute_binary_logic(execute, op_n1, op_n2, 0) : execute_relation(execute);
	if (t->tk==op) {
		CheckRightHandVar(execute, a);
		while (t->tk==op) {
			t->match(t->tk);
			CScriptVarSmartLink b = op_n1 ? execute_binary_logic(execute, op_n1, op_n2, 0) : execute_relation(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				a = a->getValue()->mathsOp(b->getValue(), op);
			}
		}
	}
	return a;
}
// L->R: Precedence 13 ==> (logical-or) &&
// L->R: Precedence 14 ==> (logical-or) ||
CScriptVarSmartLink CTinyJS::execute_logic(bool &execute, int op, int op_n) {
	CScriptVarSmartLink a = op_n ? execute_logic(execute, op_n, 0) : execute_binary_logic(execute);
	if (t->tk==op) {
		CheckRightHandVar(execute, a);
		while (t->tk==op) {
			CheckRightHandVar(execute, a);
			int bop = t->tk;
			t->match(t->tk);
			bool shortCircuit = false;
			bool boolean = false;
			// if we have short-circuit ops, then if we know the outcome
			// we don't bother to execute the other op. Even if not
			// we need to tell mathsOp it's an & or |
			if (op==LEX_ANDAND) {
				bop = '&';
				shortCircuit = !a->getValue()->getBool();
				boolean = true;
			} else if (op==LEX_OROR) {
				bop = '|';
				shortCircuit = a->getValue()->getBool();
				boolean = true;
			}
			CScriptVarSmartLink b = op_n ? execute_logic(shortCircuit ? noexecute : execute, op_n, 0) : execute_binary_logic(shortCircuit ? noexecute : execute); // L->R
			CheckRightHandVar(execute, b);
			if (execute && !shortCircuit) {
				if (boolean) {
					CheckRightHandVar(execute, b);
					a = newScriptVar(a->getValue()->getBool());
					b = newScriptVar(b->getValue()->getBool());
				}
				a = a->getValue()->mathsOp(b->getValue(), bop);
			}
		}
	}
	return a;
}

// L<-R: Precedence 15 (condition) ?: 
CScriptVarSmartLink CTinyJS::execute_condition(bool &execute)
{
	CScriptVarSmartLink a = execute_logic(execute);
	if (t->tk=='?')
	{
		CheckRightHandVar(execute, a);
		t->match(t->tk);
		bool cond = execute && a->getValue()->getBool();
		CScriptVarSmartLink b;
		a = execute_condition(cond ? execute : noexecute ); // L<-R
//		CheckRightHandVar(execute, a);
		t->match(':');
		b = execute_condition(cond ? noexecute : execute); // R-L
//		CheckRightHandVar(execute, b);
		if(!cond)
			return b;
	}
	return a;
}
	
// L<-R: Precedence 16 (assignment) = += -= *= /= %= <<= >>= >>>= &= |= ^=
CScriptVarSmartLink CTinyJS::execute_assignment(bool &execute) {
	CScriptVarSmartLink lhs = execute_condition(execute);
	if (t->tk=='=' || (t->tk>=LEX_ASSIGNMENTS_BEGIN && t->tk<=LEX_ASSIGNMENTS_END) ) {
		int op = t->tk;
		CScriptTokenizer::ScriptTokenPosition leftHandPos = t->getPos();
		t->match(t->tk);
		CScriptVarSmartLink rhs = execute_assignment(execute); // L<-R
		if (execute) {
			if (!lhs->IsOwned() && lhs->name.length()==0) {
				throw new CScriptException("invalid assignment left-hand side", t->currentFile, leftHandPos.currentLine(), leftHandPos.currentColumn());
			} else if (op != '=' && !lhs->IsOwned()) {
				throwError(execute, lhs->name + " is not defined");
			}
			else if(lhs->IsWritable()) {
				if (op=='=') {
					if (!lhs->IsOwned()) {
						CScriptVarLink *realLhs;
						if(lhs->owner)
							realLhs = lhs->owner->addChildNoDup(lhs->name, lhs);
						else
							realLhs = root->addChildNoDup(lhs->name, lhs);
						lhs = realLhs;
					}
					lhs << rhs;
				} else if (op==LEX_PLUSEQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), '+'));
				else if (op==LEX_MINUSEQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), '-'));
				else if (op==LEX_ASTERISKEQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), '*'));
				else if (op==LEX_SLASHEQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), '/'));
				else if (op==LEX_PERCENTEQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), '%'));
				else if (op==LEX_LSHIFTEQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), LEX_LSHIFT));
				else if (op==LEX_RSHIFTEQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), LEX_RSHIFT));
				else if (op==LEX_RSHIFTUEQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), LEX_RSHIFTU));
				else if (op==LEX_ANDEQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), '&'));
				else if (op==LEX_OREQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), '|'));
				else if (op==LEX_XOREQUAL)
					lhs->replaceWith(lhs->getValue()->mathsOp(rhs->getValue(), '^'));
			}
		}
	}
	else 
		CheckRightHandVar(execute, lhs);
	return lhs;
}
// L->R: Precedence 17 (comma) ,
CScriptVarSmartLink CTinyJS::execute_base(bool &execute) {
	CScriptVarSmartLink a;
	for(;;)
	{
		a = execute_assignment(execute); // L->R
		if (t->tk == ',') {
			t->match(',');
		} else
			break;
	}
	return a;
}
void CTinyJS::execute_block(bool &execute) {
	if(t->tk == '{') {
		if(execute) {
			t->match('{');
			CScopeControl ScopeControl(this);
			ScopeControl.addLetScope();
			while (t->tk && t->tk!='}')
				execute_statement(execute);
			t->match('}');
			// scopes.pop_back();
		}
		else 
			t->skip(t->getToken().Int());
	}
}
CScriptVarSmartLink CTinyJS::execute_statement(bool &execute) {
	CScriptVarSmartLink ret;
	if (t->tk=='{') {
		/* A block of code */
		execute_block(execute);
	} else if (t->tk==';') {
		/* Empty statement - to allow things like ;;; */
		t->match(';');
	} else if (t->tk==LEX_ID) {
		ret = execute_base(execute);
		t->match(';');
	} else if (t->tk==LEX_R_VAR || t->tk==LEX_R_LET) {
		if(execute)
		{
			bool let = t->tk==LEX_R_LET, let_ext=false;
			t->match(t->tk);
			CScopeControl ScopeControl(this);
			if(let && t->tk=='(') {
				let_ext = true;
				t->match(t->tk);
				ScopeControl.addLetScope();
			}
			CScriptVarPtr in_scope = let ? scope()->scopeLet() : scope()->scopeVar();
			for(;;) {
				CScriptVarSmartLink a;
				string var = t->tkStr();
				t->match(LEX_ID);
				a = in_scope->findChildOrCreate(var);
				a->SetDeletable(false);
				// sort out initialiser
				if (t->tk == '=') {
					t->match('=');
					CScriptVarSmartLink var = execute_assignment(execute);
					a << var;
				}
				if (t->tk == ',') 
					t->match(',');
				else
					break;
			}
			if(let_ext) {
				t->match(')');
				execute_statement(execute);
			} else
				t->match(';');
		} else
			t->skip(t->getToken().Int());
	} else if (t->tk==LEX_R_WITH) {
		if(execute) {
			t->match(LEX_R_WITH);
			t->match('(');
			CScriptVarSmartLink var = execute_base(execute);
			t->match(')');
			CScopeControl ScopeControl(this);
			ScopeControl.addWithScope(var);
			ret = execute_statement(execute);
		} else
			t->skip(t->getToken().Int());
	} else if (t->tk==LEX_R_IF) {
		if(execute) {
			t->match(LEX_R_IF);
			t->match('(');
			bool cond = execute_base(execute)->getValue()->getBool();
			t->match(')');
			if(cond && execute) {
				t->match(LEX_T_SKIP);
				execute_statement(execute);
			} else {
				t->check(LEX_T_SKIP);
				t->skip(t->getToken().Int());
			}
			if (t->tk==LEX_R_ELSE) {
				if(!cond && execute) {
					t->match(LEX_R_ELSE);
					execute_statement(execute);
				}
				else
					t->skip(t->getToken().Int());
			}
		} else
			t->skip(t->getToken().Int());
	} else if (t->tk==LEX_R_DO) {
		if(execute) {
			t->match(LEX_R_DO);
			CScriptTokenizer::ScriptTokenPosition loopStart = t->getPos();
			int old_loop_runtimeFlags = runtimeFlags & RUNTIME_LOOP_MASK;
			runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;
			bool loopCond = true;
			int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
			while (loopCond && execute && loopCount-->0) {
				t->setPos(loopStart);
				execute_statement(execute);
				if(!execute)
				{
					// break or continue
					if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
					{
						execute = true;
						bool Break = (runtimeFlags & RUNTIME_BREAK) != 0;
						runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
						if(Break) {
							t->match(LEX_R_WHILE);
							t->check('(');
							t->skip(t->getToken().Int());
							t->match(';');
							break;
						}
					}
					// other stuff e.g return, throw
				}
				t->match(LEX_R_WHILE);
				if(execute) {
					t->match('(');
					loopCond = execute_base(execute)->getValue()->getBool();
					t->match(')');
				} else {
					t->check('(');
					t->skip(t->getToken().Int());
				}
				t->match(';');
			}
			runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | old_loop_runtimeFlags;
			if (loopCount<=0) {
				ostringstream errorString;
				errorString << "'for'-Loop exceeded " << TINYJS_LOOP_MAX_ITERATIONS << " iterations";
				throw new CScriptException(errorString.str(), t->currentFile, loopStart.currentLine(), loopStart.currentColumn());
			}
		} else 
			t->skip(t->getToken().Int());
	} else if (t->tk==LEX_R_WHILE) {
		if(execute) {
			t->match(LEX_R_WHILE);
			bool loopCond;
			t->match('(');
			CScriptTokenizer::ScriptTokenPosition condStart = t->getPos();
			loopCond = execute_base(execute)->getValue()->getBool();
			t->match(')');
			if(loopCond && execute) {
				t->match(LEX_T_SKIP);
				CScriptTokenizer::ScriptTokenPosition loopStart = t->getPos();
				CScriptTokenizer::ScriptTokenPosition loopEnd = loopStart;
				int old_loop_runtimeFlags = runtimeFlags & RUNTIME_LOOP_MASK;
				runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;
				int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
				while (loopCond && execute && loopCount-->0) {
					t->setPos(loopStart);
					execute_statement(execute);
					if(loopEnd == loopStart) // first loop-pass
						loopEnd = t->getPos();
					if(!execute) {
						// break or continue
						if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
						{
							execute = true;
							bool Break = (runtimeFlags & RUNTIME_BREAK) != 0;
							runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
							if(Break) break;
						}
						// other stuff e.g return, throw
					}
					if(execute) {
						t->setPos(condStart);
						loopCond = execute_base(execute)->getValue()->getBool();
					}
				}
				t->setPos(loopEnd);
				runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | old_loop_runtimeFlags;
				if (loopCount<=0) {
					ostringstream errorString;
					errorString << "'for'-Loop exceeded " << TINYJS_LOOP_MAX_ITERATIONS << " iterations";
					throw new CScriptException(errorString.str(), t->currentFile, loopStart.currentLine(), loopStart.currentColumn());
				}
			} else {
				t->check(LEX_T_SKIP);
				t->skip(t->getToken().Int());
			}
		} else
			t->skip(t->getToken().Int());
	} else if (t->tk==LEX_T_FOR_IN || t->tk == LEX_T_FOR_EACH_IN) {
		if(execute) {
			bool for_each = t->tk == LEX_T_FOR_EACH_IN;
			t->match(t->tk);
			t->match('(');
			CScriptVarSmartLink for_var;
			CScriptVarSmartLink for_in_var;

			CScopeControl ScopeControl(this);
			ScopeControl.addLetScope();

			if(t->tk == LEX_R_LET) {
				t->match(LEX_R_LET);
				string var = t->tkStr();
				t->match(LEX_ID);
				for_var = scope()->scopeLet()->findChildOrCreate(var);
			}
			else
				for_var = execute_function_call(execute);

			t->match(LEX_R_IN);

			for_in_var = execute_function_call(execute);
			CheckRightHandVar(execute, for_in_var);
			t->match(')');
			if( (*for_in_var)->Childs.size() ) {
				if(!for_var->IsOwned()) {
					CScriptVarLink *real_for_var;
					if(for_var->owner)
						real_for_var = for_var->owner->addChildNoDup(for_var->name, for_var);
					else
						real_for_var = root->addChildNoDup(for_var->name, for_var);
					for_var = real_for_var;
				}

				CScriptTokenizer::ScriptTokenPosition loopStart = t->getPos();

				int old_loop_runtimeFlags = runtimeFlags & RUNTIME_LOOP_MASK;
				runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;
				for(SCRIPTVAR_CHILDS_it it = (*for_in_var)->Childs.begin(); execute && it != (*for_in_var)->Childs.end(); ++it) {
					if (for_each)
						for_var->replaceWith(*it);
					else
						for_var->replaceWith(newScriptVar((*it)->name));
					t->setPos(loopStart);
					execute_statement(execute);
					if(!execute)
					{
						// break or continue
						if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE))
						{
							execute = true;
							bool Break = (runtimeFlags & RUNTIME_BREAK)!=0;
							runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
							if(Break) break;
						}
						// other stuff e.g return, throw
					}
				}
				runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | old_loop_runtimeFlags;
			} else {
				execute_statement(noexecute);
			}
		} else {
			t->skip(t->getToken().Int());
			execute_statement(execute);
		}
	} else if (t->tk==LEX_R_FOR) {
		if(execute)
		{
			t->match(LEX_R_FOR);
			t->match('(');
			CScopeControl ScopeControl(this);
			if(t->tk == LEX_R_LET) 
				ScopeControl.addLetScope();
			execute_statement(execute); // initialisation
			CScriptTokenizer::ScriptTokenPosition conditionStart = t->getPos();
			bool cond_empty = true;
			bool loopCond = execute;	// Empty Condition -->always true
			if(t->tk != ';') {
				cond_empty = false;
				loopCond = execute && execute_base(execute)->getValue()->getBool();
			}
			t->match(';');
			CScriptTokenizer::ScriptTokenPosition iterStart = t->getPos();
			bool iter_empty = true;
			if(t->tk != ')')
			{
				iter_empty = false;
				execute_base(noexecute); // iterator
			}
			t->match(')');
			if(loopCond) { // when execute==false then loopCond always false to
				CScriptTokenizer::ScriptTokenPosition loopStart = t->getPos();
				CScriptTokenizer::ScriptTokenPosition loopEnd = t->getPos();

				int old_loop_runtimeFlags = runtimeFlags & RUNTIME_LOOP_MASK;
				runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | RUNTIME_CANBREAK | RUNTIME_CANCONTINUE;
				int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
				while (loopCond && execute && loopCount-->0) {
					t->setPos(loopStart);
					execute_statement(execute);
					if(loopEnd == loopStart) // first loop-pass
						loopEnd = t->getPos();
					if(!execute) {
						// break or continue
						if(runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE)) {
							execute = true;
							bool Break = (runtimeFlags & RUNTIME_BREAK)!=0;
							runtimeFlags &= ~(RUNTIME_BREAK | RUNTIME_CONTINUE);
							if(Break) break;
						}
						// other stuff e.g return, throw
					} 
					if(execute) {
						if(!iter_empty) {
							t->setPos(iterStart);;
							execute_base(execute);
						}
						if(!cond_empty) {
							t->setPos(conditionStart);
							loopCond = execute_base(execute)->getValue()->getBool();
						}
					}
				}
				t->setPos(loopEnd);
				runtimeFlags = (runtimeFlags & ~RUNTIME_LOOP_MASK) | old_loop_runtimeFlags;
				if (loopCount<=0) {
					ostringstream errorString;
					errorString << "'for'-Loop exceeded " << TINYJS_LOOP_MAX_ITERATIONS << " iterations";
					throw new CScriptException(errorString.str(), t->currentFile, loopStart.currentLine(), loopStart.currentColumn());
				}
			} else {
				execute_statement(noexecute);
			}
		}  else
			t->skip(t->getToken().Int());
	} else if (t->tk==LEX_R_BREAK) {
		t->match(LEX_R_BREAK);
		if(execute) {
			if(runtimeFlags & RUNTIME_CANBREAK)
			{
				runtimeFlags |= RUNTIME_BREAK;
				execute = false;
			}
			else
				throw new CScriptException("'break' must be inside loop or switch", t->currentFile, t->currentLine(), t->currentColumn());
		}
		t->match(';');
	} else if (t->tk==LEX_R_CONTINUE) {
		t->match(LEX_R_CONTINUE);
		if(execute) {
			if(runtimeFlags & RUNTIME_CANCONTINUE)
			{
				runtimeFlags |= RUNTIME_CONTINUE;
				execute = false;
			}
			else
				throw new CScriptException("'continue' must be inside loop or switch", t->currentFile, t->currentLine(), t->currentColumn());
		}
		t->match(';');
	} else if (t->tk==LEX_R_RETURN) {
		if (execute) {
			if (IS_RUNTIME_CANRETURN) {
				t->match(LEX_R_RETURN);
				CScriptVarPtr result;
				if (t->tk != ';')
					result = execute_base(execute)->getValue();
				if(result) scope()->scopeVar()->setReturnVar(result);
			} else 
				throw new CScriptException("'return' statement, but not in a function.", t->currentFile, t->currentLine(), t->currentColumn());
			execute = false;
		}
		else {
			t->match(LEX_R_RETURN);
			if (t->tk != ';')
				execute_base(execute);
		}
		t->match(';');
	} else if (t->tk == LEX_T_FUNCTION_OPERATOR) {
		// ignore force anonymous at statement-level
		ASSERT(0); // no functions-operator statement-level
		t->match(t->tk);
	} else if (t->tk == LEX_R_FUNCTION) {
		if(execute) {
			CScriptTokenDataFnc &Fnc = t->getToken().Fnc();
			if(!Fnc.name.length())
				throw new CScriptException("Functions defined at statement-level are meant to have a name.");
			else {
				CScriptVarSmartLink funcVar = parseFunctionDefinition(t->getToken());
				scope()->scopeVar()->addChildNoDup(funcVar->name, funcVar)->SetDeletable(false);
			}
		}
		t->match(t->tk);
	} else if (t->tk==LEX_R_TRY) {
		t->match(LEX_R_TRY);
		bool old_execute = execute;
		// save runtimeFlags
		int old_throw_runtimeFlags = runtimeFlags & RUNTIME_THROW_MASK;
		// set runtimeFlags
		runtimeFlags = (runtimeFlags & ~RUNTIME_THROW_MASK) | RUNTIME_CANTHROW;

		execute_block(execute);
//try{print("try1");try{ print("try2");try{print("try3");throw 3;}catch(a){print("catch3");throw 3;}finally{print("finalli3");}print("after3");}catch(a){print("catch2");}finally{print("finalli2");}print("after2");}catch(a){print("catch1");}finally{print("finalli1");}print("after1");		
		bool isThrow = (runtimeFlags & RUNTIME_THROW) != 0;
		if(isThrow) execute = old_execute;

		// restore runtimeFlags
		runtimeFlags = (runtimeFlags & ~RUNTIME_THROW_MASK) | old_throw_runtimeFlags;

		if(t->tk != LEX_R_FINALLY) // expect catch
		{
			t->match(LEX_R_CATCH);
			t->match('(');
			string exception_var_name = t->tkStr();
			t->match(LEX_ID);
			t->match(')');
			if(isThrow) {
				CScriptVarScopeFncPtr catchScope(::newScriptVar(this, ScopeFnc, (CScriptVar*)0));
				catchScope->addChild(exception_var_name, exceptionVar);
				CScopeControl ScopeControl(this);
				ScopeControl.addFncScope(catchScope);
				execute_block(execute);
			} else {
				execute_block(noexecute);
			}
		}
		if(t->tk == LEX_R_FINALLY) {
			t->match(LEX_R_FINALLY);
			bool finally_execute = true;
			execute_block(isThrow ? finally_execute : execute);
		}
	} else if (t->tk==LEX_R_THROW) {
		CScriptTokenizer::ScriptTokenPosition tokenPos = t->getPos();
//		int tokenStart = t->getToken().pos;
		t->match(LEX_R_THROW);
		CScriptVarPtr a = execute_base(execute)->getValue();
		if(execute) {
			if(runtimeFlags & RUNTIME_CANTHROW) {
				runtimeFlags |= RUNTIME_THROW;
				execute = false;
				exceptionVar = a;
			}
			else
				throw new CScriptException("uncaught exception: '"+a->getString()+"'", t->currentFile, tokenPos.currentLine(), tokenPos.currentColumn());
		}
	} else if (t->tk == LEX_R_SWITCH) {
		if(execute) {
			t->match(LEX_R_SWITCH);
			t->match('(');
			CScriptVarPtr SwitchValue = execute_base(execute)->getValue();
			t->match(')');
			if(execute) {
				// save runtimeFlags
				int old_switch_runtimeFlags = runtimeFlags & RUNTIME_BREAK_MASK;
				// set runtimeFlags
				runtimeFlags = (runtimeFlags & ~RUNTIME_BREAK_MASK) | RUNTIME_CANBREAK;
				bool found = false ,execute = false;
				t->match('{');
				CScopeControl ScopeControl(this);
				ScopeControl.addLetScope();
				if(t->tk == LEX_R_CASE || t->tk == LEX_R_DEFAULT || t->tk == '}') {
					CScriptTokenizer::ScriptTokenPosition defaultStart = t->getPos();
					bool hasDefault = false;
					while (t->tk) {
						if(t->tk == LEX_R_CASE) {
							if(found) {
								t->skip(t->getToken().Int());
								if(execute)  t->match(':');
								else t->skip(t->getToken().Int());
							} else {
								t->match(LEX_R_CASE);
								execute = true;
								CScriptVarSmartLink CaseValue = execute_base(execute);
								if(execute) {
									CaseValue = CaseValue->getValue()->mathsOp(SwitchValue, LEX_EQUAL);
									found = execute = (*CaseValue)->getBool();
									if(found) t->match(':');
									else t->skip(t->getToken().Int());
								} else {
									found = true;
									t->skip(t->getToken().Int());
								}
							}
						} else if(t->tk == LEX_R_DEFAULT) {
							t->match(LEX_R_DEFAULT);
							if(found) {
								if(execute)  t->match(':');
								else t->skip(t->getToken().Int());
							} else {
								hasDefault = true;
								defaultStart = t->getPos();
								t->skip(t->getToken().Int());
							}
						} else if(t->tk == '}') {
							if(!found && hasDefault) {
								found = execute = true;
								t->setPos(defaultStart);
								t->match(':');
							} else
								break;
						} else
							execute_statement(execute);
					}
					t->match('}');
					if(!found || (runtimeFlags & RUNTIME_BREAK) )
						execute = true;
					// restore runtimeFlags
					runtimeFlags = (runtimeFlags & ~RUNTIME_BREAK_MASK) | old_switch_runtimeFlags;
				} else
					throw new CScriptException("invalid switch statement");
			} else
				execute_block(execute);
		} else
			t->skip(t->getToken().Int());
	} else if(t->tk == LEX_T_LABEL) {
		t->match(LEX_T_LABEL); // ignore Labels
		t->match(':');
	} else if(t->tk != LEX_EOF) {
		/* Execute a simple statement that only contains basic arithmetic... */
		ret = execute_base(execute);
		t->match(';');
	} else
		t->match(LEX_EOF);
	return ret;
}


/// Finds a child, looking recursively up the scopes
CScriptVarLink *CTinyJS::findInScopes(const string &childName) {
	return scope()->findInScopes(childName);
/*
	CScriptVarLink *v = scopes.back()->findChild(childName);
	if(!v && scopes.front() != scopes.back())
		v = scopes.front()->findChild(childName);
	return v;
*/
}

/// Get all Keynames of en given object (optionial look up the prototype-chain)
void CTinyJS::keys(STRING_VECTOR_t &Keys, CScriptVarPtr object, bool WithPrototypeChain) {
	CScriptVarLink *__proto__;
	for(SCRIPTVAR_CHILDS_it it = object->Childs.begin(); it != object->Childs.end(); ++it) {
		if((*it)->IsEnumerable())
			Keys.push_back((*it)->name);

		if (WithPrototypeChain) {
			if( (__proto__ = (*(*it))->findChild(TINYJS___PROTO___VAR)) ) 
				keys(Keys, __proto__, WithPrototypeChain);
			else if((*(*it))->isString())
				keys(Keys, stringPrototype, WithPrototypeChain);
			else if((*(*it))->isArray())
				keys(Keys, arrayPrototype, WithPrototypeChain);
			else if((*(*it))->isFunction())
				keys(Keys, functionPrototype, WithPrototypeChain);
			else
				keys(Keys, objectPrototype, false);
		}
	}
}
/// Look up in any parent classes of the given object
CScriptVarLink *CTinyJS::findInPrototypeChain(CScriptVarPtr object, const string &name) {
	// Look for links to actual parent classes
	CScriptVarLink *__proto__;
//	CScriptVar *_object = object;
	while( (__proto__ = object->findChild(TINYJS___PROTO___VAR)) ) {
			CScriptVarLink *implementation = (*__proto__)->findChild(name);
			if (implementation){
				return implementation;
			}
			object = __proto__->getVarPtr();
	}
/*
	if (object->isString()) {
		CScriptVarLink *implementation = stringPrototype->findChild(name);
		if (implementation) return implementation;
	}
	if (object->isArray()) {
		CScriptVarLink *implementation = arrayPrototype->findChild(name);
		if (implementation) return implementation;
	}
	if (object->isFunction()) {
		CScriptVarLink *implementation = functionPrototype->findChild(name);
		if (implementation) return implementation;
	}
	CScriptVarLink *implementation = objectPrototype->findChild(name);
	if (implementation) return implementation;
*/
	return 0;
}
void CTinyJS::native_Object(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr objc = c->getParameter(0);
	if(objc->isUndefined() || objc->isNull())
		c->setReturnVar(newScriptVar(Object));
	else
		c->setReturnVar(objc);
}
void CTinyJS::native_Array(const CFunctionsScopePtr &c, void *data) {
//	CScriptVar *returnVar = new CScriptVarArray(c->getContext());
	CScriptVarPtr returnVar = c->newScriptVar(Array);
	c->setReturnVar(returnVar);
	int length = c->getParameterLength();
	if(length == 1 && c->getParameter(0)->isNumber())
		returnVar->setArrayIndex(c->getParameter(0)->getInt(), newScriptVar(Undefined));
	else for(int i=0; i<length; i++)
		returnVar->setArrayIndex(i, c->getParameter(i));
}
void CTinyJS::native_String(const CFunctionsScopePtr &c, void *data) {
	if(c->getParameterLength()==0)
		c->setReturnVar(c->newScriptVar(""));
	else
		c->setReturnVar(c->newScriptVar(c->getParameter(0)->getString()));
}
void CTinyJS::native_Eval(const CFunctionsScopePtr &c, void *data) {
	string Code = c->getParameter("jsCode")->getString();
	CScriptVarScopePtr scEvalScope = scope(); // save scope
	_scopes.pop_back(); // go back to the callers scope
	try {
		CScriptVarSmartLink returnVar;
		CScriptTokenizer *oldTokenizer = t; t=0;
		t = new CScriptTokenizer(Code.c_str(), "eval");
		try {
			bool execute = true;
			do {
				returnVar = execute_statement(execute);
				while (t->tk==';') t->match(';'); // skip empty statements
			} while (t->tk!=LEX_EOF);
		} catch (CScriptException *e) {
			delete t;
			t = oldTokenizer;
			throw e;
		}
		delete t;
		t = oldTokenizer;

		_scopes.push_back(scEvalScope); // restore Scopes;
		if(returnVar)
			c->setReturnVar(returnVar->getVarPtr());

		// check of exceptions
		int exceptionState = runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE);
		runtimeFlags &= ~RUNTIME_LOOP_MASK;
		if(exceptionState) throw new CScriptEvalException(exceptionState);

	} catch (CScriptException *e) {
		_scopes.push_back(scEvalScope); // restore Scopes;
		throw e;
	}
}

void CTinyJS::native_JSON_parse(const CFunctionsScopePtr &c, void *data) {
	string Code = "§" + c->getParameter("text")->getString();
	
	CScriptVarSmartLink returnVar;
	CScriptTokenizer *oldTokenizer = t; t=0;
	try {
		CScriptTokenizer Tokenizer(Code.c_str(), "JSON.parse", 0, -1);
		t = &Tokenizer;
		bool execute = true;
		returnVar = execute_literals(execute);
		t->match(LEX_EOF);
	} catch (CScriptException *e) {
		t = oldTokenizer;
		throw e;
	}
	t = oldTokenizer;

	if(returnVar)
		c->setReturnVar(returnVar);

// check of exceptions
//		int exceptionState = runtimeFlags & (RUNTIME_BREAK | RUNTIME_CONTINUE);
//		runtimeFlags &= ~RUNTIME_LOOP_MASK;
//		if(exceptionState) throw new CScriptEvalException(exceptionState);
}
void CTinyJS::native_Object_hasOwnProperty(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(c->newScriptVar(c->getParameter("this")->findChild(c->getParameter("prop")->getString()) != 0));
}

void CTinyJS::native_Function(const CFunctionsScopePtr &c, void *data) {
	int length = c->getParameterLength();
	string params, body;
	if(length>=1) 
		body = c->getParameter(length-1)->getString();
	if(length>=2) {
		params = c->getParameter(0)->getString();
		for(int i=1; i<length-1; i++)
		{
			params.append(",");
			params.append(c->getParameter(i)->getString());
		}
	}
	c->setReturnVar(parseFunctionsBodyFromString(params,body));
}

void CTinyJS::native_Function_call(const CFunctionsScopePtr &c, void *data) {
	int length = c->getParameterLength();
	CScriptVarPtr Fnc = c->getParameter("this");
	CScriptVarPtr This = c->getParameter(0);
	vector<CScriptVarPtr> Params;
	for(int i=1; i<length; i++)
		Params.push_back(c->getParameter(i));
	bool execute = true;
	callFunction(Fnc, Params, This, execute);
}
void CTinyJS::native_Function_apply(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr Fnc = c->getParameter("this");
	CScriptVarPtr This = c->getParameter(0);
	CScriptVarPtr Array = c->getParameter(1);
	CScriptVarLink *Length = Array->findChild("length");
	int length = Length ? Length->getValue()->getInt() : 0;
	vector<CScriptVarPtr> Params;
	for(int i=0; i<length; i++) {
		CScriptVarLink *value = Array->findChild(int2string(i));
		if(value) Params.push_back(value->getValue());
		else Params.push_back(newScriptVar(Undefined));
	}
	bool execute = true;
	callFunction(Fnc, Params, This, execute);
}

void CTinyJS::ClearLostVars(const CScriptVarPtr &extra/*=CScriptVarPtr()*/) {
	int UniqueID = getUniqueID(); 
	root->setTempraryIDrecursive(UniqueID);
	if(extra) extra->setTempraryIDrecursive(UniqueID);
	CScriptVar *p = first;
	while(p)
	{
		if(p->temporaryID != UniqueID)
		{
			CScriptVar *var = p;
			var->ref();
			var->removeAllChildren();
			p = var->next;
			var->unref(0);
		}
		else
			p = p->next;
	}
}
