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
    LEX_TYPEEQUAL,
    LEX_NEQUAL,
    LEX_NTYPEEQUAL,
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
    LEX_R_NULL,
    LEX_R_UNDEFINED,
};

enum SCRIPTVAR_FLAGS {
    SCRIPTVAR_UNDEFINED   = 0,
    SCRIPTVAR_FUNCTION    = 1,
    SCRIPTVAR_PARAMETER   = 2,
    SCRIPTVAR_NATIVE      = 4,
    SCRIPTVAR_DOUBLE      = 8,  // floating point double
    SCRIPTVAR_INTEGER     = 16, // integer number
    SCRIPTVAR_STRING      = 32, // string
    SCRIPTVAR_NULL        = 64, // it seems null is its own data type
    SCRIPTVAR_NUMERICMASK = SCRIPTVAR_NULL |
                            SCRIPTVAR_DOUBLE |
                            SCRIPTVAR_INTEGER,
    SCRIPTVAR_VARTYPEMASK = SCRIPTVAR_DOUBLE |
                            SCRIPTVAR_INTEGER |
                            SCRIPTVAR_STRING |
                            SCRIPTVAR_FUNCTION |
                            SCRIPTVAR_NULL,

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
    int tokenStart; ///< Position in the data at the beginning of the token we have here
    int tokenEnd; ///< Position in the data at the last character of the token we have here
    int tokenLastEnd; ///< Position in the data at the last character of the last token
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

    int dataPos; ///< Position in data (we CAN go past the end of the string here)

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
    CScriptVar(std::string varName, std::string varData=TINYJS_BLANK_DATA, int varFlags = SCRIPTVAR_UNDEFINED);
    CScriptVar(double varData);
    CScriptVar(int val);
    ~CScriptVar(void);

    CScriptVar *findChild(const std::string &childName); ///< Tries to find a child with the given name, may return 0
    CScriptVar *findChildOrCreate(const std::string &childName); ///< Tries to find a child with the given name, or will create it
    CScriptVar *findChildOrCreateByPath(const std::string &path); ///< Tries to find a child with the given path (separated by dots)
    CScriptVar *findRecursive(const std::string &childName); ///< Finds a child, looking recursively up the tree
    void addChild(CScriptVar *child);
    void addNamedChild(const std::string &childName, CScriptVar *child); ///< add the named child. if it is already owned, copy it.
    CScriptVar *addChildNoDup(CScriptVar *child); ///< add a child overwriting any with the same name
    void removeChild(CScriptVar *child);
    void removeAllChildren();
    CScriptVar *getRoot(); ///< Get the absolute root of the tree

    const std::string &getName();
    int getInt();
    bool getBool() { return getInt() != 0; }
    double getDouble();
    const std::string &getString();
    std::string getParsableString(); ///< get Data as a parsable javascript string
    void setInt(int num);
    void setDouble(double val);
    void setString(const std::string &str);
    void setUndefined();

    bool isInt() { return (flags&SCRIPTVAR_INTEGER)!=0; }
    bool isDouble() { return (flags&SCRIPTVAR_DOUBLE)!=0; }
    bool isString() { return (flags&SCRIPTVAR_STRING)!=0; }
    bool isNumeric() { return (flags&SCRIPTVAR_NUMERICMASK)!=0; }
    bool isFunction() { return (flags&SCRIPTVAR_FUNCTION)!=0; }
    bool isParameter() { return (flags&SCRIPTVAR_PARAMETER)!=0; }
    bool isNative() { return (flags&SCRIPTVAR_NATIVE)!=0; }
    bool isUndefined() { return (flags & SCRIPTVAR_VARTYPEMASK) == SCRIPTVAR_UNDEFINED; }
    bool isNull() { return (flags & SCRIPTVAR_NULL)!=0; }

    CScriptVar *mathsOp(CScriptVar *b, int op); ///< do a maths op with another script variable
    void copyValue(CScriptVar *val); ///< copy the value from the value given
    CScriptVar *deepCopy(); ///< deep copy this node and return the result


    void trace(std::string indentStr = ""); ///< Dump out the contents of this using trace
    std::string getFlagsAsString();
    void getJSON(std::ostringstream &destination); ///< Write out all the JS code needed to recreate this script variable to the stream (as JSON)
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
    const std::string *getVariable(const std::string &path);

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
    // parsing utility functions
    CScriptVar *parseFunctionDefinition();
    void parseFunctionArguments(CScriptVar *funcVar);
};

#endif
