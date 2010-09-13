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
#include <vector>
#include <map>

#ifndef __GNUC__
#	define __attribute__(x)
#endif


#undef TRACE
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
	LEX_LSHIFT,
	LEX_LSHIFTEQUAL,
	LEX_GEQUAL,
	LEX_RSHIFT,
	LEX_RSHIFTEQUAL,
	LEX_PLUSEQUAL,
	LEX_MINUSEQUAL,
	LEX_PLUSPLUS,
	LEX_MINUSMINUS,
	LEX_ANDEQUAL,
	LEX_ANDAND,
	LEX_OREQUAL,
	LEX_OROR,
	LEX_XOREQUAL,
	// reserved words
#define LEX_R_LIST_START LEX_R_IF
	LEX_R_IF,
	LEX_R_ELSE,
	LEX_R_DO,
	LEX_R_WHILE,
	LEX_R_FOR,
	LEX_R_BREAK,
	LEX_R_CONTINUE,
	LEX_R_FUNCTION,
	LEX_R_RETURN,
	LEX_R_VAR,
	LEX_R_TRUE,
	LEX_R_FALSE,
	LEX_R_NULL,
	LEX_R_UNDEFINED,
	LEX_R_INFINITY,
	LEX_R_NAN,
	LEX_R_NEW,
	LEX_R_TRY,
	LEX_R_CATCH,
	LEX_R_FINALLY,
	LEX_R_THROW,

	LEX_R_LIST_END /* always the last entry */
};

enum SCRIPTVAR_FLAGS {
	SCRIPTVAR_UNDEFINED	= 0,
	SCRIPTVAR_FUNCTION		= 1,
	SCRIPTVAR_OBJECT			= 2,
	SCRIPTVAR_ARRAY			= 4,
	SCRIPTVAR_DOUBLE			= 8,  // floating point double
	SCRIPTVAR_INTEGER			= 16, // integer number
	SCRIPTVAR_BOOLEAN			= 32, // boolean
	SCRIPTVAR_STRING			= 64, // string
	SCRIPTVAR_NULL				= 128, // it seems null is its own data type
	SCRIPTVAR_INFINITY		= 256, // it seems infinity is its own data type
	SCRIPTVAR_NAN				= 512, // it seems NaN is its own data type

	SCRIPTVAR_NATIVE_FNC		= 1024, // to specify this is a native function
	SCRIPTVAR_NATIVE_MFNC	= 2048, // to specify this is a native function from class->memberFunc
	SCRIPTVAR_NUMERICMASK	= SCRIPTVAR_NULL |
									  SCRIPTVAR_DOUBLE |
									  SCRIPTVAR_INTEGER |
									  SCRIPTVAR_BOOLEAN,
	SCRIPTVAR_VARTYPEMASK	= SCRIPTVAR_DOUBLE |
									  SCRIPTVAR_INTEGER |
									  SCRIPTVAR_BOOLEAN |
									  SCRIPTVAR_STRING |
									  SCRIPTVAR_FUNCTION |
									  SCRIPTVAR_OBJECT |
									  SCRIPTVAR_ARRAY |
									  SCRIPTVAR_NULL |
									  SCRIPTVAR_INFINITY |
									  SCRIPTVAR_NAN,
									  
	SCRIPTVAR_NATIVE			= SCRIPTVAR_NATIVE_FNC |
									  SCRIPTVAR_NATIVE_MFNC,
};
enum RUNTIME_FLAGS {
	RUNTIME_CANBREAK			= 1,
	RUNTIME_BREAK				= 2,
	RUNTIME_CANCONTINUE		= 4,
	RUNTIME_CONTINUE			= 8,
	RUNTIME_NEW					= 16,
	RUNTIME_CANTHROW			= 32,
	RUNTIME_THROW				= 64,
};

#define RUNTIME_LOOP_MASK (RUNTIME_CANBREAK | RUNTIME_BREAK | RUNTIME_CANCONTINUE | RUNTIME_CONTINUE)
#define RUNTIME_THROW_MASK (RUNTIME_CANTHROW | RUNTIME_THROW)

#define TINYJS_RETURN_VAR "return"
#define TINYJS_PROTOTYPE_CLASS "prototype"
#define TINYJS_TEMP_NAME ""
#define TINYJS_BLANK_DATA ""

/// convert the given string into a quoted string suitable for javascript
std::string getJSString(const std::string &str);

class CScriptException {
public:
	std::string text;
	int pos;
	CScriptException(const std::string &exceptionText, int Pos=-1);
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
	static std::string getTokenStr(int token); ///< Get the string representation of the given token
	void reset(int toPos=0); ///< Reset this lex so we can start again

	std::string getSubString(int pos); ///< Return a sub-string from the given position up until right now
	CScriptLex *getSubLex(int lastPosition); ///< Return a sub-lexer from the given position up until right now

	std::string getPosition(int pos=-1); ///< Return a string representing the position in lines and columns of the character pos given

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

typedef void (*JSCallback)(CScriptVar *var, void *userdata);

class CScriptVarLink
{
public:
	std::string name;
	CScriptVarLink *nextSibling;
	CScriptVarLink *prevSibling;
	CScriptVar *var;
	CScriptVar *owned; // pointer to the owner CScriptVar

	CScriptVarLink(CScriptVar *var, const std::string &name = TINYJS_TEMP_NAME);
	CScriptVarLink(const CScriptVarLink &link); ///< Copy constructor
	~CScriptVarLink();
	void replaceWith(CScriptVar *newVar); ///< Replace the Variable pointed to
	void replaceWith(CScriptVarLink *newVar); ///< Replace the Variable pointed to (just dereferences)
};

class NativeFncBase;
typedef	std::map<std::string,CScriptVarLink*> SCRIPTVAR_CHILDS;
/// Variable class (containing a doubly-linked list of children)
class CScriptVar
{
public:
	CScriptVar(); ///< Create undefined
	CScriptVar(const std::string &varData, int varFlags); ///< User defined
	CScriptVar(const std::string &str); ///< Create a string
	CScriptVar(double varData);
	CScriptVar(int val);
	CScriptVar(bool val);
	~CScriptVar(void);

	CScriptVar *getReturnVar(); ///< If this is a function, get the result value (for use by native functions)
	void setReturnVar(CScriptVar *var); ///< Set the result value. Use this when setting complex return data as it avoids a deepCopy()
	CScriptVar *getParameter(const std::string &name); ///< If this is a function, get the parameter with the given name (for use by native functions)

	CScriptVarLink *findChild(const std::string &childName); ///< Tries to find a child with the given name, may return 0
	CScriptVarLink *findChildOrCreate(const std::string &childName, int varFlags=SCRIPTVAR_UNDEFINED); ///< Tries to find a child with the given name, or will create it with the given flags
	CScriptVarLink *findChildOrCreateByPath(const std::string &path); ///< Tries to find a child with the given path (separated by dots)
	CScriptVarLink *addChild(const std::string &childName, CScriptVar *child=NULL);
	CScriptVarLink *addChildNoDup(const std::string &childName, CScriptVar *child=NULL); ///< add a child overwriting any with the same name
	void removeChild(CScriptVar *child);
	void removeLink(CScriptVarLink *link); ///< Remove a specific link (this is faster than finding via a child)
	void removeAllChildren();
	CScriptVar *getArrayIndex(int idx); ///< The the value at an array index
	void setArrayIndex(int idx, CScriptVar *value); ///< Set the value at an array index
	int getArrayLength(); ///< If this is an array, return the number of items in it (else 0)
	int getChildren() { return Childs.size(); } ///< Get the number of children

	int getInt();
	bool getBool();
	double getDouble();
	const std::string &getString();
	std::string getParsableString(); ///< get Data as a parsable javascript string
	void setInt(int num);
	void setBool(bool val);
	void setDouble(double val);
	void setString(const std::string &str);
	void setUndefined();

	bool isInt() { return (flags&SCRIPTVAR_INTEGER)!=0; }
	bool isBool() { return (flags&SCRIPTVAR_BOOLEAN)!=0; }
	bool isDouble() { return (flags&SCRIPTVAR_DOUBLE)!=0; }
	bool isString() { return (flags&SCRIPTVAR_STRING)!=0; }
	bool isNumeric() { return (flags&SCRIPTVAR_NUMERICMASK)!=0; }
	bool isFunction() { return (flags&SCRIPTVAR_FUNCTION)!=0; }
	bool isObject() { return (flags&SCRIPTVAR_OBJECT)!=0; }
	bool isArray() { return (flags&SCRIPTVAR_ARRAY)!=0; }
	bool isNative() { return (flags&SCRIPTVAR_NATIVE)!=0; }
	bool isNative_ClassMemberFnc() { return (flags&SCRIPTVAR_NATIVE_MFNC)!=0; }
	bool isUndefined() { return (flags & SCRIPTVAR_VARTYPEMASK) == SCRIPTVAR_UNDEFINED; }
	bool isNull() { return (flags & SCRIPTVAR_NULL)!=0; }
	bool isNaN() { return (flags&SCRIPTVAR_NAN)!=0; }
	bool isInfinity() { return (flags&SCRIPTVAR_INFINITY)!=0; }
	bool isBasic() { return Childs.empty(); } ///< Is this *not* an array/object/etc

	CScriptVar *mathsOp(CScriptVar *b, int op); ///< do a maths op with another script variable
	void copyValue(CScriptVar *val); ///< copy the value from the value given
	CScriptVar *deepCopy(); ///< deep copy this node and return the result

	void trace(std::string indentStr = "", const std::string &name = ""); ///< Dump out the contents of this using trace
	std::string getFlagsAsString(); ///< For debugging - just dump a string version of the flags
	void getJSON(std::ostringstream &destination, const std::string linePrefix=""); ///< Write out all the JS code needed to recreate this script variable to the stream (as JSON)
	void setCallback(JSCallback callback, void *userdata);
	void setCallback(NativeFncBase *callback, void *userdata);

	SCRIPTVAR_CHILDS Childs;

	/// For memory management/garbage collection
	CScriptVar *ref(); ///< Add reference to this variable
	void unref(); ///< Remove a reference, and delete this variable if required
	int getRefs(); ///< Get the number of references to this script variable
protected:
	int refs; ///< The number of references held to this - used for garbage collection
	CScriptVarLink *__proto__;
	std::string data; ///< The contents of this variable if it is a string
	long intData; ///< The contents of this variable if it is an int
	double doubleData; ///< The contents of this variable if it is a double
	int flags; ///< the flags determine the type of the variable - int/double/string/etc
	union
	{
	JSCallback jsCallback; ///< Callback for native functions
	NativeFncBase *jsCallbackClass; ///< Wrapper for Class-Member-Functions as Callback for native functions
	};
	void *jsCallbackUserData; ///< user data passed as second argument to native functions

	void init(); ///< initialisation of data members

	/** Copy the basic data and flags from the variable given, with no
		* children. Should be used internally only - by copyValue and deepCopy */
	void copySimpleData(CScriptVar *val);

	friend class CTinyJS;
};

class NativeFncBase
{
public:
	virtual void operator()(CScriptVar *v, void *userdata)=0;
};
template <class native>
class NativeFnc : public NativeFncBase
{
public:
	NativeFnc(native *ClassPtr, void (native::*ClassFnc)(CScriptVar *, void *))
		: classPtr(ClassPtr), classFnc(ClassFnc){}
	void operator()(CScriptVar *v, void *userdata)
	{
		(classPtr->*classFnc)(v, userdata);
	}
	native *classPtr;
	void (native::*classFnc)(CScriptVar *v, void *userdata);
};

class CTinyJS {
public:
	CTinyJS();
	~CTinyJS();

	void execute(const std::string &code);
	/** Evaluate the given code and return a link to a javascript object,
	 * useful for (dangerous) JSON parsing. If nothing to return, will return
	 * 'undefined' variable type. CScriptVarLink is returned as this will
	 * automatically unref the result as it goes out of scope. If you want to
	 * keep it, you must use ref() and unref() */
	CScriptVarLink evaluateComplex(const std::string &code);
	/** Evaluate the given code and return a string. If nothing to return, will return
	 * 'undefined' */
	std::string evaluate(const std::string &code);

	/// add a native function to be called from TinyJS
	/** example:
		\code
			void scRandInt(CScriptVar *c, void *userdata) { ... }
			tinyJS->addNative("function randInt(min, max)", scRandInt, 0);
		\endcode

		or

		\code
			void scSubstring(CScriptVar *c, void *userdata) { ... }
			tinyJS->addNative("function String.substring(lo, hi)", scSubstring, 0);
		\endcode
		or

		\code
			class Class
			{
			public:
				void scSubstring(CScriptVar *c, void *userdata) { ... }
			};
			Class Instanz;
			tinyJS->addNative("function String.substring(lo, hi)", &Instanz, &Class::*scSubstring, 0);
		\endcode
	*/
	void addNative(const std::string &funcDesc, JSCallback ptr, void *userdata=0);
	void addNative(const std::string &funcDesc, NativeFncBase *ptr, void *userdata=0);
	template<class C>
	void addNative(const std::string &funcDesc, C *class_ptr, void(C::*class_fnc)(CScriptVar *, void *), void *userdata=0)
	{
		addNative(funcDesc, new NativeFnc<C>(class_ptr, class_fnc), userdata);
	}

	/// Get the value of the given variable, or return 0
	const std::string *getVariable(const std::string &path);

	/// throws an Error
	bool throwError(bool &execute, const std::string &message);

	/// Send all variables to stdout
	void trace();

	CScriptVar *root;   /// root of symbol table
private:
	CScriptLex *l;             /// current lexer
	int runtimeFlags;
	std::vector<CScriptVar*> scopes; /// stack of scopes when parsing
	CScriptVar *stringClass; /// Built in string class
	CScriptVar *objectClass; /// Built in object class
	CScriptVar *arrayClass; /// Built in array class
	CScriptVarLink *tempScope; /// is temporary used by the '.' and '[' operator NULL meens the root-scope
	CScriptVarLink *exeption; /// containing the exeption var by (runtimeFlags&RUNTIME_THROW) == true; 
	// parsing - in order of precedence
	CScriptVarLink *factor(bool &execute);
	CScriptVarLink *unary(bool &execute);
	CScriptVarLink *term(bool &execute);
	CScriptVarLink *expression(bool &execute);
	CScriptVarLink *binary_shift(bool &execute);
	CScriptVarLink *relation(bool &execute, int set=LEX_EQUAL, int set_n='<');
	CScriptVarLink *logic_binary(bool &execute, int op='|', int op_n1='^', int op_n2='&');
	CScriptVarLink *logic(bool &execute, int op=LEX_OROR, int op_n=LEX_ANDAND);
	CScriptVarLink *condition(bool &execute);
	CScriptVarLink *assignment(bool &execute);
	CScriptVarLink *base(bool &execute);
	void block(bool &execute);
	void statement(bool &execute);
	// parsing utility functions
	CScriptVarLink *parseFunctionDefinition();
	void parseFunctionArguments(CScriptVar *funcVar);

	CScriptVarLink *findInScopes(const std::string &childName); ///< Finds a child, looking recursively up the scopes
	/// Look up in any parent classes of the given object
	CScriptVarLink *findInParentClasses(CScriptVar *object, const std::string &name);
	CScriptVar *addNative(const std::string &funcDesc);
public: // native Functions
	void scEval(CScriptVar *c, void *data);

};

#endif
