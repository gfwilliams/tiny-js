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

#include <assert.h>
#include <sys/stat.h>
#include <string>
#include <sstream>

#include "TinyJS.h"
#include "TinyJS_Functions.h"

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
  s.root->addChild(new CScriptVar("result","0",SCRIPTVAR_INTEGER));
  try {
    s.execute(buffer);
  } catch (CScriptException *e) {
    printf("ERROR: %s\n", e->text.c_str());
  }
  bool pass = s.root->findChildOrCreate("result")->getBool();

  if (pass)
    printf("PASS\n");
  else {
    char fn[64];
    sprintf(fn, "%s.fail.js", filename);
    FILE *f = fopen(fn, "wt");
    if (f) {
      std::ostringstream symbols;
      s.root->getJSON(symbols);
      fprintf(f, "%s", symbols.str().c_str());
      fclose(f);
    }

    printf("FAIL - symbols written to %s\n", fn);
  }

  delete[] buffer;
  return pass;
}

int main(int argc, char **argv)
{
  printf("TinyJS test runner\n");
  printf("USAGE:\n");
  printf("   ./run_tests test.js       : run just one test\n");
  printf("   ./run_tests               : run all tests\n");
  if (argc==2) {
    return !run_test(argv[1]);
  }

  int test_num = 1;
  int count = 0;
  int passed = 0;

  while (test_num<1000) {
    char fn[32];
    sprintf(fn, "tests/test%03d.js", test_num);
    // check if the file exists - if not, assume we're at the end of our tests
    FILE *f = fopen(fn,"r");
    if (!f) break;
    fclose(f);

    if (run_test(fn))
      passed++;
    count++;
    test_num++;
  }

  printf("Done. %d tests, %d pass, %d fail\n", count, passed, count-passed);

  return 0;
}
