// Array length test

myArray = [];
myArray.push(5);
myArray.push(1222);
myArray.push("hello");
myArray.push(7.55555);

result = true;
result = result && (myArray.length == 4);
result = result && (myArray[0] == 5);
result = result && (myArray[1] == 1222);
result = result && (myArray[2] == "hello");
result = result && (myArray[3] == 7.55555);