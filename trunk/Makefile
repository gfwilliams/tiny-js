CC=g++
CFLAGS=-c -g -Wall
LDFLAGS=

SOURCES=  \
Script.cpp \
TinyJS.cpp \
TinyJS_Functions.cpp

OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=Script

#all: $(SOURCES) $(EXECUTABLE)
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@
