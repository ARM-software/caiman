/**
 * Copyright (C) ARM Limited 2010-2016. All rights reserved.
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

#ifndef OLY_UTILITY_H
#define OLY_UTILITY_H

#include <stddef.h>

#ifdef WIN32
#define PATH_SEPARATOR '\\'
#define CAIMAN_PATH_MAX MAX_PATH
#if !defined(_MSC_VER) || _MSC_VER < 1900
#define snprintf _snprintf
#endif
#else
#include <limits.h>
#define PATH_SEPARATOR '/'
#define CAIMAN_PATH_MAX PATH_MAX
#endif

bool stringToBool(const char* string, bool defValue);
void stringToLower(char* string);
bool stringToLongLong(long long *const value, const char *str, const int base);
bool stringToLong(long *const value, const char *str, const int base);
bool stringToInt(int *const value, const char *str, const int base);
int getApplicationFullPath(char* path, int sizeOfPath);
char* readFromDisk(const char* file, unsigned int *size = NULL, bool appendNull = true);
int writeToDisk(const char* path, const char* file);
int appendToDisk(const char* path, const char* file);
int copyFile(const char* srcFile, const char* dstFile);
const char* getFilePart(const char* path);
char* getPathPart(char* path);

#endif // OLY_UTILITY_H
