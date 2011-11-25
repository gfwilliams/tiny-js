// references for arrays

// 42-tiny-js change begin --->
//var a; // in JavaScript it's not allowed to add array elements to a var with type "undefined"
var a = [];
//<--- 42-tiny-js change end

a[0] = 10;
a[1] = 22;

b = a;

b[0] = 5;

result = a[0]==5 && a[1]==22 && b[1]==22;
