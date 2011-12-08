// references with functions

var a = 42;
// 42-tiny-js change begin --->
//var b; // in JavaScript it's not allowed to add properties to a var with type "undefined"
var b = [];
//<--- 42-tiny-js change end
b[0] = 43;

function foo(myarray) {
  myarray[0]++;
}

function bar(myvalue) {
  myvalue++;
}

foo(b);
bar(a);

result = a==42 && b[0]==44;
