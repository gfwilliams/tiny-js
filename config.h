#ifndef _42TinyJS_config_h__
#define _42TinyJS_config_h__

/*
 * 42TinyJS
 *
 * A fork of TinyJS with the goal to makes a more JavaScript/ECMA compliant engine
 *
 * Authored By Armin Diedering <armin@diedering.de>
 *
 * Copyright (C) 2010-2012 ardisoft
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

//////////////////////////////////////////////////////////////////////////

/* POOL-ALLOCATOR 
 * ==============
 * To speed-up new & delete 42TinyJS adds an object-pool
 * The pool is activated by default.
 * To deactivate this stuff define NO_POOL_ALLOCATOR 
 */
//#define NO_POOL_ALLOCATOR

/*
 * for debugging-stuff you can define DEBUG_POOL_ALLOCATOR
 * if a memory-leak detected the allocator usage is printed to stderr
 */
//#define DEBUG_POOL_ALLOCATOR
/*
 * with define LOG_POOL_ALLOCATOR_MEMORY_USAGE
 * the allocator usage is always printed to stderr
 */
//#define LOG_POOL_ALLOCATOR_MEMORY_USAGE

// NOTE: _DEBUG or LOG_POOL_ALLOCATOR_MEMORY_USAGE implies DEBUG_POOL_ALLOCATOR

//////////////////////////////////////////////////////////////////////////

/* REGEXP-SUPPORT
 * ==============
 * The RegExp-support needs boost-regex or TR1-regex
 * To deactivate this stuff define NO_REGEXP 
 */
//#define NO_REGEXP

/* if NO_REGEXP not defined <regex> is included and std::regex is used
 * you can define HAVE_BOOST_REGEX and <boost/regex.hpp> is included and boost::regex is used
 */
//#define HAVE_BOOST_REGEX

/* or you can define HAVE_TR1_REGEX and <tr1/regex> is included and std::tr1::regex is used
 */
//#define HAVE_TR1_REGEX


//////////////////////////////////////////////////////////////////////////

/* LET-STUFF
 * =========
 * Redeclaration of LET-vars is not allowed in block-scopes.
 * But in the root- and functions-scopes it is currently allowed
 * In future ECMAScript versions this will be also in root-and functions-scopes forbidden
 * To enable the future behavior define PREVENT_REDECLARATION_IN_FUNCTION_SCOPES
 */
//#define PREVENT_REDECLARATION_IN_FUNCTION_SCOPES


//////////////////////////////////////////////////////////////////////////

/* MULTI-THREADING
 * ===============
 * 42TinyJS is basically thread-save.
 * You can run different or the same JS-code simultaneously in different instances of class TinyJS. 
 * The threading-stuff is currently only needed by the pool-allocator
 * to deactivate threading define NO_THREADING
 * NOTE: if NO_POOL_ALLOCATOR not defined you can not run JS-code simultaneously
 *       NO_POOL_ALLOCATOR implies NO_THREADING
 */
//#define NO_THREADING

/* on Windows the windows-threading-API is used by default.
 * on non-Windows (WIN32 is not defined) it is tried to use the POSIX pthread-API
 * to force the pthread-API define HAVE_PTHREAD (windows needs in this case 
 *   a pthread-lib e.g http://http://sourceware.org/pthreads-win32/)
 */
//#define HAVE_PTHREAD

/* you can implement your own custom thread-implementation.
 * to prevent the using of the win- or pthread-API define HAVE_CUSTOM_THREADING_IMPL
 */
//#define HAVE_CUSTOM_THREADING_IMPL

////////////////////////////////////////////////
// DO NOT MAKE CHANGES OF THE FOLLOWING STUFF //
////////////////////////////////////////////////

#if defined(NO_POOL_ALLOCATOR) && !defined(NO_THREADING)
#	define NO_THREADING
#endif

#if !defined(NO_POOL_ALLOCATOR) && defined(NO_THREADING)
#pragma message("\n***********************************************************************\n\
* You have defined NO_THREADING and not defined NO_POOL_ALLOCATOR\n\
* NOTE: you can not run JS-code simultaneously in different threads\n\
***********************************************************************\n")
#endif


#endif // _42TinyJS_config_h__
