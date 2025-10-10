// Array length test

myArray = [1, 2];
myArray.push(5);
myArray.push(1222);
myArray.push("hello");
myArray.push(7.5);

result = true;
result = result && (myArray.length == 6);
result = result && (myArray[0] == 1);
result = result && (myArray[1] == 2);
result = result && (myArray[2] == 5);
result = result && (myArray[3] == 1222);
result = result && (myArray[4] == "hello");
result = result && (myArray[5] == 7.5);