#include "Fusion_Functions.h"

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

void fs_setCodePage(CScriptVar *c, void* userdata) {
    // do nothing for now
	IGNORE_PARAMETER(c);
	IGNORE_PARAMETER(userdata);
    printf("Set Code Page to %s\n", c->getParameter("text")->getString().c_str());
}

void fs_spatial(CScriptVar* c, void* userdata) {
	IGNORE_PARAMETER(c);
	IGNORE_PARAMETER(userdata);
    wString unit{scGetString("b")}; 
    wString IN{"IN"};

    if(unit==IN) {
      scReturnDouble(scGetDouble("a")*25.00);
    }
    else
      scReturnDouble(scGetDouble("a"));
}

void fs_createFormat(CScriptVar* c, void* userdata) {
    // mockup
	IGNORE_PARAMETER(c);
	IGNORE_PARAMETER(userdata);
    if(scIsObject("a")){
         printf("CREATE FORMAT: ");
         printf("%s\n",c->getParameter("a")->getParameter("prefix")->getString().c_str());            
    }
    scReturnString(scGetString("a"));
}

void fs_createVariable(CScriptVar* c, void* userdata) {
    // mockup
	IGNORE_PARAMETER(c);
	IGNORE_PARAMETER(userdata);
    //scGetString("b");
    scReturnString(scGetString("a"));
}

void fs_createReferenceVariable(CScriptVar* c, void* userdata) {
    // mockup
	IGNORE_PARAMETER(c);
	IGNORE_PARAMETER(userdata);
    //scGetString("b");
    scReturnString(scGetString("a"));
}

void fs_createModal(CScriptVar* c, void* userdata) {
    // mockup
	IGNORE_PARAMETER(c);
	IGNORE_PARAMETER(userdata);
    //scGetString("b");
    scReturnString(scGetString("a"));
}
    
void registerFusionFunctions(CTinyJS* tinyJS) {
    tinyJS->execute("var MM=\"MM\";");
    tinyJS->execute("var IN=\"IN\";");
    tinyJS->execute("function toRad(a){ return Math.toRadians(a); };");
	tinyJS->addNative("function setCodePage(text)", fs_setCodePage, 0);
	tinyJS->addNative("function spatial(a,b)", fs_spatial, 0);
	tinyJS->addNative("function createFormat(a)", fs_createFormat, 0);
	tinyJS->addNative("function createVariable(a,b)", fs_createVariable, 0);
	tinyJS->addNative("function createReferenceVariable(a,b)", fs_createReferenceVariable, 0);
	tinyJS->addNative("function createModal(a,b)", fs_createModal, 0);

}
