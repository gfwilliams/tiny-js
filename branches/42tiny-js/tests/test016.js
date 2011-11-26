// Undefined/null from http://en.wikipedia.org/wiki/JavaScript_syntax
var testUndefined;        // variable declared but not defined, set to value of undefined
var testObj = {};

result = 1;
// 42-tiny-js change begin --->
// test variable exists NOT but testUndefined
//if ((""+test) != "undefined") result = 0; // test variable exists but value not defined, displays undefined
if ((""+testUndefined) != "undefined") result = 0; // test variable exists but value not defined, displays undefined
//<--- 42-tiny-js change end
if ((""+testObj.myProp) != "undefined") result = 0; // testObj exists, property does not, displays undefined
if (!(undefined == null)) result = 0;  // unenforced type during check, displays true
if (undefined === null) result = 0;// enforce type during check, displays false


if (null != undefined) result = 0;  // unenforced type during check, displays true
if (null === undefined) result = 0; // enforce type during check, displays false


