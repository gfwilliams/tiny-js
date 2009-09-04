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

#ifndef TINYJS_H
#define TINYJS_H

#include <string>

#ifndef TRACE
#define TRACE printf
#endif // TRACE

const int TINYJS_LOOP_MAX_ITERATIONS = 8192;

enum LEX_TYPES {
    LEX_EOF = 0,
    LEX_ID = 256,
    LEX_INT,
    LEX_FLOAT,
    LEX_STR,

    LEX_EQUAL,
    LEX_NEQUAL,
    LEX_LEQUAL,
    LEX_GEQUAL,
    LEX_PLUSEQUAL,
    LEX_MINUSEQUAL,
    LEX_PLUSPLUS,
    LEX_MINUSMINUS,
    LEX_ANDAND,
    LEX_OROR,
    // reserved words
    LEX_R_IF,
    LEX_R_ELSE,
    LEX_R_WHILE,
    LEX_R_FOR,
    LEX_R_FUNCTION,
    LEX_R_RETURN,
    LEX_R_VAR,
    LEX_R_TRUE,
    LEX_R_FALSE,
};

enum SCRIPTVAR_FLAGS {
    SCRIPTVAR_FUNCTION = 1,
    SCRIPTVAR_NATIVE = 2,
    SCRIPTVAR_NUMERIC = 4,
    SCRIPTVAR_INTEGER = 8, // eg. not floating point
    SCRIPTVAR_VARTYPEMASK = 12,
    SCRIPTVAR_PARAMETER = 16
};

#define TINYJS_RETURN_VAR "return"
#define TINYJS_TEMP_NAME ""
#define TINYJS_BLANK_DATA ""

/// convert the given string into a quoted string suitable for javascript
std::string getJSString(const std::string &str);

class CScriptException {
public:
    std::string text;
    CScriptException(const std::string &exceptionText);
};

class CScriptLex
{
public:
    CScriptLex(const std::string &input);
    CScriptLex(CScriptLex *owner, int startChar, int endChar);
    ~CScriptLex(void);

    char currCh, nextCh;
    int tk; ///< The type of the token that we have
    int tokenPosition; ///< Position in the data at the beginning of the token we have here
    std::string tkStr; ///< Data contained in the token we have here

    void match(int expected_tk); ///< Lexical match wotsit
    std::string getTokenStr(int token); ///< Get the string representation of the given token
    void reset(); ///< Reset this lex so we can start again

    std::string getSubString(int pos); ///< Return a sub-string from the given position up until right now
    CScriptLex *getSubLex(int lastPosition); ///< Return a sub-lexer from the given position up until right now

    std::string getPosition(int pos); ///< Return a string representing the position in lines and columns of the character pos given

protected:
    /* When we go into a loop, we use getSubLex to get a lexer for just the sub-part of the
       relevant string. This doesn't re-allocate and copy the string, but instead copies
       the data pointer and sets dataOwned to false, and dataStart/dataEnd to the relevant things. */
    char *data; ///< Data string to get tokens from
    int dataStart, dataEnd; ///< Start and end position in data string
    bool dataOwned; ///< Do we own this data string?

    int dataPos; ///< Position in data

    void getNextCh();
    void getNextToken(); ///< Get the text token from our text string
};

class CScriptVar;

typedef void (*JSCallback)(CScriptVar *var);

/// Variable class (containing a doubly-linked list of children)
class CScriptVar
{
public:
    CScriptVar(void);
    CScriptVar(std::string varName, std::string varData=TINYJS_BLANK_DATA, int varFlags = 0);
    CScriptVar(double varData);
    CScriptVar(int val);
    ~CScriptVar(void);

    CScriptVar *findChild(std::string childName); ///< Tries to find a child with the given name, may return 0
    CScriptVar *getChild(std::string childName); ///< Gets a child with the given name, produces error on fail
    CScriptVar *findRecursive(std::string childName); ///< Finds a child, looking recursively up the tree
    void addChild(CScriptVar *child);
    CScriptVar *addChildNoDup(CScriptVar *child); ///< add a child overwriting any with the same name
    void removeChild(CScriptVar *child);
    void removeAllChildren();
    CScriptVar *getRoot(); ///< Get the absolute root of the tree

    std::string getName();
    int getInt();
    bool getBool() { return getInt() != 0; }
    double getDouble();
    std::string &getString();
    void setInt(int num);
    void setDouble(double val);
    void setString(std::string str);
    void setVoid();

    bool isInt() { return (flags&SCRIPTVAR_NUMERIC)!=0 && (flags&SCRIPTVAR_INTEGER)!=0; }
    bool isDouble() { return (flags&SCRIPTVAR_NUMERIC)!=0 && (flags&SCRIPTVAR_INTEGER)==0; }
    bool isNumeric() { return (flags&SCRIPTVAR_NUMERIC)!=0; }
    bool isFunction() { return (flags&SCRIPTVAR_FUNCTION)!=0; }
    bool isParameter() { return (flags&SCRIPTVAR_PARAMETER)!=0; }
    bool isNative() { return (flags&SCRIPTVAR_NATIVE)!=0; }

    CScriptVar *mathsOp(CScriptVar *b, int op); ///< do a maths op with another script variable
    void copyValue(CScriptVar *val); ///< copy the value from the value given
    CScriptVar *deepCopy(); ///< deep copy this node and return the result


    void trace(std::string indentStr = ""); ///< Dump out the contents of this using trace
    void getInitialiseCode(std::ostringstream &destination, const std::string &prefix = ""); ///< Write out all the JS code needed to recreate this script variable to the stream
    void setCallback(JSCallback callback);

    CScriptVar *firstChild;
    CScriptVar *lastChild;
    CScriptVar *nextSibling;
    CScriptVar *prevSibling;
    CScriptVar *parent;
protected:
    std::string name;
    std::string data;
    int flags;
    JSCallback jsCallback;

    void init(); // initilisation of data members

    friend class CTinyJS;
};

class CTinyJS {
public:
    CTinyJS();
    ~CTinyJS();

    void execute(const std::string &code);
    std::string evaluate(const std::string &code);

    /// add a function to the root scope
    /** example:
       \code
           void scRandInt(CScriptVar *c) { ... }
           tinyJS->addNative("function randInt(min, max)", scRandInt);
       \endcode
    */
    void addNative(const std::string &funcDesc, JSCallback ptr);

    /// Get the value of the given variable, or return 0
    std::string *getVariable(const std::string &path);

    /// Send all variables to stdout
    void trace();

    CScriptVar *root;   /// root of symbol table
private:
    CScriptLex *l;             /// current lexer
    CScriptVar *symbol_base;   /// current symbol table base

    // parsing - in order of precedence
    CScriptVar *factor(bool &execute);
    CScriptVar *unary(bool &execute);
    CScriptVar *term(bool &execute);
    CScriptVar *expression(bool &execute);
    CScriptVar *condition(bool &execute);
    CScriptVar *logic(bool &execute);
    CScriptVar *base(bool &execute);
    void block(bool &execute);
    void statement(bool &execute);
};

#endif
