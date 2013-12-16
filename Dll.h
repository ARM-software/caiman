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

#ifndef DLL_H
#define DLL_H

#if defined(WIN32)
#include <windows.h>
#define DLLHANDLE  HINSTANCE
#else
#define DLLHANDLE  void *
#endif

// DLL / shared object routines

DLLHANDLE load_dll(const char *name);
void * load_symbol(DLLHANDLE handle, const char *name);

#endif // DLL_H
