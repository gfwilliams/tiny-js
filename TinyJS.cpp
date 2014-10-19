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

#ifdef _DEBUG
#	ifndef _MSC_VER
#		define DEBUG_MEMORY 1
#	endif
#endif
#include <errno.h>
#include <sstream>
#include <fstream>

#include "TinyJS.h"

#ifndef ASSERT
#	define ASSERT(X) assert(X)
#endif

#ifndef NO_REGEXP 
#	if defined HAVE_TR1_REGEX
#		include <tr1/regex>
		using namespace std::tr1;
#	elif defined HAVE_BOOST_REGEX
#		include <boost/regex.hpp>
		using namespace boost;
#	else
#		include <regex>
#	endif
#else
#	include <algorithm>
#	include <cmath>
#	include <memory>
#endif

using namespace std;

// ----------------------------------------------------------------------------------- 
//////////////////////////////////////////////////////////////////////////
/// Memory Debug
//////////////////////////////////////////////////////////////////////////

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
		printf("ALLOCATED LINK %s, allocated[%d] to \n", allocatedLinks[i]->getName().c_str(), allocatedLinks[i]->getVarPtr()->getRefs());
		allocatedLinks[i]->getVarPtr()->trace("  ");
	}
	allocatedVars.clear();
	allocatedLinks.clear();
}
#endif


//////////////////////////////////////////////////////////////////////////
/// Utils
//////////////////////////////////////////////////////////////////////////

inline bool isWhitespace(char ch) {
	return (ch==' ') || (ch=='\t') || (ch=='\n') || (ch=='\r');
}

inline bool isNumeric(char ch) {
	return (ch>='0') && (ch<='9');
}
uint32_t isArrayIndex(const string &str) {
	if(str.size()==0 || !isNumeric(str[0]) || (str.size()>1 && str[0]=='0') ) return -1; // empty or (more as 1 digit and beginning with '0')
	CNumber idx;
	const char *endptr;
	idx.parseInt(str.c_str(), 10, &endptr);
	if(*endptr || idx>uint32_t(0xFFFFFFFFUL)) return -1;
	return idx.toUInt32();
}
inline bool isHexadecimal(char ch) {
	return ((ch>='0') && (ch<='9')) || ((ch>='a') && (ch<='f')) || ((ch>='A') && (ch<='F'));
}
inline bool isOctal(char ch) {
	return ((ch>='0') && (ch<='7'));
}
inline bool isAlpha(char ch) {
	return ((ch>='a') && (ch<='z')) || ((ch>='A') && (ch<='Z')) || ch=='_' || ch=='$';
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
string int2string(int32_t intData) {
	ostringstream str;
	str << intData;
	return str.str();
}
string int2string(uint32_t intData) {
	ostringstream str;
	str << intData;
	return str.str();
}
string float2string(const double &floatData) {
	ostringstream str;
	str.unsetf(ios::floatfield);
#if (defined(_MSC_VER) && _MSC_VER >= 1600) || __cplusplus >= 201103L
	str.precision(numeric_limits<double>::max_digits10);
#else
	str.precision(numeric_limits<double>::digits10+2);
#endif
	str << floatData;
	return str.str();
}
/// convert the given string into a quoted string suitable for javascript
string getJSString(const string &str) {
	char buffer[5] = "\\x00";
	string nStr; nStr.reserve(str.length());
	nStr.push_back('\"');
	for (string::const_iterator i=str.begin();i!=str.end();i++) {
		const char *replaceWith = 0;
		switch (*i) {
			case '\\': replaceWith = "\\\\"; break;
			case '\n': replaceWith = "\\n"; break;
			case '\r': replaceWith = "\\r"; break;
			case '\a': replaceWith = "\\a"; break;
			case '\b': replaceWith = "\\b"; break;
			case '\f': replaceWith = "\\f"; break;
			case '\t': replaceWith = "\\t"; break;
			case '\v': replaceWith = "\\v"; break;
			case '"': replaceWith = "\\\""; break;
			default: {
					int nCh = ((unsigned char)*i) & 0xff;
					if(nCh<32 || nCh>127) {
						static char hex[] = "0123456789ABCDEF";
						buffer[2] = hex[(nCh>>4)&0x0f];
						buffer[3] = hex[nCh&0x0f];
						replaceWith = buffer;
					};
				}
		}
		if (replaceWith)
			nStr.append(replaceWith);
		else
			nStr.push_back(*i);
	}
	nStr.push_back('\"');
	return nStr;
}

static inline string getIDString(const string& str) {
	if(isIDString(str.c_str()) && CScriptToken::isReservedWord(str)==LEX_ID)
		return str;
	return getJSString(str);
}


//////////////////////////////////////////////////////////////////////////
/// CScriptException
//////////////////////////////////////////////////////////////////////////

string CScriptException::toString() {
	ostringstream msg;
	msg << ERROR_NAME[errorType] << ": " << message;
	if(lineNumber >= 0) msg << " at Line:" << lineNumber+1;
	if(column >=0) msg << " Column:" << column+1;
	if(fileName.length()) msg << " in " << fileName;
	return msg.str();
}


//////////////////////////////////////////////////////////////////////////
/// CScriptLex
//////////////////////////////////////////////////////////////////////////

CScriptLex::CScriptLex(const char *Code, const string &File, int Line, int Column) : data(Code) {
	currentFile = File;
	pos.currentLineStart = pos.tokenStart = data;
	pos.currentLine = Line;
	reset(pos);
}

void CScriptLex::reset(const POS &toPos) { ///< Reset this lex so we can start again
	dataPos = toPos.tokenStart;
	tk = last_tk = 0;
	tkStr = "";
	pos = toPos;
	lineBreakBeforeToken = false;
	currCh = nextCh = 0;
	getNextCh(); // currCh
	getNextCh(); // nextCh
	getNextToken();
}

void CScriptLex::check(int expected_tk, int alternate_tk/*=-1*/) {
	if (expected_tk==';' && tk==LEX_EOF) return; // ignore last missing ';'
	if (tk!=expected_tk && tk!=alternate_tk) {
		ostringstream errorString;
		if(expected_tk == LEX_EOF)
			errorString << "Got unexpected " << CScriptToken::getTokenStr(tk);
		else
			errorString << "Got '" << CScriptToken::getTokenStr(tk) << "' expected '" << CScriptToken::getTokenStr(expected_tk) << "'";
		if(alternate_tk!=-1) errorString << " or '" << CScriptToken::getTokenStr(alternate_tk) << "'";
		throw new CScriptException(SyntaxError, errorString.str(), currentFile, pos.currentLine, currentColumn());
	}
}
void CScriptLex::match(int expected_tk1, int alternate_tk/*=-1*/) {
	check(expected_tk1, alternate_tk);
	int line = pos.currentLine;
	getNextToken();
	lineBreakBeforeToken = line != pos.currentLine;
}

void CScriptLex::getNextCh() {
	if(currCh == '\n') { // Windows or Linux
		pos.currentLine++;
		pos.tokenStart = pos.currentLineStart = dataPos - (nextCh == LEX_EOF ?  0 : 1);
	}
	currCh = nextCh;
	if ( (nextCh = *dataPos) != LEX_EOF ) dataPos++; // stay on EOF
//	if(nextCh == -124) nextCh = '\n'; // 
	if(currCh == '\r') { // Windows or Mac
		if(nextCh == '\n')
			getNextCh(); // Windows '\r\n\' --> skip '\r'
		else
			currCh = '\n'; // Mac (only '\r') --> convert '\r' to '\n'
	}
}

static uint16_t not_allowed_tokens_befor_regexp[] = {LEX_ID, LEX_INT, LEX_FLOAT, LEX_STR, LEX_R_TRUE, LEX_R_FALSE, LEX_R_NULL, ']', ')', '.', LEX_PLUSPLUS, LEX_MINUSMINUS, LEX_EOF};
void CScriptLex::getNextToken() {
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
	last_tk = tk;
	tk = LEX_EOF;
	tkStr.clear();
	// record beginning of this token
	pos.tokenStart = dataPos - (nextCh == LEX_EOF ? (currCh == LEX_EOF ? 0 : 1) : 2);
	// tokens
	if (isAlpha(currCh)) { //  IDs
		while (isAlpha(currCh) || isNumeric(currCh)) {
			tkStr += currCh;
			getNextCh();
		}
		tk = CScriptToken::isReservedWord(tkStr);
#ifdef NO_GENERATORS
		if(tk == LEX_R_YIELD)
			throw new CScriptException(Error, "42TinyJS was built without support of generators (yield expression)", currentFile, pos.currentLine, currentColumn());
#endif

	} else if (isNumeric(currCh) || (currCh=='.' && isNumeric(nextCh))) { // Numbers
		if(currCh=='.') tkStr+='0';
		bool isHex = false, isOct=false;
		if (currCh=='0') { 
			tkStr += currCh; getNextCh();
			if(isOctal(currCh)) isOct = true;
		}
		if (currCh=='x' || currCh=='X') {
			isHex = true;
			tkStr += currCh; getNextCh();
		}
		tk = LEX_INT;
		while (isOctal(currCh) || (!isOct && isNumeric(currCh)) || (isHex && isHexadecimal(currCh))) {
			tkStr += currCh;
			getNextCh();
		}
		if (!isHex && !isOct && currCh=='.') {
			tk = LEX_FLOAT;
			tkStr += '.';
			getNextCh();
			while (isNumeric(currCh)) {
				tkStr += currCh;
				getNextCh();
			}
		}
		// do fancy e-style floating point
		if (!isHex && !isOct && (currCh=='e' || currCh=='E')) {
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
						if(isHexadecimal(currCh)) {
							char buf[3]="\0\0";
							buf[0] = currCh;
							for(int i=0; i<2 && isHexadecimal(nextCh); i++) {
								getNextCh(); buf[i] = currCh;
							}
							tkStr += (char)strtol(buf, 0, 16);
						} else
							throw new CScriptException(SyntaxError, "malformed hexadezimal character escape sequence", currentFile, pos.currentLine, currentColumn());	
					}
					default: {
						if(isOctal(currCh)) {
							char buf[4]="\0\0\0";
							buf[0] = currCh;
							for(int i=1; i<3 && isOctal(nextCh); i++) {
								getNextCh(); buf[i] = currCh;
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
			throw new CScriptException(SyntaxError, "unterminated string literal", currentFile, pos.currentLine, currentColumn());
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
		} else if (tk=='/') {
			// check if it's a RegExp-Literal
			tk = LEX_REGEXP;
			for(uint16_t *p = not_allowed_tokens_befor_regexp; *p; p++) {
				if(*p==last_tk) { tk = '/'; break; }
			}
			if(tk == LEX_REGEXP) {
#ifdef NO_REGEXP
				throw new CScriptException(Error, "42TinyJS was built without support of regular expressions", currentFile, pos.currentLine, currentColumn());
#endif
				tkStr = "/";
				while (currCh && currCh!='/' && currCh!='\n') {
					if (currCh == '\\' && nextCh == '/') {
						tkStr.append(1, currCh);
						getNextCh();
					}
					tkStr.append(1, currCh);
					getNextCh();
				}
				if(currCh == '/') {
#ifndef NO_REGEXP
					try { regex(tkStr.substr(1), regex_constants::ECMAScript); } catch(regex_error e) {
						throw new CScriptException(SyntaxError, string(e.what())+" - "+CScriptVarRegExp::ErrorStr(e.code()), currentFile, pos.currentLine, currentColumn());
					}
#endif /* NO_REGEXP */
					do {
						tkStr.append(1, currCh);
						getNextCh();
					} while (currCh=='g' || currCh=='i' || currCh=='m' || currCh=='y');
				} else
					throw new CScriptException(SyntaxError, "unterminated regular expression literal", currentFile, pos.currentLine, currentColumn());
			} else if(currCh=='=') {
				tk = LEX_SLASHEQUAL;
				getNextCh();
			}
		} else if (tk=='%' && currCh=='=') {
			tk = LEX_PERCENTEQUAL;
			getNextCh();
		}
	}
	/* This isn't quite right yet */
}


//////////////////////////////////////////////////////////////////////////
// CScriptTokenDataForwards
//////////////////////////////////////////////////////////////////////////

bool CScriptTokenDataForwards::compare_fnc_token_by_name::operator()(const CScriptToken& lhs, const CScriptToken& rhs) const {
	return lhs.Fnc().name < rhs.Fnc().name;
}
bool CScriptTokenDataForwards::checkRedefinition(const string &Str, bool checkVarsInLetScope) {
	STRING_SET_it it = varNames[LETS].find(Str);
	if(it!=varNames[LETS].end()) return false;
	else if(checkVarsInLetScope) {
		STRING_SET_it it = vars_in_letscope.find(Str);
		if(it!=vars_in_letscope.end()) return false;
	}
	return true;
}

void CScriptTokenDataForwards::addVars( STRING_VECTOR_t &Vars ) {
	varNames[VARS].insert(Vars.begin(), Vars.end());
}
void CScriptTokenDataForwards::addConsts( STRING_VECTOR_t &Vars ) {
	varNames[CONSTS].insert(Vars.begin(), Vars.end());
}
std::string CScriptTokenDataForwards::addVarsInLetscope( STRING_VECTOR_t &Vars )
{
	for(STRING_VECTOR_it it=Vars.begin(); it!=Vars.end(); ++it) {
		if(!checkRedefinition(*it, false)) return *it;
		vars_in_letscope.insert(*it);
	}
	return "";
}

std::string CScriptTokenDataForwards::addLets( STRING_VECTOR_t &Lets )
{
	for(STRING_VECTOR_it it=Lets.begin(); it!=Lets.end(); ++it) {
		if(!checkRedefinition(*it, true)) return *it;
		varNames[LETS].insert(*it);
	}
	return "";
}


//////////////////////////////////////////////////////////////////////////
// CScriptTokenDataLoop
//////////////////////////////////////////////////////////////////////////

std::string CScriptTokenDataLoop::getParsableString(const string &IndentString/*=""*/, const string &Indent/*=""*/ ) {
	static const char *heads[] = {"for each(", "for(", "for(", "for(", "while(", "do "};
	static const char *ops[] = {" in ", " in ", " of ", "; "};
	string out = heads[type];
	if(init.size() && type==FOR)out.append(CScriptToken::getParsableString(init));
	if(type<=WHILE) out.append(CScriptToken::getParsableString(condition.begin(), condition.end()-(type>=FOR ? 0 : 7)));
	if(type<=FOR) out.append(ops[type]);
	if(iter.size()) out.append(CScriptToken::getParsableString(iter));
	out.append(")");
	out.append(CScriptToken::getParsableString(body));
	if(type==DO)out.append(" while(").append(CScriptToken::getParsableString(condition)).append(");");
	return out;
}


//////////////////////////////////////////////////////////////////////////
// CScriptTokenDataTry
//////////////////////////////////////////////////////////////////////////

std::string CScriptTokenDataTry::getParsableString( const string &IndentString/*=""*/, const string &Indent/*=""*/ ) {
	string out = "try ";
	string nl = Indent.size() ? "\n"+IndentString : " ";

	out.append(CScriptToken::getParsableString(tryBlock, IndentString, Indent));
	for(CScriptTokenDataTry::CatchBlock_it catchBlock = catchBlocks.begin(); catchBlock!=catchBlocks.end(); catchBlock++) {
		out.append(nl).append("catch(").append(catchBlock->indentifiers->getParsableString());
		if(catchBlock->condition.size()>1) {
			out.append(" if ").append(CScriptToken::getParsableString(catchBlock->condition.begin()+1, catchBlock->condition.end()));
		}
		out.append(") ").append(CScriptToken::getParsableString(catchBlock->block, IndentString, Indent));
	}
	if(finallyBlock.size())
		out.append(nl).append("finally ").append(CScriptToken::getParsableString(finallyBlock, IndentString, Indent));
	return out;
}


//////////////////////////////////////////////////////////////////////////
// CScriptTokenDataFnc
//////////////////////////////////////////////////////////////////////////

string CScriptTokenDataFnc::getArgumentsString()
{
	ostringstream destination;
	destination << "(";
	if(arguments.size()) {
		const char *comma = "";
		for(TOKEN_VECT_it argument=arguments.begin(); argument!=arguments.end(); ++argument, comma=", ") {
			if(argument->token == LEX_ID)
				destination << comma << argument->String();
			else {
				vector<bool> isObject(1, false);
				for(DESTRUCTURING_VARS_it it=argument->DestructuringVar().vars.begin(); it!=argument->DestructuringVar().vars.end(); ++it) {
					if(it->second == "}" || it->second == "]") {
						destination << it->second;
						isObject.pop_back();
					} else {
						destination << comma;
						if(it->second == "[" || it->second == "{") {
							comma = "";
							if(isObject.back() && it->first.length())
								destination << getIDString(it->first) << ":";
							destination << it->second;
							isObject.push_back(it->second == "{");
						} else {
							comma = ", ";
							if(it->second.empty())
								continue; // skip empty entries
							if(isObject.back() && it->first!=it->second)
								destination << getIDString(it->first) << ":";
							destination << it->second;
						}
					}
				}
			}
		}
	}
	destination << ") ";
	return destination.str();
}


//////////////////////////////////////////////////////////////////////////
// CScriptTokenDataDestructuringVar
//////////////////////////////////////////////////////////////////////////

void CScriptTokenDataDestructuringVar::getVarNames(STRING_VECTOR_t Names) {
	for(DESTRUCTURING_VARS_it it = vars.begin(); it != vars.end(); ++it) {
		if(it->second.size() && it->second.find_first_of("{[]}") == string::npos)
			Names.push_back(it->second);
	}
}

std::string CScriptTokenDataDestructuringVar::getParsableString()
{
	string out;
	const char *comma = "";
	vector<bool> isObject(1, false);
	for(DESTRUCTURING_VARS_it it=vars.begin(); it!=vars.end(); ++it) {
		if(it->second == "}" || it->second == "]") {
			out.append(it->second);
			isObject.pop_back();
		} else {
			out.append(comma);
			if(it->second == "[" || it->second == "{") {
				comma = "";
				if(isObject.back() && it->first.length())
					out.append(getIDString(it->first)).append(":");
				out.append(it->second);
				isObject.push_back(it->second == "{");
			} else {
				comma = ", ";
				if(it->second.empty())
					continue; // skip empty entries
				if(isObject.back() && it->first!=it->second)
					out.append(getIDString(it->first)).append(":");
				out.append(it->second);
			}
		}
	}
	return out;
}


//////////////////////////////////////////////////////////////////////////
// CScriptTokenDataObjectLiteral
//////////////////////////////////////////////////////////////////////////

void CScriptTokenDataObjectLiteral::setMode(bool Destructuring) {
	structuring = !(destructuring = Destructuring);
	for(vector<ELEMENT>::iterator it=elements.begin(); it!=elements.end(); ++it) {
		if(it->value.size() && it->value.front().token == LEX_T_OBJECT_LITERAL) { 
			CScriptTokenDataObjectLiteral& e = it->value.front().Object();
			if(e.destructuring && e.structuring)
				e.setMode(Destructuring);
		}
	}
}

string CScriptTokenDataObjectLiteral::getParsableString()
{
	string out = type == OBJECT ? "{ " : "[ ";
	const char *comma = "";
	for(vector<ELEMENT>::iterator it=elements.begin(); it!=elements.end(); ++it) {
		out.append(comma); comma=", ";
		if(it->value.empty()) continue;
		if(type == OBJECT)
			out.append(getIDString(it->id)).append(" : ");
		out.append(CScriptToken::getParsableString(it->value));
	}
	out.append(type == OBJECT ? " }" : " ]");
	return out;
}


//////////////////////////////////////////////////////////////////////////
// CScriptToken
//////////////////////////////////////////////////////////////////////////

typedef struct { int id; const char *str; bool need_space; } token2str_t;
static token2str_t reserved_words_begin[] ={
	// reserved words
	{ LEX_R_IF,						"if",							true  },
	{ LEX_R_ELSE,					"else",						true  },
	{ LEX_R_DO,						"do",							true  },
	{ LEX_R_WHILE,					"while",						true  },
	{ LEX_R_FOR,					"for",						true  },
	{ LEX_R_IN,						"in",							true  },
	{ LEX_R_BREAK,					"break",						true  },
	{ LEX_R_CONTINUE,				"continue",					true  },
	{ LEX_R_FUNCTION,				"function",					true  },
	{ LEX_R_RETURN,				"return",					true  },
	{ LEX_R_VAR,					"var",						true  },
	{ LEX_R_LET,					"let",						true  },
	{ LEX_R_CONST,					"const",						true  },
	{ LEX_R_WITH,					"with",						true  },
	{ LEX_R_TRUE,					"true",						true  },
	{ LEX_R_FALSE,					"false",						true  },
	{ LEX_R_NULL,					"null",						true  },
	{ LEX_R_NEW,					"new",						true  },
	{ LEX_R_TRY,					"try",						true  },
	{ LEX_R_CATCH,					"catch",						true  },
	{ LEX_R_FINALLY,				"finally",					true  },
	{ LEX_R_THROW,					"throw",						true  },
	{ LEX_R_TYPEOF,				"typeof",					true  },
	{ LEX_R_VOID,					"void",						true  },
	{ LEX_R_DELETE,				"delete",					true  },
	{ LEX_R_INSTANCEOF,			"instanceof",				true  },
	{ LEX_R_SWITCH,				"switch",					true  },
	{ LEX_R_CASE,					"case",						true  },
	{ LEX_R_DEFAULT,				"default",					true  },
	{ LEX_R_YIELD,					"yield",						true  },
};
#define ARRAY_LENGTH(array) (sizeof(array)/sizeof(array[0]))
#define ARRAY_END(array) (&array[ARRAY_LENGTH(array)])
static token2str_t *reserved_words_end = ARRAY_END(reserved_words_begin);//&reserved_words_begin[ARRAY_LENGTH(reserved_words_begin)];
static token2str_t *str2reserved_begin[sizeof(reserved_words_begin)/sizeof(reserved_words_begin[0])];
static token2str_t **str2reserved_end = &str2reserved_begin[sizeof(str2reserved_begin)/sizeof(str2reserved_begin[0])];
static token2str_t tokens2str_begin[] = {
	{ LEX_EOF,						"EOF",						false },
	{ LEX_ID,						"ID",							true  },
	{ LEX_INT,						"INT",						true  },
	{ LEX_FLOAT,					"FLOAT",						true  },
	{ LEX_STR,						"STRING",					true  },
	{ LEX_REGEXP,					"REGEXP",					true  },
	{ LEX_EQUAL,					"==",							false },
	{ LEX_TYPEEQUAL,				"===",						false },
	{ LEX_NEQUAL,					"!=",							false },
	{ LEX_NTYPEEQUAL,				"!==",						false },
	{ LEX_LEQUAL,					"<=",							false },
	{ LEX_LSHIFT,					"<<",							false },
	{ LEX_LSHIFTEQUAL,			"<<=",						false },
	{ LEX_GEQUAL,					">=",							false },
	{ LEX_RSHIFT,					">>",							false },
	{ LEX_RSHIFTEQUAL,			">>=",						false },
	{ LEX_RSHIFTU,					">>>",						false },
	{ LEX_RSHIFTUEQUAL,			">>>=",						false },
	{ LEX_PLUSEQUAL,				"+=",							false },
	{ LEX_MINUSEQUAL,				"-=",							false },
	{ LEX_PLUSPLUS,				"++",							false },
	{ LEX_MINUSMINUS,				"--",							false },
	{ LEX_ANDEQUAL,				"&=",							false },
	{ LEX_ANDAND,					"&&",							false },
	{ LEX_OREQUAL,					"|=",							false },
	{ LEX_OROR,						"||",							false },
	{ LEX_XOREQUAL,				"^=",							false },
	{ LEX_ASTERISKEQUAL,			"*=",							false },
	{ LEX_SLASHEQUAL,				"/=",							false },
	{ LEX_PERCENTEQUAL,			"%=",							false },
	// special tokens
	{ LEX_T_OF,						"of",							true  },
	{ LEX_T_FUNCTION_OPERATOR,	"function",					true  },
	{ LEX_T_GET,					"get",						true  },
	{ LEX_T_SET,					"set",						true  },
	{ LEX_T_EXCEPTION_VAR,		"LEX_T_EXCEPTION_VAR",	false  },
	{ LEX_T_SKIP,					"LEX_SKIP",					false  },
	{ LEX_T_DUMMY_LABEL,			"LABEL",						true  },
	{ LEX_T_LABEL,					"LABEL",						true  },
	{ LEX_T_LOOP,					"LEX_LOOP",					true  },
	{ LEX_T_FOR_IN,				"LEX_FOR_IN",				true  },
	{ LEX_T_FORWARD,				"LEX_T_FORWARD",			false  },
	{ LEX_T_OBJECT_LITERAL,		"LEX_OBJECT_LITERAL",	false  },
	{ LEX_T_DESTRUCTURING_VAR,	"Destructuring Var",		false  },
};
static token2str_t *tokens2str_end = &tokens2str_begin[sizeof(tokens2str_begin)/sizeof(tokens2str_begin[0])];
struct token2str_cmp_t {
	bool operator()(const token2str_t &lhs, const token2str_t &rhs) {
		return lhs.id < rhs.id;
	}
	bool operator()(const token2str_t &lhs, int rhs) {
		return lhs.id < rhs;
	}
	bool operator()(const token2str_t *lhs, const token2str_t *rhs) {
		return strcmp(lhs->str, rhs->str)<0;
	}
	bool operator()(const token2str_t *lhs, const char *rhs) {
		return strcmp(lhs->str, rhs)<0;
	}
};
static bool tokens2str_sort() {
//	printf("tokens2str_sort called\n");
	sort(tokens2str_begin, tokens2str_end, token2str_cmp_t());
	sort(reserved_words_begin, reserved_words_end, token2str_cmp_t());
	for(unsigned int i=0; i<ARRAY_LENGTH(str2reserved_begin); i++)
		str2reserved_begin[i] = &reserved_words_begin[i];
	sort(str2reserved_begin, str2reserved_end, token2str_cmp_t());
	return true;
}
static bool tokens2str_sorted = tokens2str_sort();

CScriptToken::CScriptToken(CScriptLex *l, int Match, int Alternate) : line(l->currentLine()), column(l->currentColumn()), token(l->tk), intData(0)
{
	if(token == LEX_INT || LEX_TOKEN_DATA_FLOAT(token)) {
		CNumber number(l->tkStr);
		if(number.isInfinity())
			token=LEX_ID, (tokenData=new CScriptTokenDataString("Infinity"))->ref();
		else if(number.isInt32())
			token=LEX_INT, intData=number.toInt32();
		else
			token=LEX_FLOAT, floatData=new double(number.toDouble());
	} else if(LEX_TOKEN_DATA_STRING(token))
		(tokenData = new CScriptTokenDataString(l->tkStr))->ref();
	else if(LEX_TOKEN_DATA_FUNCTION(token))
		(tokenData = new CScriptTokenDataFnc)->ref();
	else if (LEX_TOKEN_DATA_LOOP(token))
		(tokenData = new CScriptTokenDataLoop)->ref();
	else if (LEX_TOKEN_DATA_TRY(token))
		(tokenData = new CScriptTokenDataTry)->ref();
	if(Match>=0)
		l->match(Match, Alternate);
	else
		l->match(l->tk); 
#ifdef _DEBUG
	token_str = getTokenStr(token);
#endif
}
CScriptToken::CScriptToken(uint16_t Tk, int IntData) : line(0), column(0), token(Tk), intData(0) {
	if (LEX_TOKEN_DATA_SIMPLE(token))
		intData = IntData;
	else if (LEX_TOKEN_DATA_FUNCTION(token))
		(tokenData = new CScriptTokenDataFnc)->ref();
	else if (LEX_TOKEN_DATA_DESTRUCTURING_VAR(token))
		(tokenData = new CScriptTokenDataDestructuringVar)->ref();
	else if (LEX_TOKEN_DATA_OBJECT_LITERAL(token))
		(tokenData = new CScriptTokenDataObjectLiteral)->ref();
	else if (LEX_TOKEN_DATA_LOOP(token))
		(tokenData = new CScriptTokenDataLoop)->ref();
	else if (LEX_TOKEN_DATA_TRY(token))
		(tokenData = new CScriptTokenDataTry)->ref();
	else if (LEX_TOKEN_DATA_FORWARDER(token))
		(tokenData = new CScriptTokenDataForwards)->ref();
	else 
		ASSERT(0);
#ifdef _DEBUG
	token_str = getTokenStr(token);
#endif
}

CScriptToken::CScriptToken(uint16_t Tk, const string &TkStr) : line(0), column(0), token(Tk), intData(0) {
	ASSERT(LEX_TOKEN_DATA_STRING(token));
	(tokenData = new CScriptTokenDataString(TkStr))->ref();
#ifdef _DEBUG
	token_str = getTokenStr(token);
#endif
}

CScriptToken &CScriptToken::operator =(const CScriptToken &Copy)
{
	if(this == &Copy) return *this;
	clear();
#ifdef _DEBUG
	token_str	= Copy.token_str;
#endif
	line			= Copy.line;
	column		= Copy.column; 
	token			= Copy.token;
	if(LEX_TOKEN_DATA_FLOAT(token))
		floatData = new double(*Copy.floatData);
	else if(!LEX_TOKEN_DATA_SIMPLE(token))
		(tokenData = Copy.tokenData)->ref();
	else
		intData	= Copy.intData;
	return *this;
}
string CScriptToken::getParsableString(TOKEN_VECT &Tokens, const string &IndentString, const string &Indent) {
	return getParsableString(Tokens.begin(), Tokens.end(), IndentString, Indent);
}
string CScriptToken::getParsableString(TOKEN_VECT_it Begin, TOKEN_VECT_it End, const string &IndentString, const string &Indent) {
	ostringstream destination;
	string nl = Indent.size() ? "\n" : " ";
	string my_indentString = IndentString;
	bool add_nl=false, block_start=false, need_space=false;
	int skip_collon = 0;

	for(TOKEN_VECT_it it=Begin; it != End; ++it) {
		string OutString;
		if(add_nl) OutString.append(nl).append(my_indentString);
		bool old_block_start = block_start;
		bool old_need_space = need_space;
		add_nl = block_start = need_space =false;
		if(it->token == LEX_STR)
			OutString.append(getJSString(it->String())), need_space=true;
		else if(LEX_TOKEN_DATA_STRING(it->token))
			OutString.append(it->String()), need_space=true;
		else if(LEX_TOKEN_DATA_FLOAT(it->token))
			OutString.append(CNumber(it->Float()).toString()), need_space=true;
		else if(it->token == LEX_INT)
			OutString.append(CNumber(it->Int()).toString()), need_space=true;
		else if(LEX_TOKEN_DATA_FUNCTION(it->token)) {
			OutString.append("function ");
			if(it->Fnc().name.size() )
				OutString.append(it->Fnc().name);
			OutString.append(it->Fnc().getArgumentsString());
			OutString.append(getParsableString(it->Fnc().body, my_indentString, Indent));
			if(it->Fnc().body.front().token != '{') {
				OutString.append(";");
			}
		} else if(LEX_TOKEN_DATA_LOOP(it->token)) {
			OutString.append(it->Loop().getParsableString(my_indentString, Indent));
		} else if(LEX_TOKEN_DATA_TRY(it->token)) {
			OutString.append(it->Try().getParsableString(my_indentString, Indent));
		} else if(LEX_TOKEN_DATA_DESTRUCTURING_VAR(it->token)) {
			OutString.append(it->DestructuringVar().getParsableString());
		} else if(LEX_TOKEN_DATA_OBJECT_LITERAL(it->token)) {
			OutString.append(it->Object().getParsableString());
		} else if(it->token == '{') {
			OutString.append("{");
			my_indentString.append(Indent);
			add_nl = block_start = true;
		} else if(it->token == '}') {
			my_indentString.resize(my_indentString.size() - min(my_indentString.size(),Indent.size()));
			if(old_block_start) 
				OutString =  "}";
			else
				OutString = nl + my_indentString + "}";
			add_nl = true;
		} else if(it->token == LEX_T_SKIP) {
			// ignore SKIP-Token
		} else if(it->token == LEX_T_FORWARD) {
			// ignore Forwarder-Token
		} else if(it->token == LEX_R_FOR) {
			OutString.append(CScriptToken::getTokenStr(it->token));
			skip_collon=2;
		} else {
			OutString.append(CScriptToken::getTokenStr(it->token,&need_space));
			if(it->token==';') {
				if(skip_collon) { --skip_collon; }
				else add_nl=true; 
			} 
		}
		if(need_space && old_need_space) destination << " ";
		destination << OutString;
	}
	return destination.str();

}

void CScriptToken::clear()
{
	if(LEX_TOKEN_DATA_FLOAT(token))
		delete floatData;
	else if(!LEX_TOKEN_DATA_SIMPLE(token))
		tokenData->unref();
	token = 0;
}
string CScriptToken::getTokenStr( int token, bool *need_space/*=0*/ )
{
	if(!tokens2str_sorted) tokens2str_sorted=tokens2str_sort();
	token2str_t *found = lower_bound(reserved_words_begin, reserved_words_end, token, token2str_cmp_t());
	if(found != reserved_words_end && found->id==token) {
		if(need_space) *need_space=found->need_space;
		return found->str;
	}
	found = lower_bound(tokens2str_begin, tokens2str_end, token, token2str_cmp_t());
	if(found != tokens2str_end && found->id==token) {
		if(need_space) *need_space=found->need_space;
		return found->str;
	}
	if(need_space) *need_space=false;

	if (token>32 && token<128) {
		char buf[2] = " ";
		buf[0] = (char)token;
		return buf;
	}

	ostringstream msg;
	msg << "?[" << token << "]";
	return msg.str();
}
const char *CScriptToken::isReservedWord(int Token) {
	if(!tokens2str_sorted) tokens2str_sorted=tokens2str_sort();
	token2str_t *found = lower_bound(reserved_words_begin, reserved_words_end, Token, token2str_cmp_t());
	if(found != reserved_words_end && found->id==Token) {
		return found->str;
	}
	return 0;
}
int CScriptToken::isReservedWord(const string &Str) {
	const char *str = Str.c_str();
	if(!tokens2str_sorted) tokens2str_sorted=tokens2str_sort();
	token2str_t **found = lower_bound(str2reserved_begin, str2reserved_end, str, token2str_cmp_t());
	if(found != str2reserved_end && strcmp((*found)->str, str)==0) {
		return (*found)->id;
	}
	return LEX_ID;
}


//////////////////////////////////////////////////////////////////////////
/// CScriptTokenizer
//////////////////////////////////////////////////////////////////////////

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
		ScriptTokenState state;
		pushForwarder(state);
		if(l->tk == '§') { // special-Token at Start means the code begins not at Statement-Level
			l->match('§');
			tokenizeLiteral(state, 0);
		} else do {
			tokenizeStatement(state, 0);
		} while (l->tk!=LEX_EOF);
		pushToken(state.Tokens, LEX_EOF); // add LEX_EOF-Token
		removeEmptyForwarder(state);
//		TOKEN_VECT(tokens).swap(tokens);//	tokens.shrink_to_fit();
		tokens.swap(state.Tokens);
		pushTokenScope(tokens);
		currentFile = l->currentFile;
		tk = getToken().token;
	} catch (...) {
		l=0;
		throw;
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



void CScriptTokenizer::match(int ExpectedToken, int AlternateToken/*=-1*/) {
	if(check(ExpectedToken, AlternateToken))
		getNextToken();
}
bool CScriptTokenizer::check(int ExpectedToken, int AlternateToken/*=-1*/) {
	int currentToken = getToken().token;
	if (ExpectedToken==';' && (currentToken==LEX_EOF || currentToken=='}')) return false; // ignore last missing ';'
	if (currentToken!=ExpectedToken && currentToken!=AlternateToken) {
		ostringstream errorString;
		if(ExpectedToken == LEX_EOF)
			errorString << "Got unexpected " << CScriptToken::getTokenStr(currentToken);
		else {
			errorString << "Got '" << CScriptToken::getTokenStr(currentToken) << "' expected '" << CScriptToken::getTokenStr(ExpectedToken) << "'";
			if(AlternateToken!=-1) errorString << " or '" << CScriptToken::getTokenStr(AlternateToken) << "'";
		}
		throw new CScriptException(SyntaxError, errorString.str(), currentFile, currentLine(), currentColumn());
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
	ASSERT(tokenScopeStack.back().tokens->end()-tokenScopeStack.back().pos-Tokens>=0);
	tokenScopeStack.back().pos+=Tokens-1;
	getNextToken();
}

static inline void setTokenSkip(CScriptTokenizer::ScriptTokenState &State) {
	int tokenBeginIdx = State.Marks.back();
	State.Marks.pop_back();
	State.Tokens[tokenBeginIdx].Int() = State.Tokens.size()-tokenBeginIdx;
}

enum {
	TOKENIZE_FLAGS_canLabel			= 1<<0,
	TOKENIZE_FLAGS_canBreak			= 1<<1,
	TOKENIZE_FLAGS_canContinue		= 1<<2,
	TOKENIZE_FLAGS_canReturn		= 1<<3,
	TOKENIZE_FLAGS_canYield			= 1<<4,
	TOKENIZE_FLAGS_asStatement		= 1<<5,
	TOKENIZE_FLAGS_noIn				= 1<<6,
	TOKENIZE_FLAGS_isAccessor		= 1<<7,
	TOKENIZE_FLAGS_callForNew		= 1<<8,
	TOKENIZE_FLAGS_noBlockStart	= 1<<9,
	TOKENIZE_FLAGS_nestedObject	= 1<<10,
};
void CScriptTokenizer::tokenizeTry(ScriptTokenState &State, int Flags) {
	l->match(LEX_R_TRY);
	CScriptToken TryToken(LEX_T_TRY);
	CScriptTokenDataTry &TryData = TryToken.Try();
	pushToken(State.Tokens, TryToken);

	TOKEN_VECT mainTokens;
	State.Tokens.swap(mainTokens);

	// try-block
	tokenizeBlock(State, Flags);
	State.Tokens.swap(TryData.tryBlock);

	// catch-blocks
	l->check(LEX_R_CATCH, LEX_R_FINALLY);
	bool unconditionalCatch = false;
	while(l->tk == LEX_R_CATCH) {
		if(unconditionalCatch) throw new CScriptException(SyntaxError, "catch after unconditional catch", l->currentFile, l->currentLine(), l->currentColumn());

		// vars & condition
		l->match(LEX_R_CATCH);
		l->match('(');
		TryData.catchBlocks.resize(TryData.catchBlocks.size()+1);
		CScriptTokenDataTry::CatchBlock &catchBlock = TryData.catchBlocks.back();
		pushForwarder(State, true);
		STRING_VECTOR_t vars;
		catchBlock.indentifiers = tokenizeVarIdentifier(&vars).DestructuringVar();
		State.Forwarders.back()->addLets(vars);
		if(l->tk == LEX_R_IF) {
			l->match(LEX_R_IF);
			tokenizeExpression(State, Flags);
		} else
			unconditionalCatch = true;
		State.Tokens.swap(catchBlock.condition);
		l->match(')');

		// catch-block
		tokenizeBlock(State, Flags | TOKENIZE_FLAGS_noBlockStart);
		State.Tokens.swap(catchBlock.block);
		State.Forwarders.pop_back();
	}
	// finally-block
	if(l->tk == LEX_R_FINALLY) {
		l->match(LEX_R_FINALLY);
		tokenizeBlock(State, Flags);
		State.Tokens.swap(TryData.finallyBlock);
	}
	State.Tokens.swap(mainTokens);
}
void CScriptTokenizer::tokenizeSwitch(ScriptTokenState &State, int Flags) {

	State.Marks.push_back(pushToken(State.Tokens)); // push Token & push tokenBeginIdx
	pushToken(State.Tokens, '(');
	tokenizeExpression(State, Flags);
	pushToken(State.Tokens, ')');

	State.Marks.push_back(pushToken(State.Tokens, '{')); // push Token & push blockBeginIdx
	pushForwarder(State);
	

	vector<int>::size_type MarksSize = State.Marks.size();
	Flags |= TOKENIZE_FLAGS_canBreak;
	for(bool hasDefault=false;;) {
		if( l->tk == LEX_R_CASE || l->tk == LEX_R_DEFAULT) {
			if(l->tk == LEX_R_CASE) {
				State.Marks.push_back(pushToken(State.Tokens)); // push Token & push caseBeginIdx
				State.Marks.push_back(pushToken(State.Tokens,CScriptToken(LEX_T_SKIP))); //  skipper to skip case-expression
				tokenizeExpression(State, Flags); 
				setTokenSkip(State);
			} else { // default
				State.Marks.push_back(pushToken(State.Tokens)); // push Token & push caseBeginIdx
				if(hasDefault) throw new CScriptException(SyntaxError, "more than one switch default", l->currentFile, l->currentLine(), l->currentColumn());
				hasDefault = true;
			}

			State.Marks.push_back(pushToken(State.Tokens, ':'));
			while(l->tk != '}' && l->tk != LEX_R_CASE && l->tk != LEX_R_DEFAULT && l->tk != LEX_EOF )
				tokenizeStatement(State, Flags); 
			setTokenSkip(State);
		} else if(l->tk == '}')
			break;
		else
			throw new CScriptException(SyntaxError, "invalid switch statement", l->currentFile, l->currentLine(), l->currentColumn());
	}
	while(MarksSize < State.Marks.size()) setTokenSkip(State);
	removeEmptyForwarder(State); // remove Forwarder if empty
	pushToken(State.Tokens, '}');
	setTokenSkip(State); // switch-block
	setTokenSkip(State); // switch-statement
}
void CScriptTokenizer::tokenizeWith(ScriptTokenState &State, int Flags) {
	State.Marks.push_back(pushToken(State.Tokens)); // push Token & push tokenBeginIdx

	pushToken(State.Tokens, '(');
	tokenizeExpression(State, Flags);
	pushToken(State.Tokens, ')');
	tokenizeStatementNoLet(State, Flags);

	setTokenSkip(State);
}

static inline uint32_t GetLoopLabels(CScriptTokenizer::ScriptTokenState &State, CScriptTokenDataLoop &LoopData) {
	uint32_t label_count = 0;
	if(State.Tokens.size()>=2) {
		for(TOKEN_VECT::reverse_iterator it = State.Tokens.rbegin(); it!=State.Tokens.rend(); ++it) {
			if(it->token == ':' && (++it)->token == LEX_T_LABEL) {
				++label_count;
				LoopData.labels.push_back(it->String());
				State.LoopLabels.push_back(it->String());
				it->token = LEX_T_DUMMY_LABEL;
			} else 
				break;
		}
	}
	return label_count;
}
static inline void PopLoopLabels(uint32_t label_count, STRING_VECTOR_t &LoopLabels) {
	ASSERT(label_count <= LoopLabels.size());
	LoopLabels.resize(LoopLabels.size()-label_count);
}
void CScriptTokenizer::tokenizeWhileAndDo(ScriptTokenState &State, int Flags) {
	
	bool do_while = l->tk==LEX_R_DO;

	CScriptToken LoopToken(LEX_T_LOOP);
	CScriptTokenDataLoop &LoopData = LoopToken.Loop();
	LoopData.type = do_while ? CScriptTokenDataLoop::DO : CScriptTokenDataLoop::WHILE;

	// get loop-labels
	uint32_t label_count = GetLoopLabels(State, LoopData);

	l->match(l->tk); // match while or do

	pushToken(State.Tokens, LoopToken);
	
	TOKEN_VECT mainTokens;
	State.Tokens.swap(mainTokens);

	if(!do_while) {
		l->match('(');
		tokenizeExpression(State, Flags);
		State.Tokens.swap(LoopData.condition);
		l->match(')');
	}
	tokenizeStatementNoLet(State, Flags | TOKENIZE_FLAGS_canBreak | TOKENIZE_FLAGS_canContinue);
	State.Tokens.swap(LoopData.body);
	if(do_while) {
		l->match(LEX_R_WHILE);
		l->match('(');
		tokenizeExpression(State, Flags);
		State.Tokens.swap(LoopData.condition);
		l->match(')');
		l->match(';');
	}
	State.Tokens.swap(mainTokens);
	PopLoopLabels(label_count, State.LoopLabels);
}

void CScriptTokenizer::tokenizeIf(ScriptTokenState &State, int Flags) {

	State.Marks.push_back(pushToken(State.Tokens)); // push Token & push tokenBeginIdx

	pushToken(State.Tokens, '(');
	tokenizeExpression(State, Flags);
	pushToken(State.Tokens, ')');
	State.Marks.push_back(pushToken(State.Tokens, CScriptToken(LEX_T_SKIP))); // push skip & skiperBeginIdx
	tokenizeStatementNoLet(State, Flags);

	setTokenSkip(State);

	if(l->tk == LEX_R_ELSE) {
		State.Marks.push_back(pushToken(State.Tokens)); // push Token & push tokenBeginIdx
		tokenizeStatementNoLet(State, Flags);
		setTokenSkip(State);
	}

	setTokenSkip(State);
}

void CScriptTokenizer::tokenizeFor(ScriptTokenState &State, int Flags) {
	bool for_in=false, for_of=false, for_each_in=false;
	CScriptToken LoopToken(LEX_T_LOOP);
	CScriptTokenDataLoop &LoopData = LoopToken.Loop();

	// get loop-labels
	uint32_t label_count = GetLoopLabels(State, LoopData);

	l->match(LEX_R_FOR);
	if((for_in = for_each_in = (l->tk == LEX_ID && l->tkStr == "each")))
		l->match(LEX_ID); // match "each"

	pushToken(State.Tokens, LoopToken);

	l->match('(');
	TOKEN_VECT mainTokens;
	State.Tokens.swap(mainTokens);

	bool haveLetScope = false;

	if(l->tk == LEX_R_VAR || l->tk == LEX_R_LET) {
		if(l->tk == LEX_R_VAR)
			tokenizeVarNoConst(State, Flags | TOKENIZE_FLAGS_noIn);
		else { //if(l->tk == LEX_R_LET)
			haveLetScope = true;
			pushForwarder(State, true); // no clean up empty tokenizer
			tokenizeLet(State, Flags | TOKENIZE_FLAGS_noIn | TOKENIZE_FLAGS_asStatement);
		}
	} else if(l->tk!=';') {
		tokenizeExpression(State, Flags | TOKENIZE_FLAGS_noIn);
	}
	if((for_in=(l->tk==LEX_R_IN || (l->tk==LEX_ID && l->tkStr=="of")))) {
		if(!State.LeftHand)
			throw new CScriptException(ReferenceError, "invalid for/in left-hand side", l->currentFile, l->currentLine(), l->currentColumn());
		if(l->tk==LEX_ID && l->tkStr=="of") l->tk = LEX_T_OF; // fake token
		if((for_of = (!for_each_in && l->tk==LEX_T_OF))) {
			l->match(LEX_T_OF);
			LoopData.type = CScriptTokenDataLoop::FOR_OF;
		} else {
			l->match(LEX_R_IN);
			LoopData.type = for_each_in ? CScriptTokenDataLoop::FOR_EACH : CScriptTokenDataLoop::FOR_IN;
		}
		State.Tokens.swap(LoopData.condition);

		if(LoopData.condition.front().token == LEX_T_FORWARD) {
			LoopData.init.push_back(LoopData.condition.front());
			LoopData.condition.erase(LoopData.condition.begin());
		}
		mainTokens.back().token = LEX_T_FOR_IN;
	} else {
		l->check(';'); // no automatic ;-injection
		pushToken(State.Tokens, ';');
		State.Tokens.swap(LoopData.init);
		if(l->tk != ';') tokenizeExpression(State, Flags);
		l->check(';'); // no automatic ;-injection
		l->match(';'); // no automatic ;-injection
		State.Tokens.swap(LoopData.condition);
	}

	if(for_in || l->tk != ')') tokenizeExpression(State, Flags); 
	l->match(')');
	State.Tokens.swap(LoopData.iter);
	Flags = (Flags & (TOKENIZE_FLAGS_canReturn | TOKENIZE_FLAGS_canYield)) | TOKENIZE_FLAGS_canBreak | TOKENIZE_FLAGS_canContinue; 
	if(haveLetScope) Flags |= TOKENIZE_FLAGS_noBlockStart;
	tokenizeStatementNoLet(State, Flags);
	if(haveLetScope) State.Forwarders.pop_back();

	State.Tokens.swap(LoopData.body);
	State.Tokens.swap(mainTokens);
	if(for_in) {
		LoopData.condition.push_back('=');
		LoopData.condition.push_back(LEX_T_EXCEPTION_VAR);
		LoopData.condition.push_back('.');
		LoopData.condition.push_back(CScriptToken(LEX_ID, "next"));
		LoopData.condition.push_back('(');
		LoopData.condition.push_back(')');
		LoopData.condition.push_back(';');
	}
	PopLoopLabels(label_count, State.LoopLabels);
}

static void tokenizeVarIdentifierDestructuring( CScriptLex *Lexer, DESTRUCTURING_VARS_t &Vars, const std::string &Path, STRING_VECTOR_t *VarNames );
static void tokenizeVarIdentifierDestructuringObject(CScriptLex *Lexer, DESTRUCTURING_VARS_t &Vars, STRING_VECTOR_t *VarNames) {
	Lexer->match('{');
	while(Lexer->tk != '}') {
		CScriptLex::POS prev_pos = Lexer->pos;
		string Path = Lexer->tkStr;
		Lexer->match(LEX_ID, LEX_STR);
		if(Lexer->tk == ':') {
			Lexer->match(':');
			tokenizeVarIdentifierDestructuring(Lexer, Vars, Path, VarNames);
		} else {
			Lexer->reset(prev_pos);
			if(VarNames) VarNames->push_back(Lexer->tkStr);
			Vars.push_back(DESTRUCTURING_VAR_t(Lexer->tkStr, Lexer->tkStr));
			Lexer->match(LEX_ID);
		}
		if (Lexer->tk!='}') Lexer->match(',', '}'); 
	}
	Lexer->match('}');
}
static void tokenizeVarIdentifierDestructuringArray(CScriptLex *Lexer, DESTRUCTURING_VARS_t &Vars, STRING_VECTOR_t *VarNames) {
	int idx = 0;
	Lexer->match('[');
	while(Lexer->tk != ']') {
		if(Lexer->tk == ',')
			Vars.push_back(DESTRUCTURING_VAR_t("", "")); // empty
		else
			tokenizeVarIdentifierDestructuring(Lexer, Vars, int2string(idx), VarNames);
		++idx;
		if (Lexer->tk!=']') Lexer->match(',',']'); 
	}
	Lexer->match(']');
}
static void tokenizeVarIdentifierDestructuring(CScriptLex *Lexer, DESTRUCTURING_VARS_t &Vars, const std::string &Path, STRING_VECTOR_t *VarNames ) {
	if(Lexer->tk == '[') {
		Vars.push_back(DESTRUCTURING_VAR_t(Path, "[")); // marks array begin
		tokenizeVarIdentifierDestructuringArray(Lexer, Vars, VarNames);
		Vars.push_back(DESTRUCTURING_VAR_t("", "]")); // marks array end
	} else if(Lexer->tk == '{') {
		Vars.push_back(DESTRUCTURING_VAR_t(Path, "{")); // marks object begin
		tokenizeVarIdentifierDestructuringObject(Lexer, Vars, VarNames);
		Vars.push_back(DESTRUCTURING_VAR_t("", "}")); // marks object end
	} else {
		if(VarNames) VarNames->push_back(Lexer->tkStr);
		Vars.push_back(DESTRUCTURING_VAR_t(Path, Lexer->tkStr));
		Lexer->match(LEX_ID);
	}
}
CScriptToken CScriptTokenizer::tokenizeVarIdentifier( STRING_VECTOR_t *VarNames/*=0*/, bool *NeedAssignment/*=0*/ ) {
	CScriptToken token(LEX_T_DESTRUCTURING_VAR);
	if(NeedAssignment) *NeedAssignment=(l->tk == '[' || l->tk=='{');
	token.column = l->currentColumn();
	token.line = l->currentLine();
	tokenizeVarIdentifierDestructuring(l, token.DestructuringVar().vars, "", VarNames);
	return token;
}

void CScriptTokenizer::tokenizeFunction(ScriptTokenState &State, int Flags, bool noLetDef/*=false*/) {
	bool forward = false;
	bool Statement = (Flags & TOKENIZE_FLAGS_asStatement) != 0;
	bool Accessor = (Flags & TOKENIZE_FLAGS_isAccessor) != 0;
	CScriptLex::POS functionPos = l->pos;

	int tk = l->tk;
	if(Accessor) {
		tk = State.Tokens.back().String()=="get"?LEX_T_GET:LEX_T_SET;
		State.Tokens.pop_back();
	} else {
		l->match(LEX_R_FUNCTION);
		if(!Statement) tk = LEX_T_FUNCTION_OPERATOR;
	}
	if(tk == LEX_R_FUNCTION) // only forward functions 
		forward = !noLetDef && State.Forwarders.front() == State.Forwarders.back();

	CScriptToken FncToken(tk);
	CScriptTokenDataFnc &FncData = FncToken.Fnc();

	if(l->tk == LEX_ID || Accessor) {
		FncData.name = l->tkStr;
		l->match(LEX_ID, LEX_STR);
	} else if(Statement)
		throw new CScriptException(SyntaxError, "Function statement requires a name.", l->currentFile, l->currentLine(), l->currentColumn());
	l->match('(');
	while(l->tk != ')') {
		FncData.arguments.push_back(tokenizeVarIdentifier());
		if (l->tk!=')') l->match(',',')'); 
	}
	// l->match(')');
	// to allow regexp at the beginning of a lambda-function fake last token
	l->tk = '{';
	l->match('{');
	FncData.file = l->currentFile;
	FncData.line = l->currentLine();

	ScriptTokenState functionState;
	functionState.HaveReturnValue = functionState.FunctionIsGenerator = false;
	if(l->tk == '{' || tk==LEX_T_GET || tk==LEX_T_SET)
		tokenizeBlock(functionState, TOKENIZE_FLAGS_canReturn | TOKENIZE_FLAGS_canYield);
	else {
		tokenizeExpression(functionState, TOKENIZE_FLAGS_canYield);
		l->match(';');
		functionState.HaveReturnValue = true;
	}
	if(functionState.HaveReturnValue == true && functionState.FunctionIsGenerator == true)
		throw new CScriptException(TypeError, "generator function returns a value.", l->currentFile, functionPos.currentLine, functionPos.currentColumn());
	FncData.isGenerator = functionState.FunctionIsGenerator;

	functionState.Tokens.swap(FncData.body);
	if(forward) {
		State.Forwarders.front()->functions.insert(FncToken);
		FncToken.token = LEX_T_FUNCTION_PLACEHOLDER;
	}
	State.Tokens.push_back(FncToken);
}

void CScriptTokenizer::tokenizeLet(ScriptTokenState &State, int Flags, bool noLetDef/*=false*/) {
	bool Definition = (Flags & TOKENIZE_FLAGS_asStatement)!=0; 
	bool noIN = (Flags & TOKENIZE_FLAGS_noIn)!=0;
	bool Statement = Definition & !noIN;
	bool Expression = !Definition;
	Flags &= ~(TOKENIZE_FLAGS_asStatement);
	if(!Definition) noIN=false, Flags &= ~TOKENIZE_FLAGS_noIn;

	bool foundIN = false;
	bool leftHand = true;
	int currLine = l->currentLine(), currColumn = l->currentColumn();

	State.Marks.push_back(pushToken(State.Tokens)); // push Token & push BeginIdx

	if(l->tk == '(' || !Definition) { // no definition needs statement or expression
		leftHand = false;
		Expression = true;
		pushToken(State.Tokens, '(');
		pushForwarder(State);
	} else if(noLetDef)
		throw new CScriptException(SyntaxError, "let declaration not directly within block", l->currentFile, currLine, currColumn);
	STRING_VECTOR_t vars;
	for(;;) {
		bool needAssignment = false;
		State.Tokens.push_back(tokenizeVarIdentifier(&vars, &needAssignment));
		if(noIN && (foundIN=(l->tk==LEX_R_IN || (l->tk==LEX_ID && l->tkStr=="of")))) 
			break;
		if(needAssignment || l->tk=='=') {
			leftHand = false;
			pushToken(State.Tokens, '=');
			tokenizeAssignment(State, Flags);
			if(noIN && (foundIN=(l->tk==LEX_R_IN || (l->tk==LEX_ID && l->tkStr=="of")))) 
				break;
		}
		if(l->tk==',') {
			leftHand = false;
			pushToken(State.Tokens);
		}
		else
			break;
	}
	if(Expression) {
		string redeclared = State.Forwarders.back()->addLets(vars);
		if(redeclared.size())
			throw new CScriptException(TypeError, "redeclaration of variable '"+redeclared+"'", l->currentFile, currLine, currColumn);
		if(!foundIN) {
			pushToken(State.Tokens, ')');
			if(Statement) { 
				if(l->tk == '{') // no extra BlockStart by expression
					tokenizeBlock(State, Flags|=TOKENIZE_FLAGS_noBlockStart);
				else
					tokenizeStatementNoLet(State, Flags);
			} else
				tokenizeAssignment(State, Flags);
		}
		// never remove Forwarder-token here -- popForwarder(Tokens, BlockStart, Marks);
		State.Forwarders.back()->vars_in_letscope.clear(); // only clear vars_in_letscope
		State.Marks.pop_back();
	} else {
		if(!noIN) pushToken(State.Tokens, ';');

		string redeclared;
		if(State.Forwarders.size()<=1) {
			// Currently it is allowed in javascript, to redeclare "let"-declared vars
			// in root- or function-scopes. In this case, "let" handled like "var"
			// To prevent redeclaration in root- or function-scopes define PREVENT_REDECLARATION_IN_FUNCTION_SCOPES 
#ifdef PREVENT_REDECLARATION_IN_FUNCTION_SCOPES
			redeclared = State.Forwarders.front()->addLets(vars);
#else
			State.Forwarders.front()->addVars(vars);
#endif
		} else
			redeclared = State.Forwarders.back()->addLets(vars);
		if(redeclared.size())
			throw new CScriptException(TypeError, "redeclaration of variable '"+redeclared+"'", l->currentFile, currLine, currColumn);
	}
	setTokenSkip(State);
	if(leftHand) State.LeftHand = true;
}

void CScriptTokenizer::tokenizeVarNoConst( ScriptTokenState &State, int Flags) {
	l->check(LEX_R_VAR);
	tokenizeVarAndConst(State, Flags);
}
void CScriptTokenizer::tokenizeVarAndConst( ScriptTokenState &State, int Flags) {
	bool noIN = (Flags & TOKENIZE_FLAGS_noIn)!=0;
	int currLine = l->currentLine(), currColumn = l->currentColumn();

	bool leftHand = true;
	int tk = l->tk;
	State.Marks.push_back(pushToken(State.Tokens)); // push Token & push BeginIdx

	STRING_VECTOR_t vars;
	for(;;) 
	{
		bool needAssignment = false;
		State.Tokens.push_back(tokenizeVarIdentifier(&vars, &needAssignment));
		if(noIN && (l->tk==LEX_R_IN || (l->tk==LEX_ID && l->tkStr=="of"))) 
			break;
		if(needAssignment || l->tk=='=') {
			leftHand = false;
			pushToken(State.Tokens, '=');
			tokenizeAssignment(State, Flags);
			if(noIN && (l->tk==LEX_R_IN || (l->tk==LEX_ID && l->tkStr=="of"))) 
				break;
		}
		if(l->tk==',') {
			leftHand = false;
			pushToken(State.Tokens);
		}
		else
			break;
	}
	if(!noIN) pushToken(State.Tokens, ';');

	setTokenSkip(State);

	if(tk==LEX_R_VAR)
		State.Forwarders.front()->addVars(vars);
	else
		State.Forwarders.front()->addConsts(vars);
	string redeclared;
	if(State.Forwarders.size()>1) // have let-scope
		redeclared = State.Forwarders.back()->addVarsInLetscope(vars);
#ifdef PREVENT_REDECLARATION_IN_FUNCTION_SCOPES
	else
		redeclared = State.Forwarders.front()->addVarsInLetscope(vars);
#endif
	if(redeclared.size())
		throw new CScriptException(TypeError, "redeclaration of variable '"+redeclared+"'", l->currentFile, currLine, currColumn);
	if(leftHand) State.LeftHand = true;
}

void CScriptTokenizer::_tokenizeLiteralObject(ScriptTokenState &State, int Flags) {
	bool forFor = (Flags & TOKENIZE_FLAGS_noIn)!=0;
	bool nestedObject = 	(Flags & TOKENIZE_FLAGS_nestedObject) != 0; 
	Flags &= ~(TOKENIZE_FLAGS_noIn | TOKENIZE_FLAGS_nestedObject);
	CScriptToken ObjectToken(LEX_T_OBJECT_LITERAL);
	CScriptTokenDataObjectLiteral &Objc = ObjectToken.Object();

	Objc.type = CScriptTokenDataObjectLiteral::OBJECT;
	Objc.destructuring = Objc.structuring = true;

	string msg, msgFile;
	int msgLine=0, msgColumn=0;

	l->match('{');
	while (l->tk != '}') {
		CScriptTokenDataObjectLiteral::ELEMENT element;
		bool assign = false;
		if(CScriptToken::isReservedWord(l->tk))
			l->tk = LEX_ID; // fake reserved-word as member.ID
		if(l->tk == LEX_ID) {
			element.id = l->tkStr;
			CScriptToken Token(l, LEX_ID);
			if((l->tk==LEX_ID || l->tk==LEX_STR ) && (element.id=="get" || element.id=="set")) {
				element.id = l->tkStr;
				element.value.push_back(Token);
				State.Tokens.swap(element.value);
				tokenizeFunction(State, Flags|TOKENIZE_FLAGS_isAccessor);
				State.Tokens.swap(element.value);
				Objc.destructuring = false;
			} else {
				if(Objc.destructuring && (l->tk == ',' || l->tk == '}')) {
					if(!msg.size()) {
						Objc.structuring = false;
						msg.append("Got '").append(CScriptToken::getTokenStr(l->tk)).append("' expected ':'");
						msgFile = l->currentFile;
						msgLine = l->currentLine();
						msgColumn = l->currentColumn();
						;
					}
					element.value.push_back(Token);
				} else
					assign = true;
			}
		} else if(l->tk == LEX_INT) {
			element.id = int2string((int32_t)strtol(l->tkStr.c_str(),0,0)); 
			l->match(LEX_INT);
			assign = true;
		} else if(l->tk == LEX_FLOAT) {
			element.id = float2string(strtod(l->tkStr.c_str(),0)); 
			l->match(LEX_FLOAT);
			assign = true;
		} else if(LEX_TOKEN_DATA_STRING(l->tk) && l->tk != LEX_REGEXP) {
			element.id = l->tkStr; 
			l->match(l->tk);
			assign = true;
		} else
			l->match(LEX_ID, LEX_STR);
		if(assign) {
			l->match(':');
			int dFlags = Flags | (l->tk == '{' || l->tk == '[') ? TOKENIZE_FLAGS_nestedObject : 0;
			State.pushLeftHandState();
			State.Tokens.swap(element.value);
			tokenizeAssignment(State, dFlags);
			State.Tokens.swap(element.value);
			if(Objc.destructuring) Objc.destructuring = State.LeftHand;
			State.popLeftHandeState();
		}
		
		if(!Objc.destructuring && msg.size())
			throw new CScriptException(SyntaxError, msg, msgFile, msgLine, msgColumn);
		Objc.elements.push_back(element);
		if (l->tk != '}') l->match(',', '}');
	}
	l->match('}');
	if(Objc.destructuring && Objc.structuring) {
		if(nestedObject) {
			if(l->tk!=',' && l->tk!='}' && l->tk!='=')
				Objc.destructuring = false;
		}
		else 
			Objc.setMode(l->tk=='=' || (forFor && (l->tk==LEX_R_IN ||(l->tk==LEX_ID && l->tkStr=="of"))));
	} else {
		if(!Objc.destructuring && msg.size())
			throw new CScriptException(SyntaxError, msg, msgFile, msgLine, msgColumn);
		if(!nestedObject) Objc.setMode(Objc.destructuring);
	}

	if(Objc.destructuring)
		State.LeftHand = true;
	State.Tokens.push_back(ObjectToken);
}
void CScriptTokenizer::_tokenizeLiteralArray(ScriptTokenState &State, int Flags) {
	bool forFor = (Flags & TOKENIZE_FLAGS_noIn)!=0;
	bool nestedObject = 	(Flags & TOKENIZE_FLAGS_nestedObject) != 0; 
	Flags &= ~(TOKENIZE_FLAGS_noIn | TOKENIZE_FLAGS_nestedObject);
	CScriptToken ObjectToken(LEX_T_OBJECT_LITERAL);
	CScriptTokenDataObjectLiteral &Objc = ObjectToken.Object();

	Objc.type = CScriptTokenDataObjectLiteral::ARRAY;
	Objc.destructuring = Objc.structuring = true;
	int idx = 0;

	l->match('[');
	while (l->tk != ']') {
		CScriptTokenDataObjectLiteral::ELEMENT element;
		element.id = int2string(idx++);
		if(l->tk != ',') {
			int dFlags = Flags | (l->tk == '{' || l->tk == '[') ? TOKENIZE_FLAGS_nestedObject : 0;
			State.pushLeftHandState();
			State.Tokens.swap(element.value);
			tokenizeAssignment(State, dFlags);
			State.Tokens.swap(element.value);
			if(Objc.destructuring) Objc.destructuring = State.LeftHand;
			State.popLeftHandeState();
		}
		Objc.elements.push_back(element);
		if (l->tk != ']') l->match(',', ']');
	}
	l->match(']');
	if(Objc.destructuring && Objc.structuring) {
		if(nestedObject) {
			if(l->tk!=',' && l->tk!=']' && l->tk!='=')
				Objc.destructuring = false;
		}
		else 
			Objc.setMode(l->tk=='=' || (forFor && (l->tk==LEX_R_IN ||(l->tk==LEX_ID && l->tkStr=="of"))));
	} else 
		if(!nestedObject) Objc.setMode(Objc.destructuring);
	if(Objc.destructuring)
		State.LeftHand = true;
	State.Tokens.push_back(ObjectToken);
}

void CScriptTokenizer::tokenizeLiteral(ScriptTokenState &State, int Flags) {
	State.LeftHand = 0;
	bool canLabel = Flags & TOKENIZE_FLAGS_canLabel; Flags &= ~TOKENIZE_FLAGS_canLabel;
	int ObjectLiteralFlags = Flags;
	Flags &= ~TOKENIZE_FLAGS_noIn;
	switch(l->tk) {
	case LEX_ID:
		{
			string label = l->tkStr;
			pushToken(State.Tokens);
			if(l->tk==':' && canLabel) {
				if(find(State.Labels.begin(), State.Labels.end(), label) != State.Labels.end()) 
					throw new CScriptException(SyntaxError, "dublicate label '"+label+"'", l->currentFile, l->currentLine(), l->currentColumn()-label.size());
				State.Tokens[State.Tokens.size()-1].token = LEX_T_LABEL; // change LEX_ID to LEX_T_LABEL
				State.Labels.push_back(label);
			} else if(label=="this") {
				if( l->tk == '=' || (l->tk >= LEX_ASSIGNMENTS_BEGIN && l->tk <= LEX_ASSIGNMENTS_END) )
					throw new CScriptException(SyntaxError, "invalid assignment left-hand side", l->currentFile, l->currentLine(), l->currentColumn()-label.size());
				if( l->tk==LEX_PLUSPLUS || l->tk==LEX_MINUSMINUS )
					throw new CScriptException(SyntaxError, l->tk==LEX_PLUSPLUS?"invalid increment operand":"invalid decrement operand", l->currentFile, l->currentLine(), l->currentColumn()-label.size());
			} else
				State.LeftHand = true;
		}
		break;
	case LEX_INT:
	case LEX_FLOAT:
	case LEX_STR:
	case LEX_REGEXP:
	case LEX_R_TRUE:
	case LEX_R_FALSE:
	case LEX_R_NULL:
		pushToken(State.Tokens);
		break;
	case '{':
		_tokenizeLiteralObject(State, ObjectLiteralFlags);
		break;
	case '[':
		_tokenizeLiteralArray(State, ObjectLiteralFlags);
		break;
	case LEX_R_LET: // let as expression
		tokenizeLet(State, Flags);
		break;
	case LEX_R_FUNCTION:
		tokenizeFunction(State, Flags);
		break;
	case LEX_R_NEW: 
		State.Marks.push_back(pushToken(State.Tokens)); // push Token & push BeginIdx
		{
			tokenizeFunctionCall(State, (Flags | TOKENIZE_FLAGS_callForNew) & ~TOKENIZE_FLAGS_noIn);
			State.LeftHand = 0;
		}
		setTokenSkip(State);
		break;
#ifndef NO_GENERATORS
	case LEX_R_YIELD:
		if( (Flags & TOKENIZE_FLAGS_canYield)==0) 
			throw new CScriptException(SyntaxError, "'yield' expression, but not in a function.", l->currentFile, l->currentLine(), l->currentColumn());
		pushToken(State.Tokens);
		if(l->tk != ';' && l->tk != '}' && !l->lineBreakBeforeToken) {
			tokenizeExpression(State, Flags);
		}
		State.FunctionIsGenerator = true;
		break;
#endif
	case '(':
		State.Marks.push_back(pushToken(State.Tokens)); // push Token & push BeginIdx
		tokenizeExpression(State, Flags & ~TOKENIZE_FLAGS_noIn);
		State.LeftHand = 0;
		pushToken(State.Tokens, ')');
		setTokenSkip(State);
		break;
	default:
		l->check(LEX_EOF);
	}
}
void CScriptTokenizer::tokenizeMember(ScriptTokenState &State, int Flags) {
	while(l->tk == '.' || l->tk == '[') {
		if(l->tk == '.') {
			pushToken(State.Tokens);
			if(CScriptToken::isReservedWord(l->tk))
				l->tk = LEX_ID; // fake reserved-word as member.ID
			pushToken(State.Tokens , LEX_ID);
		} else {
			State.Marks.push_back(pushToken(State.Tokens));
			State.pushLeftHandState();
			tokenizeExpression(State, Flags & ~TOKENIZE_FLAGS_noIn);
			State.popLeftHandeState();
			pushToken(State.Tokens, ']');
			setTokenSkip(State);
		}
		State.LeftHand = true;
	}
}
void CScriptTokenizer::tokenizeFunctionCall(ScriptTokenState &State, int Flags) {
	bool for_new = (Flags & TOKENIZE_FLAGS_callForNew)!=0; Flags &= ~TOKENIZE_FLAGS_callForNew;
	tokenizeLiteral(State, Flags);
	tokenizeMember(State, Flags);
	while(l->tk == '(') {
		State.LeftHand = false;
		State.Marks.push_back(pushToken(State.Tokens)); // push Token & push BeginIdx
		State.pushLeftHandState();
		while(l->tk!=')') {
			tokenizeAssignment(State, Flags & ~TOKENIZE_FLAGS_noIn);
			if (l->tk!=')') pushToken(State.Tokens, ',', ')');
		}
		State.popLeftHandeState();
		pushToken(State.Tokens);
		setTokenSkip(State);
		if(for_new) break;
		tokenizeMember(State, Flags);
	}
}

void CScriptTokenizer::tokenizeSubExpression(ScriptTokenState &State, int Flags) {
	static int Left2Right_begin[] = { 
		/* Precedence 5 */		'*', '/', '%', 
		/* Precedence 6 */		'+', '-',
		/* Precedence 7 */		LEX_LSHIFT, LEX_RSHIFT, LEX_RSHIFTU,
		/* Precedence 8 */		LEX_EQUAL, LEX_NEQUAL, LEX_TYPEEQUAL, LEX_NTYPEEQUAL,
		/* Precedence 9 */		'<', LEX_LEQUAL, '>', LEX_GEQUAL, LEX_R_IN, LEX_R_INSTANCEOF,
		/* Precedence 10-12 */	'&', '^', '|', 
	};
	static int *Left2Right_end = &Left2Right_begin[sizeof(Left2Right_begin)/sizeof(Left2Right_begin[0])];
	static bool Left2Right_sorted = false;
	if(!Left2Right_sorted) Left2Right_sorted = (sort(Left2Right_begin, Left2Right_end), true);
	bool noLeftHand = false;
	for(;;) {
		bool right2left_end = false;
		while(!right2left_end) {
			switch(l->tk) {
			case '-':
			case '+':
			case '!':
			case '~':
			case LEX_R_TYPEOF:
			case LEX_R_VOID:
			case LEX_R_DELETE:
				Flags &= ~TOKENIZE_FLAGS_canLabel;
				noLeftHand = true;
				pushToken(State.Tokens); // Precedence 3
				break;
			case LEX_PLUSPLUS: // pre-increment		
			case LEX_MINUSMINUS: // pre-decrement 	
				{
					int tk = l->tk;
					Flags &= ~TOKENIZE_FLAGS_canLabel;
					noLeftHand = true;
					pushToken(State.Tokens); // Precedence 4
					if(l->tk == LEX_ID && l->tkStr == "this")
						throw new CScriptException(SyntaxError, tk==LEX_PLUSPLUS?"invalid increment operand":"invalid decrement operand", l->currentFile, l->currentLine(), l->currentColumn());
				}
			default:
				right2left_end = true;
			}
		}
		tokenizeFunctionCall(State, Flags);
		
		if (!l->lineBreakBeforeToken && (l->tk==LEX_PLUSPLUS || l->tk==LEX_MINUSMINUS)) { // post-in-/de-crement
			noLeftHand = true;;
			pushToken(State.Tokens); // Precedence 4
		}
		if(Flags&TOKENIZE_FLAGS_noIn && l->tk==LEX_R_IN)
			break;
		int *found = lower_bound(Left2Right_begin, Left2Right_end, l->tk);
		if(found != Left2Right_end && *found == l->tk) {
			noLeftHand = true;
			pushToken(State.Tokens); // Precedence 5-14
		}
		else
			break;
	}
	if(noLeftHand) State.LeftHand = false;
}

void CScriptTokenizer::tokenizeLogic(ScriptTokenState &State, int Flags, int op /*= LEX_OROR*/, int op_n /*= LEX_ANDAND*/) {
	op_n ? tokenizeLogic(State, Flags, op_n, 0) : tokenizeSubExpression(State, Flags);
	if(l->tk==op) {
		unsigned int marks_count = State.Marks.size();
		while(l->tk==op) {
			State.Marks.push_back(pushToken(State.Tokens));
			op_n ? tokenizeLogic(State, Flags, op_n, 0) : tokenizeSubExpression(State, Flags);
		}
		while(State.Marks.size()>marks_count) setTokenSkip(State);
		State.LeftHand = false;
	}
}

void CScriptTokenizer::tokenizeCondition(ScriptTokenState &State, int Flags) {
	tokenizeLogic(State, Flags);
	if(l->tk == '?') {
		Flags &= ~(TOKENIZE_FLAGS_noIn | TOKENIZE_FLAGS_canLabel); 
		State.Marks.push_back(pushToken(State.Tokens));
		tokenizeAssignment(State, Flags);
		setTokenSkip(State);
		State.Marks.push_back(pushToken(State.Tokens, ':'));
		tokenizeAssignment(State, Flags);
		setTokenSkip(State);
		State.LeftHand = false;
	}
}
void CScriptTokenizer::tokenizeAssignment(ScriptTokenState &State, int Flags) {
	tokenizeCondition(State, Flags);
	if (l->tk=='=' || (l->tk>=LEX_ASSIGNMENTS_BEGIN && l->tk<=LEX_ASSIGNMENTS_END) ) {
		if(!State.LeftHand)
			throw new CScriptException(ReferenceError, "invalid assignment left-hand side", l->currentFile, l->currentLine(), l->currentColumn());
		pushToken(State.Tokens);
		tokenizeAssignment(State, Flags);
		State.LeftHand = false;
	}
}
void CScriptTokenizer::tokenizeExpression(ScriptTokenState &State, int Flags) {
	tokenizeAssignment(State, Flags);
	while(l->tk == ',') {
		pushToken(State.Tokens);
		tokenizeAssignment(State, Flags);
		State.LeftHand = false;
	}
}
void CScriptTokenizer::tokenizeBlock(ScriptTokenState &State, int Flags) {
	bool addBlockStart = (Flags&TOKENIZE_FLAGS_noBlockStart)==0; 
	Flags&=~(TOKENIZE_FLAGS_noBlockStart);
	State.Marks.push_back(pushToken(State.Tokens, '{')); // push Token & push BeginIdx
	if(addBlockStart) pushForwarder(State);

	while(l->tk != '}' && l->tk != LEX_EOF) 
		tokenizeStatement(State, Flags);
	pushToken(State.Tokens, '}');

	if(addBlockStart) removeEmptyForwarder(State); // clean-up BlockStarts

	setTokenSkip(State);
}

void CScriptTokenizer::tokenizeStatementNoLet(ScriptTokenState &State, int Flags) {
	if(l->tk == LEX_R_LET)
		tokenizeLet(State, Flags | TOKENIZE_FLAGS_asStatement, true); 
	else if(l->tk==LEX_R_FUNCTION)
		tokenizeFunction(State, Flags | TOKENIZE_FLAGS_asStatement, true);
	else
		tokenizeStatement(State, Flags);
}
void CScriptTokenizer::tokenizeStatement(ScriptTokenState &State, int Flags) {
	int tk = l->tk;
	switch(l->tk)
	{
	case '{':				tokenizeBlock(State, Flags); break;
	case ';':				pushToken(State.Tokens); break;
	case LEX_R_CONST:
	case LEX_R_VAR:		tokenizeVarAndConst(State, Flags); break;
	case LEX_R_LET:		tokenizeLet(State, Flags | TOKENIZE_FLAGS_asStatement); break;
	case LEX_R_WITH:		tokenizeWith(State, Flags); break;
	case LEX_R_IF:			tokenizeIf(State, Flags); break;
	case LEX_R_SWITCH:	tokenizeSwitch(State, Flags); break;
	case LEX_R_DO:
	case LEX_R_WHILE:		tokenizeWhileAndDo(State, Flags); break;
	case LEX_R_FOR:		tokenizeFor(State, Flags); break;
	case LEX_R_FUNCTION:	tokenizeFunction(State, Flags | TOKENIZE_FLAGS_asStatement); break;
	case LEX_R_TRY:		tokenizeTry(State, Flags); break;
	case LEX_R_RETURN:
			if( (Flags & TOKENIZE_FLAGS_canReturn)==0) 
				throw new CScriptException(SyntaxError, "'return' statement, but not in a function.", l->currentFile, l->currentLine(), l->currentColumn());
	case LEX_R_THROW:	
		State.Marks.push_back(pushToken(State.Tokens)); // push Token & push BeginIdx
		if(l->tk != ';' && l->tk != '}' && !l->lineBreakBeforeToken) {
			if(tk==LEX_R_RETURN) State.HaveReturnValue = true;
			tokenizeExpression(State, Flags);
		}
		pushToken(State.Tokens, ';'); // push ';'
		setTokenSkip(State);
		break;
	case LEX_R_BREAK:		
	case LEX_R_CONTINUE:	
		{
			bool isBreak = l->tk == LEX_R_BREAK;
			State.Marks.push_back(pushToken(State.Tokens)); // push Token

			if(l->tk != ';' && !l->lineBreakBeforeToken) {
				l->check(LEX_ID);
				STRING_VECTOR_t &L = isBreak ? State.Labels : State.LoopLabels;
				if(find(L.begin(), L.end(), l->tkStr) == L.end())
					throw new CScriptException(SyntaxError, "label '"+l->tkStr+"' not found", l->currentFile, l->currentLine(), l->currentColumn());
				pushToken(State.Tokens); // push 'Label'
			} else if((Flags & (isBreak ? TOKENIZE_FLAGS_canBreak : TOKENIZE_FLAGS_canContinue) )==0) 
				throw new CScriptException(SyntaxError, 
											isBreak ? "'break' must be inside loop, switch or labeled statement" : "'continue' must be inside loop", 
											l->currentFile, l->currentLine(), l->currentColumn());
			pushToken(State.Tokens, ';'); // push ';'
			setTokenSkip(State);
		}
		break;
	case LEX_ID:
		{
			State.Marks.push_back(pushToken(State.Tokens, CScriptToken(LEX_T_SKIP))); // push skip & skiperBeginIdx
			STRING_VECTOR_t::size_type label_count = State.Labels.size();
			tokenizeExpression(State, Flags | TOKENIZE_FLAGS_canLabel); 
			if(label_count < State.Labels.size() && l->tk == ':') {
				State.Tokens.erase(State.Tokens.begin()+State.Marks.back()); // remove skip
				State.Marks.pop_back();
				pushToken(State.Tokens); // push ':'
				tokenizeStatement(State, Flags);
				State.Labels.pop_back();
			} else {
				pushToken(State.Tokens, ';');
				setTokenSkip(State);
			}
		}
		break;
	case 0:
		break;
	default: 
		State.Marks.push_back(pushToken(State.Tokens, CScriptToken(LEX_T_SKIP))); // push skip & skiperBeginIdx
		tokenizeExpression(State, Flags); 
		pushToken(State.Tokens, ';'); 
		setTokenSkip(State);
		break;
	}

}

int CScriptTokenizer::pushToken(TOKEN_VECT &Tokens, int Match, int Alternate) {
	if(Match == ';' && l->tk != ';' && (l->lineBreakBeforeToken || l->tk=='}' || l->tk==LEX_EOF))
		Tokens.push_back(CScriptToken(';')); // inject ';'
	else
		Tokens.push_back(CScriptToken(l, Match, Alternate));
	return Tokens.size()-1;
}
int CScriptTokenizer::pushToken(TOKEN_VECT &Tokens, const CScriptToken &Token) {
	int ret = Tokens.size();
	Tokens.push_back(Token);
	return ret;
}
void CScriptTokenizer::pushForwarder(TOKEN_VECT &Tokens, FORWARDER_VECTOR_t &Forwarders, vector<int> &Marks) {
	Marks.push_back(Tokens.size());
	CScriptToken token(LEX_T_FORWARD);
	Tokens.push_back(token);
	Forwarders.push_back(token.Forwarder());
}
void CScriptTokenizer::pushForwarder(ScriptTokenState &State, bool noMarks/*=false*/) {
	if(!noMarks) State.Marks.push_back(State.Tokens.size());
	CScriptToken token(LEX_T_FORWARD);
	State.Tokens.push_back(token);
	State.Forwarders.push_back(token.Forwarder());
}
void CScriptTokenizer::removeEmptyForwarder(ScriptTokenState &State)
{
	CScriptTokenDataForwardsPtr &forwarder = State.Forwarders.back();
	forwarder->vars_in_letscope.clear();
	if(forwarder->empty())
		State.Tokens.erase(State.Tokens.begin()+State.Marks.back());
	State.Forwarders.pop_back();
	State.Marks.pop_back();
}

void CScriptTokenizer::removeEmptyForwarder( TOKEN_VECT &Tokens, FORWARDER_VECTOR_t &Forwarders, vector<int> &Marks )
{
	CScriptTokenDataForwardsPtr &forwarder = Forwarders.back();
	forwarder->vars_in_letscope.clear();
	if(forwarder->empty())
		Tokens.erase(Tokens.begin()+Marks.back());
	Forwarders.pop_back();
	Marks.pop_back();
}
void CScriptTokenizer::throwTokenNotExpected() {
	throw new CScriptException(SyntaxError, "'"+CScriptToken::getTokenStr(l->tk)+"' was not expected", l->currentFile, l->currentLine(), l->currentColumn());
}


//////////////////////////////////////////////////////////////////////////
/// CScriptVar
//////////////////////////////////////////////////////////////////////////

CScriptVar::CScriptVar(CTinyJS *Context, const CScriptVarPtr &Prototype) {
	extensible = true;
	context = Context;
	memset(temporaryMark, 0, sizeof(temporaryMark));
	if(context->first) {
		next = context->first;
		next->prev = this;
	} else {
		next = 0;
	}
	context->first = this;
	prev = 0;
	refs = 0;
	if(Prototype)
		addChild(TINYJS___PROTO___VAR, Prototype, SCRIPTVARLINK_WRITABLE);
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
}
CScriptVar::CScriptVar(const CScriptVar &Copy) {
	extensible = Copy.extensible;
	context = Copy.context;
	memset(temporaryMark, 0, sizeof(temporaryMark));
	if(context->first) {
		next = context->first;
		next->prev = this;
	} else {
		next = 0;
	}
	context->first = this;
	prev = 0;
	refs = 0;
	SCRIPTVAR_CHILDS_cit it;
	for(it = Copy.Childs.begin(); it!= Copy.Childs.end(); ++it) {
		addChild((*it)->getName(), (*it)->getVarPtr(), (*it)->getFlags());
	}

#if DEBUG_MEMORY
	mark_allocated(this);
#endif
}
CScriptVar::~CScriptVar(void) {
#if DEBUG_MEMORY
	mark_deallocated(this);
#endif
	for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it)
		(*it)->setOwner(0);
	removeAllChildren();
	if(prev)
		prev->next = next;
	else
		context->first = next;
	if(next)
		next->prev = prev;
}

/// Type

bool CScriptVar::isObject()		{return false;}
bool CScriptVar::isError()			{return false;}
bool CScriptVar::isArray()			{return false;}
bool CScriptVar::isRegExp()		{return false;}
bool CScriptVar::isAccessor()		{return false;}
bool CScriptVar::isNull()			{return false;}
bool CScriptVar::isUndefined()	{return false;}
bool CScriptVar::isNaN()			{return false;}
bool CScriptVar::isString()		{return false;}
bool CScriptVar::isInt()			{return false;}
bool CScriptVar::isBool()			{return false;}
int CScriptVar::isInfinity()		{ return 0; } ///< +1==POSITIVE_INFINITY, -1==NEGATIVE_INFINITY, 0==is not an InfinityVar
bool CScriptVar::isDouble()		{return false;}
bool CScriptVar::isRealNumber()	{return false;}
bool CScriptVar::isNumber()		{return false;}
bool CScriptVar::isPrimitive()	{return false;}
bool CScriptVar::isFunction()		{return false;}
bool CScriptVar::isNative()		{return false;}
bool CScriptVar::isBounded()		{return false;}
bool CScriptVar::isIterator()		{return false;}
bool CScriptVar::isGenerator()	{return false;}

//////////////////////////////////////////////////////////////////////////
/// Value
//////////////////////////////////////////////////////////////////////////

CScriptVarPrimitivePtr CScriptVar::getRawPrimitive() {
	return CScriptVarPrimitivePtr(); // default NULL-Ptr
}
CScriptVarPrimitivePtr CScriptVar::toPrimitive() {
	return toPrimitive_hintNumber();
}

CScriptVarPrimitivePtr CScriptVar::toPrimitive(CScriptResult &execute) {
	return toPrimitive_hintNumber(execute);
} 

CScriptVarPrimitivePtr CScriptVar::toPrimitive_hintString(int32_t radix) {
	CScriptResult execute;
	CScriptVarPrimitivePtr var = toPrimitive_hintString(execute, radix);
	execute.cThrow();
	return var;
}
CScriptVarPrimitivePtr CScriptVar::toPrimitive_hintString(CScriptResult &execute, int32_t radix) {
	if(execute) {
		if(!isPrimitive()) {
			CScriptVarPtr ret = callJS_toString(execute, radix);
			if(execute && !ret->isPrimitive()) {
				ret = callJS_valueOf(execute);
				if(execute && !ret->isPrimitive())
					context->throwError(execute, TypeError, "can't convert to primitive type");
			}
			return ret;
		}
		return this;
	}
	return constScriptVar(Undefined);
}
CScriptVarPrimitivePtr CScriptVar::toPrimitive_hintNumber() {
	CScriptResult execute;
	CScriptVarPrimitivePtr var = toPrimitive_hintNumber(execute);
	execute.cThrow();
	return var;
}
CScriptVarPrimitivePtr CScriptVar::toPrimitive_hintNumber(CScriptResult &execute) {
	if(execute) {
		if(!isPrimitive()) {
			CScriptVarPtr ret = callJS_valueOf(execute);
			if(execute && !ret->isPrimitive()) {
				ret = callJS_toString(execute);
				if(execute && !ret->isPrimitive())
					context->throwError(execute, TypeError, "can't convert to primitive type");
			}
			return ret;
		}
		return this;
	}
	return constScriptVar(Undefined);
}

CScriptVarPtr CScriptVar::callJS_valueOf(CScriptResult &execute) {
	if(execute) {
		CScriptVarPtr FncValueOf = findChildWithPrototypeChain("valueOf").getter(execute);
		if(FncValueOf && FncValueOf != context->objectPrototype_valueOf) { // custom valueOf in JavaScript
			if(FncValueOf->isFunction()) { // no Error if toString not callable
				vector<CScriptVarPtr> Params;
				return context->callFunction(execute, FncValueOf, Params, this);
			}
		} else
			return valueOf_CallBack();
	}
	return this;
}
CScriptVarPtr CScriptVar::valueOf_CallBack() {
	return this;
}

CScriptVarPtr CScriptVar::callJS_toString(CScriptResult &execute, int radix/*=0*/) {
	if(execute) {
		CScriptVarPtr FncToString = findChildWithPrototypeChain("toString").getter(execute);
		if(FncToString && FncToString != context->objectPrototype_toString) { // custom valueOf in JavaScript
			if(FncToString->isFunction()) { // no Error if toString not callable
				vector<CScriptVarPtr> Params;
				Params.push_back(newScriptVar(radix));
				return context->callFunction(execute, FncToString, Params, this);
			}
		} else
			return toString_CallBack(execute, radix);
	}
	return this;
}
CScriptVarPtr CScriptVar::toString_CallBack(CScriptResult &execute, int radix/*=0*/) {
	return this;
}

CNumber CScriptVar::toNumber() { return toPrimitive_hintNumber()->toNumber_Callback(); };
CNumber CScriptVar::toNumber(CScriptResult &execute) { return toPrimitive_hintNumber(execute)->toNumber_Callback(); };
bool CScriptVar::toBoolean() { return true; }
string CScriptVar::toString(int32_t radix) { return toPrimitive_hintString(radix)->toCString(radix); }
string CScriptVar::toString(CScriptResult &execute, int32_t radix/*=0*/) { return toPrimitive_hintString(execute, radix)->toCString(radix); }

int CScriptVar::getInt() { return toNumber().toInt32(); }
double CScriptVar::getDouble() { return toNumber().toDouble(); } 
bool CScriptVar::getBool() { return toBoolean(); }
string CScriptVar::getString() { return toPrimitive_hintString()->toCString(); }

CScriptTokenDataFnc *CScriptVar::getFunctionData() { return 0; }

CScriptVarPtr CScriptVar::toIterator(int Mode/*=3*/) {
	CScriptResult execute;
	CScriptVarPtr var = toIterator(execute, Mode);
	execute.cThrow();
	return var;
}
CScriptVarPtr CScriptVar::toIterator(CScriptResult &execute, int Mode/*=3*/) {
	if(!execute) return constScriptVar(Undefined);
	if(isIterator()) return this;
	CScriptVarFunctionPtr Generator(findChildWithPrototypeChain("__iterator__").getter(execute));
	vector<CScriptVarPtr> args;
	if(Generator) return context->callFunction(execute, Generator, args, this);
	return newScriptVarDefaultIterator(context, this, Mode);
}

string CScriptVar::getParsableString() {
	uint32_t UniqueID = context->allocUniqueID();
	bool hasRecursion=false;
	string ret = getParsableString("", "   ", UniqueID, hasRecursion);
	context->freeUniqueID();
	return ret;
}

string CScriptVar::getParsableString(const string &indentString, const string &indent, uint32_t uniqueID, bool &hasRecursion) {
	getParsableStringRecursionsCheck();
	return indentString+toString();
}

CScriptVarPtr CScriptVar::getNumericVar() { return newScriptVar(toNumber()); }

////// Flags

void CScriptVar::seal() {
	preventExtensions(); 
	for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it)
		(*it)->setConfigurable(false);
}
bool CScriptVar::isSealed() const {
	if(isExtensible()) return false; 
	for(SCRIPTVAR_CHILDS_cit it = Childs.begin(); it != Childs.end(); ++it)
		if((*it)->isConfigurable()) return false;
	return true;
}
void CScriptVar::freeze() {
	preventExtensions(); 
	for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it)
		(*it)->setConfigurable(false), (*it)->setWritable(false);
}
bool CScriptVar::isFrozen() const {
	if(isExtensible()) return false; 
	for(SCRIPTVAR_CHILDS_cit it = Childs.begin(); it != Childs.end(); ++it)
		if((*it)->isConfigurable() || (*it)->isWritable()) return false;
	return true;
}

////// Childs

CScriptVarPtr CScriptVar::getOwnPropertyDescriptor(const string &Name) {
	CScriptVarLinkPtr child = findChild(Name);
	if(!child) {
		CScriptVarStringPtr strVar = getRawPrimitive();
		uint32_t Idx;
		if (strVar && (Idx=isArrayIndex(Name))!=uint32_t(-1) && Idx<strVar->stringLength()) {
			int Char = strVar->getChar(Idx);
			CScriptVarPtr ret = newScriptVar(Object);
			ret->addChild("value", newScriptVar(string(1, (char)Char)));
			ret->addChild("writable", constScriptVar(false));
			ret->addChild("enumerable", constScriptVar(true));
			ret->addChild("configurable", constScriptVar(false));
			return ret;
		}
	}

	if(!child || child->getVarPtr()->isUndefined()) return constScriptVar(Undefined);
	CScriptVarPtr ret = newScriptVar(Object);
	if(child->getVarPtr()->isAccessor()) {
		CScriptVarLinkPtr value = child->getVarPtr()->findChild(TINYJS_ACCESSOR_GET_VAR);
		ret->addChild("get", value ? value->getVarPtr() : constScriptVar(Undefined));
		value = child->getVarPtr()->findChild(TINYJS_ACCESSOR_SET_VAR);
		ret->addChild("set", value ? value->getVarPtr() : constScriptVar(Undefined));
	} else {
		ret->addChild("value", child->getVarPtr()->valueOf_CallBack());
		ret->addChild("writable", constScriptVar(child->isWritable()));
	}
	ret->addChild("enumerable", constScriptVar(child->isEnumerable()));
	ret->addChild("configurable", constScriptVar(child->isConfigurable()));
	return ret;
}
const char *CScriptVar::defineProperty(const string &Name, CScriptVarPtr Attributes) {
	CScriptVarPtr attr;
	CScriptVarLinkPtr child = findChildWithStringChars(Name);

	CScriptVarPtr attr_value			= Attributes->findChild("value");
	CScriptVarPtr attr_writable		= Attributes->findChild("writable");
	CScriptVarPtr attr_get				= Attributes->findChild("get");
	CScriptVarPtr attr_set				= Attributes->findChild("set");
	CScriptVarPtr attr_enumerable		= Attributes->findChild("enumerable");
	CScriptVarPtr attr_configurable	= Attributes->findChild("configurable");
	bool attr_isDataDescriptor			= !attr_get && !attr_set;
	if(!attr_isDataDescriptor && (attr_value || attr_writable)) return "property descriptors must not specify a value or be writable when a getter or setter has been specified";
	if(attr_isDataDescriptor) {
		if(attr_get && (!attr_get->isUndefined() || !attr_get->isFunction())) return "property descriptor's getter field is neither undefined nor a function";
		if(attr_set && (!attr_set->isUndefined() || !attr_set->isFunction())) return "property descriptor's setter field is neither undefined nor a function";
	}
	if(!child) {
		if(!isExtensible()) return "is not extensible";
		if(attr_isDataDescriptor) {
			child = addChild(Name, attr_value?attr_value:constScriptVar(Undefined), 0);
			if(attr_writable) child->setWritable(attr_writable->toBoolean());
		} else {
			child = addChild(Name, newScriptVarAccessor(context, attr_get, attr_set), SCRIPTVARLINK_WRITABLE);
		}
	} else {
		if(!child->isConfigurable()) {
			if(attr_configurable && attr_configurable->toBoolean()) goto cant_redefine;
			if(attr_enumerable && attr_enumerable->toBoolean() != child->isEnumerable()) goto cant_redefine;
			if(child->getVarPtr()->isAccessor()) {
				if(attr_isDataDescriptor) goto cant_redefine;
				if(attr_get && attr_get != child->getVarPtr()->findChild(TINYJS_ACCESSOR_GET_VAR)) goto cant_redefine;
				if(attr_set && attr_set != child->getVarPtr()->findChild(TINYJS_ACCESSOR_SET_VAR)) goto cant_redefine;
			} else if(!attr_isDataDescriptor) goto cant_redefine;
			else if(!child->isWritable()) {
				if(attr_writable && attr_writable->toBoolean()) goto cant_redefine;
				if(attr_value && !attr_value->mathsOp(child, LEX_EQUAL)->toBoolean()) goto cant_redefine;
			}
		}
		if(attr_isDataDescriptor) {
			if(child->getVarPtr()->isAccessor()) child->setWritable(false);
			child->setVarPtr(attr_value?attr_value:constScriptVar(Undefined));
			if(attr_writable) child->setWritable(attr_writable->toBoolean());
		} else {
			if(child->getVarPtr()->isAccessor()) {
				if(!attr_get) attr_get = child->getVarPtr()->findChild(TINYJS_ACCESSOR_GET_VAR);
				if(!attr_set) attr_set = child->getVarPtr()->findChild(TINYJS_ACCESSOR_SET_VAR);
			}
			child->setVarPtr(newScriptVarAccessor(context, attr_get, attr_set));
			child->setWritable(true);
		}
	}
	if(attr_enumerable) child->setEnumerable(attr_enumerable->toBoolean());
	if(attr_configurable) child->setConfigurable(attr_configurable->toBoolean());
	return 0;
cant_redefine:
	return "can't redefine non-configurable property";
}

CScriptVarLinkPtr CScriptVar::findChild(const string &childName) {
	if(Childs.empty()) return 0;
	SCRIPTVAR_CHILDS_it it = lower_bound(Childs.begin(), Childs.end(), childName);
	if(it != Childs.end() && (*it)->getName() == childName)
		return *it;
	return 0;
}

CScriptVarLinkWorkPtr CScriptVar::findChildWithStringChars(const string &childName) {
	CScriptVarLinkWorkPtr child = findChild(childName);
	if(child) return child;
	CScriptVarStringPtr strVar = getRawPrimitive();
	uint32_t Idx;
	if (strVar && (Idx=isArrayIndex(childName))!=uint32_t(-1)) {
		if (Idx<strVar->stringLength()) {
			int Char = strVar->getChar(Idx);
			child(newScriptVar(string(1, (char)Char)), childName, SCRIPTVARLINK_ENUMERABLE);
		} else {
			child(constScriptVar(Undefined), childName, SCRIPTVARLINK_ENUMERABLE);
		}
		child.setReferencedOwner(this); // fake referenced Owner
		return child;
	}
	return 0;
}

CScriptVarLinkPtr CScriptVar::findChildInPrototypeChain(const string &childName) {
	unsigned int uniqueID = context->allocUniqueID();
	// Look for links to actual parent classes
	CScriptVarPtr object = this;
	CScriptVarLinkPtr __proto__;
	while( object->getTemporaryMark() != uniqueID && (__proto__ = object->findChild(TINYJS___PROTO___VAR)) ) {
		CScriptVarLinkPtr implementation = __proto__->getVarPtr()->findChild(childName);
		if (implementation) {
			context->freeUniqueID();
			return implementation;
		}
		object->setTemporaryMark(uniqueID); // prevents recursions
		object = __proto__;
	}
	context->freeUniqueID();
	return 0;
}

CScriptVarLinkWorkPtr CScriptVar::findChildWithPrototypeChain(const string &childName) {
	CScriptVarLinkWorkPtr child = findChildWithStringChars(childName);
	if(child) return child;
	child = findChildInPrototypeChain(childName);
	if(child) {
		child(child->getVarPtr(), child->getName(), child->getFlags()); // recreate implementation
		child.setReferencedOwner(this); // fake referenced Owner
	}
	return child;
}
CScriptVarLinkPtr CScriptVar::findChildByPath(const string &path) {
	string::size_type p = path.find('.');
	CScriptVarLinkPtr child;
	if (p == string::npos)
		return findChild(path);
	if( (child = findChild(path.substr(0,p))) )
		return child->getVarPtr()->findChildByPath(path.substr(p+1));
	return 0;
}

CScriptVarLinkPtr CScriptVar::findChildOrCreate(const string &childName/*, int varFlags*/) {
	CScriptVarLinkPtr l = findChild(childName);
	if (l) return l;
	return addChild(childName, constScriptVar(Undefined));
	//	return addChild(childName, new CScriptVar(context, TINYJS_BLANK_DATA, varFlags));
}

CScriptVarLinkPtr CScriptVar::findChildOrCreateByPath(const string &path) {
	string::size_type p = path.find('.');
	if (p == string::npos)
		return findChildOrCreate(path);
	string childName(path, 0, p);
	CScriptVarLinkPtr l = findChild(childName);
	if (!l) l = addChild(childName, newScriptVar(Object));
	return l->getVarPtr()->findChildOrCreateByPath(path.substr(p+1));
}

void CScriptVar::keys(set<string> &Keys, bool OnlyEnumerable/*=true*/, uint32_t ID/*=0*/)
{
	if(ID) setTemporaryMark(ID);
	for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
		if(!OnlyEnumerable || (*it)->isEnumerable())
			Keys.insert((*it)->getName());
	}
	CScriptVarStringPtr isStringObj = this->getRawPrimitive();
	if(isStringObj) {
		uint32_t length = isStringObj->stringLength();
		for(uint32_t i=0; i<length; ++i)
			Keys.insert(int2string(i));
	}
	CScriptVarLinkPtr __proto__;
	if( ID && (__proto__ = findChild(TINYJS___PROTO___VAR)) && __proto__->getVarPtr()->getTemporaryMark() != ID )
		__proto__->getVarPtr()->keys(Keys, OnlyEnumerable, ID);
}

/// add & remove
CScriptVarLinkPtr CScriptVar::addChild(const string &childName, const CScriptVarPtr &child, int linkFlags /*= SCRIPTVARLINK_DEFAULT*/) {
	CScriptVarLinkPtr link;
	SCRIPTVAR_CHILDS_it it = lower_bound(Childs.begin(), Childs.end(), childName);
	if(it == Childs.end() || (*it)->getName() != childName) {
		link = CScriptVarLinkPtr(child?child:constScriptVar(Undefined), childName, linkFlags);
		link->setOwner(this);

		Childs.insert(it, 1, link);
#ifdef _DEBUG
	} else {
		ASSERT(0); // addChild - the child exists 
#endif
	}
	return link;
}
CScriptVarLinkPtr CScriptVar::addChildNoDup(const string &childName, const CScriptVarPtr &child, int linkFlags /*= SCRIPTVARLINK_DEFAULT*/) { 
	return addChildOrReplace(childName, child, linkFlags); 
}
CScriptVarLinkPtr CScriptVar::addChildOrReplace(const string &childName, const CScriptVarPtr &child, int linkFlags /*= SCRIPTVARLINK_DEFAULT*/) {
	SCRIPTVAR_CHILDS_it it = lower_bound(Childs.begin(), Childs.end(), childName);
	if(it == Childs.end() || (*it)->getName() != childName) {
		CScriptVarLinkPtr link(child, childName, linkFlags);
		link->setOwner(this);
		Childs.insert(it, 1, link);
		return link;
	} else {
		(*it)->setVarPtr(child);
		return (*it);
	}
}

bool CScriptVar::removeLink(CScriptVarLinkPtr &link) {
	if (!link) return false;
	SCRIPTVAR_CHILDS_it it = lower_bound(Childs.begin(), Childs.end(), link->getName());
	if(it != Childs.end() && (*it) == link) {
		Childs.erase(it);
#ifdef _DEBUG
	} else {
		ASSERT(0); // removeLink - the link is not atached to this var 
#endif
	}
	link.clear();
	return true;
}
void CScriptVar::removeAllChildren() {
	Childs.clear();
}

CScriptVarPtr CScriptVar::getArrayIndex(uint32_t idx) {
	CScriptVarLinkPtr link = findChild(int2string(idx));
	if (link) return link;
	else return constScriptVar(Undefined); // undefined
}

void CScriptVar::setArrayIndex(uint32_t idx, const CScriptVarPtr &value) {
	string sIdx = int2string(idx);
	CScriptVarLinkPtr link = findChild(sIdx);

	if (link) {
		link->setVarPtr(value);
	} else {
		addChild(sIdx, value);
	}
}

uint32_t CScriptVar::getArrayLength() {
	if (!isArray() || Childs.size()==0) return 0;
	return isArrayIndex(Childs.back()->getName())+1; 
}

CScriptVarPtr CScriptVar::mathsOp(const CScriptVarPtr &b, int op) {
	CScriptResult execute;
	return context->mathsOp(execute, this, b, op);
}

void CScriptVar::trace(const string &name) {
	string indentStr;
	uint32_t uniqueID = context->allocUniqueID();
	trace(indentStr, uniqueID, name);
	context->freeUniqueID();
}
void CScriptVar::trace(string &indentStr, uint32_t uniqueID, const string &name) {
	string indent = "  ";
	const char *extra="";
	if(getTemporaryMark() == uniqueID)
		extra = " recursion detected";
	TRACE("%s'%s' = '%s' %s%s\n",
		indentStr.c_str(),
		name.c_str(),
		toString().c_str(),
		getFlagsAsString().c_str(),
		extra);
	if(getTemporaryMark() != uniqueID) {
		setTemporaryMark(uniqueID);
		indentStr+=indent;
		for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
			if((*it)->isEnumerable())
				(*it)->getVarPtr()->trace(indentStr, uniqueID, (*it)->getName());
		}
		indentStr = indentStr.substr(0, indentStr.length()-2);
	}
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
	if (isRegExp()) flagstr = flagstr + "REGEXP ";
	if (isNaN()) flagstr = flagstr + "NaN ";
	if (isInfinity()) flagstr = flagstr + "INFINITY ";
	return flagstr;
}

CScriptVar *CScriptVar::ref() {
	refs++;
	return this;
}
void CScriptVar::unref() {
	refs--;
	ASSERT(refs>=0); // printf("OMFG, we have unreffed too far!\n");
	if (refs==0)
		delete this;
}

int CScriptVar::getRefs() {
	return refs;
}

void CScriptVar::setTemporaryMark_recursive(uint32_t ID)
{
	if(getTemporaryMark() != ID) {
		setTemporaryMark(ID);
		for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
			(*it)->getVarPtr()->setTemporaryMark_recursive(ID);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
/// CScriptVarLink
//////////////////////////////////////////////////////////////////////////

CScriptVarLink::CScriptVarLink(const CScriptVarPtr &Var, const string &Name /*=TINYJS_TEMP_NAME*/, int Flags /*=SCRIPTVARLINK_DEFAULT*/) 
	: name(Name), owner(0), flags(Flags), refs(0) {
#if DEBUG_MEMORY
	mark_allocated(this);
#endif
	var = Var;
}

CScriptVarLink::~CScriptVarLink() {
#if DEBUG_MEMORY
	mark_deallocated(this);
#endif
}

CScriptVarLink *CScriptVarLink::ref() {
	refs++;
	return this;
}
void CScriptVarLink::unref() {
	refs--;
	ASSERT(refs>=0); // printf("OMFG, we have unreffed too far!\n");
	if (refs==0)
		delete this;
}


//////////////////////////////////////////////////////////////////////////
/// CScriptVarLinkPtr
//////////////////////////////////////////////////////////////////////////

CScriptVarLinkPtr & CScriptVarLinkPtr::operator()( const CScriptVarPtr &var, const std::string &name /*= TINYJS_TEMP_NAME*/, int flags /*= SCRIPTVARLINK_DEFAULT*/ ) {
	if(link && link->refs == 1) { // the link is only refered by this
		link->name = name;
		link->owner = 0;
		link->flags = flags;
		link->var = var;
	} else {
		if(link) link->unref();
		link = (new CScriptVarLink(var, name, flags))->ref();
	} 
	return *this;
}

CScriptVarLinkWorkPtr CScriptVarLinkPtr::getter() {
	return CScriptVarLinkWorkPtr(*this).getter();
}

CScriptVarLinkWorkPtr CScriptVarLinkPtr::getter( CScriptResult &execute ) {
	return CScriptVarLinkWorkPtr(*this).getter(execute);
}

CScriptVarLinkWorkPtr CScriptVarLinkPtr::setter( const CScriptVarPtr &Var ) {
	return CScriptVarLinkWorkPtr(*this).setter(Var);
}

CScriptVarLinkWorkPtr CScriptVarLinkPtr::setter( CScriptResult &execute, const CScriptVarPtr &Var ) {
	return CScriptVarLinkWorkPtr(*this).setter(execute, Var);
}

bool CScriptVarLinkPtr::operator <(const string &rhs) const {
	uint32_t lhs_int = isArrayIndex(link->getName());
	uint32_t rhs_int = isArrayIndex(rhs);
	if(lhs_int==uint32_t(-1)) {
		if(rhs_int==uint32_t(-1)) 
			return link->getName() < rhs;
		else 
			return true;
	} else if(rhs_int==uint32_t(-1)) 
		return false;
	return lhs_int < rhs_int;
}


//////////////////////////////////////////////////////////////////////////
/// CScriptVarLinkWorkPtr
//////////////////////////////////////////////////////////////////////////

CScriptVarLinkWorkPtr CScriptVarLinkWorkPtr::getter() {
	if(link && link->getVarPtr()) {
		CScriptResult execute;
		CScriptVarPtr ret = getter(execute);
		execute.cThrow();
		return ret;
	}
	return *this;
}
CScriptVarLinkWorkPtr CScriptVarLinkWorkPtr::getter(CScriptResult &execute) {
	if(execute && link && link->getVarPtr() && link->getVarPtr()->isAccessor()) {
		const CScriptVarPtr &var = link->getVarPtr();
		CScriptVarLinkPtr getter = var->findChild(TINYJS_ACCESSOR_GET_VAR);
		if(getter) {
			vector<CScriptVarPtr> Params;
			ASSERT(getReferencedOwner());
			return getter->getVarPtr()->getContext()->callFunction(execute, getter->getVarPtr(), Params, getReferencedOwner());
		} else
			return var->constScriptVar(Undefined);
	} else
		return *this;
}
CScriptVarLinkWorkPtr CScriptVarLinkWorkPtr::setter( const CScriptVarPtr &Var ) {
	if(link && link->getVarPtr()) {
		CScriptResult execute;
		CScriptVarPtr ret = setter(execute, Var);
		execute.cThrow();
		return ret;
	}
	return *this;
}

CScriptVarLinkWorkPtr CScriptVarLinkWorkPtr::setter( CScriptResult &execute, const CScriptVarPtr &Var ) {
	if(execute) {
		if(link) {
			if(link->getVarPtr() && link->getVarPtr()->isAccessor()) {
				const CScriptVarPtr &var = link->getVarPtr();
				CScriptVarLinkPtr setter = var->findChild(TINYJS_ACCESSOR_SET_VAR);
				if(setter) {
					vector<CScriptVarPtr> Params;
					Params.push_back(Var);
					ASSERT(getReferencedOwner());
					setter->getVarPtr()->getContext()->callFunction(execute, setter->getVarPtr(), Params, getReferencedOwner());
				}
			} else
				link->setVarPtr(Var);
		}
	}
	return *this;
}


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarPrimitive
//////////////////////////////////////////////////////////////////////////

CScriptVarPrimitive::~CScriptVarPrimitive(){}

bool CScriptVarPrimitive::isPrimitive() { return true; }
CScriptVarPrimitivePtr CScriptVarPrimitive::getRawPrimitive() { return this; }
bool CScriptVarPrimitive::toBoolean() { return false; }
CScriptVarPtr CScriptVarPrimitive::toObject() { return this; }
CScriptVarPtr CScriptVarPrimitive::toString_CallBack( CScriptResult &execute, int radix/*=0*/ ) {
	return newScriptVar(toCString(radix));
}


////////////////////////////////////////////////////////////////////////// 
// CScriptVarUndefined
//////////////////////////////////////////////////////////////////////////

declare_dummy_t(Undefined);
CScriptVarUndefined::CScriptVarUndefined(CTinyJS *Context) : CScriptVarPrimitive(Context, Context->objectPrototype) { }
CScriptVarUndefined::~CScriptVarUndefined() {}
CScriptVarPtr CScriptVarUndefined::clone() { return new CScriptVarUndefined(*this); }
bool CScriptVarUndefined::isUndefined() { return true; }

CNumber CScriptVarUndefined::toNumber_Callback() { return NaN; }
string CScriptVarUndefined::toCString(int radix/*=0*/) { return "undefined"; }
string CScriptVarUndefined::getVarType() { return "undefined"; }


////////////////////////////////////////////////////////////////////////// 
// CScriptVarNull
//////////////////////////////////////////////////////////////////////////

declare_dummy_t(Null);
CScriptVarNull::CScriptVarNull(CTinyJS *Context) : CScriptVarPrimitive(Context, Context->objectPrototype) { }
CScriptVarNull::~CScriptVarNull() {}
CScriptVarPtr CScriptVarNull::clone() { return new CScriptVarNull(*this); }
bool CScriptVarNull::isNull() { return true; }

CNumber CScriptVarNull::toNumber_Callback() { return 0; }
string CScriptVarNull::toCString(int radix/*=0*/) { return "null"; }
string CScriptVarNull::getVarType() { return "null"; }


//////////////////////////////////////////////////////////////////////////
/// CScriptVarString
//////////////////////////////////////////////////////////////////////////

CScriptVarString::CScriptVarString(CTinyJS *Context, const string &Data) : CScriptVarPrimitive(Context, Context->stringPrototype), data(Data) {
	addChild("length", newScriptVar(data.size()), SCRIPTVARLINK_CONSTANT);
/*
	CScriptVarLinkPtr acc = addChild("length", newScriptVar(Accessor), 0);
	CScriptVarFunctionPtr getter(::newScriptVar(Context, this, &CScriptVarString::native_Length, 0));
	getter->setFunctionData(new CScriptTokenDataFnc);
	acc->getVarPtr()->addChild(TINYJS_ACCESSOR_GET_VAR, getter, 0);
*/
}
CScriptVarString::~CScriptVarString() {}
CScriptVarPtr CScriptVarString::clone() { return new CScriptVarString(*this); }
bool CScriptVarString::isString() { return true; }

bool CScriptVarString::toBoolean() { return data.length()!=0; }
CNumber CScriptVarString::toNumber_Callback() { return data.c_str(); }
string CScriptVarString::toCString(int radix/*=0*/) { return data; }

string CScriptVarString::getParsableString(const string &indentString, const string &indent, uint32_t uniqueID, bool &hasRecursion) { return indentString+getJSString(data); }
string CScriptVarString::getVarType() { return "string"; }

CScriptVarPtr CScriptVarString::toObject() { 
	CScriptVarPtr ret = newScriptVar(CScriptVarPrimitivePtr(this), context->stringPrototype); 
	ret->addChild("length", newScriptVar(data.size()), SCRIPTVARLINK_CONSTANT);
	return ret;
}

CScriptVarPtr CScriptVarString::toString_CallBack( CScriptResult &execute, int radix/*=0*/ ) {
	return this;
}

int CScriptVarString::getChar(uint32_t Idx) {
	if((string::size_type)Idx >= data.length())
		return -1;
	else
		return (unsigned char)data[Idx];
}


//////////////////////////////////////////////////////////////////////////
/// CNumber
//////////////////////////////////////////////////////////////////////////

NegativeZero_t NegativeZero;
declare_dummy_t(NaN);
Infinity InfinityPositive(1);
Infinity InfinityNegative(-1);
#if 1
static inline bool _isNaN(volatile double *Value1, volatile double *Value2) {
	return !(*Value1==*Value2);
}

static inline bool isNaN(double Value) {
	return _isNaN(&Value, &Value);
}
inline bool isNegZero(double d) {
	double x=-0.0;
	return memcmp(&d, &x, sizeof(double))==0;
}

CNumber &CNumber::operator=(double Value) { 
	double integral;
	if(isNegZero(Value))
		type=tnNULL, Int32=0;
	else if(numeric_limits<double>::has_infinity && Value == numeric_limits<double>::infinity())
		type=tInfinity, Int32=1; 
	else if(numeric_limits<double>::has_infinity && Value == -numeric_limits<double>::infinity())
		type=tInfinity, Int32=-1; 
	else if(::isNaN(Value) || Value == numeric_limits<double>::quiet_NaN() || Value == std::numeric_limits<double>::signaling_NaN())
		type=tNaN, Int32=0; 
	else if(modf(Value, &integral)==0.0 && numeric_limits<int32_t>::min()<=integral && integral<=numeric_limits<int32_t>::max()) 
		type=tInt32, Int32=int32_t(integral);
	else 
		type=tDouble, Double=Value; 
	return *this; 
}
CNumber &CNumber::operator=(const char *str) {
	
	while(isWhitespace(*str)) str++;
	const char *start = str, *endptr;
	if(*str == '-' || *str == '+') str++;
	if(*str == '0' && ( str[1]=='x' || str[1]=='X'))
		parseInt(start, 16, &endptr);
	else if(*str == '0' && str[1]>='0' && str[1]<='7')
		parseInt(start, 8, &endptr);
	else
		parseFloat(start, &endptr);
	while(isWhitespace(*endptr)) endptr++;
	if(*endptr != '\0')
		type=tNaN, Int32=0;
	return *this;
}
int32_t CNumber::parseInt(const char * str, int32_t radix/*=0*/, const char **endptr/*=0*/) {
	type=tInt32, Int32=0;
	if(endptr) *endptr = str;
	bool stripPrefix = false; //< is true if radix==0 or radix==16
	if(radix == 0) {
		radix=10;
		stripPrefix = true;
	} else if(radix < 2 || radix > 36) {
		type = tNaN;
		return 0;
	} else
		stripPrefix = radix == 16;
	while(isWhitespace(*str)) str++;
	int sign=1;
	if(*str=='-') sign=-1,str++;
	else if(*str=='+') str++;
	if(stripPrefix && *str=='0' && (str[1]=='x' || str[1]=='X')) str+=2, radix=16;
	else if(stripPrefix && *str=='0' && str[1]>='0' && str[1]<='7') str+=1, radix=8;
	int32_t max = 0x7fffffff/radix;
	const char *start = str;
	for( ; *str; str++) {
		if(*str >= '0' && *str <= '0'-1+radix) Int32 = Int32*radix+*str-'0';
		else if(*str>='a' && *str<='a'-11+radix) Int32 = Int32*radix+*str-'a'+10;
		else if(*str>='A' && *str<='A'-11+radix) Int32 = Int32*radix+*str-'A'+10;
		else break;
		if(Int32 >= max) {
			type=tDouble, Double=double(Int32);
			for(str++ ; *str; str++) {
				if(*str >= '0' && *str <= '0'-1+radix) Double = Double *radix+*str-'0';
				else if(*str>='a' && *str<='a'-11+radix) Double = Double *radix+*str-'a'+10;
				else if(*str>='A' && *str<='A'-11+radix) Double = Double *radix+*str-'A'+10;
				else break;
			}
			break;
		}
	}
	if(str == start) {
		type= tNaN;
		return 0;
	}
	if(sign<0 && ((type==tInt32 && Int32==0) || (type==tDouble && Double==0.0))) { type=tnNULL,Int32=0; return radix; }
	if(type==tInt32) operator=(sign<0 ? -Int32 : Int32);
	else operator=(sign<0 ? -Double : Double);
	if(endptr) *endptr = (char*)str;
	return radix;
}

void CNumber::parseFloat(const char * str, const char **endptr/*=0*/) {
	type=tInt32, Int32=0;
	if(endptr) *endptr = str;
	while(isWhitespace(*str)) str++;
	int sign=1;
	if(*str=='-') sign=-1,str++;
	else if(*str=='+') str++;
	if(strncmp(str, "Infinity", 8) == 0) { type=tInfinity, Int32=sign; return; }
	double d = strtod(str, (char**)endptr);
	operator=(sign>0 ? d : -d);
	return;
}

CNumber CNumber::add(const CNumber &Value) const {
	if(type==tNaN || Value.type==tNaN) 
		return CNumber(tNaN);
	else if(type==tInfinity || Value.type==tInfinity) {
		if(type!=tInfinity)
			return Value;
		else if(Value.type!=tInfinity || sign()==Value.sign())
			return *this;
		else
			return CNumber(tNaN);
	} else if(type==tnNULL)
		return Value;
	else if(Value.type==tnNULL)
		return *this;
	else if(type==tDouble || Value.type==tDouble)
		return CNumber(toDouble()+Value.toDouble());
	else {
		int32_t range_max = numeric_limits<int32_t>::max();
		int32_t range_min = numeric_limits<int32_t>::min();
		if(Int32>0) range_max-=Int32;
		else if(Int32<0) range_min-=Int32;
		if(range_min<=Value.Int32 && Value.Int32<=range_max)
			return CNumber(Int32+Value.Int32);
		else
			return CNumber(double(Int32)+double(Value.Int32));
	}
}

CNumber CNumber::operator-() const {
	switch(type) {
	case tInt32:
		if(Int32==0)
			return CNumber(NegativeZero);
	case tnNULL:
		return CNumber(-Int32);
	case tDouble:
		return CNumber(-Double);
	case tInfinity:
		return CNumber(tInfinity, -Int32);
	default:
		return CNumber(tNaN);
	}
}

static inline int bits(uint32_t Value) {
	uint32_t b=0, mask=0xFFFF0000UL;
	for(int shift=16; shift>0 && Value!=0; shift>>=1, mask>>=shift) {
		if(Value & mask) {
			b += shift;
			Value>>=shift;
		}
	}
	return b;
}
static inline int bits(int32_t Value) {
	return bits(uint32_t(Value<0?-Value:Value));
}

CNumber CNumber::multi(const CNumber &Value) const {
	if(type==tNaN || Value.type==tNaN)
		return CNumber(tNaN);
	else if(type==tInfinity || Value.type==tInfinity) {
		if(isZero() || Value.isZero())
			return CNumber(tNaN);
		else 
			return CNumber(tInfinity, sign()==Value.sign()?1:-1);
	} else if(isZero() || Value.isZero()) {
		if(sign()==Value.sign())
			return CNumber(0);
		else
			return CNumber(NegativeZero);
	} else if(type==tDouble || Value.type==tDouble)
		return CNumber(toDouble()*Value.toDouble());
	else {
		// Int32*Int32
		if(bits(Int32)+bits(Value.Int32) <= 29)
			return CNumber(Int32*Value.Int32);
		else
			return CNumber(double(Int32)*double(Value.Int32));
	}
}


CNumber CNumber::div( const CNumber &Value ) const {
	if(type==tNaN || Value.type==tNaN) return CNumber(tNaN);
	int Sign = sign()*Value.sign();
	if(type==tInfinity) {
		if(Value.type==tInfinity) return CNumber(tNaN);
		else return CNumber(tInfinity, Sign);
	}
	if(Value.type==tInfinity) {
		if(Sign<0) return CNumber(NegativeZero);
		else return CNumber(0);
	} else if(Value.isZero()) {
		if(isZero()) return CNumber(tNaN);
		else return CNumber(tInfinity, Sign);
	} else
		return CNumber(toDouble() / Value.toDouble());
}

CNumber CNumber::modulo( const CNumber &Value ) const {
	if(type==tNaN || type==tInfinity || Value.type==tNaN || Value.isZero()) return CNumber(tNaN);
	if(Value.type==tInfinity) return CNumber(*this);
	if(isZero()) return CNumber(0);
	if(type==tDouble || Value.type==tDouble || (Int32==numeric_limits<int32_t>::min() && Value == -1) /* use double to prevent integer overflow */ ) {
		double n = toDouble(), d = Value.toDouble(), q;
		modf(n/d, &q);
		return CNumber(n - (d * q));
	} else
		return CNumber(Int32 % Value.Int32);
}

CNumber CNumber::round() const {
	if(type != tDouble) return CNumber(*this);
	if(Double < 0.0 && Double >= -0.5)
		return CNumber(NegativeZero);
	return CNumber(::floor(Double+0.5));
}

CNumber CNumber::floor() const {
	if(type != tDouble) return CNumber(*this);
	return CNumber(::floor(Double));
}

CNumber CNumber::ceil() const {
	if(type != tDouble) return CNumber(*this);
	return CNumber(::ceil(Double));
}

CNumber CNumber::abs() const {
	if(sign()<0) return -CNumber(*this);
	else return CNumber(*this);
}

CNumber CNumber::shift(const CNumber &Value, bool Right) const {
	int32_t lhs = toInt32();
	uint32_t rhs = Value.toUInt32() & 0x1F;
	return CNumber(Right ? lhs>>rhs : lhs<<rhs);
}

CNumber CNumber::ushift(const CNumber &Value, bool Right) const {
	uint32_t lhs = toUInt32();
	uint32_t rhs = Value.toUInt32() & 0x1F;
	return CNumber(Right ? lhs>>rhs : lhs<<rhs);
}

CNumber CNumber::binary(const CNumber &Value, char Mode) const {
	int32_t lhs = toInt32();
	int32_t rhs = Value.toInt32();

	switch(Mode) {
	case '&':	lhs&=rhs; break;
	case '|':	lhs|=rhs; break;
	case '^':	lhs^=rhs; break;
	}
	return CNumber(lhs);
}


int CNumber::less( const CNumber &Value ) const {
	if(type==tNaN || Value.type==tNaN) return 0;
	else if(type==tInfinity) {
		if(Value.type==tInfinity) return Int32<Value.Int32 ? 1 : -1;
		return -Int32;
	} else if(Value.type==tInfinity) 
		return Value.Int32;
	else if(isZero() && Value.isZero()) return -1;
	else if(type==tDouble || Value.type==tDouble) return toDouble() < Value.toDouble() ? 1 : -1;
	return toInt32() < Value.toInt32() ? 1 : -1;
}

bool CNumber::equal( const CNumber &Value ) const {
	if(type==tNaN || Value.type==tNaN) return false;
	else if(type==tInfinity) {
		if(Value.type==tInfinity) return Int32==Value.Int32;
		return false;
	} else if(Value.type==tInfinity) 
		return false;
	else if(isZero() && Value.isZero()) return true;
	else if(type==tDouble || Value.type==tDouble) return toDouble() == Value.toDouble();
	return toInt32() == Value.toInt32();
}

bool CNumber::isZero() const
{
	switch(type) {
	case tInt32:
		return Int32==0;
	case tnNULL:
		return true;
	case tDouble:
		return Double==0.0;
	default:
		return false;
	}
}

bool CNumber::isInteger() const
{
	double integral;
	switch(type) {
	case tInt32:
	case tnNULL:
		return true;
	case tDouble:
		return modf(Double, &integral)==0.0;
	default:
		return false;
	}
}

int CNumber::sign() const {
	switch(type) {
	case tInt32:
	case tInfinity:
		return Int32<0?-1:1;
	case tnNULL:
		return -1;
	case tDouble:
		return Double<0.0?-1:1;
	default:
		return 1;
	}
}
char *tiny_ltoa(int32_t val, unsigned radix) {
	char *buf, *buf_end, *p, *firstdig, temp;
	unsigned digval;

	buf = (char*)malloc(64);
	if(!buf) return 0;
	buf_end = buf+64-1; // -1 for '\0'

	p = buf;
	if (val < 0) {
		*p++ = '-';
		val = -val;
	}

	do {
		digval = (unsigned) (val % radix);
		val /= radix;
		*p++ = (char) (digval + (digval > 9 ? ('a'-10) : '0'));
		if(p==buf_end) {
			char *new_buf = (char *)realloc(buf, buf_end-buf+16+1); // for '\0'
			if(!new_buf) { free(buf); return 0; }
			p = new_buf + (buf_end - buf);
			buf_end = p + 16;
			buf = new_buf;
		}
	} while (val > 0);

	// We now have the digit of the number in the buffer, but in reverse
	// order.  Thus we reverse them now.
	*p-- = '\0';
	firstdig = buf;
	if(*firstdig=='-') firstdig++;
	do	{
		temp = *p;
		*p = *firstdig;
		*firstdig = temp;
		p--;
		firstdig++;
	} while (firstdig < p);
	return buf;
}

static char *tiny_dtoa(double val, unsigned radix) {
	char *buf, *buf_end, *p, temp;
	unsigned digval;

	buf = (char*)malloc(64);
	if(!buf) return 0;
	buf_end = buf+64-2; // -1 for '.' , -1 for '\0'

	p = buf;
	if (val < 0.0) {
		*p++ = '-';
		val = -val;
	}

	double val_1 = floor(val);
	double val_2 = val - val_1;


	do {
		double tmp = val_1 / radix;
		val_1 = floor(tmp);
		digval = (unsigned)((tmp - val_1) * radix);

		*p++ = (char) (digval + (digval > 9 ? ('a'-10) : '0'));
		if(p==buf_end) {
			char *new_buf = (char *)realloc(buf, buf_end-buf+16+2); // +2 for '.' + '\0'
			if(!new_buf) { free(buf); return 0; }
			p = new_buf + (buf_end - buf);
			buf_end = p + 16;
			buf = new_buf;
		}
	} while (val_1 > 0.0);

	// We now have the digit of the number in the buffer, but in reverse
	// order.  Thus we reverse them now.
	char *p1 = buf;
	char *p2 = p-1;
	do	{
		temp = *p2;
		*p2-- = *p1;
		*p1++ = temp;
	} while (p1 < p2);

	if(val_2) {
		*p++ = '.';
		do {
			val_2 *= radix;
			digval = (unsigned)(val_2);
			val_2 -= digval;

			*p++ = (char) (digval + (digval > 9 ? ('a'-10) : '0'));
			if(p==buf_end) {
				char *new_buf = (char *)realloc(buf, buf_end-buf+16);
				if(!new_buf) { free(buf); return 0; }
				p = new_buf + (buf_end - buf);
				buf_end = p + 16;
				buf = new_buf;
			}
		} while (val_2 > 0.0);

	}
	*p = '\0';
	return buf;
}
std::string CNumber::toString( uint32_t Radix/*=10*/ ) const {
	char *str;
	if(2 > Radix || Radix > 36)
		Radix = 10; // todo error;
	switch(type) {
	case tInt32:
		if( (str = tiny_ltoa(Int32, Radix)) ) {
			string ret(str); free(str);
			return ret;
		}
		break;
	case tnNULL:
		return "0";
	case tDouble:
		if(Radix==10) {
			ostringstream str;
			str.unsetf(ios::floatfield);
#if (defined(_MSC_VER) && _MSC_VER >= 1600) || __cplusplus >= 201103L
			str.precision(numeric_limits<double>::max_digits10);
#else
			str.precision(numeric_limits<double>::digits10+2);
#endif
			str << Double;
			return str.str();
		} else if( (str = tiny_dtoa(Double, Radix)) ) {
			string ret(str); free(str);
			return ret;
		}
		break;
	case tInfinity:
		return Int32<0?"-Infinity":"Infinity";
	case tNaN:
		return "NaN";
	}
	return "";
}

double CNumber::toDouble() const
{
	switch(type) {
	case tnNULL:
		return -0.0;
	case tInt32:
		return double(Int32);
	case tDouble:
		return Double;
	case tNaN:
		return std::numeric_limits<double>::quiet_NaN();
	case tInfinity:
		return Int32<0 ? -std::numeric_limits<double>::infinity():std::numeric_limits<double>::infinity();
	}
	return 0.0;
}

#endif


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarNumber
//////////////////////////////////////////////////////////////////////////

CScriptVarNumber::CScriptVarNumber(CTinyJS *Context, const CNumber &Data) : CScriptVarPrimitive(Context, Context->numberPrototype), data(Data) {}
CScriptVarNumber::~CScriptVarNumber() {}
CScriptVarPtr CScriptVarNumber::clone() { return new CScriptVarNumber(*this); }
bool CScriptVarNumber::isNumber() { return true; }
bool CScriptVarNumber::isInt() { return data.isInt32(); }
bool CScriptVarNumber::isDouble() { return data.isDouble(); }
bool CScriptVarNumber::isRealNumber() { return isInt() || isDouble(); }
bool CScriptVarNumber::isNaN() { return data.isNaN(); }
int CScriptVarNumber::isInfinity() { return data.isInfinity(); }

bool CScriptVarNumber::toBoolean() { return data.toBoolean(); }
CNumber CScriptVarNumber::toNumber_Callback() { return data; }
string CScriptVarNumber::toCString(int radix/*=0*/) { return data.toString(radix); }

string CScriptVarNumber::getVarType() { return "number"; }

CScriptVarPtr CScriptVarNumber::toObject() { return newScriptVar(CScriptVarPrimitivePtr(this), context->numberPrototype); }
inline define_newScriptVar_Fnc(Number, CTinyJS *Context, const CNumber &Obj) { 
	if(!Obj.isInt32() && !Obj.isDouble()) {
		if(Obj.isNaN()) return Context->constScriptVar(NaN);
		if(Obj.isInfinity()) return Context->constScriptVar(Infinity(Obj.sign()));
		if(Obj.isNegativeZero()) return Context->constScriptVar(NegativeZero);
	}
	return new CScriptVarNumber(Context, Obj); 
}


////////////////////////////////////////////////////////////////////////// 
// CScriptVarBool
//////////////////////////////////////////////////////////////////////////

CScriptVarBool::CScriptVarBool(CTinyJS *Context, bool Data) : CScriptVarPrimitive(Context, Context->booleanPrototype), data(Data) {}
CScriptVarBool::~CScriptVarBool() {}
CScriptVarPtr CScriptVarBool::clone() { return new CScriptVarBool(*this); }
bool CScriptVarBool::isBool() { return true; }

bool CScriptVarBool::toBoolean() { return data; }
CNumber CScriptVarBool::toNumber_Callback() { return data?1:0; }
string CScriptVarBool::toCString(int radix/*=0*/) { return data ? "true" : "false"; }

string CScriptVarBool::getVarType() { return "boolean"; }

CScriptVarPtr CScriptVarBool::toObject() { return newScriptVar(CScriptVarPrimitivePtr(this), context->booleanPrototype); }

////////////////////////////////////////////////////////////////////////// 
/// CScriptVarObject
//////////////////////////////////////////////////////////////////////////

declare_dummy_t(Object);
CScriptVarObject::CScriptVarObject(CTinyJS *Context) : CScriptVar(Context, Context->objectPrototype) { }
CScriptVarObject::~CScriptVarObject() {}
CScriptVarPtr CScriptVarObject::clone() { return new CScriptVarObject(*this); }

void CScriptVarObject::removeAllChildren()
{
	CScriptVar::removeAllChildren();
	value.clear();
}

CScriptVarPrimitivePtr CScriptVarObject::getRawPrimitive() { return value; }
bool CScriptVarObject::isObject() { return true; }

string CScriptVarObject::getParsableString(const string &indentString, const string &indent, uint32_t uniqueID, bool &hasRecursion) {
	getParsableStringRecursionsCheck();
	string destination;
	const char *nl = indent.size() ? "\n" : " ";
	const char *comma = "";
	destination.append("{");
	if(Childs.size()) {
		string new_indentString = indentString + indent;
		for(SCRIPTVAR_CHILDS_it it = Childs.begin(); it != Childs.end(); ++it) {
			if((*it)->isEnumerable()) {
				destination.append(comma); comma=",";
				destination.append(nl).append(new_indentString).append(getIDString((*it)->getName()));
				destination.append(" : ");
				destination.append((*it)->getVarPtr()->getParsableString(new_indentString, indent, uniqueID, hasRecursion));
			}
		}
		destination.append(nl).append(indentString);
	}
	destination.append("}");
	return destination;
}
string CScriptVarObject::getVarType() { return "object"; }
string CScriptVarObject::getVarTypeTagName() { return "Object"; }

CScriptVarPtr CScriptVarObject::toObject() { return this; }

CScriptVarPtr CScriptVarObject::valueOf_CallBack() {
	if(value)
		return value->valueOf_CallBack();
	return CScriptVar::valueOf_CallBack();
}
CScriptVarPtr CScriptVarObject::toString_CallBack(CScriptResult &execute, int radix) { 
	if(value)
		return value->toString_CallBack(execute, radix);
	return newScriptVar("[object "+getVarTypeTagName()+"]"); 
};

void CScriptVarObject::setTemporaryMark_recursive( uint32_t ID) {
	CScriptVar::setTemporaryMark_recursive(ID);
	if(value) value->setTemporaryMark_recursive(ID);
}


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarObjectTyped (simple Object with Typename
//////////////////////////////////////////////////////////////////////////

declare_dummy_t(StopIteration);
CScriptVarObjectTypeTagged::~CScriptVarObjectTypeTagged() {}
CScriptVarPtr CScriptVarObjectTypeTagged::clone() { return new CScriptVarObjectTypeTagged(*this); }
std::string CScriptVarObjectTypeTagged::getVarTypeTagName() { return typeTagName; }
	
	
////////////////////////////////////////////////////////////////////////// 
/// CScriptVarError
//////////////////////////////////////////////////////////////////////////

const char *ERROR_NAME[] = {"Error", "EvalError", "RangeError", "ReferenceError", "SyntaxError", "TypeError"};

CScriptVarError::CScriptVarError(CTinyJS *Context, ERROR_TYPES type, const char *message, const char *file, int line, int column) : CScriptVarObject(Context, Context->getErrorPrototype(type)) {
	if(message && *message) addChild("message", newScriptVar(message));
	if(file && *file) addChild("fileName", newScriptVar(file));
	if(line>=0) addChild("lineNumber", newScriptVar(line+1));
	if(column>=0) addChild("column", newScriptVar(column+1));
}

CScriptVarError::~CScriptVarError() {}
CScriptVarPtr CScriptVarError::clone() { return new CScriptVarError(*this); }
bool CScriptVarError::isError() { return true; }

CScriptVarPtr CScriptVarError::toString_CallBack(CScriptResult &execute, int radix) {
	CScriptVarLinkPtr link;
	string name = ERROR_NAME[Error];
	link = findChildWithPrototypeChain("name"); if(link) name = link->toString(execute);
	string message; link = findChildWithPrototypeChain("message"); if(link) message = link->toString(execute);
	string fileName; link = findChildWithPrototypeChain("fileName"); if(link) fileName = link->toString(execute);
	int lineNumber=-1; link = findChildWithPrototypeChain("lineNumber"); if(link) lineNumber = link->toNumber().toInt32();
	int column=-1; link = findChildWithPrototypeChain("column"); if(link) column = link->toNumber().toInt32();
	ostringstream msg;
	msg << name << ": " << message;
	if(lineNumber >= 0) msg << " at Line:" << lineNumber+1;
	if(column >=0) msg << " Column:" << column+1;
	if(fileName.length()) msg << " in " << fileName;
	return newScriptVar(msg.str());
}

CScriptException *CScriptVarError::toCScriptException()
{
	CScriptVarLinkPtr link;
	string name = ERROR_NAME[Error];
	link = findChildWithPrototypeChain("name"); if(link) name = link->toString();
	int ErrorCode;
	for(ErrorCode=(sizeof(ERROR_NAME)/sizeof(ERROR_NAME[0]))-1; ErrorCode>0; ErrorCode--) {
		if(name == ERROR_NAME[ErrorCode]) break;
	}
	string message; link = findChildWithPrototypeChain("message"); if(link) message = link->toString();
	string fileName; link = findChildWithPrototypeChain("fileName"); if(link) fileName = link->toString();
	int lineNumber=-1; link = findChildWithPrototypeChain("lineNumber"); if(link) lineNumber = link->toNumber().toInt32()-1;
	int column=-1; link = findChildWithPrototypeChain("column"); if(link) column = link->toNumber().toInt32()-1;
	return new CScriptException((enum ERROR_TYPES)ErrorCode, message, fileName, lineNumber, column); 
}


////////////////////////////////////////////////////////////////////////// 
// CScriptVarArray
//////////////////////////////////////////////////////////////////////////

declare_dummy_t(Array);
CScriptVarArray::CScriptVarArray(CTinyJS *Context) : CScriptVarObject(Context, Context->arrayPrototype), toStringRecursion(false) {
	CScriptVarLinkPtr acc = addChild("length", newScriptVar(Accessor), 0);
	CScriptVarFunctionPtr getter(::newScriptVar(Context, this, &CScriptVarArray::native_Length, 0));
	getter->setFunctionData(new CScriptTokenDataFnc);
	acc->getVarPtr()->addChild(TINYJS_ACCESSOR_GET_VAR, getter, 0);
}

CScriptVarArray::~CScriptVarArray() {}
CScriptVarPtr CScriptVarArray::clone() { return new CScriptVarArray(*this); }
bool CScriptVarArray::isArray() { return true; }
string CScriptVarArray::getParsableString(const string &indentString, const string &indent, uint32_t uniqueID, bool &hasRecursion) {
	getParsableStringRecursionsCheck();
	string destination;
	const char *nl = indent.size() ? "\n" : " ";
	const char *comma = "";
	destination.append("[");
	int len = getArrayLength();
	if(len) {
		string new_indentString = indentString + indent;
		for (int i=0;i<len;i++) {
			destination.append(comma); comma = ",";
			destination.append(nl).append(new_indentString).append(getArrayIndex(i)->getParsableString(new_indentString, indent, uniqueID, hasRecursion));
		}
		destination.append(nl).append(indentString);
	}
	destination.append("]");
	return destination;
}
CScriptVarPtr CScriptVarArray::toString_CallBack( CScriptResult &execute, int radix/*=0*/ ) {
	ostringstream destination;
	if(toStringRecursion) {
		return newScriptVar("");
	}
	toStringRecursion = true;
	try {
		int len = getArrayLength();
		for (int i=0;i<len;i++) {
			destination << getArrayIndex(i)->toString(execute);
			if (i<len-1) destination  << ", ";
		}
	} catch(...) {
		toStringRecursion = false;
		throw; // rethrow
	} 
	toStringRecursion = false;
	return newScriptVar(destination.str());

}

void CScriptVarArray::native_Length(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(newScriptVar(c->getArgument("this")->getArrayLength()));
}


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarRegExp
//////////////////////////////////////////////////////////////////////////

#ifndef NO_REGEXP

CScriptVarRegExp::CScriptVarRegExp(CTinyJS *Context, const string &Regexp, const string &Flags) : CScriptVarObject(Context, Context->regexpPrototype), regexp(Regexp), flags(Flags) {
	addChild("global", ::newScriptVarAccessor<CScriptVarRegExp>(Context, this, &CScriptVarRegExp::native_Global, 0, 0, 0), 0);
	addChild("ignoreCase", ::newScriptVarAccessor<CScriptVarRegExp>(Context, this, &CScriptVarRegExp::native_IgnoreCase, 0, 0, 0), 0);
	addChild("multiline", ::newScriptVarAccessor<CScriptVarRegExp>(Context, this, &CScriptVarRegExp::native_Multiline, 0, 0, 0), 0);
	addChild("sticky", ::newScriptVarAccessor<CScriptVarRegExp>(Context, this, &CScriptVarRegExp::native_Sticky, 0, 0, 0), 0);
	addChild("regexp", ::newScriptVarAccessor<CScriptVarRegExp>(Context, this, &CScriptVarRegExp::native_Source, 0, 0, 0), 0);
	addChild("lastIndex", newScriptVar(0));
}
CScriptVarRegExp::~CScriptVarRegExp() {}
CScriptVarPtr CScriptVarRegExp::clone() { return new CScriptVarRegExp(*this); }
bool CScriptVarRegExp::isRegExp() { return true; }
//int CScriptVarRegExp::getInt() {return strtol(regexp.c_str(),0,0); }
//bool CScriptVarRegExp::getBool() {return regexp.length()!=0;}
//double CScriptVarRegExp::getDouble() {return strtod(regexp.c_str(),0);}
//string CScriptVarRegExp::getString() { return "/"+regexp+"/"+flags; }
//string CScriptVarRegExp::getParsableString(const string &indentString, const string &indent, uint32_t uniqueID, bool &hasRecursion) { return getString(); }
CScriptVarPtr CScriptVarRegExp::toString_CallBack(CScriptResult &execute, int radix) {
	return newScriptVar("/"+regexp+"/"+flags);
}
void CScriptVarRegExp::native_Global(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(constScriptVar(Global()));
}
void CScriptVarRegExp::native_IgnoreCase(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(constScriptVar(IgnoreCase()));
}
void CScriptVarRegExp::native_Multiline(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(constScriptVar(Multiline()));
}
void CScriptVarRegExp::native_Sticky(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(constScriptVar(Sticky()));
}
void CScriptVarRegExp::native_Source(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(newScriptVar(regexp));
}
unsigned int CScriptVarRegExp::LastIndex() {
	CScriptVarPtr lastIndex = findChild("lastIndex");
	if(lastIndex) return lastIndex->toNumber().toInt32();
	return 0;
}
void CScriptVarRegExp::LastIndex(unsigned int Idx) {
	addChildOrReplace("lastIndex", newScriptVar((int)Idx));
}

CScriptVarPtr CScriptVarRegExp::exec( const string &Input, bool Test /*= false*/ )
{
	regex::flag_type flags = regex_constants::ECMAScript;
	if(IgnoreCase()) flags |= regex_constants::icase;
	bool global = Global(), sticky = Sticky();
	unsigned int lastIndex = LastIndex();
	int offset = 0;
	if(global || sticky) {
		if(lastIndex > Input.length()) goto failed; 
		offset=lastIndex;
	}
	{
		regex_constants::match_flag_type mflag = sticky?regex_constants::match_continuous:regex_constants::match_default;
		if(offset) mflag |= regex_constants::match_prev_avail;
		smatch match;
		if(regex_search(Input.begin()+offset, Input.end(), match, regex(regexp, flags), mflag) ) {
			LastIndex(offset+match.position()+match.str().length());
			if(Test) return constScriptVar(true);

			CScriptVarArrayPtr retVar = newScriptVar(Array);
			retVar->addChild("input", newScriptVar(Input));
			retVar->addChild("index", newScriptVar(match.position()));
			for(smatch::size_type idx=0; idx<match.size(); idx++)
				retVar->addChild(int2string(idx), newScriptVar(match[idx].str()));
			return retVar;
		}
	}
failed:
	if(global || sticky) 
		LastIndex(0); 
	if(Test) return constScriptVar(false);
	return constScriptVar(Null);
}

const char * CScriptVarRegExp::ErrorStr( int Error )
{
	switch(Error) {
	case regex_constants::error_badbrace: return "the expression contained an invalid count in a { } expression";
	case regex_constants::error_badrepeat: return "a repeat expression (one of '*', '?', '+', '{' in most contexts) was not preceded by an expression";
	case regex_constants::error_brace: return "the expression contained an unmatched '{' or '}'";
	case regex_constants::error_brack: return "the expression contained an unmatched '[' or ']'";
	case regex_constants::error_collate: return "the expression contained an invalid collating element name";
	case regex_constants::error_complexity: return "an attempted match failed because it was too complex";
	case regex_constants::error_ctype: return "the expression contained an invalid character class name";
	case regex_constants::error_escape: return "the expression contained an invalid escape sequence";
	case regex_constants::error_paren: return "the expression contained an unmatched '(' or ')'";
	case regex_constants::error_range: return "the expression contained an invalid character range specifier";
	case regex_constants::error_space: return "parsing a regular expression failed because there were not enough resources available";
	case regex_constants::error_stack: return "an attempted match failed because there was not enough memory available";
	case regex_constants::error_backref: return "the expression contained an invalid back reference";
	default: return "";
	}
}

#endif /* NO_REGEXP */


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarDefaultIterator
//////////////////////////////////////////////////////////////////////////

//declare_dummy_t(DefaultIterator);
CScriptVarDefaultIterator::CScriptVarDefaultIterator(CTinyJS *Context, const CScriptVarPtr &Object, int Mode) 
	: CScriptVarObject(Context, Context->iteratorPrototype), mode(Mode), object(Object) {
	object->keys(keys, true);
	pos = keys.begin();
	addChild("next", ::newScriptVar(context, this, &CScriptVarDefaultIterator::native_next, 0));
}
CScriptVarDefaultIterator::~CScriptVarDefaultIterator() {}
CScriptVarPtr CScriptVarDefaultIterator::clone() { return new CScriptVarDefaultIterator(*this); }
bool CScriptVarDefaultIterator::isIterator()		{return true;}
void CScriptVarDefaultIterator::native_next(const CFunctionsScopePtr &c, void *data) {
	if(pos==keys.end()) throw constScriptVar(StopIteration);
	CScriptVarPtr ret, ret0, ret1;
	if(mode&1) ret0 = newScriptVar(*pos);
	if(mode&2) ret1 = object->findChildWithStringChars(*pos);
	pos++;
	if(mode==3) {
		ret = newScriptVar(Array);
		ret->setArrayIndex(0, ret0);
		ret->setArrayIndex(1, ret1);
	} else if(mode==1) 
		ret = ret0;
	else
		ret = ret1;
	c->setReturnVar(ret);
}


#ifndef NO_GENERATORS
////////////////////////////////////////////////////////////////////////// 
/// CScriptVarGenerator
//////////////////////////////////////////////////////////////////////////

//declare_dummy_t(Generator);
CScriptVarGenerator::CScriptVarGenerator(CTinyJS *Context, const CScriptVarPtr &FunctionRoot, const CScriptVarFunctionPtr &Function) 
	: CScriptVarObject(Context, Context->generatorPrototype), functionRoot(FunctionRoot), function(Function), 
	closed(false), yieldVarIsException(false), coroutine(this) {
//		addChild("next", ::newScriptVar(context, this, &CScriptVarGenerator::native_send, 0, "Generator.next"));
	//	addChild("send", ::newScriptVar(context, this, &CScriptVarGenerator::native_send, (void*)1, "Generator.send"));
		//addChild("close", ::newScriptVar(context, this, &CScriptVarGenerator::native_throw, (void*)0, "Generator.close"));
		//addChild("throw", ::newScriptVar(context, this, &CScriptVarGenerator::native_throw, (void*)1, "Generator.throw"));
}
CScriptVarGenerator::~CScriptVarGenerator() {
	if(coroutine.isStarted() && coroutine.isRunning()) {
		coroutine.Stop(false);
		coroutine.next();
		coroutine.Stop();
	}
}
CScriptVarPtr CScriptVarGenerator::clone() { return new CScriptVarGenerator(*this); }
bool CScriptVarGenerator::isIterator()		{return true;}
bool CScriptVarGenerator::isGenerator()	{return true;}

string CScriptVarGenerator::getVarType() { return "generator"; }
string CScriptVarGenerator::getVarTypeTagName() { return "Generator"; }

void CScriptVarGenerator::setTemporaryMark_recursive( uint32_t ID ) {
	CScriptVarObject::setTemporaryMark_recursive(ID);
	functionRoot->setTemporaryMark_recursive(ID);
	function->setTemporaryMark_recursive(ID);
	if(yieldVar) yieldVar->setTemporaryMark_recursive(ID);
	for(std::vector<CScriptVarScopePtr>::iterator it=generatorScopes.begin(); it != generatorScopes.end(); ++it)
		(*it)->setTemporaryMark_recursive(ID);
}
void CScriptVarGenerator::native_send(const CFunctionsScopePtr &c, void *data) {
	// data == 0 ==> next()
	// data != 0 ==> send(...)
	if(closed)
		throw constScriptVar(StopIteration);

	yieldVar = data ? c->getArgument(0) : constScriptVar(Undefined);
	yieldVarIsException = false;

	if(!coroutine.isStarted() && data && !yieldVar->isUndefined())
		c->throwError(TypeError, "attempt to send value to newborn generator");
	if(coroutine.next()) {
		c->setReturnVar(yieldVar);
		return;
	}
	closed = true;
	throw yieldVar;
}
void CScriptVarGenerator::native_throw(const CFunctionsScopePtr &c, void *data) {
	// data == 0 ==> close()
	// data != 0 ==> throw(...)
	if(closed || !coroutine.isStarted()) {
		closed = true;
		if(data)
			throw c->getArgument(0);
		else 
			return;
	}
	yieldVar = data ? c->getArgument(0) : CScriptVarPtr();
	yieldVarIsException = true;
	closed = data==0;
	if(coroutine.next()) {
		c->setReturnVar(yieldVar);
		return;
	}
	closed = true;
	/* 
	 * from http://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Iterators_and_Generators
	 * Generators have a close() method that forces the generator to close itself. The effects of closing a generator are:
	 * - Any finally clauses active in the generator function are run.
	 * - If a finally clause throws any exception other than StopIteration, the exception is propagated to the caller of the close() method.
	 * - The generator terminates.
	 *
	 * but in Firefox is also "StopIteration" propagated to the caller 
	 * define GENERATOR_CLOSE_LIKE_IN_FIREFOX to enable this behavior
	*/
//#define GENERATOR_CLOSE_LIKE_IN_FIREFOX
#ifdef GENERATOR_CLOSE_LIKE_IN_FIREFOX
	if(data || yieldVar)
		throw yieldVar;
#else
	if(data || (yieldVar && yieldVar != constScriptVar(StopIteration)))
		throw yieldVar;
#endif
}

int CScriptVarGenerator::Coroutine()
{
	context->generator_start(this);
	return 0;
}

CScriptVarPtr CScriptVarGenerator::yield( CScriptResult &execute, CScriptVar *YieldIn )
{
	yieldVar = YieldIn;
	coroutine.yield();
	if(yieldVarIsException) {
		execute.set(CScriptResult::Throw, yieldVar);
		return constScriptVar(Undefined);
	}
	return yieldVar;
}

#endif /*NO_GENERATORS*/

////////////////////////////////////////////////////////////////////////// 
// CScriptVarFunction
//////////////////////////////////////////////////////////////////////////

CScriptVarFunction::CScriptVarFunction(CTinyJS *Context, CScriptTokenDataFnc *Data) : CScriptVarObject(Context, Context->functionPrototype), data(0) { 
	setFunctionData(Data); 
}
CScriptVarFunction::~CScriptVarFunction() { setFunctionData(0); }
CScriptVarPtr CScriptVarFunction::clone() { return new CScriptVarFunction(*this); }
bool CScriptVarFunction::isObject() { return true; }
bool CScriptVarFunction::isFunction() { return true; }
bool CScriptVarFunction::isPrimitive()	{ return false; } 

//string CScriptVarFunction::getString() {return "[ Function ]";}
string CScriptVarFunction::getVarType() { return "function"; }
string CScriptVarFunction::getParsableString(const string &indentString, const string &indent, uint32_t uniqueID, bool &hasRecursion) {
	getParsableStringRecursionsCheck();
	string destination;
	destination.append("function ").append(data->name);
	// get list of arguments
	destination.append(data->getArgumentsString()); 

	if(isNative() || isBounded())
		destination.append("{ [native code] }");
	else {
		destination.append(CScriptToken::getParsableString(data->body, indentString, indent));
		if(!data->body.size() || data->body.front().token != '{')
			destination.append(";");
	}
	return destination;
}

CScriptVarPtr CScriptVarFunction::toString_CallBack(CScriptResult &execute, int radix){
	bool hasRecursion;
	return newScriptVar(getParsableString("", "  ", 0, hasRecursion));
}

CScriptTokenDataFnc *CScriptVarFunction::getFunctionData() { return data; }

void CScriptVarFunction::setFunctionData(CScriptTokenDataFnc *Data) {
	if(data) { data->unref(); data = 0; }
	if(Data) { 
		data = Data; data->ref(); 
		addChildOrReplace("length", newScriptVar((int)data->arguments.size()), 0);
		// can not add "name" here because name is a StingVar with length as getter
		// length-getter is a function with a function name -> endless recursion
		//addChildNoDup("name", newScriptVar(data->name), 0);
	}
}


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarFunctionBounded
//////////////////////////////////////////////////////////////////////////

CScriptVarFunctionBounded::CScriptVarFunctionBounded(CScriptVarFunctionPtr BoundedFunction, CScriptVarPtr BoundedThis, const std::vector<CScriptVarPtr> &BoundedArguments) 
	: CScriptVarFunction(BoundedFunction->getContext(), new CScriptTokenDataFnc) ,
	boundedFunction(BoundedFunction),
	boundedThis(BoundedThis),
	boundedArguments(BoundedArguments) {
		getFunctionData()->name = BoundedFunction->getFunctionData()->name;
}
CScriptVarFunctionBounded::~CScriptVarFunctionBounded(){}
CScriptVarPtr CScriptVarFunctionBounded::clone() { return new CScriptVarFunctionBounded(*this); }
bool CScriptVarFunctionBounded::isBounded() { return true; }
void CScriptVarFunctionBounded::setTemporaryMark_recursive(uint32_t ID) {
	CScriptVarFunction::setTemporaryMark_recursive(ID);
	boundedThis->setTemporaryMark_recursive(ID);
	for(vector<CScriptVarPtr>::iterator it=boundedArguments.begin(); it!=boundedArguments.end(); ++it)
		(*it)->setTemporaryMark_recursive(ID);
}

CScriptVarPtr CScriptVarFunctionBounded::callFunction( CScriptResult &execute, vector<CScriptVarPtr> &Arguments, const CScriptVarPtr &This, CScriptVarPtr *newThis/*=0*/ )
{
	vector<CScriptVarPtr> newArgs=boundedArguments;
	newArgs.insert(newArgs.end(), Arguments.begin(), Arguments.end());
	return context->callFunction(execute, boundedFunction, newArgs, newThis ? This : boundedThis, newThis);
}


//////////////////////////////////////////////////////////////////////////
/// CScriptVarFunctionNative
//////////////////////////////////////////////////////////////////////////

CScriptVarFunctionNative::~CScriptVarFunctionNative() {}
bool CScriptVarFunctionNative::isNative() { return true; }


//////////////////////////////////////////////////////////////////////////
/// CScriptVarFunctionNativeCallback
//////////////////////////////////////////////////////////////////////////

CScriptVarFunctionNativeCallback::~CScriptVarFunctionNativeCallback() {}
CScriptVarPtr CScriptVarFunctionNativeCallback::clone() { return new CScriptVarFunctionNativeCallback(*this); }
void CScriptVarFunctionNativeCallback::callFunction(const CFunctionsScopePtr &c) { jsCallback(c, jsUserData); }


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarAccessor
//////////////////////////////////////////////////////////////////////////

declare_dummy_t(Accessor);
CScriptVarAccessor::CScriptVarAccessor(CTinyJS *Context) : CScriptVarObject(Context, Context->objectPrototype) { }
CScriptVarAccessor::CScriptVarAccessor(CTinyJS *Context, JSCallback getterFnc, void *getterData, JSCallback setterFnc, void *setterData) 
	: CScriptVarObject(Context) 
{
	if(getterFnc)
		addChild(TINYJS_ACCESSOR_GET_VAR, ::newScriptVar(Context, getterFnc, getterData), 0);
	if(setterFnc)
		addChild(TINYJS_ACCESSOR_SET_VAR, ::newScriptVar(Context, setterFnc, setterData), 0);
}

CScriptVarAccessor::CScriptVarAccessor( CTinyJS *Context, const CScriptVarFunctionPtr &getter, const CScriptVarFunctionPtr &setter) : CScriptVarObject(Context, Context->objectPrototype) {
	if(getter)
		addChild(TINYJS_ACCESSOR_GET_VAR, getter, 0);
	if(setter)
		addChild(TINYJS_ACCESSOR_SET_VAR, setter, 0);
}

CScriptVarAccessor::~CScriptVarAccessor() {}
CScriptVarPtr CScriptVarAccessor::clone() { return new CScriptVarAccessor(*this); }
bool CScriptVarAccessor::isAccessor() { return true; }
bool CScriptVarAccessor::isPrimitive()	{ return false; } 
string CScriptVarAccessor::getParsableString(const string &indentString, const string &indent, uint32_t uniqueID, bool &hasRecursion) {
	return "";
}
string CScriptVarAccessor::getVarType() { return "accessor"; }


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarScope
//////////////////////////////////////////////////////////////////////////

declare_dummy_t(Scope);
CScriptVarScope::~CScriptVarScope() {}
CScriptVarPtr CScriptVarScope::clone() { return CScriptVarPtr(); }
bool CScriptVarScope::isObject() { return false; }
CScriptVarPtr CScriptVarScope::scopeVar() { return this; }	///< to create var like: var a = ...
CScriptVarPtr CScriptVarScope::scopeLet() { return this; }	///< to create var like: let a = ...
CScriptVarLinkWorkPtr CScriptVarScope::findInScopes(const string &childName) { 
	return  CScriptVar::findChild(childName); 
}
CScriptVarScopePtr CScriptVarScope::getParent() { return CScriptVarScopePtr(); } ///< no Parent


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarScopeFnc
//////////////////////////////////////////////////////////////////////////

declare_dummy_t(ScopeFnc);
CScriptVarScopeFnc::~CScriptVarScopeFnc() {}
CScriptVarLinkWorkPtr CScriptVarScopeFnc::findInScopes(const string &childName) { 
	CScriptVarLinkWorkPtr ret = findChild(childName); 
	if( !ret ) {
		if(closure) ret = CScriptVarScopePtr(closure)->findInScopes(childName);
		else ret = context->getRoot()->findChild(childName);
	}
	return ret;
}

void CScriptVarScopeFnc::setReturnVar(const CScriptVarPtr &var) {
	addChildOrReplace(TINYJS_RETURN_VAR, var);
}

CScriptVarPtr CScriptVarScopeFnc::getParameter(const string &name) {
	return getArgument(name);
}

CScriptVarPtr CScriptVarScopeFnc::getParameter(int Idx) {
	return getArgument(Idx);
}
CScriptVarPtr CScriptVarScopeFnc::getArgument(const string &name) {
	return findChildOrCreate(name);
}
CScriptVarPtr CScriptVarScopeFnc::getArgument(int Idx) {
	CScriptVarLinkPtr arguments = findChildOrCreate(TINYJS_ARGUMENTS_VAR);
	if(arguments) arguments = arguments->getVarPtr()->findChild(int2string(Idx));
	return arguments ? arguments->getVarPtr() : constScriptVar(Undefined);
}
int CScriptVarScopeFnc::getParameterLength() {
	return getArgumentsLength();
}
int CScriptVarScopeFnc::getArgumentsLength() {
	CScriptVarLinkPtr arguments = findChild(TINYJS_ARGUMENTS_VAR);
	if(arguments) arguments = arguments->getVarPtr()->findChild("length");
	return arguments ? arguments.getter()->toNumber().toInt32() : 0;
}

void CScriptVarScopeFnc::throwError( ERROR_TYPES ErrorType, const string &message ) {
	throw newScriptVarError(context, ErrorType, message.c_str());
}


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarScopeLet
//////////////////////////////////////////////////////////////////////////

declare_dummy_t(ScopeLet);
CScriptVarScopeLet::CScriptVarScopeLet(const CScriptVarScopePtr &Parent) // constructor for LetScope
	: CScriptVarScope(Parent->getContext()), parent(addChild(TINYJS_SCOPE_PARENT_VAR, Parent, 0))
	, letExpressionInitMode(false) {}

CScriptVarScopeLet::~CScriptVarScopeLet() {}
CScriptVarPtr CScriptVarScopeLet::scopeVar() {						// to create var like: var a = ...
	return getParent()->scopeVar(); 
}
CScriptVarScopePtr CScriptVarScopeLet::getParent() { return (CScriptVarPtr)parent; }
CScriptVarLinkWorkPtr CScriptVarScopeLet::findInScopes(const string &childName) { 
	CScriptVarLinkWorkPtr ret;
	if(letExpressionInitMode) {
		return getParent()->findInScopes(childName);
	} else {
		ret = findChild(childName); 
		if( !ret ) ret = getParent()->findInScopes(childName);
	}
	return ret;
}


////////////////////////////////////////////////////////////////////////// 
/// CScriptVarScopeWith
//////////////////////////////////////////////////////////////////////////

declare_dummy_t(ScopeWith);
CScriptVarScopeWith::~CScriptVarScopeWith() {}
CScriptVarPtr CScriptVarScopeWith::scopeLet() { 							// to create var like: let a = ...
	return getParent()->scopeLet();
}
CScriptVarLinkWorkPtr CScriptVarScopeWith::findInScopes(const string &childName) { 
	if(childName == "this") return with;
	CScriptVarLinkWorkPtr ret = with->getVarPtr()->findChild(childName); 
	if( !ret ) {
		ret = with->getVarPtr()->findChildInPrototypeChain(childName);
		if(ret) {
			ret(ret->getVarPtr(), ret->getName()); // recreate ret
			ret.setReferencedOwner(with->getVarPtr()); // fake referenced Owner
		}
	}
	if( !ret ) ret = getParent()->findInScopes(childName);
	return ret;
}


//////////////////////////////////////////////////////////////////////////
/// CTinyJS
//////////////////////////////////////////////////////////////////////////

extern "C" void _registerFunctions(CTinyJS *tinyJS);
extern "C" void _registerStringFunctions(CTinyJS *tinyJS);
extern "C" void _registerMathFunctions(CTinyJS *tinyJS);

CTinyJS::CTinyJS() {
	CScriptVarPtr var;
	t = 0;
	haveTry = false;
	first = 0;
	uniqueID = 0;
	currentMarkSlot = -1;
	stackBase = 0;

	
	//////////////////////////////////////////////////////////////////////////
	// Object-Prototype
	// must be created as first object because this prototype is the base of all objects
	objectPrototype = newScriptVar(Object); 
	
	// all objects have a prototype. Also the prototype of prototypes
	objectPrototype->addChild(TINYJS___PROTO___VAR, objectPrototype, 0);

	//////////////////////////////////////////////////////////////////////////
	// Function-Prototype
	// must be created as second object because this is the base of all functions (also constructors)
	functionPrototype = newScriptVar(Object);


	//////////////////////////////////////////////////////////////////////////
	// Scopes
	root = ::newScriptVar(this, Scope);
	scopes.push_back(root);

	//////////////////////////////////////////////////////////////////////////
	// Add built-in classes
	//////////////////////////////////////////////////////////////////////////
	// Object
	var = addNative("function Object()", this, &CTinyJS::native_Object, 0, SCRIPTVARLINK_CONSTANT);
	objectPrototype = var->findChild(TINYJS_PROTOTYPE_CLASS);
	objectPrototype->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	addNative("function Object.getPrototypeOf(obj)", this, &CTinyJS::native_Object_getPrototypeOf); 
	addNative("function Object.preventExtensions(obj)", this, &CTinyJS::native_Object_setObjectSecure); 
	addNative("function Object.isExtensible(obj)", this, &CTinyJS::native_Object_isSecureObject); 
	addNative("function Object.seel(obj)", this, &CTinyJS::native_Object_setObjectSecure, (void*)1); 
	addNative("function Object.isSealed(obj)", this, &CTinyJS::native_Object_isSecureObject, (void*)1); 
	addNative("function Object.freeze(obj)", this, &CTinyJS::native_Object_setObjectSecure, (void*)2); 
	addNative("function Object.isFrozen(obj)", this, &CTinyJS::native_Object_isSecureObject, (void*)2); 
	addNative("function Object.keys(obj)", this, &CTinyJS::native_Object_keys); 
	addNative("function Object.getOwnPropertyNames(obj)", this, &CTinyJS::native_Object_keys, (void*)1); 
	addNative("function Object.getOwnPropertyDescriptor(obj,name)", this, &CTinyJS::native_Object_getOwnPropertyDescriptor); 
	addNative("function Object.defineProperty(obj,name,attributes)", this, &CTinyJS::native_Object_defineProperty); 
	addNative("function Object.defineProperties(obj,properties)", this, &CTinyJS::native_Object_defineProperties); 
	addNative("function Object.create(obj,properties)", this, &CTinyJS::native_Object_defineProperties, (void*)1); 

	addNative("function Object.prototype.hasOwnProperty(prop)", this, &CTinyJS::native_Object_prototype_hasOwnProperty); 
	objectPrototype_valueOf = addNative("function Object.prototype.valueOf()", this, &CTinyJS::native_Object_prototype_valueOf); 
	objectPrototype_toString = addNative("function Object.prototype.toString(radix)", this, &CTinyJS::native_Object_prototype_toString); 
	pseudo_refered.push_back(&objectPrototype);
	pseudo_refered.push_back(&objectPrototype_valueOf);
	pseudo_refered.push_back(&objectPrototype_toString);

	//////////////////////////////////////////////////////////////////////////
	// Array
	var = addNative("function Array()", this, &CTinyJS::native_Array, 0, SCRIPTVARLINK_CONSTANT);
	arrayPrototype = var->findChild(TINYJS_PROTOTYPE_CLASS);
	arrayPrototype->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	arrayPrototype->addChild("valueOf", objectPrototype_valueOf, SCRIPTVARLINK_BUILDINDEFAULT);
	arrayPrototype->addChild("toString", objectPrototype_toString, SCRIPTVARLINK_BUILDINDEFAULT);
	pseudo_refered.push_back(&arrayPrototype);
	var = addNative("function Array.__constructor__()", this, &CTinyJS::native_Array, (void*)1, SCRIPTVARLINK_CONSTANT);
	var->getFunctionData()->name = "Array";

	//////////////////////////////////////////////////////////////////////////
	// String
	var = addNative("function String()", this, &CTinyJS::native_String, 0, SCRIPTVARLINK_CONSTANT);
	stringPrototype  = var->findChild(TINYJS_PROTOTYPE_CLASS);
	stringPrototype->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	stringPrototype->addChild("valueOf", objectPrototype_valueOf, SCRIPTVARLINK_BUILDINDEFAULT);
	stringPrototype->addChild("toString", objectPrototype_toString, SCRIPTVARLINK_BUILDINDEFAULT);
	pseudo_refered.push_back(&stringPrototype);
	var = addNative("function String.__constructor__()", this, &CTinyJS::native_String, (void*)1, SCRIPTVARLINK_CONSTANT);
	var->getFunctionData()->name = "String";

	//////////////////////////////////////////////////////////////////////////
	// RegExp
#ifndef NO_REGEXP
	var = addNative("function RegExp()", this, &CTinyJS::native_RegExp, 0, SCRIPTVARLINK_CONSTANT);
	regexpPrototype  = var->findChild(TINYJS_PROTOTYPE_CLASS);
	regexpPrototype->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	regexpPrototype->addChild("valueOf", objectPrototype_valueOf, SCRIPTVARLINK_BUILDINDEFAULT);
	regexpPrototype->addChild("toString", objectPrototype_toString, SCRIPTVARLINK_BUILDINDEFAULT);
	pseudo_refered.push_back(&regexpPrototype);
#endif /* NO_REGEXP */

	//////////////////////////////////////////////////////////////////////////
	// Number
	var = addNative("function Number()", this, &CTinyJS::native_Number, 0, SCRIPTVARLINK_CONSTANT);
	var->addChild("NaN", constNaN = newScriptVarNumber(this, NaN), SCRIPTVARLINK_CONSTANT);
	var->addChild("MAX_VALUE", newScriptVarNumber(this, numeric_limits<double>::max()), SCRIPTVARLINK_CONSTANT);
	var->addChild("MIN_VALUE", newScriptVarNumber(this, numeric_limits<double>::min()), SCRIPTVARLINK_CONSTANT);
	var->addChild("POSITIVE_INFINITY", constInfinityPositive = newScriptVarNumber(this, InfinityPositive), SCRIPTVARLINK_CONSTANT);
	var->addChild("NEGATIVE_INFINITY", constInfinityNegative = newScriptVarNumber(this, InfinityNegative), SCRIPTVARLINK_CONSTANT);
	numberPrototype = var->findChild(TINYJS_PROTOTYPE_CLASS);
	numberPrototype->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	numberPrototype->addChild("valueOf", objectPrototype_valueOf, SCRIPTVARLINK_BUILDINDEFAULT);
	numberPrototype->addChild("toString", objectPrototype_toString, SCRIPTVARLINK_BUILDINDEFAULT);
	pseudo_refered.push_back(&numberPrototype);
	pseudo_refered.push_back(&constNaN);
	pseudo_refered.push_back(&constInfinityPositive);
	pseudo_refered.push_back(&constInfinityNegative);
	var = addNative("function Number.__constructor__()", this, &CTinyJS::native_Number, (void*)1, SCRIPTVARLINK_CONSTANT);
	var->getFunctionData()->name = "Number";

	//////////////////////////////////////////////////////////////////////////
	// Boolean
	var = addNative("function Boolean()", this, &CTinyJS::native_Boolean, 0, SCRIPTVARLINK_CONSTANT);
	booleanPrototype = var->findChild(TINYJS_PROTOTYPE_CLASS);
	booleanPrototype->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	booleanPrototype->addChild("valueOf", objectPrototype_valueOf, SCRIPTVARLINK_BUILDINDEFAULT);
	booleanPrototype->addChild("toString", objectPrototype_toString, SCRIPTVARLINK_BUILDINDEFAULT);
	pseudo_refered.push_back(&booleanPrototype);
	var = addNative("function Boolean.__constructor__()", this, &CTinyJS::native_Boolean, (void*)1, SCRIPTVARLINK_CONSTANT);
	var->getFunctionData()->name = "Boolean";

	//////////////////////////////////////////////////////////////////////////
	// Iterator
	var = addNative("function Iterator(obj,mode)", this, &CTinyJS::native_Iterator, 0, SCRIPTVARLINK_CONSTANT); 
	iteratorPrototype = var->findChild(TINYJS_PROTOTYPE_CLASS);
	iteratorPrototype->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	pseudo_refered.push_back(&iteratorPrototype);

	//////////////////////////////////////////////////////////////////////////
	// Generator
//	var = addNative("function Iterator(obj,mode)", this, &CTinyJS::native_Iterator, 0, SCRIPTVARLINK_CONSTANT); 
#ifndef NO_GENERATORS
	generatorPrototype = newScriptVar(Object);
	generatorPrototype->addChild("next", ::newScriptVar(this, this, &CTinyJS::native_Generator_prototype_next, (void*)0, "Generator.next"), SCRIPTVARLINK_BUILDINDEFAULT);
	generatorPrototype->addChild("send", ::newScriptVar(this, this, &CTinyJS::native_Generator_prototype_next, (void*)1, "Generator.send"), SCRIPTVARLINK_BUILDINDEFAULT);
	generatorPrototype->addChild("close", ::newScriptVar(this, this, &CTinyJS::native_Generator_prototype_next, (void*)2, "Generator.close"), SCRIPTVARLINK_BUILDINDEFAULT);
	generatorPrototype->addChild("throw", ::newScriptVar(this, this, &CTinyJS::native_Generator_prototype_next, (void*)3, "Generator.throw"), SCRIPTVARLINK_BUILDINDEFAULT);
	pseudo_refered.push_back(&generatorPrototype);
#endif /*NO_GENERATORS*/

	//////////////////////////////////////////////////////////////////////////
	// Function
	var = addNative("function Function(params, body)", this, &CTinyJS::native_Function, 0, SCRIPTVARLINK_CONSTANT); 
	var->addChildOrReplace(TINYJS_PROTOTYPE_CLASS, functionPrototype);
	functionPrototype->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	addNative("function Function.prototype.call(objc)", this, &CTinyJS::native_Function_prototype_call); 
	addNative("function Function.prototype.apply(objc, args)", this, &CTinyJS::native_Function_prototype_apply); 
	addNative("function Function.prototype.bind(objc, args)", this, &CTinyJS::native_Function_prototype_bind); 
	functionPrototype->addChild("valueOf", objectPrototype_valueOf, SCRIPTVARLINK_BUILDINDEFAULT);
	functionPrototype->addChild("toString", objectPrototype_toString, SCRIPTVARLINK_BUILDINDEFAULT);
	pseudo_refered.push_back(&functionPrototype);

	//////////////////////////////////////////////////////////////////////////
	// Error
	var = addNative("function Error(message, fileName, lineNumber, column)", this, &CTinyJS::native_Error, 0, SCRIPTVARLINK_CONSTANT); 
	errorPrototypes[Error] = var->findChild(TINYJS_PROTOTYPE_CLASS);
	errorPrototypes[Error]->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	errorPrototypes[Error]->addChild("message", newScriptVar(""));
	errorPrototypes[Error]->addChild("name", newScriptVar("Error"));
	errorPrototypes[Error]->addChild("fileName", newScriptVar(""));
	errorPrototypes[Error]->addChild("lineNumber", newScriptVar(-1));	// -1 means not viable
	errorPrototypes[Error]->addChild("column", newScriptVar(-1));			// -1 means not viable

	var = addNative("function EvalError(message, fileName, lineNumber, column)", this, &CTinyJS::native_EvalError, 0, SCRIPTVARLINK_CONSTANT); 
	errorPrototypes[EvalError] = var->findChild(TINYJS_PROTOTYPE_CLASS);
	errorPrototypes[EvalError]->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	errorPrototypes[EvalError]->addChildOrReplace(TINYJS___PROTO___VAR, errorPrototypes[Error], SCRIPTVARLINK_WRITABLE);
	errorPrototypes[EvalError]->addChild("name", newScriptVar("EvalError"));

	var = addNative("function RangeError(message, fileName, lineNumber, column)", this, &CTinyJS::native_RangeError, 0, SCRIPTVARLINK_CONSTANT); 
	errorPrototypes[RangeError] = var->findChild(TINYJS_PROTOTYPE_CLASS);
	errorPrototypes[RangeError]->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	errorPrototypes[RangeError]->addChildOrReplace(TINYJS___PROTO___VAR, errorPrototypes[Error], SCRIPTVARLINK_WRITABLE);
	errorPrototypes[RangeError]->addChild("name", newScriptVar("RangeError"));

	var = addNative("function ReferenceError(message, fileName, lineNumber, column)", this, &CTinyJS::native_ReferenceError, 0, SCRIPTVARLINK_CONSTANT); 
	errorPrototypes[ReferenceError] = var->findChild(TINYJS_PROTOTYPE_CLASS);
	errorPrototypes[ReferenceError]->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	errorPrototypes[ReferenceError]->addChildOrReplace(TINYJS___PROTO___VAR, errorPrototypes[Error], SCRIPTVARLINK_WRITABLE);
	errorPrototypes[ReferenceError]->addChild("name", newScriptVar("ReferenceError"));

	var = addNative("function SyntaxError(message, fileName, lineNumber, column)", this, &CTinyJS::native_SyntaxError, 0, SCRIPTVARLINK_CONSTANT); 
	errorPrototypes[SyntaxError] = var->findChild(TINYJS_PROTOTYPE_CLASS);
	errorPrototypes[SyntaxError]->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	errorPrototypes[SyntaxError]->addChildOrReplace(TINYJS___PROTO___VAR, errorPrototypes[Error], SCRIPTVARLINK_WRITABLE);
	errorPrototypes[SyntaxError]->addChild("name", newScriptVar("SyntaxError"));

	var = addNative("function TypeError(message, fileName, lineNumber, column)", this, &CTinyJS::native_TypeError, 0, SCRIPTVARLINK_CONSTANT); 
	errorPrototypes[TypeError] = var->findChild(TINYJS_PROTOTYPE_CLASS);
	errorPrototypes[TypeError]->addChild(TINYJS_CONSTRUCTOR_VAR, var, SCRIPTVARLINK_BUILDINDEFAULT);
	errorPrototypes[TypeError]->addChildOrReplace(TINYJS___PROTO___VAR, errorPrototypes[Error], SCRIPTVARLINK_WRITABLE);
	errorPrototypes[TypeError]->addChild("name", newScriptVar("TypeError"));






	//////////////////////////////////////////////////////////////////////////
	// add global built-in vars & constants
	root->addChild("undefined", constUndefined = newScriptVarUndefined(this), SCRIPTVARLINK_CONSTANT);
	pseudo_refered.push_back(&constUndefined);
	constNull	= newScriptVarNull(this);	pseudo_refered.push_back(&constNull);
	root->addChild("NaN", constNaN, SCRIPTVARLINK_CONSTANT);
	root->addChild("Infinity", constInfinityPositive, SCRIPTVARLINK_CONSTANT);
	root->addChild("StopIteration", constStopIteration=newScriptVar(Object, var=newScriptVar(Object), "StopIteration"), SCRIPTVARLINK_CONSTANT);
	constStopIteration->addChild(TINYJS_PROTOTYPE_CLASS, var, SCRIPTVARLINK_CONSTANT);	pseudo_refered.push_back(&constStopIteration);
	constNegativZero	= newScriptVarNumber(this, NegativeZero);	pseudo_refered.push_back(&constNegativZero);
	constFalse	= newScriptVarBool(this, false);	pseudo_refered.push_back(&constFalse);
	constTrue	= newScriptVarBool(this, true);	pseudo_refered.push_back(&constTrue);
	
	//////////////////////////////////////////////////////////////////////////
	// add global functions
	addNative("function eval(jsCode)", this, &CTinyJS::native_eval);
	native_require_read = 0;
	addNative("function require(jsFile)", this, &CTinyJS::native_require);
	addNative("function isNaN(objc)", this, &CTinyJS::native_isNAN);
	addNative("function isFinite(objc)", this, &CTinyJS::native_isFinite);
	addNative("function parseInt(string, radix)", this, &CTinyJS::native_parseInt);
	addNative("function parseFloat(string)", this, &CTinyJS::native_parseFloat);
	
	root->addChild("JSON", newScriptVar(Object), SCRIPTVARLINK_BUILDINDEFAULT);
	addNative("function JSON.parse(text, reviver)", this, &CTinyJS::native_JSON_parse);
	
	_registerFunctions(this);
	_registerStringFunctions(this);
	_registerMathFunctions(this);
}

CTinyJS::~CTinyJS() {
	ASSERT(!t);
	for(vector<CScriptVarPtr*>::iterator it = pseudo_refered.begin(); it!=pseudo_refered.end(); ++it)
		**it = CScriptVarPtr();
	for(int i=Error; i<ERROR_COUNT; i++)
		errorPrototypes[i] = CScriptVarPtr();
	root->removeAllChildren();
	scopes.clear();
	ClearUnreferedVars();
	root = CScriptVarPtr();
#ifdef _DEBUG
	for(CScriptVar *p = first; p; p=p->next)
		printf("%p\n", p);
#endif
#if DEBUG_MEMORY
	show_allocated();
#endif
}

//////////////////////////////////////////////////////////////////////////
/// throws an Error & Exception
//////////////////////////////////////////////////////////////////////////

void CTinyJS::throwError(CScriptResult &execute, ERROR_TYPES ErrorType, const string &message ) {
	if(execute && haveTry) {
		execute.set(CScriptResult::Throw, newScriptVarError(this, ErrorType, message.c_str(), t->currentFile.c_str(), t->currentLine(), t->currentColumn()));
		return;
	}
	throw new CScriptException(ErrorType, message, t->currentFile, t->currentLine(), t->currentColumn());
}
void CTinyJS::throwException(ERROR_TYPES ErrorType, const string &message ) {
	throw new CScriptException(ErrorType, message, t->currentFile, t->currentLine(), t->currentColumn());
}

void CTinyJS::throwError(CScriptResult &execute, ERROR_TYPES ErrorType, const string &message, CScriptTokenizer::ScriptTokenPosition &Pos ){
	if(execute && haveTry) {
		execute.set(CScriptResult::Throw, newScriptVarError(this, ErrorType, message.c_str(), t->currentFile.c_str(), Pos.currentLine(), Pos.currentColumn()));
		return;
	}
	throw new CScriptException(ErrorType, message, t->currentFile, Pos.currentLine(), Pos.currentColumn());
}
void CTinyJS::throwException(ERROR_TYPES ErrorType, const string &message, CScriptTokenizer::ScriptTokenPosition &Pos ){
	throw new CScriptException(ErrorType, message, t->currentFile, Pos.currentLine(), Pos.currentColumn());
}

//////////////////////////////////////////////////////////////////////////

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

CScriptVarLinkPtr CTinyJS::evaluateComplex(CScriptTokenizer &Tokenizer) {
	t = &Tokenizer;
	CScriptResult execute;
	try {
		do {
			execute_statement(execute);
			while (t->tk==';') t->match(';'); // skip empty statements
		} while (t->tk!=LEX_EOF);
	} catch (...) {
		haveTry = false;
		t=0; // clean up Tokenizer
		throw; // 
	}
	t=0;
	ClearUnreferedVars(execute.value);

	uint32_t UniqueID = allocUniqueID(); 
	setTemporaryID_recursive(UniqueID);
	if(execute.value) execute.value->setTemporaryMark_recursive(UniqueID);
	for(CScriptVar *p = first; p; p=p->next)
	{
		if(p->getTemporaryMark() != UniqueID)
			printf("%s %p\n", p->getVarType().c_str(), p);
	}
	freeUniqueID();

	if (execute.value)
		return CScriptVarLinkPtr(execute.value);
	// return undefined...
	return CScriptVarLinkPtr(constScriptVar(Undefined));
}
CScriptVarLinkPtr CTinyJS::evaluateComplex(const char *Code, const string &File, int Line, int Column) {
	CScriptTokenizer Tokenizer(Code, File, Line, Column);
	return evaluateComplex(Tokenizer);
}
CScriptVarLinkPtr CTinyJS::evaluateComplex(const string &Code, const string &File, int Line, int Column) {
	CScriptTokenizer Tokenizer(Code.c_str(), File, Line, Column);
	return evaluateComplex(Tokenizer);
}

string CTinyJS::evaluate(CScriptTokenizer &Tokenizer) {
	return evaluateComplex(Tokenizer)->toString();
}
string CTinyJS::evaluate(const char *Code, const string &File, int Line, int Column) {
	return evaluateComplex(Code, File, Line, Column)->toString();
}
string CTinyJS::evaluate(const string &Code, const string &File, int Line, int Column) {
	return evaluate(Code.c_str(), File, Line, Column);
}

CScriptVarFunctionNativePtr CTinyJS::addNative(const string &funcDesc, JSCallback ptr, void *userdata, int LinkFlags) {
	return addNative(funcDesc, ::newScriptVar(this, ptr, userdata), LinkFlags);
}

CScriptVarFunctionNativePtr CTinyJS::addNative(const string &funcDesc, CScriptVarFunctionNativePtr Var, int LinkFlags) {
	CScriptLex lex(funcDesc.c_str());
	CScriptVarPtr base = root;

	lex.match(LEX_R_FUNCTION);
	string funcName = lex.tkStr;
	lex.match(LEX_ID);
	/* Check for dots, we might want to do something like function String.substring ... */
	while (lex.tk == '.') {
		lex.match('.');
		CScriptVarLinkPtr link = base->findChild(funcName);
		// if it doesn't exist, make an object class
		if (!link) link = base->addChild(funcName, newScriptVar(Object));
		base = link->getVarPtr();
		funcName = lex.tkStr;
		lex.match(LEX_ID);
	}

	auto_ptr<CScriptTokenDataFnc> pFunctionData(new CScriptTokenDataFnc);
	pFunctionData->name = funcName;
	lex.match('(');
	while (lex.tk!=')') {
		pFunctionData->arguments.push_back(CScriptToken(LEX_ID, lex.tkStr));
		lex.match(LEX_ID);
		if (lex.tk!=')') lex.match(',',')');
	}
	lex.match(')');
	Var->setFunctionData(pFunctionData.release());
	Var->addChild(TINYJS_PROTOTYPE_CLASS, newScriptVar(Object), SCRIPTVARLINK_WRITABLE);

	base->addChild(funcName,  Var, LinkFlags);
	return Var;

}

CScriptVarLinkWorkPtr CTinyJS::parseFunctionDefinition(const CScriptToken &FncToken) {
	const CScriptTokenDataFnc &Fnc = FncToken.Fnc();
//	string fncName = (FncToken.token == LEX_T_FUNCTION_OPERATOR) ? TINYJS_TEMP_NAME : Fnc.name;
	CScriptVarLinkWorkPtr funcVar(newScriptVar((CScriptTokenDataFnc*)&Fnc), Fnc.name);
	if(scope() != root)
		funcVar->getVarPtr()->addChild(TINYJS_FUNCTION_CLOSURE_VAR, scope(), 0);
	funcVar->getVarPtr()->addChild(TINYJS_PROTOTYPE_CLASS, newScriptVar(Object), SCRIPTVARLINK_WRITABLE)->getVarPtr()->addChild(TINYJS_CONSTRUCTOR_VAR, funcVar->getVarPtr(), SCRIPTVARLINK_WRITABLE);
	return funcVar;
}

CScriptVarLinkWorkPtr CTinyJS::parseFunctionsBodyFromString(const string &ArgumentList, const string &FncBody) {
	string Fnc = "function ("+ArgumentList+"){"+FncBody+"}";
	CScriptTokenizer tokenizer(Fnc.c_str());
	return parseFunctionDefinition(tokenizer.getToken());
}
CScriptVarPtr CTinyJS::callFunction(const CScriptVarFunctionPtr &Function, vector<CScriptVarPtr> &Arguments, const CScriptVarPtr &This, CScriptVarPtr *newThis) {
	CScriptResult execute;
	CScriptVarPtr retVar = callFunction(execute, Function, Arguments, This, newThis);
	execute.cThrow();
	return retVar;
}

CScriptVarPtr CTinyJS::callFunction(CScriptResult &execute, const CScriptVarFunctionPtr &Function, vector<CScriptVarPtr> &Arguments, const CScriptVarPtr &This, CScriptVarPtr *newThis) {
	ASSERT(Function && Function->isFunction());

	if(Function->isBounded()) return CScriptVarFunctionBoundedPtr(Function)->callFunction(execute, Arguments, This, newThis);

	CScriptTokenDataFnc *Fnc = Function->getFunctionData();
	CScriptVarScopeFncPtr functionRoot(::newScriptVar(this, ScopeFnc, CScriptVarPtr(Function->findChild(TINYJS_FUNCTION_CLOSURE_VAR))));
	if(Fnc->name.size()) functionRoot->addChild(Fnc->name, Function);
	functionRoot->addChild("this", This);
	CScriptVarPtr arguments = functionRoot->addChild(TINYJS_ARGUMENTS_VAR, newScriptVar(Object));

	CScriptResult function_execute;
	int length_proto = Fnc->arguments.size();
	int length_arguments = Arguments.size();
	int length = max(length_proto, length_arguments);
	for(int arguments_idx = 0; arguments_idx<length; ++arguments_idx) {
		string arguments_idx_str = int2string(arguments_idx);
		CScriptVarLinkWorkPtr value;
		if(arguments_idx < length_arguments) {
			value = arguments->addChild(arguments_idx_str, Arguments[arguments_idx]);
		} else {
			value = constScriptVar(Undefined);
		}
		if(arguments_idx < length_proto) {
			CScriptToken &FncArguments = Fnc->arguments[arguments_idx];
			if(FncArguments.token == LEX_ID)
				functionRoot->addChildOrReplace(FncArguments.String(), value);
			else
				assign_destructuring_var(functionRoot, FncArguments.DestructuringVar(), value, function_execute);
		}
	}
	arguments->addChild("length", newScriptVar(length_arguments));

#ifndef NO_GENERATORS
	if(Fnc->isGenerator) {
		return ::newScriptVarCScriptVarGenerator(this, functionRoot, Function);
	} 
#endif /*NO_GENERATORS*/
	// execute function!
	// add the function's execute space to the symbol table so we can recurse
	CScopeControl ScopeControl(this);
	ScopeControl.addFncScope(functionRoot);
	if (Function->isNative()) {
		try {
			CScriptVarFunctionNativePtr(Function)->callFunction(functionRoot);
			CScriptVarLinkPtr ret = functionRoot->findChild(TINYJS_RETURN_VAR);
			function_execute.set(CScriptResult::Return, ret ? CScriptVarPtr(ret) : constUndefined);
		} catch (CScriptVarPtr v) {
			if(haveTry) {
				function_execute.setThrow(v, "native function '"+Fnc->name+"'");
			} else if(v->isError()) {
				CScriptException *err = CScriptVarErrorPtr(v)->toCScriptException();
				if(err->fileName.empty()) err->fileName = "native function '"+Fnc->name+"'";
				throw err;
			}
			else
				throw new CScriptException(Error, "uncaught exception: '"+v->toString(function_execute)+"' in native function '"+Fnc->name+"'");
		}
	} else {
		/* we just want to execute the block, but something could
			* have messed up and left us with the wrong ScriptLex, so
			* we want to be careful here... */
		string oldFile = t->currentFile;
		t->currentFile = Fnc->file;
		t->pushTokenScope(Fnc->body);
		if(Fnc->body.front().token == '{')
			execute_block(function_execute);
		else {
			CScriptVarPtr ret = execute_base(function_execute);
			if(function_execute) function_execute.set(CScriptResult::Return, ret);
		}
		t->currentFile = oldFile;

		// because return will probably have called this, and set execute to false
	}
	if(function_execute.isReturnNormal()) {
		if(newThis) *newThis = functionRoot->findChild("this");
		if(function_execute.isReturn()) {
			CScriptVarPtr ret = function_execute.value;
			return ret;
		}
	} else
		execute = function_execute;
	return constScriptVar(Undefined);
}
#ifndef NO_GENERATORS
void CTinyJS::generator_start(CScriptVarGenerator *Generator)
{
	// push current Generator
	generatorStack.push_back(Generator);

	// safe callers stackBase & set generators one	
	Generator->callersStackBase = stackBase;
	stackBase = 0;

	// safe callers ScopeSize
	Generator->callersScopeSize = scopes.size();

	// safe callers Tokenizer & set generators one
	Generator->callersTokenizer = t;
	CScriptTokenizer generatorTokenizer;
	t = &generatorTokenizer;

	// safe callers haveTry
	Generator->callersHaveTry = haveTry;
	haveTry = true;

	// push generator's FunctionRoot
	CScopeControl ScopeControl(this);
	ScopeControl.addFncScope(Generator->getFunctionRoot());

	// call generator-function
	CScriptTokenDataFnc *Fnc = Generator->getFunction()->getFunctionData();
	CScriptResult function_execute;
	TOKEN_VECT eof(1, CScriptToken());
	t->pushTokenScope(eof);
	t->pushTokenScope(Fnc->body);
	t->currentFile = Fnc->file;
	try {
		if(Fnc->body.front().token == '{')
			execute_block(function_execute);
		else {
			execute_base(function_execute);
		}
		if(function_execute.isThrow())
			Generator->setException(function_execute.value);
		else
			Generator->setException(constStopIteration);
//	} catch(CScriptVarPtr &e) {
//		Generator->setException(e);
	} catch(CScriptCoroutine::StopIteration_t &) {
		Generator->setException(CScriptVarPtr());
//	} catch(CScriptException *e) {
//		Generator->setException(newScriptVarError(this, *e));
	} catch(...) {
		// pop current Generator
		generatorStack.pop_back();

		// restore callers stackBase
		stackBase = Generator->callersStackBase;

		// restore callers Scope (restored by ScopeControl
		//		scopes.erase(scopes.begin()+Generator->callersScopeSize, scopes.end());

		// restore callers Tokenizer
		t = Generator->callersTokenizer;

		// restore callers haveTry
		haveTry = Generator->callersHaveTry;

		Generator->setException(constStopIteration);
		// re-throw
		throw;
	}
	// pop current Generator
	generatorStack.pop_back();

	// restore callers stackBase
	stackBase = Generator->callersStackBase;

	// restore callers Scope (restored by ScopeControl
	//		scopes.erase(scopes.begin()+Generator->callersScopeSize, scopes.end());

	// restore callers Tokenizer
	t = Generator->callersTokenizer;

	// restore callers haveTry
	haveTry = Generator->callersHaveTry;
}
CScriptVarPtr CTinyJS::generator_yield(CScriptResult &execute, CScriptVar *YieldIn)
{
	if(!execute) return constUndefined;
	CScriptVarGenerator *Generator = generatorStack.back(); 
	if(Generator->isClosed()) {
		throwError(execute, TypeError, "yield from closing generator function");
		return constUndefined;
	}

	// pop current Generator
	generatorStack.pop_back(); 

	// safe generators and restore callers stackBase
	void *generatorStckBase = stackBase;
	stackBase = Generator->callersStackBase;

	// safe generators and restore callers scopes
	Generator->generatorScopes.assign(scopes.begin()+Generator->callersScopeSize, scopes.end());
	scopes.erase(scopes.begin()+Generator->callersScopeSize, scopes.end());

	// safe generators and restore callers Tokenizer
	CScriptTokenizer *generatorTokenizer = t;
	t = Generator->callersTokenizer;

	// safe generators and restore callers haveTry
	bool generatorsHaveTry = haveTry;
	haveTry = Generator->callersHaveTry;

	CScriptVarPtr ret;
	try {
		ret = Generator->yield(execute, YieldIn);
	} catch(...) {
		// normaly catch(CScriptCoroutine::CScriptCoroutineFinish_t &)
		// force StopIteration with call CScriptCoroutine::Stop() before CScriptCoroutine::next()
		// but catch(...) is for paranoia
		
		// push current Generator
		generatorStack.push_back(Generator);

		// safe callers and restore generators stackBase
		Generator->callersStackBase = stackBase;
		stackBase = generatorStckBase;

		// safe callers and restore generator Scopes
		Generator->callersScopeSize = scopes.size();
		scopes.insert(scopes.end(), Generator->generatorScopes.begin(), Generator->generatorScopes.end());
		Generator->generatorScopes.clear();

		// safe callers and restore generator Tokenizer
		Generator->callersTokenizer = t;
		t = generatorTokenizer;

		// safe callers and restore generator haveTry
		Generator->callersHaveTry = haveTry;
		haveTry = generatorsHaveTry;

		// re-throw
		throw;
	} 
	// push current Generator
	generatorStack.push_back(Generator);

	// safe callers and restore generators stackBase
	Generator->callersStackBase = stackBase;
	stackBase = generatorStckBase;

	// safe callers and restore generator Scopes
	Generator->callersScopeSize = scopes.size();
	scopes.insert(scopes.end(), Generator->generatorScopes.begin(), Generator->generatorScopes.end());
	Generator->generatorScopes.clear();

	// safe callers and restore generator Tokenizer
	Generator->callersTokenizer = t;
	t = generatorTokenizer;

	// safe callers and restore generator haveTry
	Generator->callersHaveTry = haveTry;
	haveTry = generatorsHaveTry;

	return ret;
}
#endif /*NO_GENERATORS*/



CScriptVarPtr CTinyJS::mathsOp(CScriptResult &execute, const CScriptVarPtr &A, const CScriptVarPtr &B, int op) {
	if(!execute) return constUndefined;
	if (op == LEX_TYPEEQUAL || op == LEX_NTYPEEQUAL) {
		// check type first
		if( (A->getVarType() == B->getVarType()) ^ (op == LEX_TYPEEQUAL)) return constFalse;
		// check value second
		return mathsOp(execute, A, B, op == LEX_TYPEEQUAL ? LEX_EQUAL : LEX_NEQUAL);
	}
	if (!A->isPrimitive() && !B->isPrimitive()) { // Objects both
		// check pointers
		switch (op) {
		case LEX_EQUAL:	return constScriptVar(A==B);
		case LEX_NEQUAL:	return constScriptVar(A!=B);
		}
	}

	CScriptVarPtr a = A->toPrimitive_hintNumber(execute);
	CScriptVarPtr b = B->toPrimitive_hintNumber(execute);
	if(!execute) return constUndefined;
	// do maths...
	bool a_isString = a->isString();
	bool b_isString = b->isString();
	// both a String or one a String and op='+'
	if( (a_isString && b_isString) || ((a_isString || b_isString) && op == '+')) {
		string da = a->isNull() ? "" : a->toString(execute);
		string db = b->isNull() ? "" : b->toString(execute);
		switch (op) {
		case '+':
			try{
				return newScriptVar(da+db);
			} catch(exception& e) {
				throwError(execute, Error, e.what());
				return constUndefined;
			}
		case LEX_EQUAL:	return constScriptVar(da==db);
		case LEX_NEQUAL:	return constScriptVar(da!=db);
		case '<':			return constScriptVar(da<db);
		case LEX_LEQUAL:	return constScriptVar(da<=db);
		case '>':			return constScriptVar(da>db);
		case LEX_GEQUAL:	return constScriptVar(da>=db);
		}
	}
	// special for undefined and null --> every true: undefined==undefined, undefined==null, null==undefined and null=null
	else if( (a->isUndefined() || a->isNull()) && (b->isUndefined() || b->isNull()) ) {
		switch (op) {
		case LEX_EQUAL:	return constScriptVar(true);
		case LEX_NEQUAL:
		case LEX_GEQUAL:	
		case LEX_LEQUAL:
		case '<':
		case '>':			return constScriptVar(false);
		}
	} 
	CNumber da = a->toNumber();
	CNumber db = b->toNumber();
	switch (op) {
	case '+':			return a->newScriptVar(da+db);
	case '-':			return a->newScriptVar(da-db);
	case '*':			return a->newScriptVar(da*db);
	case '/':			return a->newScriptVar(da/db);
	case '%':			return a->newScriptVar(da%db);
	case '&':			return a->newScriptVar(da.toInt32()&db.toInt32());
	case '|':			return a->newScriptVar(da.toInt32()|db.toInt32());
	case '^':			return a->newScriptVar(da.toInt32()^db.toInt32());
	case '~':			return a->newScriptVar(~da);
	case LEX_LSHIFT:	return a->newScriptVar(da<<db);
	case LEX_RSHIFT:	return a->newScriptVar(da>>db);
	case LEX_RSHIFTU:	return a->newScriptVar(da.ushift(db));
	case LEX_EQUAL:	return a->constScriptVar(da==db);
	case LEX_NEQUAL:	return a->constScriptVar(da!=db);
	case '<':			return a->constScriptVar(da<db);
	case LEX_LEQUAL:	return a->constScriptVar(da<=db);
	case '>':			return a->constScriptVar(da>db);
	case LEX_GEQUAL:	return a->constScriptVar(da>=db);
	default: throw new CScriptException("This operation not supported on the int datatype");
	}	
}

void CTinyJS::assign_destructuring_var(const CScriptVarPtr &Scope, const CScriptTokenDataDestructuringVar &Objc, const CScriptVarPtr &Val, CScriptResult &execute) {
	if(!execute) return;
	if(Objc.vars.size() == 1) {
		if(Scope)
			Scope->addChildOrReplace(Objc.vars.front().second, Val);
		else {
			CScriptVarLinkWorkPtr v(findInScopes(Objc.vars.front().second));
			ASSERT(v==true);
			if(v) v->setVarPtr(Val);
		}
	} else {
		vector<CScriptVarPtr> Path(1, Val);
		for(DESTRUCTURING_VARS_cit it=Objc.vars.begin()+1; it!=Objc.vars.end(); ++it) {
			if(it->second == "}" || it->second == "]")
				Path.pop_back();
			else {
				if(it->second.empty()) continue; // skip empty entries
				CScriptVarLinkWorkPtr var = Path.back()->findChildWithStringChars(it->first);
				if(var) var = var.getter(execute); else var = constUndefined;
				if(!execute) return;
				if(it->second == "{" || it->second == "[") {
					Path.push_back(var);
				} else if(Scope)
					Scope->addChildOrReplace(it->second, var);
				else {
					CScriptVarLinkWorkPtr v(findInScopes(it->second));
					ASSERT(v==true);
					if(v) v->setVarPtr(var);
				}
			}
		}
	}
}

void CTinyJS::execute_var_init( bool hideLetScope, CScriptResult &execute )
{
	for(;;) {
		t->check(LEX_T_DESTRUCTURING_VAR);
		CScriptTokenDataDestructuringVar &Objc = t->getToken().DestructuringVar();
		t->match(LEX_T_DESTRUCTURING_VAR);
		if(t->tk == '=') {
			t->match('=');
			if(hideLetScope) CScriptVarScopeLetPtr(scope())->setletExpressionInitMode(true);
			CScriptVarPtr Val = execute_assignment(execute);
			if(hideLetScope) CScriptVarScopeLetPtr(scope())->setletExpressionInitMode(false);
			assign_destructuring_var(0, Objc, Val, execute);
		}
		if (t->tk == ',') 
			t->match(',');
		else
			break;
	}
}
void CTinyJS::execute_destructuring(CScriptTokenDataObjectLiteral &Objc, const CScriptVarPtr &Val, CScriptResult &execute) {
	for(vector<CScriptTokenDataObjectLiteral::ELEMENT>::iterator it=Objc.elements.begin(); execute && it!=Objc.elements.end(); ++it) {
		if(it->value.empty()) continue;
		CScriptVarPtr rhs = Val->findChildWithStringChars(it->id).getter(execute);
		if(!rhs) rhs=constUndefined;
		if(it->value.front().token == LEX_T_OBJECT_LITERAL && it->value.front().Object().destructuring) {
			execute_destructuring(it->value.front().Object(), rhs, execute);
		} else {
			t->pushTokenScope(it->value);
			CScriptVarLinkWorkPtr lhs = execute_condition(execute);
			if(lhs->isWritable()) {
				if (!lhs->isOwned()) {
					CScriptVarPtr fakedOwner = lhs.getReferencedOwner();
					if(fakedOwner) {
						if(!fakedOwner->isExtensible())
							continue;
						lhs = fakedOwner->addChildOrReplace(lhs->getName(), lhs);
					} else
						lhs = root->addChildOrReplace(lhs->getName(), lhs);
				}
				lhs.setter(execute, rhs);
			}
		}
	}
}

CScriptVarLinkWorkPtr CTinyJS::execute_literals(CScriptResult &execute) {
	switch(t->tk) {
	case LEX_ID: 
		if(execute) {
			CScriptVarLinkWorkPtr a(findInScopes(t->tkStr()));
			if (!a) {
				/* Variable doesn't exist! JavaScript says we should create it
				 * (we won't add it here. This is done in the assignment operator)*/
				if(t->tkStr() == "this") 
					a = root; // fake this
				else
					a = CScriptVarLinkPtr(constScriptVar(Undefined), t->tkStr());
			} 
/*
			prvention for assignment to this is now done by the tokenizer
			else if(t->tkStr() == "this")
				a(a->getVarPtr()); // prevent assign to this
*/
			t->match(LEX_ID);
			return a;
		}
		t->match(LEX_ID);
		break;
	case LEX_INT:
		{
			CScriptVarPtr a = newScriptVar(t->getToken().Int());
			a->setExtensible(false);
			t->match(LEX_INT);
			return a;
		}
		break;
	case LEX_FLOAT:
		{
			CScriptVarPtr a = newScriptVar(t->getToken().Float());
			t->match(LEX_FLOAT);
			return a;
		}
		break;
	case LEX_STR:
		{
			CScriptVarPtr a = newScriptVar(t->getToken().String());
			t->match(LEX_STR);
			return a;
		}
		break;
#ifndef NO_REGEXP
	case LEX_REGEXP:
		{
			string::size_type pos = t->getToken().String().find_last_of('/');
			string source = t->getToken().String().substr(1, pos-1);
			string flags = t->getToken().String().substr(pos+1);
			CScriptVarPtr a = newScriptVar(source, flags);
			t->match(LEX_REGEXP);
			return a;
		}
		break;
#endif /* NO_REGEXP */
	case LEX_T_OBJECT_LITERAL:
		if(execute) {
			CScriptTokenDataObjectLiteral &Objc = t->getToken().Object();
			t->match(LEX_T_OBJECT_LITERAL);
			if(Objc.destructuring) {
				t->match('=');
				CScriptVarPtr a = execute_assignment(execute);
				if(execute) execute_destructuring(Objc, a, execute);
				return a;
			} else {
				CScriptVarPtr a = Objc.type==CScriptTokenDataObjectLiteral::OBJECT ? newScriptVar(Object) : newScriptVar(Array);
				for(vector<CScriptTokenDataObjectLiteral::ELEMENT>::iterator it=Objc.elements.begin(); execute && it!=Objc.elements.end(); ++it) {
					if(it->value.empty()) continue;
					CScriptToken &tk = it->value.front();
					if(tk.token==LEX_T_GET || tk.token==LEX_T_SET) {
						CScriptTokenDataFnc &Fnc = tk.Fnc();
						if((tk.token == LEX_T_GET && Fnc.arguments.size()==0) || (tk.token == LEX_T_SET && Fnc.arguments.size()==1)) {
							CScriptVarLinkWorkPtr funcVar = parseFunctionDefinition(tk);
							CScriptVarLinkWorkPtr child = a->findChild(Fnc.name);
							if(child && !child->getVarPtr()->isAccessor()) child.clear();
							if(!child) child = a->addChildOrReplace(Fnc.name, newScriptVar(Accessor));
							child->getVarPtr()->addChildOrReplace((tk.token==LEX_T_GET?TINYJS_ACCESSOR_GET_VAR:TINYJS_ACCESSOR_SET_VAR), funcVar->getVarPtr());
						}
					} else {
						t->pushTokenScope(it->value);
						a->addChildOrReplace(it->id, execute_assignment(execute));
						while(0);
					}
				}
				return a;
			}
		} else
			t->match(LEX_T_OBJECT_LITERAL);
		break;
	case LEX_R_LET: // let as expression
		if(execute) {
			CScopeControl ScopeControl(this);
			t->match(LEX_R_LET);
			t->match('(');
			t->check(LEX_T_FORWARD);
			ScopeControl.addLetScope();
			execute_statement(execute); // execute forwarder
			execute_var_init(true, execute);
			t->match(')');
			return execute_assignment(execute);
		} else {
			t->skip(t->getToken().Int());
		}
		break;
	case LEX_T_FUNCTION_OPERATOR:
		if(execute) {
			CScriptVarLinkWorkPtr a = parseFunctionDefinition(t->getToken());
			t->match(LEX_T_FUNCTION_OPERATOR);
			return a;
		}
		t->match(LEX_T_FUNCTION_OPERATOR);
		break;
#ifndef NO_GENERATORS
	case LEX_R_YIELD:
		if (execute) {
			t->match(LEX_R_YIELD);
			CScriptVarPtr result = constUndefined;
			if (t->tk != ';')
				result = execute_base(execute);
			if(execute)
				return generator_yield(execute, result.getVar());
			else
				return constUndefined;
		} else
			t->skip(t->getToken().Int());
		break;
#endif /*NO_GENERATORS*/
	case LEX_R_NEW: // new -> create a new object
		if (execute) {
			t->match(LEX_R_NEW);
			CScriptVarLinkWorkPtr parent = execute_literals(execute);
			CScriptVarLinkWorkPtr objClass = execute_member(parent, execute).getter(execute);
			if (execute) {
				CScriptVarPtr Constructor = objClass->getVarPtr();
				if(Constructor->isFunction()) {
					CScriptVarPtr obj(newScriptVar(Object));
					CScriptVarLinkPtr prototype = Constructor->findChild(TINYJS_PROTOTYPE_CLASS);
					if(!prototype || prototype->getVarPtr()->isUndefined() || prototype->getVarPtr()->isNull()) {
						prototype = Constructor->addChild(TINYJS_PROTOTYPE_CLASS, newScriptVar(Object), SCRIPTVARLINK_WRITABLE);
						obj->addChildOrReplace(TINYJS___PROTO___VAR, prototype, SCRIPTVARLINK_WRITABLE);
					}
					CScriptVarLinkPtr __constructor__ = Constructor->findChild("__constructor__");
					if(__constructor__ && __constructor__->getVarPtr()->isFunction())
						Constructor = __constructor__;
					if (stackBase) {
						int dummy;
						if(&dummy < stackBase)
							throwError(execute, Error, "too much recursion");
					}
					vector<CScriptVarPtr> arguments;
					if (t->tk == '(') {
						t->match('(');
						while(t->tk!=')') {
							CScriptVarPtr value = execute_assignment(execute).getter(execute);
							if (execute)
								arguments.push_back(value);
							if (t->tk!=')') t->match(',', ')');
						}
						t->match(')');
					}
					if(execute) {
						CScriptVarPtr returnVar = callFunction(execute, Constructor, arguments, obj, &obj);
						if(returnVar->isObject())
							return CScriptVarLinkWorkPtr(returnVar);
						return CScriptVarLinkWorkPtr(obj);
					}
				} else
					throwError(execute, TypeError, objClass->getName() + " is not a constructor");
			} else
				if (t->tk == '(') t->skip(t->getToken().Int());
		} else
			t->skip(t->getToken().Int());
		break;
	case LEX_R_TRUE:
		t->match(LEX_R_TRUE);
		return constScriptVar(true);
	case LEX_R_FALSE:
		t->match(LEX_R_FALSE);
		return constScriptVar(false);
	case LEX_R_NULL:
		t->match(LEX_R_NULL);
		return constScriptVar(Null);
	case '(':
		if(execute) {
			t->match('(');
			CScriptVarLinkWorkPtr a = execute_base(execute).getter(execute);
			t->match(')');
			return a;
		} else
			t->skip(t->getToken().Int());
		break;
	case LEX_T_EXCEPTION_VAR:
		t->match(LEX_T_EXCEPTION_VAR);
		if(execute.value) return execute.value;
		break;
	default:
		t->match(LEX_EOF);
		break;
	}
	return constScriptVar(Undefined);

}
CScriptVarLinkWorkPtr CTinyJS::execute_member(CScriptVarLinkWorkPtr &parent, CScriptResult &execute) {
	CScriptVarLinkWorkPtr a;
	parent.swap(a);
	if(t->tk == '.' || t->tk == '[') {
		while(t->tk == '.' || t->tk == '[') {
			parent.swap(a);
			a = parent.getter(execute); // a is now the "getted" var
			if(execute && (a->getVarPtr()->isUndefined() || a->getVarPtr()->isNull())) {
				throwError(execute, ReferenceError, a->getName() + " is " + a->toString(execute));
			}
			string name;
			if(t->tk == '.') {
				t->match('.');
				name = t->tkStr();
				t->match(LEX_ID);
			} else {
				if(execute) {
					t->match('[');
					name = execute_base(execute)->toString(execute);
					t->match(']');
				} else
					t->skip(t->getToken().Int());
			}
			if (execute) {
				CScriptVarPtr aVar = a;
				a = aVar->findChildWithPrototypeChain(name);
				if(!a) {
					a(constScriptVar(Undefined), name);
					a.setReferencedOwner(aVar);
				}
			}
		}
	}
	return a;
}

CScriptVarLinkWorkPtr CTinyJS::execute_function_call(CScriptResult &execute) {
	CScriptVarLinkWorkPtr parent = execute_literals(execute);
	CScriptVarLinkWorkPtr a = execute_member(parent, execute);
	while (t->tk == '(') {
		if (execute) {
			if(a->getVarPtr()->isUndefined() || a->getVarPtr()->isNull())
				throwError(execute, ReferenceError, a->getName() + " is " + a->toString(execute));
			CScriptVarPtr fnc = a.getter(execute)->getVarPtr();
			if (!fnc->isFunction())
				throwError(execute, TypeError, a->getName() + " is not a function");
			if (stackBase) {
				int dummy;
				if(&dummy < stackBase)
					throwError(execute, Error, "too much recursion");
			}

			t->match('('); // path += '(';

			// grab in all parameters
			vector<CScriptVarPtr> arguments;
			while(t->tk!=')') {
				CScriptVarLinkWorkPtr value = execute_assignment(execute).getter(execute);
//				path += (*value)->getString();
				if (execute) {
					arguments.push_back(value);
				}
				if (t->tk!=')') { t->match(','); /*path+=',';*/ }
			}
			t->match(')'); //path+=')';
			// setup a return variable
			CScriptVarLinkWorkPtr returnVar;
			if(execute) {
				if (!parent)
					parent = findInScopes("this");
				// if no parent use the root-scope
				CScriptVarPtr This(parent ? parent->getVarPtr() : (CScriptVarPtr )root);
				a = callFunction(execute, fnc, arguments, This);
			}
		} else {
			// function, but not executing - just parse args and be done
			t->match('(');
			while (t->tk != ')') {
				CScriptVarLinkWorkPtr value = execute_base(execute);
				//	if (t->tk!=')') t->match(',');
			}
			t->match(')');
		}
		a = execute_member(parent = a, execute);
	}
	return a;
}
// R->L: Precedence 3 (in-/decrement) ++ --
// R<-L: Precedence 4 (unary) ! ~ + - typeof void delete 
bool CTinyJS::execute_unary_rhs(CScriptResult &execute, CScriptVarLinkWorkPtr& a) {
	t->match(t->tk);
	a = execute_unary(execute).getter(execute);
	if(execute) CheckRightHandVar(execute, a);
	return execute;
};
CScriptVarLinkWorkPtr CTinyJS::execute_unary(CScriptResult &execute) {
	CScriptVarLinkWorkPtr a;
	switch(t->tk) {
	case '-':
		if(execute_unary_rhs(execute, a)) 
			a(newScriptVar(-a->getVarPtr()->toNumber(execute)));
		break;
	case '+':
		if(execute_unary_rhs(execute, a)) 
			a = newScriptVar(a->getVarPtr()->toNumber(execute));
		break;
	case '!':
		if(execute_unary_rhs(execute, a)) 
			a = constScriptVar(!a->getVarPtr()->toBoolean());
		break;
	case '~':
		if(execute_unary_rhs(execute, a)) 
			a = newScriptVar(~a->getVarPtr()->toNumber(execute));
		break;
	case LEX_R_TYPEOF:
		if(execute_unary_rhs(execute, a)) 
			a = newScriptVar(a->getVarPtr()->getVarType());
		break;
	case LEX_R_VOID:
		if(execute_unary_rhs(execute, a)) 
			a = constScriptVar(Undefined);
		break;
	case LEX_R_DELETE:
		t->match(LEX_R_DELETE); // delete
		a = execute_unary(execute); // no getter - delete can remove the accessor
		if (execute) {
			// !!! no right-hand-check by delete
			if(a->isOwned() && a->isConfigurable() && a->getName() != "this") {
				a->getOwner()->removeLink(a);	// removes the link from owner
				a = constScriptVar(true);
			}
			else
				a = constScriptVar(false);
		}
		break;
	case LEX_PLUSPLUS:
	case LEX_MINUSMINUS:
		{
			int op = t->tk;
			t->match(op); // pre increment/decrement
			CScriptTokenizer::ScriptTokenPosition ErrorPos = t->getPos();
			a = execute_function_call(execute);
			if (execute) {
				if(a->getName().empty())
					throwError(execute, SyntaxError, string("invalid ")+(op==LEX_PLUSPLUS ? "increment" : "decrement")+" operand", ErrorPos);
				else if(!a->isOwned() && !a.hasReferencedOwner() && !a->getName().empty())
					throwError(execute, ReferenceError, a->getName() + " is not defined", ErrorPos);
				CScriptVarPtr res = newScriptVar(a.getter(execute)->getVarPtr()->toNumber(execute).add(op==LEX_PLUSPLUS ? 1 : -1));
				if(a->isWritable()) {
					if(!a->isOwned() && a.hasReferencedOwner() && a.getReferencedOwner()->isExtensible())
						a.getReferencedOwner()->addChildOrReplace(a->getName(), res);
					else
						a.setter(execute, res);
				}
				a = res;
			}
		}
		break;
	default:
		a = execute_function_call(execute);
		break;
	}
	// post increment/decrement
	if (t->tk==LEX_PLUSPLUS || t->tk==LEX_MINUSMINUS) {
		int op = t->tk;
		t->match(op);
		if (execute) {
			if(a->getName().empty())
				throwError(execute, SyntaxError, string("invalid ")+(op==LEX_PLUSPLUS ? "increment" : "decrement")+" operand", t->getPrevPos());
			else if(!a->isOwned() && !a.hasReferencedOwner() && !a->getName().empty())
				throwError(execute, ReferenceError, a->getName() + " is not defined", t->getPrevPos());
			CNumber num = a.getter(execute)->getVarPtr()->toNumber(execute);
			CScriptVarPtr res = newScriptVar(num.add(op==LEX_PLUSPLUS ? 1 : -1));
			if(a->isWritable()) {
				if(!a->isOwned() && a.hasReferencedOwner() && a.getReferencedOwner()->isExtensible())
					a.getReferencedOwner()->addChildOrReplace(a->getName(), res);
				else
					a.setter(execute, res);
			}
			a = newScriptVar(num);
		}
	}
	return a;
}

// L->R: Precedence 5 (term) * / %
CScriptVarLinkWorkPtr CTinyJS::execute_term(CScriptResult &execute) {
	CScriptVarLinkWorkPtr a = execute_unary(execute);
	if (t->tk=='*' || t->tk=='/' || t->tk=='%') {
		CheckRightHandVar(execute, a);
		while (t->tk=='*' || t->tk=='/' || t->tk=='%') {
			int op = t->tk;
			t->match(t->tk);
			CScriptVarLinkWorkPtr b = execute_unary(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				a = mathsOp(execute, a.getter(execute), b.getter(execute), op);
			}
		}
	}
	return a;
}

// L->R: Precedence 6 (addition/subtraction) + -
CScriptVarLinkWorkPtr CTinyJS::execute_expression(CScriptResult &execute) {
	CScriptVarLinkWorkPtr a = execute_term(execute);
	if (t->tk=='+' || t->tk=='-') {
		CheckRightHandVar(execute, a);
		while (t->tk=='+' || t->tk=='-') {
			int op = t->tk;
			t->match(t->tk);
			CScriptVarLinkWorkPtr b = execute_term(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				a = mathsOp(execute, a.getter(execute), b.getter(execute), op);
			}
		}
	}
	return a;
}

// L->R: Precedence 7 (bitwise shift) << >> >>>
CScriptVarLinkWorkPtr CTinyJS::execute_binary_shift(CScriptResult &execute) {
	CScriptVarLinkWorkPtr a = execute_expression(execute);
	if (t->tk==LEX_LSHIFT || t->tk==LEX_RSHIFT || t->tk==LEX_RSHIFTU) {
		CheckRightHandVar(execute, a);
		while (t->tk>=LEX_SHIFTS_BEGIN && t->tk<=LEX_SHIFTS_END) {
			int op = t->tk;
			t->match(t->tk);

			CScriptVarLinkWorkPtr b = execute_expression(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, a);
				 // not in-place, so just replace
				 a = mathsOp(execute, a.getter(execute), b.getter(execute), op);
			}
		}
	}
	return a;
}
// L->R: Precedence 8 (relational) < <= > <= in instanceof
// L->R: Precedence 9 (equality) == != === !===
CScriptVarLinkWorkPtr CTinyJS::execute_relation(CScriptResult &execute, int set, int set_n) {
	CScriptVarLinkWorkPtr a = set_n ? execute_relation(execute, set_n, 0) : execute_binary_shift(execute);
	if ((set==LEX_EQUAL && t->tk>=LEX_RELATIONS_1_BEGIN && t->tk<=LEX_RELATIONS_1_END)
				||	(set=='<' && (t->tk==LEX_LEQUAL || t->tk==LEX_GEQUAL || t->tk=='<' || t->tk=='>' || t->tk == LEX_R_IN || t->tk == LEX_R_INSTANCEOF))) {
		CheckRightHandVar(execute, a);
		a = a.getter(execute);
		while ((set==LEX_EQUAL && t->tk>=LEX_RELATIONS_1_BEGIN && t->tk<=LEX_RELATIONS_1_END)
					||	(set=='<' && (t->tk==LEX_LEQUAL || t->tk==LEX_GEQUAL || t->tk=='<' || t->tk=='>' || t->tk == LEX_R_IN || t->tk == LEX_R_INSTANCEOF))) {
			int op = t->tk;
			t->match(t->tk);
			CScriptVarLinkWorkPtr b = set_n ? execute_relation(execute, set_n, 0) : execute_binary_shift(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				string nameOf_b = b->getName();
				b = b.getter(execute);
				if(op == LEX_R_IN) {
					if(!b->getVarPtr()->isObject())
						throwError(execute, TypeError, "invalid 'in' operand "+nameOf_b);
					a(constScriptVar( (bool)b->getVarPtr()->findChildWithPrototypeChain(a->toString(execute))));
				} else if(op == LEX_R_INSTANCEOF) {
					CScriptVarLinkPtr prototype = b->getVarPtr()->findChild(TINYJS_PROTOTYPE_CLASS);
					if(!prototype)
						throwError(execute, TypeError, "invalid 'instanceof' operand "+nameOf_b);
					else {
						unsigned int uniqueID = allocUniqueID();
						CScriptVarPtr object = a->getVarPtr()->findChild(TINYJS___PROTO___VAR);
						while( object && object!=prototype->getVarPtr() && object->getTemporaryMark() != uniqueID) {
							object->setTemporaryMark(uniqueID); // prevents recursions
							object = object->findChild(TINYJS___PROTO___VAR);
						}
						freeUniqueID();
						a(constScriptVar(object && object==prototype->getVarPtr()));
					}
				} else
					a = mathsOp(execute, a, b, op);
			}
		}
	}
	return a;
}

// L->R: Precedence 10 (bitwise-and) &
// L->R: Precedence 11 (bitwise-xor) ^
// L->R: Precedence 12 (bitwise-or) |
CScriptVarLinkWorkPtr CTinyJS::execute_binary_logic(CScriptResult &execute, int op, int op_n1, int op_n2) {
	CScriptVarLinkWorkPtr a = op_n1 ? execute_binary_logic(execute, op_n1, op_n2, 0) : execute_relation(execute);
	if (t->tk==op) {
		CheckRightHandVar(execute, a);
		a = a.getter(execute);
		while (t->tk==op) {
			t->match(t->tk);
			CScriptVarLinkWorkPtr b = op_n1 ? execute_binary_logic(execute, op_n1, op_n2, 0) : execute_relation(execute); // L->R
			if (execute) {
				CheckRightHandVar(execute, b);
				a = mathsOp(execute, a, b.getter(execute), op);
			}
		}
	}
	return a;
}
// L->R: Precedence 13 ==> (logical-and) &&
// L->R: Precedence 14 ==> (logical-or) ||
CScriptVarLinkWorkPtr CTinyJS::execute_logic(CScriptResult &execute, int op /*= LEX_OROR*/, int op_n /*= LEX_ANDAND*/) {
	CScriptVarLinkWorkPtr a = op_n ? execute_logic(execute, op_n, 0) : execute_binary_logic(execute);
	if (t->tk==op) {
		if(execute) {
			CScriptVarLinkWorkPtr b;
			CheckRightHandVar(execute, a);
			a(a.getter(execute)); // rebuild a
			do {
				if(execute && (op==LEX_ANDAND ? a->toBoolean() : !a->toBoolean())) {
					t->match(t->tk);
					b = op_n ? execute_logic(execute, op_n, 0) : execute_binary_logic(execute);
					CheckRightHandVar(execute, b); a(b.getter(execute)); // rebuild a
				} else
					t->skip(t->getToken().Int());
			} while(t->tk==op);
		} else
			t->skip(t->getToken().Int());
	} 
	return a; 
} 

// L<-R: Precedence 15 (condition) ?: 
CScriptVarLinkWorkPtr CTinyJS::execute_condition(CScriptResult &execute) {
	CScriptVarLinkWorkPtr a = execute_logic(execute);
	if (t->tk=='?') {
		CheckRightHandVar(execute, a);
		bool cond = execute && a.getter(execute)->toBoolean();
		if(execute) {
			if(cond) {
				t->match('?');
//				a = execute_condition(execute);
				a = execute_assignment(execute);
				t->check(':');
				t->skip(t->getToken().Int());
			} else { 
				CScriptVarLinkWorkPtr b;
				t->skip(t->getToken().Int());
				t->match(':');
				return execute_assignment(execute);
//				return execute_condition(execute);
			}
		} else {
			t->skip(t->getToken().Int());
			t->skip(t->getToken().Int());
		}
	}
	return a;
}
	
// L<-R: Precedence 16 (assignment) = += -= *= /= %= <<= >>= >>>= &= |= ^=
// now we can return CScriptVarLinkPtr execute_assignment returns always no setters/getters
// force life of the Owner is no more needed
CScriptVarLinkPtr CTinyJS::execute_assignment(CScriptResult &execute) {
	return execute_assignment(execute_condition(execute), execute);
}
CScriptVarLinkPtr CTinyJS::execute_assignment(CScriptVarLinkWorkPtr lhs, CScriptResult &execute) {
	if (t->tk=='=' || (t->tk>=LEX_ASSIGNMENTS_BEGIN && t->tk<=LEX_ASSIGNMENTS_END) ) {
		int op = t->tk;
		CScriptTokenizer::ScriptTokenPosition leftHandPos = t->getPos();
		t->match(t->tk);
		CScriptVarLinkWorkPtr rhs = execute_assignment(execute).getter(execute); // L<-R
		if (execute) {
			if (!lhs->isOwned() && !lhs.hasReferencedOwner() && lhs->getName().empty()) {
				throw new CScriptException(ReferenceError, "invalid assignment left-hand side (at runtime)", t->currentFile, leftHandPos.currentLine(), leftHandPos.currentColumn());
			} else if (op != '=' && !lhs->isOwned()) {
				throwError(execute, ReferenceError, lhs->getName() + " is not defined");
			}
			else if(lhs->isWritable()) {
				if (op=='=') {
					if (!lhs->isOwned()) {
						CScriptVarPtr fakedOwner = lhs.getReferencedOwner();
						if(fakedOwner) {
							if(!fakedOwner->isExtensible())
								return rhs->getVarPtr();
							lhs = fakedOwner->addChildOrReplace(lhs->getName(), lhs);
						} else
							lhs = root->addChildOrReplace(lhs->getName(), lhs);
					}
					lhs.setter(execute, rhs);
					return rhs->getVarPtr();
				} else {
					CScriptVarPtr result;
					static int assignments[] = {'+', '-', '*', '/', '%', LEX_LSHIFT, LEX_RSHIFT, LEX_RSHIFTU, '&', '|', '^'};
					result = mathsOp(execute, lhs, rhs, assignments[op-LEX_PLUSEQUAL]);
					lhs.setter(execute, result);
					return result;
				}
			} else {
				// lhs is not writable we ignore lhs & use rhs
				return rhs->getVarPtr();
			}
		}
	}
	else 
		CheckRightHandVar(execute, lhs);
	return lhs.getter(execute);
}
// L->R: Precedence 17 (comma) ,
CScriptVarLinkPtr CTinyJS::execute_base(CScriptResult &execute) {
	CScriptVarLinkPtr a;
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
void CTinyJS::execute_block(CScriptResult &execute) {
	if(execute) {
		t->match('{');
		CScopeControl ScopeControl(this);
		if(t->tk==LEX_T_FORWARD) // add a LetScope only if needed
			ScopeControl.addLetScope();
		while (t->tk && t->tk!='}')
			execute_statement(execute);
		t->match('}');
		// scopes.pop_back();
	}
	else 
		t->skip(t->getToken().Int());
}
void CTinyJS::execute_statement(CScriptResult &execute) {
	switch(t->tk) {
	case '{':		/* A block of code */
		execute_block(execute);
		break;
	case ';':		/* Empty statement - to allow things like ;;; */
		t->match(';');
		break;
	case LEX_T_FORWARD:
		{
			CScriptVarPtr in_scope = scope()->scopeLet();
			STRING_SET_t *varNames = t->getToken().Forwarder().varNames;
			for(int i=0; i<CScriptTokenDataForwards::END; ++i) {
				for(STRING_SET_it it=varNames[i].begin(); it!=varNames[i].end(); ++it) {
					CScriptVarLinkPtr a = in_scope->findChild(*it);
					if(!a) in_scope->addChild(*it, constScriptVar(Undefined), i==CScriptTokenDataForwards::CONSTS ? SCRIPTVARLINK_CONSTDEFAULT : SCRIPTVARLINK_VARDEFAULT);
				}
					in_scope = scope()->scopeVar();
			}
			CScriptTokenDataForwards::FNC_SET_t &functions = t->getToken().Forwarder().functions;
			for(CScriptTokenDataForwards::FNC_SET_it it=functions.begin(); it!=functions.end(); ++it) {
				CScriptVarLinkWorkPtr funcVar = parseFunctionDefinition(*it);
				in_scope->addChildOrReplace(funcVar->getName(), funcVar, SCRIPTVARLINK_VARDEFAULT);
			}
			t->match(LEX_T_FORWARD);
		}
		break;
	case LEX_R_VAR:
	case LEX_R_LET:
	case LEX_R_CONST:
		if(execute)
		{
			CScopeControl ScopeControl(this);
			bool isLet = t->tk==LEX_R_LET, let_ext=false;
			t->match(t->tk);
			if(isLet && t->tk=='(') {
				let_ext = true;
				t->match('(');
				t->check(LEX_T_FORWARD);
				ScopeControl.addLetScope();
				execute_statement(execute); // forwarder
			}
			execute_var_init(let_ext, execute);
			if(let_ext) {
				t->match(')');
				execute_statement(execute);
			} else
				t->match(';');
		} else
			t->skip(t->getToken().Int());
		break;
	case LEX_R_WITH:
		if(execute) {
			t->match(LEX_R_WITH);
			t->match('(');
			CScriptVarLinkPtr var = execute_base(execute);
			t->match(')');
			CScopeControl ScopeControl(this);
			ScopeControl.addWithScope(var);
			execute_statement(execute);
		} else
			t->skip(t->getToken().Int());
		break;
	case LEX_R_IF:
		if(execute) {
			t->match(LEX_R_IF);
			t->match('(');
			bool cond = execute_base(execute)->toBoolean();
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
		break;
	case LEX_T_FOR_IN:
		{
			CScriptTokenDataLoop &LoopData = t->getToken().Loop();
			t->match(LEX_T_FOR_IN);
			if(!execute) break;

			CScopeControl ScopeControl(this);
			if(LoopData.init.size()) {
				t->pushTokenScope(LoopData.init);
				ScopeControl.addLetScope();
				execute_statement(execute); // forwarder
			}
			if(!execute) break;
	
			t->pushTokenScope(LoopData.iter);
			CScriptVarPtr for_in_var = execute_base(execute);

			if(!execute) break;

			CScriptVarPtr Iterator(for_in_var->toIterator(execute, LoopData.type!=CScriptTokenDataLoop::FOR_IN ? 2:1));
			CScriptVarFunctionPtr Iterator_next(Iterator->findChildWithPrototypeChain("next").getter(execute));
			if(execute && !Iterator_next) throwError(execute, TypeError, "'" + for_in_var->toString(execute) + "' is not iterable", t->getPrevPos());
			if(!execute) break;
			CScriptResult tmp_execute;
			for(;;) {
				bool old_haveTry = haveTry;
				haveTry = true;
				tmp_execute.set(CScriptResult::Normal, Iterator);
				t->pushTokenScope(LoopData.condition);
				execute_statement(tmp_execute);
				haveTry = old_haveTry;
				if(tmp_execute.isThrow()){
					if(tmp_execute.value != constStopIteration) {
						if(!haveTry) 
							throw new CScriptException("uncaught exception: '"+tmp_execute.value->toString(CScriptResult())+"'", t->currentFile, t->currentLine(), t->currentColumn());
						else
							execute = tmp_execute;
					}
					break;
				}
				t->pushTokenScope(LoopData.body);
				execute_statement(execute);
				if(!execute) {
					bool Continue = false;
					if(execute.isBreakContinue() 
						&& 
						(execute.target.empty() || find(LoopData.labels.begin(), LoopData.labels.end(), execute.target) != LoopData.labels.end())) {
							Continue = execute.isContinue();
							execute.set(CScriptResult::Normal, false);
					}
					if(!Continue) break;
				}
			}
		}
		break;
	case LEX_T_LOOP:
		{
			CScriptTokenDataLoop &LoopData = t->getToken().Loop();
			t->match(LEX_T_LOOP);
			if(!execute) break;

			CScopeControl ScopeControl(this);
			if(LoopData.type == CScriptTokenDataLoop::FOR) {
				CScriptResult tmp_execute;
				t->pushTokenScope(LoopData.init);
				if(t->tk == LEX_T_FORWARD) {
					ScopeControl.addLetScope();
					execute_statement(tmp_execute); // forwarder
				}
				if(t->tk==LEX_R_VAR || t->tk==LEX_R_LET)
					execute_statement(tmp_execute); // initialisation
				else if (t->tk != ';') 
					execute_base(tmp_execute); // initialisation
				if(!execute(tmp_execute)) break;
			}

			bool loopCond = true;	// Empty Condition -->always true
			if(LoopData.type != CScriptTokenDataLoop::DO && LoopData.condition.size()) {
				t->pushTokenScope(LoopData.condition);
				loopCond = execute_base(execute)->toBoolean();
				if(!execute) break;
			}
			while (loopCond && execute) {
				t->pushTokenScope(LoopData.body);
				execute_statement(execute);
				if(!execute) {
					bool Continue = false;
					if(execute.isBreakContinue() 
						&& 
						(execute.target.empty() || find(LoopData.labels.begin(), LoopData.labels.end(), execute.target) != LoopData.labels.end())) {
							Continue = execute.isContinue();
							execute.set(CScriptResult::Normal, false);
					}
					if(!Continue) break;
				}
				if(LoopData.type == CScriptTokenDataLoop::FOR && execute && LoopData.iter.size()) {
					t->pushTokenScope(LoopData.iter);
					execute_base(execute);
				}
				if(execute && LoopData.condition.size()) {
					t->pushTokenScope(LoopData.condition);
					loopCond = execute_base(execute)->toBoolean();
				}
			}
		}
		break;
	case LEX_R_BREAK:
	case LEX_R_CONTINUE:
		if (execute)
		{
			CScriptResult::TYPE type = t->tk==LEX_R_BREAK ? CScriptResult::Break : CScriptResult::Continue;
			string label;
			t->match(t->tk);
			if(t->tk == LEX_ID) {
				label = t->tkStr();
				t->match(LEX_ID);
			}
			t->match(';');
			execute.set(type, label);
		} else
			t->skip(t->getToken().Int());
		break;
	case LEX_R_RETURN:
		if (execute) {
			t->match(LEX_R_RETURN);
			CScriptVarPtr result = constUndefined;
			if (t->tk != ';')
				result = execute_base(execute);
			t->match(';');
			execute.set(CScriptResult::Return, result);
		} else
			t->skip(t->getToken().Int());
		break;
	case LEX_R_FUNCTION:
		if(execute) {
			CScriptVarLinkWorkPtr funcVar = parseFunctionDefinition(t->getToken());
			scope()->scopeVar()->addChildOrReplace(funcVar->getName(), funcVar, SCRIPTVARLINK_VARDEFAULT);
		}
	case LEX_T_FUNCTION_PLACEHOLDER:
		t->match(t->tk);
		break;
	case LEX_T_TRY:
		if(execute) {
			CScriptTokenDataTry &TryData = t->getToken().Try();

			bool old_haveTry = haveTry;
			haveTry = true;

			// execute try-block
			t->pushTokenScope(TryData.tryBlock);
			execute_block(execute);

			bool isThrow = execute.isThrow();

			if(isThrow && execute.value) {
				// execute catch-blocks only if value set (spezial case Generator.close() -> only finally-blocks are executed)
				for(CScriptTokenDataTry::CatchBlock_it catchBlock = TryData.catchBlocks.begin(); catchBlock!=TryData.catchBlocks.end(); catchBlock++) {
					CScriptResult catch_execute;
					CScopeControl ScopeControl(this);
					ScopeControl.addLetScope();
					t->pushTokenScope(catchBlock->condition); // condition;
					execute_statement(catch_execute); // forwarder
					assign_destructuring_var(0, *catchBlock->indentifiers, execute.value, catch_execute);
					bool condition = true;
					if(catchBlock->condition.size()>1)
						condition = execute_base(catch_execute)->toBoolean();
					if(!catch_execute) {
						execute = catch_execute;
						break;
					} else if(condition) {
						t->pushTokenScope(catchBlock->block); // condition;
						execute_block(catch_execute);
						execute = catch_execute;
						break;
					}
				}
			}
			if(TryData.finallyBlock.size()) {
				CScriptResult finally_execute; // alway execute finally-block
				t->pushTokenScope(TryData.finallyBlock); // finally;
				execute_block(finally_execute);
				execute(finally_execute);
			}
			// restore haveTry
			haveTry = old_haveTry;
			if(execute.isThrow() && !haveTry) { // (exception in catch or finally or no catch-clause found) and no parent try-block 
				if(execute.value->isError())
					throw CScriptVarErrorPtr(execute.value)->toCScriptException();
				throw new CScriptException("uncaught exception: '"+execute.value->toString()+"'", execute.throw_at_file, execute.throw_at_line, execute.throw_at_column);
			}

		}
		t->match(LEX_T_TRY);
		break;
	case LEX_R_THROW:
		if(execute) {
			CScriptTokenizer::ScriptTokenPosition tokenPos = t->getPos();
			//		int tokenStart = t->getToken().pos;
			t->match(LEX_R_THROW);
			CScriptVarPtr a = execute_base(execute);
			if(execute) {
				if(haveTry)
					execute.setThrow(a, t->currentFile, tokenPos.currentLine(), tokenPos.currentColumn());
				else
					throw new CScriptException("uncaught exception: '"+a->toString(execute)+"'", t->currentFile, tokenPos.currentLine(), tokenPos.currentColumn());
			}
		} else
			t->skip(t->getToken().Int());
		break;
	case LEX_R_SWITCH:
		if(execute) {
			t->match(LEX_R_SWITCH);
			t->match('(');
			CScriptVarPtr SwitchValue = execute_base(execute);
			t->match(')');
			if(execute) {
				t->match('{');
				CScopeControl ScopeControl(this);
				if(t->tk == LEX_T_FORWARD) {
					ScopeControl.addLetScope(); // add let-scope only if needed
					execute_statement(execute); // execute forwarder
				}
				CScriptTokenizer::ScriptTokenPosition defaultStart = t->getPos();
				bool hasDefault = false, found = false;
				while (t->tk) {
					switch(t->tk) {
					case LEX_R_CASE:
						if(!execute)
							t->skip(t->getToken().Int());				// skip up to'}'
						else if(found) {	// execute && found
							t->match(LEX_R_CASE);
							t->skip(t->getToken().Int());				// skip up to ':'
							t->match(':');									// skip ':' and execute all after ':'
						} else {	// execute && !found
							t->match(LEX_R_CASE);
							t->match(LEX_T_SKIP);						// skip 'L_T_SKIP'
							CScriptVarLinkPtr CaseValue = execute_base(execute);
							CaseValue = mathsOp(execute, CaseValue, SwitchValue, LEX_TYPEEQUAL);
							if(execute) {
								found = CaseValue->toBoolean();
								if(found) t->match(':'); 				// skip ':' and execute all after ':'
								else t->skip(t->getToken().Int());	// skip up to next 'case'/'default' or '}'
							} else
								t->skip(t->getToken().Int());			// skip up to next 'case'/'default' or '}'
						}
						break;
					case LEX_R_DEFAULT:
						if(!execute)
							t->skip(t->getToken().Int());				// skip up to'}' NOTE: no extra 'L_T_SKIP' for skipping tp ':'
						else {
							t->match(LEX_R_DEFAULT);
							if(found)
								t->match(':'); 							// skip ':' and execute all after ':'
							else {
								hasDefault = true;						// in fist pass: skip default-area
								defaultStart = t->getPos(); 			// remember pos of default
								t->skip(t->getToken().Int());			// skip up to next 'case' or '}'
							}
						}
						break;
					case '}':
						if(execute && !found && hasDefault) {		// if not found & have default -> execute default
							found = true;
							t->setPos(defaultStart);
							t->match(':');
						} else
							goto end_while; // goto isn't fine but C supports no "break lable;"
						break;
					default:
						ASSERT(found);
						execute_statement(execute);
						break;
					}
				}
end_while:
				t->match('}');
				if(execute.isBreak() && execute.target.empty()) {
					execute.set(CScriptResult::Normal);
				}
			} else
				t->skip(t->getToken().Int());
		} else
			t->skip(t->getToken().Int());
		break;
	case LEX_T_DUMMY_LABEL:
		t->match(LEX_T_DUMMY_LABEL);
		t->match(':');
		break;
	case LEX_T_LABEL:
		{
			STRING_VECTOR_t Labels;
			while(t->tk == LEX_T_LABEL) {
				Labels.push_back(t->tkStr());
				t->match(LEX_T_LABEL);
				t->match(':');
			}
			if(execute) {
				execute_statement(execute);
				if(execute.isBreak() && find(Labels.begin(), Labels.end(), execute.target) != Labels.end()) { // break this label
					execute.set(CScriptResult::Normal, false);
				}
			}
			else
				execute_statement(execute);
		}
		break;
	case LEX_EOF:
		t->match(LEX_EOF);
		break;
	default:
		if(t->tk!=LEX_T_SKIP || execute) {
			if(t->tk==LEX_T_SKIP) t->match(LEX_T_SKIP);
			/* Execute a simple statement that only contains basic arithmetic... */
			CScriptVarPtr ret = execute_base(execute);
			if(execute) execute.set(CScriptResult::Normal, CScriptVarPtr(ret));
			t->match(';');
		} else
			t->skip(t->getToken().Int());
		break;
	}
}


/// Finds a child, looking recursively up the scopes
CScriptVarLinkPtr CTinyJS::findInScopes(const string &childName) {
	return scope()->findInScopes(childName);
}

//////////////////////////////////////////////////////////////////////////
/// Object
//////////////////////////////////////////////////////////////////////////

void CTinyJS::native_Object(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(c->getArgument(0)->toObject());
}
void CTinyJS::native_Object_getPrototypeOf(const CFunctionsScopePtr &c, void *data) {
	if(c->getArgumentsLength()>=1) {
		CScriptVarPtr obj = c->getArgument(0);
		if(obj->isObject()) {
			c->setReturnVar(obj->findChild(TINYJS___PROTO___VAR));
			return;
		}
	}
	c->throwError(TypeError, "argument is not an object");
}

void CTinyJS::native_Object_setObjectSecure(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getArgument(0);
	if(!obj->isObject()) c->throwError(TypeError, "argument is not an object");
	if(data==(void*)2)
		obj->freeze();
	else if(data==(void*)1)
		obj->seal();
	else
		obj->preventExtensions();
	c->setReturnVar(obj);
}

void CTinyJS::native_Object_isSecureObject(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getArgument(0);
	if(!obj->isObject()) c->throwError(TypeError, "argument is not an object");
	bool ret;
	if(data==(void*)2)
		ret = obj->isFrozen();
	else if(data==(void*)1)
		ret = obj->isSealed();
	else
		ret = obj->isExtensible();
	c->setReturnVar(constScriptVar(ret));
}

void CTinyJS::native_Object_keys(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getArgument(0);
	if(!obj->isObject()) c->throwError(TypeError, "argument is not an object");
	CScriptVarPtr returnVar = c->newScriptVar(Array);
	c->setReturnVar(returnVar);

	STRING_SET_t keys;
	obj->keys(keys, data==0);

	uint32_t idx=0;
	for(STRING_SET_it it=keys.begin(); it!=keys.end(); ++it)
		returnVar->setArrayIndex(idx++, newScriptVar(*it));
}

void CTinyJS::native_Object_getOwnPropertyDescriptor(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getArgument(0);
	if(!obj->isObject()) c->throwError(TypeError, "argument is not an object");
	c->setReturnVar(obj->getOwnPropertyDescriptor(c->getArgument(1)->toString()));
}

void CTinyJS::native_Object_defineProperty(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getArgument(0);
	if(!obj->isObject()) c->throwError(TypeError, "argument is not an object");
	string name = c->getArgument(1)->toString();
	CScriptVarPtr attributes = c->getArgument(2);
	if(!attributes->isObject()) c->throwError(TypeError, "attributes is not an object");
	const char *err = obj->defineProperty(name, attributes);
	if(err) c->throwError(TypeError, err);
	c->setReturnVar(obj);
}

void CTinyJS::native_Object_defineProperties(const CFunctionsScopePtr &c, void *data) {
	bool ObjectCreate = data!=0;
	CScriptVarPtr obj = c->getArgument(0);
	if(ObjectCreate) {
		if(!obj->isObject() && !obj->isNull()) c->throwError(TypeError, "argument is not an object or null");
		obj = newScriptVar(Object, obj);
	} else
		if(!obj->isObject()) c->throwError(TypeError, "argument is not an object");
	c->setReturnVar(obj);
	if(c->getArrayLength()<2) {
		if(ObjectCreate) return;
		c->throwError(TypeError, "Object.defineProperties requires 2 arguments");
	}

	CScriptVarPtr properties = c->getArgument(1);

	STRING_SET_t names;
	properties->keys(names, true);

	for(STRING_SET_it it=names.begin(); it!=names.end(); ++it) {
		CScriptVarPtr attributes = properties->findChildWithStringChars(*it).getter();
		if(!attributes->isObject()) c->throwError(TypeError, "descriptor for "+*it+" is not an object");
		const char *err = obj->defineProperty(*it, attributes);
		if(err) c->throwError(TypeError, err);
	}
}


//////////////////////////////////////////////////////////////////////////
/// Object.prototype
//////////////////////////////////////////////////////////////////////////

void CTinyJS::native_Object_prototype_hasOwnProperty(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr This = c->getArgument("this");
	string PropStr = c->getArgument("prop")->toString();
	CScriptVarLinkPtr Prop = This->findChild(PropStr);
	bool res = Prop && !Prop->getVarPtr()->isUndefined();
	if(!res) {
		CScriptVarStringPtr This_asString = This->getRawPrimitive();
		if(This_asString) {
			uint32_t Idx = isArrayIndex(PropStr);
			res = Idx!=uint32_t(-1) && Idx<This_asString->stringLength();
		}
	}
	c->setReturnVar(c->constScriptVar(res));
}
void CTinyJS::native_Object_prototype_valueOf(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(c->getArgument("this")->valueOf_CallBack());
}
void CTinyJS::native_Object_prototype_toString(const CFunctionsScopePtr &c, void *data) {
	CScriptResult execute;
	int radix = 10;
	if(c->getArgumentsLength()>=1) radix = c->getArgument("radix")->toNumber().toInt32();
	c->setReturnVar(c->getArgument("this")->toString_CallBack(execute, radix));
	if(!execute) {
		// TODO
	}
}

//////////////////////////////////////////////////////////////////////////
/// Array
//////////////////////////////////////////////////////////////////////////

void CTinyJS::native_Array(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr returnVar = c->newScriptVar(Array);
	c->setReturnVar(returnVar);
	int length = c->getArgumentsLength();
	CScriptVarPtr Argument_0_Var = c->getArgument(0);
	if(data!=0 && length == 1 && Argument_0_Var->isNumber()) {
		CNumber Argument_0 = Argument_0_Var->toNumber();
		uint32_t new_size = Argument_0.toUInt32();
		if(Argument_0.isFinite() && Argument_0 == new_size)
			returnVar->setArrayIndex(new_size-1, constScriptVar(Undefined));
		else
			c->throwError(RangeError, "invalid array length");
	} else for(int i=0; i<length; i++)
		returnVar->setArrayIndex(i, c->getArgument(i));
}

//////////////////////////////////////////////////////////////////////////
/// String
//////////////////////////////////////////////////////////////////////////

void CTinyJS::native_String(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr arg;
	if(c->getArgumentsLength()==0)
		arg = newScriptVar("");
	else
		arg = newScriptVar(c->getArgument(0)->toString());
	if(data)
		c->setReturnVar(arg->toObject());
	else
		c->setReturnVar(arg);
}


//////////////////////////////////////////////////////////////////////////
/// RegExp
//////////////////////////////////////////////////////////////////////////
#ifndef NO_REGEXP

void CTinyJS::native_RegExp(const CFunctionsScopePtr &c, void *data) {
	int arglen = c->getArgumentsLength();
	string RegExp, Flags;
	if(arglen>=1) {
		RegExp = c->getArgument(0)->toString();
		try { regex(RegExp, regex_constants::ECMAScript); } catch(regex_error e) {
			c->throwError(SyntaxError, string(e.what())+" - "+CScriptVarRegExp::ErrorStr(e.code()));
		}
		if(arglen>=2) {
			Flags = c->getArgument(1)->toString();
			string::size_type pos = Flags.find_first_not_of("gimy");
			if(pos != string::npos) {
				c->throwError(SyntaxError, string("invalid regular expression flag ")+Flags[pos]);
			}
		} 
	}
	c->setReturnVar(newScriptVar(RegExp, Flags));
}
#endif /* NO_REGEXP */

//////////////////////////////////////////////////////////////////////////
/// Number
//////////////////////////////////////////////////////////////////////////

void CTinyJS::native_Number(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr arg;
	if(c->getArgumentsLength()==0)
		arg = newScriptVar(0);
	else
		arg = newScriptVar(c->getArgument(0)->toNumber());
	if(data)
		c->setReturnVar(arg->toObject());
	else
		c->setReturnVar(arg);
}


//////////////////////////////////////////////////////////////////////////
/// Boolean
//////////////////////////////////////////////////////////////////////////

void CTinyJS::native_Boolean(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr arg;
	if(c->getArgumentsLength()==0)
		arg = constScriptVar(false);
	else
		arg = constScriptVar(c->getArgument(0)->toBoolean());
	if(data)
		c->setReturnVar(arg->toObject());
	else
		c->setReturnVar(arg);
}

////////////////////////////////////////////////////////////////////////// 
/// Iterator
//////////////////////////////////////////////////////////////////////////

void CTinyJS::native_Iterator(const CFunctionsScopePtr &c, void *data) {
	if(c->getArgumentsLength()<1) c->throwError(TypeError, "missing argument 0 when calling function Iterator");
	c->setReturnVar(c->getArgument(0)->toIterator(c->getArgument(1)->toBoolean()?1:3));
}

////////////////////////////////////////////////////////////////////////// 
/// Generator
//////////////////////////////////////////////////////////////////////////

#ifndef NO_GENERATORS
void CTinyJS::native_Generator_prototype_next(const CFunctionsScopePtr &c, void *data) {
	CScriptVarGeneratorPtr Generator(c->getArgument("this"));
	if(!Generator) {
		static const char *fnc[] = {"next","send","close","throw"};
		c->throwError(TypeError, string(fnc[(int)data])+" method called on incompatible Object");
	}
	if((int)data >=2)
		Generator->native_throw(c, (void*)(((int)data)-2));
	else
		Generator->native_send(c, data);
}
#endif /*NO_GENERATORS*/


////////////////////////////////////////////////////////////////////////// 
/// Function
//////////////////////////////////////////////////////////////////////////

void CTinyJS::native_Function(const CFunctionsScopePtr &c, void *data) {
	int length = c->getArgumentsLength();
	string params, body;
	if(length>=1) 
		body = c->getArgument(length-1)->toString();
	if(length>=2) {
		params = c->getArgument(0)->toString();
		for(int i=1; i<length-1; i++)
		{
			params.append(",");
			params.append(c->getArgument(i)->toString());
		}
	}
	c->setReturnVar(parseFunctionsBodyFromString(params,body));
}

void CTinyJS::native_Function_prototype_call(const CFunctionsScopePtr &c, void *data) {
	int length = c->getArgumentsLength();
	CScriptVarPtr Fnc = c->getArgument("this");
	if(!Fnc->isFunction()) c->throwError(TypeError, "Function.prototype.call called on incompatible Object");
	CScriptVarPtr This = c->getArgument(0);
	vector<CScriptVarPtr> Args;
	for(int i=1; i<length; i++)
		Args.push_back(c->getArgument(i));
	c->setReturnVar(callFunction(Fnc, Args, This));
}
void CTinyJS::native_Function_prototype_apply(const CFunctionsScopePtr &c, void *data) {
	int length=0;
	CScriptVarPtr Fnc = c->getArgument("this");
	if(!Fnc->isFunction()) c->throwError(TypeError, "Function.prototype.apply called on incompatible Object");
	// Argument_0
	CScriptVarPtr This = c->getArgument(0)->toObject();
	if(This->isNull() || This->isUndefined()) This=root;
	// Argument_1
	CScriptVarPtr Array = c->getArgument(1);
	if(!Array->isNull() && !Array->isUndefined()) { 
		CScriptVarLinkWorkPtr Length = Array->findChild("length");
		if(!Length) c->throwError(TypeError, "second argument to Function.prototype.apply must be an array or an array like object");
		length = Length.getter()->toNumber().toInt32();
	}
	vector<CScriptVarPtr> Args;
	for(int i=0; i<length; i++) {
		CScriptVarLinkPtr value = Array->findChild(int2string(i));
		if(value) Args.push_back(value);
		else Args.push_back(constScriptVar(Undefined));
	}
	c->setReturnVar(callFunction(Fnc, Args, This));
}
void CTinyJS::native_Function_prototype_bind(const CFunctionsScopePtr &c, void *data) {
	int length = c->getArgumentsLength();
	CScriptVarPtr Fnc = c->getArgument("this");
	if(!Fnc->isFunction()) c->throwError(TypeError, "Function.prototype.bind called on incompatible Object");
	CScriptVarPtr This = c->getArgument(0);
	if(This->isUndefined() || This->isNull()) This = root;
	vector<CScriptVarPtr> Args;
	for(int i=1; i<length; i++) Args.push_back(c->getArgument(i));
	c->setReturnVar(newScriptVarFunctionBounded(Fnc, This, Args));
}


////////////////////////////////////////////////////////////////////////// 
/// Error
//////////////////////////////////////////////////////////////////////////

static CScriptVarPtr _newError(CTinyJS *context, ERROR_TYPES type, const CFunctionsScopePtr &c) {
	int i = c->getArgumentsLength();
	string message, fileName;
	int line=-1, column=-1; 
	if(i>0) message	= c->getArgument(0)->toString();
	if(i>1) fileName	= c->getArgument(1)->toString();
	if(i>2) line		= c->getArgument(2)->toNumber().toInt32();
	if(i>3) column		= c->getArgument(3)->toNumber().toInt32();
	return ::newScriptVarError(context, type, message.c_str(), fileName.c_str(), line, column);
}
void CTinyJS::native_Error(const CFunctionsScopePtr &c, void *data) { c->setReturnVar(_newError(this, Error,c)); }
void CTinyJS::native_EvalError(const CFunctionsScopePtr &c, void *data) { c->setReturnVar(_newError(this, EvalError,c)); }
void CTinyJS::native_RangeError(const CFunctionsScopePtr &c, void *data) { c->setReturnVar(_newError(this, RangeError,c)); }
void CTinyJS::native_ReferenceError(const CFunctionsScopePtr &c, void *data){ c->setReturnVar(_newError(this, ReferenceError,c)); }
void CTinyJS::native_SyntaxError(const CFunctionsScopePtr &c, void *data){ c->setReturnVar(_newError(this, SyntaxError,c)); }
void CTinyJS::native_TypeError(const CFunctionsScopePtr &c, void *data){ c->setReturnVar(_newError(this, TypeError,c)); }

////////////////////////////////////////////////////////////////////////// 
/// global functions
//////////////////////////////////////////////////////////////////////////

void CTinyJS::native_eval(const CFunctionsScopePtr &c, void *data) {
	string Code = c->getArgument("jsCode")->toString();
	CScriptVarScopePtr scEvalScope = scopes.back(); // save scope
	scopes.pop_back(); // go back to the callers scope
	CScriptResult execute;
	CScriptTokenizer *oldTokenizer = t; t=0;
	try {
		CScriptTokenizer Tokenizer(Code.c_str(), "eval");
		t = &Tokenizer;
		do {
			execute_statement(execute);
			while (t->tk==';') t->match(';'); // skip empty statements
		} while (t->tk!=LEX_EOF);
	} catch (CScriptException *e) { // script exceptions
		t = oldTokenizer; // restore tokenizer
		scopes.push_back(scEvalScope); // restore Scopes;
		if(haveTry) { // an Error in eval is always catchable
			CScriptVarPtr E = newScriptVarError(this, e->errorType, e->message.c_str(), e->fileName.c_str(), e->lineNumber, e->column);
			delete e;
			throw E;
		} else
			throw e;
	} catch (...) { // all other exceptions
		t = oldTokenizer; // restore tokenizer
		scopes.push_back(scEvalScope); // restore Scopes;
		throw; // re-throw
	}
	t = oldTokenizer; // restore tokenizer
	scopes.push_back(scEvalScope); // restore Scopes;
	if(execute.value)
		c->setReturnVar(execute.value);
}

static int _native_require_read(const string &Fname, std::string &Data) {
	std::ifstream in(Fname.c_str(), std::ios::in | std::ios::binary);
	if (in) {
		in.seekg(0, std::ios::end);
		Data.resize((string::size_type)in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&Data[0], Data.size());
		in.close();
		return(0);
	}
	return errno;
}

void CTinyJS::native_require(const CFunctionsScopePtr &c, void *data) {
	string File = c->getArgument("jsFile")->toString();
	string Code;
	int ErrorNo;
	if(!native_require_read) 
		native_require_read = _native_require_read; // use builtin if no callback

	if((ErrorNo = native_require_read(File, Code))) {
		ostringstream msg;
		msg << "can't read \"" << File << "\" (Error=" << ErrorNo << ")";
		c->throwError(Error, msg.str());
	}
	c->addChildOrReplace("jsCode", c->newScriptVar(Code));
	native_eval(c, data);
}

void CTinyJS::native_isNAN(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(constScriptVar(c->getArgument("objc")->toNumber().isNaN()));
}

void CTinyJS::native_isFinite(const CFunctionsScopePtr &c, void *data) {
	c->setReturnVar(constScriptVar(c->getArgument("objc")->toNumber().isFinite()));
}

void CTinyJS::native_parseInt(const CFunctionsScopePtr &c, void *) {
	CNumber result;
	result.parseInt(c->getArgument("string")->toString(), c->getArgument("radix")->toNumber().toInt32());
	c->setReturnVar(c->newScriptVar(result));
}

void CTinyJS::native_parseFloat(const CFunctionsScopePtr &c, void *) {
	CNumber result;
	result.parseFloat(c->getArgument("string")->toString());
	c->setReturnVar(c->newScriptVar(result));
}



void CTinyJS::native_JSON_parse(const CFunctionsScopePtr &c, void *data) {
	string Code = "§" + c->getArgument("text")->toString();
	// "§" is a spezal-token - it's for the tokenizer and means the code begins not in Statement-level
	CScriptVarLinkWorkPtr returnVar;
	CScriptTokenizer *oldTokenizer = t; t=0;
	try {
		CScriptTokenizer Tokenizer(Code.c_str(), "JSON.parse", 0, -1);
		t = &Tokenizer;
		CScriptResult execute;
		returnVar = execute_literals(execute);
		t->match(LEX_EOF);
	} catch (CScriptException *e) {
		t = oldTokenizer;
		throw e;
	}
	t = oldTokenizer;

	if(returnVar)
		c->setReturnVar(returnVar);
}

void CTinyJS::setTemporaryID_recursive(uint32_t ID) {
	for(vector<CScriptVarPtr*>::iterator it = pseudo_refered.begin(); it!=pseudo_refered.end(); ++it)
		if(**it) (**it)->setTemporaryMark_recursive(ID);
	for(int i=Error; i<ERROR_COUNT; i++)
		if(errorPrototypes[i]) errorPrototypes[i]->setTemporaryMark_recursive(ID);
	root->setTemporaryMark_recursive(ID);
}

void CTinyJS::ClearUnreferedVars(const CScriptVarPtr &extra/*=CScriptVarPtr()*/) {
	uint32_t UniqueID = allocUniqueID(); 
	setTemporaryID_recursive(UniqueID);
	if(extra) extra->setTemporaryMark_recursive(UniqueID);
	CScriptVar *p = first;

	while(p)
	{
		if(p->getTemporaryMark() != UniqueID)
		{
			CScriptVarPtr var = p;
			var->removeAllChildren();
			p = var->next;
		}
		else
			p = p->next;
	}
	freeUniqueID();
}

