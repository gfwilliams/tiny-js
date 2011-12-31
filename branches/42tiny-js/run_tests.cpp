/*
 * TinyJS
 *
 * A single-file Javascript-alike engine
 *
 * Authored By Gordon Williams <gw@pur3.co.uk>
 *
 * Copyright (C) 2009 Pur3 Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * This is a program to run all the tests in the tests folder...
 */

#ifdef _DEBUG
#	ifdef _MSC_VER
#		ifdef USE_DEBUG_NEW
//#			include "targetver.h"
//#			include <afx.h>
//#			define new DEBUG_NEW
#		endif
#	else
#		define DEBUG_MEMORY 1
#	endif
#endif

#include "TinyJS.h"
#include "TinyJS_Functions.h"
#include "TinyJS_MathFunctions.h"
#include "TinyJS_StringFunctions.h"
#include <assert.h>
#include <sys/stat.h>
#include <string>
#include <sstream>
#include <stdio.h>

//#define WITH_TIME_LOGGER

#ifdef WITH_TIME_LOGGER
#	include "time_logger.h"
#endif
//#define INSANE_MEMORY_DEBUG

#ifdef INSANE_MEMORY_DEBUG
// needs -rdynamic when compiling/linking
#include <execinfo.h>
#include <malloc.h>
#include <map>
#include <vector>
using namespace std;

void **get_stackframe() {
  void **trace = (void**)malloc(sizeof(void*)*17);
  int trace_size = 0;

  for (int i=0;i<17;i++) trace[i]=(void*)0;
  trace_size = backtrace(trace, 16);
  return trace;
}

void print_stackframe(char *header, void **trace) {
  char **messages = (char **)NULL;
  int trace_size = 0;

  trace_size = 0;
  while (trace[trace_size]) trace_size++;
  messages = backtrace_symbols(trace, trace_size);

  printf("%s\n", header);
  for (int i=0; i<trace_size; ++i) {
    printf("%s\n", messages[i]);
  }
  //free(messages);
}

/* Prototypes for our hooks.  */
     static void *my_malloc_hook (size_t, const void *);
     static void my_free_hook (void*, const void *);
     static void *(*old_malloc_hook) (size_t, const void *);
     static void (*old_free_hook) (void*, const void *);

     map<void *, void **> malloced;

static void *my_malloc_hook(size_t size, const void *caller) {
    /* Restore all old hooks */
    __malloc_hook = old_malloc_hook;
    __free_hook = old_free_hook;
    /* Call recursively */
    void *result = malloc (size);
    /* we call malloc here, so protect it too. */
    //printf ("malloc (%u) returns %p\n", (unsigned int) size, result);
    malloced[result] = get_stackframe();

    /* Restore our own hooks */
    __malloc_hook = my_malloc_hook;
    __free_hook = my_free_hook;
    return result;
}

static void my_free_hook(void *ptr, const void *caller) {
    /* Restore all old hooks */
    __malloc_hook = old_malloc_hook;
    __free_hook = old_free_hook;
    /* Call recursively */
    free (ptr);
    /* we call malloc here, so protect it too. */
    //printf ("freed pointer %p\n", ptr);
    if (malloced.find(ptr) == malloced.end()) {
      /*fprintf(stderr, "INVALID FREE\n");
      void *trace[16];
      int trace_size = 0;
      trace_size = backtrace(trace, 16);
      backtrace_symbols_fd(trace, trace_size, STDERR_FILENO);*/
    } else
      malloced.erase(ptr);
    /* Restore our own hooks */
    __malloc_hook = my_malloc_hook;
    __free_hook = my_free_hook;
}

void memtracing_init() {
    old_malloc_hook = __malloc_hook;
    old_free_hook = __free_hook;
    __malloc_hook = my_malloc_hook;
    __free_hook = my_free_hook;
}

long gethash(void **trace) {
    unsigned long hash = 0;
    while (*trace) {
      hash = (hash<<1) ^ (hash>>63) ^ (unsigned long)*trace;
      trace++;
    }
    return hash;
}

void memtracing_kill() {
    /* Restore all old hooks */
    __malloc_hook = old_malloc_hook;
    __free_hook = old_free_hook;

    map<long, void**> hashToReal;
    map<long, int> counts;
    map<void *, void **>::iterator it = malloced.begin();
    while (it!=malloced.end()) {
      long hash = gethash(it->second);
      hashToReal[hash] = it->second;

      if (counts.find(hash) == counts.end())
        counts[hash] = 1;
      else
        counts[hash]++;

      it++;
    }

    vector<pair<int, long> > sorting;
    map<long, int>::iterator countit = counts.begin();
    while (countit!=counts.end()) {
      sorting.push_back(pair<int, long>(countit->second, countit->first));
      countit++;
    }

    // sort
    bool done = false;
    while (!done) {
      done = true;
      for (int i=0;i<sorting.size()-1;i++) {
        if (sorting[i].first < sorting[i+1].first) {
          pair<int, long> t = sorting[i];
          sorting[i] = sorting[i+1];
          sorting[i+1] = t;
          done = false;
        }
      }
    }


    for (int i=0;i<sorting.size();i++) {
      long hash = sorting[i].second;
      int count = sorting[i].first;
      char header[256];
      sprintf(header, "--------------------------- LEAKED %d", count);
      print_stackframe(header, hashToReal[hash]);
    }
}
#endif // INSANE_MEMORY_DEBUG
class end { // this is for VisualStudio debugging stuff. It's holds the console open up to ENTER is pressed
public:
	end() : active(false) {}
 	~end()
	{
		if(active) {
			printf("press Enter (end)");
			getchar();
		}
	}
	bool active;
} end;

bool run_test(const char *filename) {
  printf("TEST %s ", filename);
  struct stat results;
  if (!stat(filename, &results) == 0) {
    printf("Cannot stat file! '%s'\n", filename);
    return false;
  }
  int size = results.st_size;
  FILE *file = fopen( filename, "rb" );
  /* if we open as text, the number of bytes read may be > the size we read */
  if( !file ) {
     printf("Unable to open file! '%s'\n", filename);
     return false;
  }
  char *buffer = new char[size+1];
  long actualRead = fread(buffer,1,size,file);
  buffer[actualRead]=0;
  buffer[size]=0;
  fclose(file);

  CTinyJS s;
  registerFunctions(&s);
  registerMathFunctions(&s);
  registerStringFunctions(&s);
  s.getRoot()->addChild("result", s.newScriptVar(0));
#ifdef WITH_TIME_LOGGER
  TimeLoggerCreate(Test, true, filename);
#endif
  try {
    s.execute(buffer);
  } catch (CScriptException *e) {
    printf("ERROR: %s\n", e->text.c_str());
	delete e;
  }
  bool pass = s.getRoot()->getParameter("result")->getBool();
#ifdef WITH_TIME_LOGGER
  TimeLoggerLogprint(Test);
#endif

  if (pass)
    printf("PASS\n");
  else {
    char fn[64];
    sprintf(fn, "%s.fail.txt", filename);
    FILE *f = fopen(fn, "wt");
    if (f) {
      std::string symbols = s.getRoot()->getParsableString("", "   ");
      fprintf(f, "%s", symbols.c_str());
      fclose(f);
    }

    printf("FAIL - symbols written to %s\n", fn);
  }

  delete[] buffer;
  return pass;
}

int main(int argc, char **argv)
{
#ifdef INSANE_MEMORY_DEBUG
    memtracing_init();
#endif
  printf("TinyJS test runner\n");
  printf("USAGE:\n");
  printf("   ./run_tests [-k] test.js [test2.js]   : run tests\n");
  printf("   ./run_tests [-k]                      : run all tests\n");
  int arg_num = 1;
  bool runs = false;
  for(; arg_num<argc; arg_num++) {
    if(argv[arg_num][0] == '-') {
      if(strcmp(argv[arg_num], "-k")==0)
			end.active = true;
	 } else {
		run_test(argv[arg_num]);
		runs=true;
	 }
  }
  if (runs) {
    return 0;
  }

  int count = 0;
  int passed = 0;
  const char *mask[] = {"tests/test%03d%s.js", "tests/42tests/test%03d%s.js"};
#ifdef WITH_TIME_LOGGER
  TimeLoggerCreate(Tests, true);
#endif
  for(int js42 = 0; js42<2; js42++) {
    int test_num = 1;
    while (test_num<1000) {
      char fn[32];
      sprintf(fn, mask[js42], test_num, ".42");
      // check if the file exists - if not, assume we're at the end of our tests
      FILE *f = fopen(fn,"r");
      if (!f) {
        sprintf(fn, mask[js42], test_num, "");
        f = fopen(fn,"r");
        if(!f) break;
      }
      fclose(f);

      if (run_test(fn))
        passed++;
        count++;
        test_num++;
    }
  }
  printf("Done. %d tests, %d pass, %d fail\n", count, passed, count-passed);
#ifdef WITH_TIME_LOGGER
  TimeLoggerLogprint(Tests);
#endif
#ifdef INSANE_MEMORY_DEBUG
    memtracing_kill();
#endif
#ifdef _WIN32
#ifdef _DEBUG
//  _CrtDumpMemoryLeaks();
  /*
  no dump momoryleaks here
  _CrtSetDbgFlag(..) force dump memoryleake after call of all global deconstructors
*/
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
#endif
  return 0;
}
