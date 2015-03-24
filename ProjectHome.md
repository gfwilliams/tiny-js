This project aims to be an extremely simple (~2000 line) JavaScript interpreter, meant for inclusion in applications that require a simple, familiar script language that can be included with no dependencies other than normal C++ libraries. It currently consists of two source files -  one containing the interpreter, another containing built-in functions such as String.substring.

TinyJS is not designed to be fast or full-featured. However it is great for scripting simple behaviour, or loading & saving settings.

I make absolutely no guarantees that this is compliant to JavaScript/EcmaScript standard. In fact I am sure it isn't. However I welcome suggestions for changes that will bring it closer to compliance without overly complicating the code, or useful test cases to add to the test suite.

Currently TinyJS supports:

  * Variables, Arrays, Structures
  * JSON parsing and output
  * Functions
  * Calling C/C++ code from JavaScript
  * Objects with Inheritance (not fully implemented)

Please see CodeExamples for examples of code that works...

For a list of known issues, please see the comments at the top of the TinyJS.cpp file.

To download TinyJS, just get SVN and run the following command:

` svn checkout http://tiny-js.googlecode.com/svn/trunk/ tiny-js-read-only `

There is also the 42tiny-js branch - this is maintained by Armin and provides a more fully-featured JavaScript implementation than SVN trunk.

` svn checkout http://tiny-js.googlecode.com/svn/branches/42tiny-js 42tiny-js `

TinyJS is released under an MIT licence.

## Internal Structure ##

TinyJS uses a Recursive Descent Parser, so there is no 'Parser Generator' required. It does not compile to an intermediate code, and instead executes directly from source code. This makes it quite fast for code that is executed infrequently, and slow for loops.

Variables, Arrays and Objects are stored in a simple linked list tree structure (42tiny-js uses a C++ Map). This is simple, but relatively slow for large structures or arrays.

## JavaScript for Microcontrollers ##

If you're after JavaScript for Microcontrollers, take a look at the [Espruino JavaScript Interpreter](http://www.espruino.com) - it is a complete re-write of TinyJS targeted at processors with extremely low RAM (8kb or more). It is currently available for a range of STM32 ARM Microcontrollers.

We've just launched a KickStarter for a board with it pre-installed at http://www.kickstarter.com/projects/48651611/espruino-javascript-for-things
If it completes successfully we'll be releasing all hardware and software as Open Source!