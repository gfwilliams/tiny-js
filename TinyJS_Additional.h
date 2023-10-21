#ifndef TINYJS_ADDITIONALFUNCTIONS_H
#define TINYJS_ADDITIONALFUNCTIONS_H

#include "TinyJS.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

extern void registerAdditionalFunctions(CTinyJS *tinyJS);
extern void fileOpen(char* filename);
extern void fileClose();

#endif
