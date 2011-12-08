// mikael.kindborg@mobilesorcery.com - Function symbol is evaluated in bracket-less body of false if-statement
// 42-tiny-js change begin --->
var foo; // a var is only created automated by assignment
//<--- 42-tiny-js change end

if (foo !== undefined) foo();

result = 1;
