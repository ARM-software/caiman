/**
 * Copyright (C) Arm Limited 2013-2016. All rights reserved.
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

    logg.logMessage("Trying to load '%s'", name);
#if defined (WIN32)
    handle = LoadLibrary(name);
#elif defined (__linux__) || defined(DARWIN)
    handle = dlopen(name, RTLD_NOW);
#endif

    if (handle == NULL) {
        logg.logMessage("Couldn't find shared library '%s'", name);
    }

    return handle; // NULL on failure
}

void *load_symbol(DLLHANDLE handle, const char *name) {
    void *symbol;

    logg.logMessage("Trying to find symbol '%s'", name);
#if defined (WIN32)
                  symbol = (void *)GetProcAddress(handle, name);
#else
                  symbol = dlsym(handle, name);
#endif

                  if (symbol == NULL) {
                      logg.logMessage("Couldn't find symbol '%s'", name);
                  }

                  return symbol;
              }
