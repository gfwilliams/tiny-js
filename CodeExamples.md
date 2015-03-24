The Variable 'result' evaluates to 'true' for each of these examples...

```
// simple for loop
var a = 0;
for (var i=1;i<10;i++) a = a + i;
result = a==45;
```

```
// simple function
function add(x,y) { return x+y; }
result = add(3,6)==9;
```


```
// functions in variables using JSON-style initialisation
var bob = { add : function(x,y) { return x+y; } };

result = bob.add(3,6)==9;
```

```
a = 345;    // an "integer", although there is only one numeric type in JavaScript
b = 34.5;   // a floating-point number
c = 3.45e2; // another floating-point, equivalent to 345
d = 0377;   // an octal integer equal to 255
e = 0xFF;   // a hexadecimal integer equal to 255, digits represented by the letters A-F may be upper or lowercase

result = a==345 && b*10==345 && c==345 && d==255 && e==255;
```

```
// references for arrays
var a;
a[0] = 10;
a[1] = 22;

b = a;

b[0] = 5;

result = a[0]==5 && a[1]==22 && b[1]==22;
```

```
// references with functions
var a = 42;
var b;
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
```

```
// built-in functions

foo = "foo bar stuff";
r = Math.rand();

parsed = Integer.parseInt("42");

aStr = "ABCD";
aChar = aStr.charAt(0);

obj1 = new Object();
obj1.food = "cake";
obj1.desert = "pie";

obj2 = obj1.clone();
obj2.food = "kittens";

result = foo.length()==13 && foo.indexOf("bar")==4 && foo.substring(8,13)=="stuff" && parsed==42 && 
         Integer.valueOf(aChar)==65 && obj1.food=="cake" && obj2.desert=="pie";
```