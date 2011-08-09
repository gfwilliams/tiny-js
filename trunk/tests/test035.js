// the 'lf' in the printf caused issues writing doubles on some compilers
function Person(name) {
  this.name = name;
  this.kill = function() { this.name += " is dead"; };
}

var a = new Person("Kenny");
a.kill();
result = a.name == "Kenny is dead";

