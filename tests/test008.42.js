// functions in variables
// 42-tiny-js change begin --->
//var bob; // in JavaScript it's not allowed to add a child to a var with type "undefined"
var bob = {};
//<--- 42-tiny-js change end
bob.add = function(x,y) { return x+y; };

result = bob.add(3,6)==9;
