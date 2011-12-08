/* Javascript eval */

// 42-tiny-js change begin --->
// in JavaScript eval is not a JSON-parser
//myfoo = eval("{ foo: 42 }");
myfoo = eval("tmp = { foo: 42 }");
//<--- 42-tiny-js change end

result = eval("4*10+2")==42 && myfoo.foo==42;
