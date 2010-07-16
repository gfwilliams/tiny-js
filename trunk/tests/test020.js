// Test reported by sterowang, Variable attribute defines conflict with function.

function a (){};
b = {};
b.a = {};
a();

result = 1;
