/**
 * Copyright (C) ARM Limited 2013. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Dll.h"

#include <stdio.h>

#if !defined(WIN32)
#include <dlfcn.h>
#endif

#include "Logging.h"

DLLHANDLE load_dll(const char *name) {
  DLLHANDLE handle;

#if defined (WIN32)
  handle = LoadLibrary(name);
#elif defined (__linux__) || defined(DARWIN)
  handle = dlopen(name, RTLD_NOW);
#endif

  if (handle == NULL) {
    char str[80];
    snprintf(str, 80, "Couldn't find shared library '%s'", name);
    logg->logMessage(str);
  }

  return handle;  // NULL on failure
}

void * load_symbol(DLLHANDLE handle, const char *name) {
  void * symbol;

#if defined (WIN32)
  symbol = GetProcAddress(handle, name);
#else
  symbol = dlsym(handle, name);
#endif

  if (symbol == NULL) {
    char str[80];
    snprintf(str, 80, "Couldn't find symbol '%s'", name);
    logg->logMessage(str);
  }

  return symbol;
}
