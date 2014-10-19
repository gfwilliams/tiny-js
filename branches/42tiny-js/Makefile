CC=g++
CFLAGS=-c -g -Wall -rdynamic
LDFLAGS=-g -rdynamic

SOURCES=  \
TinyJS.cpp \
pool_allocator.cpp \
TinyJS_Functions.cpp \
TinyJS_MathFunctions.cpp \
TinyJS_StringFunctions.cpp \
TinyJS_Threading.cpp

OBJECTS=$(SOURCES:.cpp=.o)

all: run_tests Script

run_tests: run_tests.o $(OBJECTS)
	$(CC) $(LDFLAGS) run_tests.o $(OBJECTS) -o $@

Script: Script.o $(OBJECTS)
	$(CC) $(LDFLAGS) Script.o $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm run_tests Script run_tests.o Script.o $(OBJECTS)
