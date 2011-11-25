// Variable creation and scope from http://en.wikipedia.org/wiki/JavaScript_syntax
x = 0; // A global variable
var y = 'Hello!'; // Another global variable
// 42-tiny-js change begin --->
z = 0; // a var is only created automated by assignment
//<--- 42-tiny-js change end
function f(){
  var z = 'foxes'; // A local variable
  twenty = 20; // Global because keyword var is not used
  return x; // We can use x here because it is global
}
// The value of z is no longer available


// testing
blah = f();
result = blah==0 && z!='foxes' && twenty==20;
