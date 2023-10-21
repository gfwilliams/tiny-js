#include "TinyJS_Additional.h"
/* These are my added functions for things that seem like
   they should be in the standard javascript, are mentioned
   in the Fusion manual, yet, don't exist. If / when I verify
   that they are standard javascript and what the behavior
   should be, I will move them to the main body of JS functions.

   Jared McLaughlin ( tinyJS-F360Post ) 21 Oct 2023
*/

//CScriptVar shortcut macro
#define scIsInt(a)          ( c->getParameter(a)->isInt() )
#define scIsDouble(a)       ( c->getParameter(a)->isDouble() )  
#define scIsObject(a)       ( c->getParameter(a)->isObject() )  
#define scGetInt(a)         ( c->getParameter(a)->getInt() )
#define scGetDouble(a)      ( c->getParameter(a)->getDouble() )  
#define scGetString(a)      ( c->getParameter(a)->getString() )  
#define scReturnInt(a)      ( c->getReturnVar()->setInt(a) )
#define scReturnDouble(a)   ( c->getReturnVar()->setDouble(a) )  
#define scReturnString(a)   ( c->getReturnVar()->setString(a) )  

// we're never writing to more than one output file at a time
// this allows our writeln function to keep the same signature
int fd=-1; 

int myopen(const char* filename, int amode, int option)
{
#ifdef linux
    if (option != 0) {
        return open(filename, amode, option);
    }
    else {
        return open(filename, amode);
    }
#else
    char work[1024];
    strcpy(work, filename);
    int ptr = 0;
    while (work[ptr]) {
        if (work[ptr] == '/') {
            work[ptr] = '\\';
        }
        ptr++;
    }
    if (option != 0) {
        return _open(work, amode, option);
    }
    else {
        return _open(work, amode);
    }
#endif
}

void fileOpen(char *filename){

#ifdef linux
    fd=myopen("out.nc",O_CREAT | O_TRUNC | O_RDWR, S_IREAD | S_IWRITE );
#else
    fd=myopen("out.nc",O_CREAT | O_TRUNC | O_RDWR | O_BINARY, S_IREAD | S_IWRITE );
#endif
    if(fd<0) throw new CScriptException("Error opening file.");
}

// one at a time!
void fileClose() {
    close(fd);
}

void js_writeln(CScriptVar* c, void* userdata) {

    if(fd<0) {
        throw new CScriptException("No output file open.");
    }
    
    const char *source=scGetString("text").c_str(); 
    int len=strlen(source);
    char string[len+1];  
    strcpy(string,source);
    strcat(string,"\n");
    if(write(fd,string,len+1)<0) 
        throw new CScriptException("Error writing to file."); 
}
    
void registerAdditionalFunctions(CTinyJS* tinyJS) {
	tinyJS->addNative("function writeln(text)", js_writeln, 0);
}
